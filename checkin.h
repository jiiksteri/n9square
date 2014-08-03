#ifndef CHECKIN_H
#define CHECKIN_H

struct checkin;

int checkin_parse(struct checkin **checkinp, const char *str);
void checkin_free(struct checkin *checkin);

void checkin_message_foreach(struct checkin *checkin,
			     void (*cb)(const char *msg, void *user_data),
			     void *user_data);

const char *checkin_venue(struct checkin *checkin);

#endif
