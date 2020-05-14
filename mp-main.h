#ifndef _SEC_CLIENT_MOSQ_H_
#define _SEC_CLIENT_MOSQ_H_
#include "mp-jansson.h"

#define TOPIC_MAX_LEN 1024

extern int mp_main_ticket_responce(/*@temp@*/ const json_t *req, /*@temp@*/const char *status, /*@temp@*/const char *comment);

#endif /* _SEC_CLIENT_MOSQ_H_ */
