#ifndef CHECKIN_RESULT_H__INCLUDED
#define CHECKIN_RESULT_H__INCLUDED

#include "checkin.h"
#include <gtk/gtk.h>

typedef void (*checkin_result_done_cb)(GtkDialog *dialog, void *user_data);


int checkin_result_show(struct checkin *checkin);

#endif
