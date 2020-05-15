#ifndef _SEC_CLIENT_MOSQ_H_
#define _SEC_CLIENT_MOSQ_H_
#include "mp-jansson.h"

#define TOPIC_MAX_LEN 1024

err_t mp_main_ticket_responce(/*@temp@*/const json_t *req, const char *status, const char *comment);

#endif /* _SEC_CLIENT_MOSQ_H_ */
