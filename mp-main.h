#ifndef _SEC_CLIENT_MOSQ_H_
#define _SEC_CLIENT_MOSQ_H_
#include "mp-jansson.h"

#define TOPIC_MAX_LEN 1024

extern int mp_main_ticket_responce(/*@only@*/ const json_t *req, /*@only@*/const char *status, /*@only@*/const char *comment);

#endif /* _SEC_CLIENT_MOSQ_H_ */
