#ifndef _SEC_CLIENT_MOSQ_H_
#define _SEC_CLIENT_MOSQ_H_

#include "mp-limits.h"
#include "mp-jansson.h"


err_t mp_main_ticket_responce(/*@temp@*/const j_t *req, const char *status, const char *comment);

#endif /* _SEC_CLIENT_MOSQ_H_ */
