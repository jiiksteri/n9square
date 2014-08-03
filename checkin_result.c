#include "checkin_result.h"

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

static void add_message(const char *message, void *_box)
{
	GtkWidget *label;

	label = gtk_label_new(message);
	gtk_widget_show_all(label);
	gtk_container_add(GTK_CONTAINER(_box), label);

	printf("\n%s(): (%p) '%s'", __func__, _box, message);
}

int checkin_result_show(struct checkin *checkin)
{
	GtkDialog *dialog;
	const char *venue;
	char *title_buf;
	size_t title_size;
	int res;

	venue = checkin_venue(checkin);
	title_size = strlen("Checkin: ") + strlen(venue) + 1;
	title_buf = malloc(title_size);
	if (title_buf != NULL) {
		snprintf(title_buf, title_size, "Checkin: %s", venue);
	}

	dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(title_buf ? title_buf : "Checkin",
							NULL,
							GTK_DIALOG_MODAL,
							"OK", GTK_RESPONSE_ACCEPT,
							NULL));

	printf("\n%s(): %p", __func__, gtk_dialog_get_content_area(dialog));
	checkin_message_foreach(checkin, add_message, gtk_dialog_get_content_area(dialog));

	res = gtk_dialog_run(dialog);
	gtk_widget_destroy(GTK_WIDGET(dialog));

	free(title_buf);

	return res;

}
