
#include <stdlib.h>

#include <errno.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <json-glib/json-glib.h>
#include <webkit2/webkit2.h>

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
	char *access_token;
	char auth_uri[512];
	char redirect_uri[512];

	char *pending_checkin;
	int kill_browser;

	SoupSession *soup;

	GtkComboBoxText *search_type;
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

	free(tmp);
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

static void request_venues(GtkEntry *entry, struct app *app)
{
	SoupMessage *msg;
	char *uri;
	char *s;
	const char *needle;
	const char *query;
	gchar **pieces;

	pieces = g_strsplit(gtk_entry_get_text(entry), ":", 2);
	needle = pieces[0];
	query = pieces[1];

	msg = soup_form_request_new("GET", "https://api.foursquare.com/v2/venues/explore",
				    "client_id", app->creds.client_id,
				    "client_secret", app->creds.client_secret,
				    "v", FOURSQUARE_API_VERSION,
				    s = gtk_combo_box_text_get_active_text(app->search_type), needle,
				    "limit", "50",
				    "sortByDistance", "1",
				    "query", query ? query : "",
				    NULL);

	g_strfreev(pieces);
	free(s);

	soup_session_send_message(app->soup, msg);
	printf("%s(): %s -> %d\n", __func__,  uri = soup_uri_to_string(soup_message_get_uri(msg), TRUE), msg->status_code);
	free(uri);
	if (msg->status_code == 200) {
		s = content_string(msg->response_body);
		gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(app->venues)));
		parse_venues(app, s);
		free(s);
	}
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

static void do_checkin(struct app *app, const char *venue)
{
	SoupMessage *msg;
	GtkWidget *confirm;
	int response;
	int status = 0;
	char *s;

	confirm = gtk_dialog_new_with_buttons("Check in? Really?",
					      NULL, GTK_DIALOG_MODAL,
					      "Why not?", GTK_RESPONSE_ACCEPT,
					      "I shouldn't..", GTK_RESPONSE_REJECT,
					      NULL);

	switch (response = gtk_dialog_run(GTK_DIALOG(confirm))) {
	case GTK_RESPONSE_ACCEPT:
		msg = soup_form_request_new("POST", "https://api.foursquare.com/v2/checkins/add",
					    "v", FOURSQUARE_API_VERSION,
					    "venueId", venue,
					    "oauth_token", app->access_token,
					    NULL);
		soup_session_send_message(app->soup, msg);
		status = msg->status_code;
		s = soup_uri_to_string(soup_message_get_uri(msg), FALSE);
		printf("%s(): %s -> %d\n", __func__, s, status);
		free(s);
		s = content_string(msg->response_body);
		printf("%s(): ---- response start ----\n%s\n---- response end ----\n", __func__, s);
		free(s);
		break;
	default:
		printf("%s(): cancelled (%d)\n", __func__, response);
		break;
	}

	gtk_widget_destroy(confirm);
}

static const char *loadevent_name(WebKitLoadEvent event)
{
	const char *event_name;

	switch (event) {
	case WEBKIT_LOAD_STARTED:
		event_name = "[started]";
		break;
	case WEBKIT_LOAD_REDIRECTED:
		event_name = "[redirected]";
		break;
	case WEBKIT_LOAD_COMMITTED:
		event_name = "[committed]";
		break;
	case WEBKIT_LOAD_FINISHED:
		event_name = "[finished]";
		break;
	}

	return event_name;
}

static void grab_redirect(WebKitWebView *browser, WebKitLoadEvent event, struct app *app)
{
	const char *event_name;
	const char *uri;

	uri = webkit_web_view_get_uri(browser);
	event_name = loadevent_name(event);

	printf("%s(): %s %s\n", __func__, event_name, uri);

	if (event == WEBKIT_LOAD_REDIRECTED && strncmp(uri, app->redirect_uri, strlen(app->redirect_uri)) == 0) {
		/* Blind assumptions like this are why we cannot have nice things. */
		app->access_token = strdup(uri + strlen(app->redirect_uri) + strlen("#access_token="));
		printf("%s(): access token: '%s'\n", __func__, app->access_token);

		/* This will emit load-failed on the browser, where we do necessary cleanup */
		webkit_web_view_stop_loading(browser);

		/* If there was a pending checking, do it now. */
		if (app->pending_checkin) {
			do_checkin(app, app->pending_checkin);
			free(app->pending_checkin);
			app->pending_checkin = NULL;
		}

	} else if (event == WEBKIT_LOAD_FINISHED && app->kill_browser) {
		printf("%s(): killing browser as instructed\n", __func__);
		/* Nah, that's a bit.. sudden. */
		/* gtk_widget_destroy(GTK_WIDGET(browser)); */
		app->kill_browser = 0;
	}
}

static gboolean kill_browser(WebKitWebView *browser, WebKitLoadEvent event, gchar *failing_uri, gpointer error,
			     struct app *app)
{
	printf("%s(): %s %s\n", __func__, loadevent_name(event), failing_uri);
	app->kill_browser = 1;
	return FALSE;
}

static void do_auth(struct app *app)
{
	GtkContainer *win;
	GtkWidget *browser;

	win = GTK_CONTAINER(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_widget_set_size_request(GTK_WIDGET(win), 540, 520);
	browser = webkit_web_view_new();
	g_signal_connect(browser, "load-changed", G_CALLBACK(grab_redirect), app);
	/* We expect to see load-failed when we've grabbed the authorization token. */
	g_signal_connect(browser, "load-failed", G_CALLBACK(kill_browser), app);
	/* When the browser is killed, close the window too. */
	g_signal_connect_swapped(browser, "destroy", G_CALLBACK(gtk_widget_destroy), win);
	gtk_container_add(win, browser);
	printf("%s(): loading %s\n", __func__, app->auth_uri);
	gtk_widget_show_all(GTK_WIDGET(win));
	webkit_web_view_load_uri((WebKitWebView *)browser, app->auth_uri);

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

	if (app->access_token != NULL) {
		do_checkin(app, id);
	} else {
		app->pending_checkin = strdup(id);
		do_auth(app);
	}
}

static int dilbert_rng_digit(void)
{
	return 9;
}

int main(int argc, char **argv)
{
	struct app app;
	GtkWindow *win;
	GtkBox *box, *search_box;
	int err;
	char *armored_redirect;

	memset(&app, 0, sizeof(app));

	if ((err = read_creds(&app.creds)) != 0) {
		return err;
	}

	snprintf(app.redirect_uri, sizeof(app.redirect_uri),
		 "http://localhost:%d%d%d%d/EAT_SHIT",
		 dilbert_rng_digit(),
		 dilbert_rng_digit(),
		 dilbert_rng_digit(),
		 dilbert_rng_digit());

	armored_redirect = g_uri_escape_string(app.redirect_uri, NULL, FALSE);

	snprintf(app.auth_uri, sizeof(app.auth_uri),
		 "https://foursquare.com/oauth2/authenticate"
		 "?client_id=%s"
		 "&response_type=token"
		 "&redirect_uri=%s",
		 app.creds.client_id,
		 armored_redirect);

	g_free(armored_redirect);

	if ((app.soup = soup_session_new()) == NULL) {
		fprintf(stderr, "Failed to construct soup session\n");
		return 1;
	}

	gtk_init(&argc, &argv);

	app.search_type = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
	gtk_combo_box_text_append_text(app.search_type, "near");
	gtk_combo_box_text_append_text(app.search_type, "ll");
	gtk_combo_box_set_active(GTK_COMBO_BOX(app.search_type), 0);

	app.search = GTK_ENTRY(gtk_entry_new());
	g_signal_connect(app.search, "activate", G_CALLBACK(request_venues), &app);
	gtk_entry_set_activates_default(app.search, TRUE);

	search_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_box_pack_start(search_box, GTK_WIDGET(app.search_type), FALSE, FALSE, 0);
	gtk_box_pack_start(search_box, GTK_WIDGET(app.search), TRUE, TRUE, 0);


	app.venues = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING))));
	gtk_tree_view_append_column(app.venues, gtk_tree_view_column_new_with_attributes("",
											 gtk_cell_renderer_text_new(),
											 "text", VENUE_COLUMN_NAME,
											 NULL));
	g_signal_connect(app.venues, "row-activated", G_CALLBACK(checkin), &app);

	win = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_widget_set_size_request(GTK_WIDGET(win), 400, 500);
	box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(search_box), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), scrollable(app.venues), TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));

	gtk_window_set_title(win, "n9square");

	g_signal_connect(win, "destroy", gtk_main_quit, NULL);
	gtk_widget_show_all(GTK_WIDGET(win));

	gtk_main();

	g_object_unref(app.soup);

	return 0;
}
