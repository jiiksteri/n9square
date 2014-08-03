
#include <CUnit/CUnit.h>
#include "test.h"

#include <gtk/gtk.h>

#include "../checkin.h"
#include "../checkin_result.h"

static struct {
	GtkWidget *win;
	struct checkin *checkin;
	int response_code;
} ctx;

static gboolean kill_dialog_and_quit(gpointer unused)
{
	gtk_widget_destroy(ctx.win);
	gtk_main_quit();
	return FALSE;
}

static void run_dialog(GtkWidget *win, gpointer unused)
{
	ctx.response_code = checkin_result_show(ctx.checkin);
	g_idle_add(kill_dialog_and_quit, win);
}


static void test_checkin_dialog(void)
{
	char *s;
	int err;

	gtk_init(NULL, NULL);

	CU_ASSERT((err = read_file(&s, "test/checkin_response.json")) == 0);
	if (err != 0) {
		return;
	}

	CU_ASSERT((err = checkin_parse(&ctx.checkin, s)) == 0);
	if (err != 0) {
		fprintf(stderr, "checkin_parse(): %s\n", strerror(err));
		return;
	}

	ctx.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	ctx.response_code = GTK_RESPONSE_NONE;

	g_signal_connect(ctx.win, "show", G_CALLBACK(run_dialog), &ctx);
	//g_signal_connect(ctx.win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(ctx.win);
	gtk_main();

	CU_ASSERT(ctx.response_code == GTK_RESPONSE_ACCEPT);

	checkin_free(ctx.checkin);
}

void register_suite_checkin_result(void)
{
	CU_pSuite suite;

	suite = CU_add_suite("checkin_result", NULL, NULL);
	CU_add_test(suite, "Checkin dialog test", test_checkin_dialog);
}
