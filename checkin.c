#include "checkin.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-glib/json-glib.h>

struct checkin {
	JsonParser *parser;
	GSList *messages;
};



struct checkin_message_foreach_ctx {
	void (*cb)(const char *msg, void *user_data);
	void *user_data;
};

static void checkin_message_foreach_trampoline(gpointer _msg, gpointer _ctx)
{
	struct checkin_message_foreach_ctx *ctx = _ctx;
	ctx->cb((char *)_msg, ctx->user_data);
}


void checkin_message_foreach(struct checkin *checkin,
			     void (*cb)(const char *msg, void *user_data),
			     void *user_data)
{
	struct checkin_message_foreach_ctx ctx = {
		.cb = cb,
		.user_data = user_data,
	};

	g_slist_foreach(checkin->messages, checkin_message_foreach_trampoline, &ctx);
}


static void checkin_add_message(struct checkin *checkin, const char *message)
{
	/* No need to copy, the strings are kept alive as long long
	 * as checkin->parser is.
	 */
	checkin->messages = g_slist_append(checkin->messages, (gpointer)message);
}

static void dump_node_info(JsonObject *object,
			   const gchar *member_name, JsonNode *member_node,
			   gpointer user_data)
{
	printf("%s(): %s (%s)\n", __func__, member_name, json_node_type_name(member_node));
}

struct dump_context {
	const char *prefix;
	int depth;
};

static void print_prefix_and_padding(struct dump_context *ctx)
{
	int i;

	printf("\n%s: ", ctx->prefix);
	for (i = 0; i < ctx->depth; i++) {
		printf("  ");
	}
}

static void dump_node_tree_named(const char *name, JsonNode *node, struct dump_context *ctx);

static void dump_node_tree(JsonObject *object,
			   const char *name, JsonNode *node,
			   gpointer _ctx);

static void dump_node_tree_array(JsonArray *array,
				 guint index, JsonNode *node,
				 gpointer _ctx)
{

	struct dump_context *ctx = _ctx;
	char name[16];

	snprintf(name, sizeof(name), "[%u]", index);

	print_prefix_and_padding(ctx);
	ctx->depth++;
	dump_node_tree_named(name, node, ctx);
	ctx->depth--;
}

static void dump_node_tree_named(const char *name, JsonNode *node, struct dump_context *ctx)
{
	printf("%s(%s)", name, json_node_type_name(node));
	switch (JSON_NODE_TYPE(node)) {
	case JSON_NODE_VALUE:
		printf(" %s(", g_type_name(json_node_get_value_type(node)));
		switch (json_node_get_value_type(node)) {
		case G_TYPE_STRING:
			printf("'%s'", json_node_get_string(node));
			break;
		case G_TYPE_INT64:
			printf("%" PRId64, json_node_get_int(node));
			break;
		case G_TYPE_DOUBLE:
			printf("%g", json_node_get_double(node));
			break;
		case G_TYPE_BOOLEAN:
			printf("%s", json_node_get_boolean(node) ? "TRUE" : "FALSE");
			break;
		default:
			printf("unsupported");
			break;
		}
		printf(")");
		break;
	case JSON_NODE_OBJECT:
		ctx->depth++;
		json_object_foreach_member(json_node_get_object(node), dump_node_tree, ctx);
		ctx->depth--;
		break;
	case JSON_NODE_ARRAY:
		ctx->depth++;
		json_array_foreach_element(json_node_get_array(node), dump_node_tree_array, ctx);
		ctx->depth--;
		break;
	default:
		printf(" (Unsupported node type)");
		break;
	}
}

static void dump_node_tree(JsonObject *object,
			   const char *name, JsonNode *node,
			   gpointer _ctx)
{
	struct dump_context *ctx = _ctx;

	print_prefix_and_padding(ctx);
	dump_node_tree_named(name, node, ctx);
}

static inline int is_string(JsonNode *node)
{
	return
		JSON_NODE_TYPE(node) == JSON_NODE_VALUE &&
		json_node_get_value_type(node) == G_TYPE_STRING;
}

static void add_score(JsonArray *array, guint index, JsonNode *score_node, gpointer _checkin)
{
	struct checkin *checkin = _checkin;
	JsonNode *message_node;

	if (JSON_NODE_TYPE(score_node) == JSON_NODE_OBJECT) {
		message_node = json_object_get_member(json_node_get_object(score_node), "message");
		if (is_string(message_node)) {
			checkin_add_message(checkin, json_node_get_string(message_node));
		}
	}
}

static void parse_checkin_object(struct checkin *checkin, JsonObject *checkin_object)
{
	JsonNode *score_node, *scores_array;

	score_node = json_object_get_member(checkin_object, "score");
	if (score_node != NULL && JSON_NODE_TYPE(score_node) == JSON_NODE_OBJECT) {
		scores_array = json_object_get_member(json_node_get_object(score_node), "scores");
		if (scores_array != NULL && JSON_NODE_TYPE(scores_array) == JSON_NODE_ARRAY) {
			json_array_foreach_element(json_node_get_array(scores_array),
						   add_score, checkin);
		}
	}
}

static void parse_notification(JsonArray *array, guint index, JsonNode *notification_node,
			       gpointer _checkin)
{
	JsonNode *item, *message, *type;

	if (JSON_NODE_TYPE(notification_node) == JSON_NODE_OBJECT) {
		type = json_object_get_member(json_node_get_object(notification_node), "type");
		if (is_string(type) && strcmp(json_node_get_string(type), "message") == 0) {
			item = json_object_get_member(json_node_get_object(notification_node), "item");
			if (JSON_NODE_TYPE(item) == JSON_NODE_OBJECT) {
				message = json_object_get_member(json_node_get_object(item), "message");
				if (is_string(message)) {
					checkin_add_message(_checkin, json_node_get_string(message));
				}
			}
		}
	}
}

static void parse_response_node(struct checkin *checkin, JsonNode *node)
{
	struct dump_context ctx = { .prefix = __func__, .depth = 0 };
	JsonNode *checkin_node, *notifications_node;

	checkin_node = json_object_get_member(json_node_get_object(node), "checkin");
	if (checkin_node != NULL && JSON_NODE_TYPE(checkin_node) == JSON_NODE_OBJECT) {
		parse_checkin_object(checkin, json_node_get_object(checkin_node));
	}
	notifications_node = json_object_get_member(json_node_get_object(node), "notifications");
	if (notifications_node != NULL && JSON_NODE_TYPE(notifications_node) == JSON_NODE_ARRAY) {
		json_array_foreach_element(json_node_get_array(notifications_node),
					   parse_notification, checkin);
	}

	dump_node_tree((JsonObject *)NULL, "response", node, &ctx);

}

static void parse_notifications_node(struct checkin *checkin, JsonNode *node)
{
	struct dump_context ctx = { .prefix = __func__, .depth = 0 };
	dump_node_tree((JsonObject *)NULL, "notifications", node, &ctx);
	printf("\n");
}

static void parse_meta_node(struct checkin *checkin, JsonNode *node)
{
	struct dump_context ctx = { .prefix = __func__, .depth = 0 };
	dump_node_tree((JsonObject *)NULL, "meta", node, &ctx);
	printf("\n");
}

int checkin_parse(struct checkin **checkinp, const char *str)
{
	struct checkin *checkin;
	JsonObject *root;

	checkin = *checkinp = malloc(sizeof(*checkin));
	if (checkin == NULL) {
		return errno;
	}
	memset(checkin, 0, sizeof(*checkin));

	checkin->parser = json_parser_new();

	if (!json_parser_load_from_data(checkin->parser, str, strlen(str), NULL)) {
		g_object_unref(checkin->parser);
		checkin->parser = NULL;
		free(checkin);
		return EINVAL;
	}

	root = json_node_get_object(json_parser_get_root(checkin->parser));
	json_object_foreach_member(root, dump_node_info, NULL);

	parse_meta_node(checkin, json_object_get_member(root, "meta"));
	parse_notifications_node(checkin, json_object_get_member(root, "notifications"));
	parse_response_node(checkin, json_object_get_member(root, "response"));


	return 0;
}

void checkin_free(struct checkin *checkin)
{
	if (checkin->messages != NULL) {
		g_slist_free(checkin->messages);
		checkin->messages = NULL;
	}

	if (checkin->parser != NULL) {
		g_object_unref(checkin->parser);
		checkin->parser = NULL;
	}

	free(checkin);
}


int checkin_notification_count(struct checkin *checkin)
{
	return 0;
}

const struct checkin_info_item *checkin_notification_get(struct checkin *checkin, int ind)
{
	return 0;
}

