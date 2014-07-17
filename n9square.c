
#include <stdlib.h>

#include <errno.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <json-glib/json-glib.h>

#define FOURSQUARE_API_VERSION "20140715"

struct credentials {
	char client_id[49];
	char client_secret[49];
};

enum {
	VENUE_COLUMN_ID,
	VENUE_COLUMN_NAME,
};

struct app {
	struct credentials creds;
	SoupSession *soup;

	GtkEntry *search;
	GtkTreeView *venues;
};

static int read_conf(char *buf, size_t sz, const char *file)
{
	char path[512];
	int err;
	int fd;
	int n;

	snprintf(path, sizeof(path), "%s/.n9square/%s", getenv("HOME"), file);
	path[sizeof(path) - 1] = '\0';

	if ((fd = open(path, O_RDONLY)) == -1) {
		perror("open()");
		return errno;
	}

	err = 0;
	buf[sz - 1] = '\0';
	if ((n = read(fd, buf, sz - 1)) != sz - 1) {
		if (n == -1) {
			err = errno;
			perror(path);
		} else {
			fprintf(stderr, "%s: short read (%d/%zd)\n",
				path, n, sz);
		}
	}

	close(fd);

	return err;
}

static SoupMessage *build_message(struct credentials *creds, const char *method, const char *uri_base)
{
	char *uri_buf;
	size_t sz;

	SoupMessage *msg = NULL;

	sz = strlen(uri_base) + 1 +
		strlen("client_id=") + sizeof(creds->client_id) + 1 +
		strlen("client_secret=") + sizeof(creds->client_secret) + 1;
	uri_buf = malloc(sz);
	if (uri_buf != NULL) {
		snprintf(uri_buf, sz, "%s&client_id=%s&client_secret=%s",
			 uri_base, creds->client_id, creds->client_secret);
		msg = soup_message_new(method, uri_buf);
		free(uri_buf);
	}

	return msg;
}

static char *content_string(SoupMessageBody *body)
{
	SoupBuffer *buf;
	const guint8 *data;
	gsize length = 0;
	char *copy;

	buf = soup_message_body_flatten(body);
	soup_buffer_get_data(buf, &data, &length);
	copy = strndup((char *)data, length);
	soup_buffer_free(buf);

	return copy;

}

static void add_venue(struct app *app, const gchar *id, const gchar *name, const gchar *address)
{
	GtkTreeIter iter;
	GtkListStore *store;
	size_t len;
	char *tmp = NULL;

	printf("%s(): %s %s (%s)\n", __func__, id, name, address);

	len = snprintf(tmp, 0, "%s (%s)", name, address);
	tmp = malloc(len + 1);
	snprintf(tmp, len + 1, "%s (%s)", name, address);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(app->venues));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			   VENUE_COLUMN_ID, id,
			   VENUE_COLUMN_NAME, tmp,
			   -1);
}

static void parse_venue_item(JsonArray *array, guint index_, JsonNode *element_node, gpointer _app)
{
	JsonObject *venue;
	JsonNode *address_node;

	const gchar *id;
	const gchar *name;
	const gchar *address;

	venue = json_node_get_object(json_object_get_member(json_node_get_object(element_node), "venue"));
	id = json_node_get_string(json_object_get_member(venue, "id"));
	name = json_node_get_string(json_object_get_member(venue, "name"));
	address_node = json_object_get_member(json_node_get_object(json_object_get_member(venue, "location")), "address");
	address = address_node ? json_node_get_string(address_node) : "No address";

	add_venue((struct app *)_app, id, name, address);

}

static void parse_random_member(JsonObject *obj, const gchar *member_name, JsonNode *member_node, gpointer _app)
{
	/* printf("%s(): %s (%s)\n", __func__, member_name, json_node_type_name(member_node)); */
	if (JSON_NODE_TYPE(member_node) == JSON_NODE_OBJECT) {
		if (strcmp(member_name, "response") == 0) {
			json_object_foreach_member(json_node_get_object(member_node), parse_random_member, _app);
		}
	} else if (JSON_NODE_TYPE(member_node) == JSON_NODE_ARRAY) {
		if (strcmp(member_name, "groups") == 0) {
			/* We just know there's a single JsonObject element in this one.. */
			json_object_foreach_member(json_node_get_object(json_array_get_element(json_node_get_array(member_node), 0)),
						   parse_random_member, _app);
		} else if (strcmp(member_name, "items") == 0) {
			json_array_foreach_element(json_node_get_array(member_node), parse_venue_item, _app);
		}
	}
}

static void parse_venues(struct app *app, const char *content)
{
	JsonParser *parser;

	if (0) {
		printf("%s():\n"
		       "--- CONTENT START ---\n%s\n--- CONTENT END ---\n",
		       __func__, content);
	}

	parser = json_parser_new();

	json_parser_load_from_data(parser, content, strlen(content), NULL);

	json_object_foreach_member(json_node_get_object(json_parser_get_root(parser)),
				   parse_random_member, app);

	g_object_unref(parser);

}



#define VENUES_FMT "https://api.foursquare.com/v2/venues/explore?v=" FOURSQUARE_API_VERSION "&near=%s"

static void request_venues(GtkEntry *entry, struct app *app)
{
	SoupMessage *msg;
	char *uri;
	char *s;
	char *uri_buf;
	const char *needle;
	size_t sz;

	needle = gtk_entry_get_text(entry);
	sz = strlen(VENUES_FMT) + strlen(needle);

	/* That's a few bytes extra, like */
	uri_buf = malloc(sz);
	if (uri_buf == NULL) {
		perror("request_venues(): malloc failure");
		return;
	}

	snprintf(uri_buf, sz, VENUES_FMT, needle);

	msg = build_message(&app->creds, "GET", uri_buf);
	soup_session_send_message(app->soup, msg);
	printf("%s(): %s -> %d\n", __func__,  uri = soup_uri_to_string(soup_message_get_uri(msg), TRUE), msg->status_code);
	if (msg->status_code == 200) {
		s = content_string(msg->response_body);
		gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(app->venues)));
		parse_venues(app, s);
		free(s);
	}

	free(uri_buf);
}



static int read_creds(struct credentials *creds)
{
	int err;

	memset(creds, 0, sizeof(*creds));

	if ((err = read_conf(creds->client_id, sizeof(creds->client_id), "client_id")) != 0) {
		return errno;
	}

	return read_conf(creds->client_secret, sizeof(creds->client_secret), "client_secret");
}

static GtkWidget *scrollable(GtkTreeView *view)
{
	GtkWidget *scrolled_win;

	scrolled_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_win), GTK_WIDGET(view));
	return scrolled_win;
}

static void checkin(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, struct app *app)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *id;
	const gchar *name;

	model = gtk_tree_view_get_model(tree_view);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter,
			   VENUE_COLUMN_ID, &id,
			   VENUE_COLUMN_NAME, &name,
			   -1);

	printf("%s(): NOT IMPLEMENTED (venue: %s %s)\n", __func__, id, name);
}

int main(int argc, char **argv)
{
	struct app app;
	GtkWindow *win;
	GtkBox *box;
	int err;

	memset(&app, 0, sizeof(app));

	if ((err = read_creds(&app.creds)) != 0) {
		return err;
	}

	if ((app.soup = soup_session_new()) == NULL) {
		fprintf(stderr, "Failed to construct soup session\n");
		return 1;
	}

	gtk_init(&argc, &argv);

	app.search = GTK_ENTRY(gtk_entry_new());
	g_signal_connect(app.search, "activate", G_CALLBACK(request_venues), &app);
	gtk_entry_set_activates_default(app.search, TRUE);

	app.venues = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING))));
	gtk_tree_view_append_column(app.venues, gtk_tree_view_column_new_with_attributes("",
											 gtk_cell_renderer_text_new(),
											 "text", VENUE_COLUMN_NAME,
											 NULL));
	g_signal_connect(app.venues, "row-activated", G_CALLBACK(checkin), &app);

	win = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_widget_set_size_request(GTK_WIDGET(win), 400, 500);
	box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(app.search), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), scrollable(app.venues), TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));

	gtk_window_set_title(win, "n9square");

	g_signal_connect(win, "destroy", gtk_main_quit, NULL);
	gtk_widget_show_all(GTK_WIDGET(win));

	gtk_main();

	g_object_unref(app.soup);

	return 0;
}
