#ifndef S_SPLINT_S
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/prctl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "openssl/ssl.h"
#include "openssl/err.h"

#endif

#include "mosquitto.h"
#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-cli.h"
#include "mp-config.h"
#include "mp-network.h"
#include "mp-requests.h"
#include "mp-communicate.h"
#include "mp-security.h"
#include "mp-os.h"
#include "mp-dict.h"
#include "mp-limits.h"
#include "mp-dispatcher.h"
#include "mp-htable.h"
#include "mp-dict.h"

/*
 * Dispatcher:
 * This code is resposible for JSON message routing.
 * Each message should have a ticket which is a large random number.
 * The dispatcher gets the message, analyses its origin and destination,
 * and adds keeps the ticket internally.
 * Then the message sent to its destination.
 * When a responce returned from a remote host, the dispatcher
 * analyzes it again, and decide where this message dedicated to.
 */

/* The callback every active side should registr to receive responces */

static disp_t *disp_t_alloc(void)
{
	disp_t *d = malloc(sizeof(disp_t));
	TESTP_MES(d, NULL, "Can't allocate dip_t\n");

	memset(d, 0, sizeof(disp_t));
	return (d);
}

/* Here every sender registers itself: the "src_id" is
  the source ID (integer) that the sender adds to JSON, and mp_dispatcher_cb_t
  is a callback to receive messages */
int mp_disp_register(size_t src_id, mp_disp_cb_t *func_send, mp_disp_cb_t *func_recv)
{
	control_t *ctl = NULL;
	disp_t    *d   = NULL;

	TESTP_MES(func_send, EBAD, "Got NULL as dispatcher callback");
	TESTP_MES(func_recv, EBAD, "Got NULL as dispatcher callback");

	d = disp_t_alloc();
	TESTP(d, EBAD);

	d->disp_id = src_id;
	d->recv = *func_recv;
	d->send = *func_send;

	ctl = ctl_get_locked();
	htable_insert_by_int(ctl->dispatcher, src_id, NULL, d);
	ctl_unlock();

	return (EOK);
}

/* This function analyses the JSOM message and detects 
   whether this message dedicated to this hot or to a remote.
   Return:
   0 - for this host
   1  - for a remote
   -1 - error */
int mp_disp_is_mes_for_me(void *json)
{
	control_t  *ctl    = NULL;
	const char *target = NULL;
	const char *my_uid = NULL;
	TESTP(json, -1);

	target = j_find_ref(json, JK_UID_DST);
	TESTP(target, -1);

	ctl = ctl_get();
	my_uid = ctl_uid_get();

	if (0 == strcmp(target, my_uid)) {
		return (0);
	}

	return (1);
}

/* Send a message: the message is running from its source to destination */
int mp_disp_send(void *json)
{
	control_t *ctl    = NULL;
	ssize_t   disp_id = -1;
	disp_t    *d      = NULL;
	int       error   = 0;

	TESTP(json, EBAD);

	/* TODO: decide where to send it based on dest_id */
	disp_id = j_find_int(json, JK_DISP_TGT_APP, &error);

	if (EBAD == disp_id && EBAD == error) {
		DE("Can't find %s record in JSON\n", JK_DISP_SRC_APP);
		DE("JSON dump:\n");
		j_print(json, "No JK_DISP_SRC_APP in this JSON");
		return (EBAD);
	}

	/* Now find the callbacks by ID */
	ctl = ctl_get();

	d = htable_find_by_int(ctl->dispatcher, disp_id);
	if (NULL == d) {
		//DE("No handler is set for this type: %z\n", disp_id);
		DE("No handler is set fot this type: %zd\n", disp_id);
		return (EBAD);
	}

	return (d->send(json));
}

/* A message received from remote / local comes here, we dispatch the messages */
int mp_disp_recv(void *json)
{
	control_t *ctl    = NULL;
	ssize_t   disp_id;
	disp_t    *d;
	int       error   = 0;

	TESTP(json, EBAD);

	/* TODO: decide where to send it based on dest_id */
	/* When an application sends the answer, it set SOURCE APP ID
	of the request as TARGET APP ID as the reponce */
	disp_id = j_find_int(json, JK_DISP_TGT_APP, &error);

	if (EBAD == disp_id && EBAD == error) {
		DE("Can't find %s record in JSON\n", JK_DISP_SRC_APP);
		DE("JSON dump:\n");
		j_print(json, "No JK_DISP_SRC_APP in this JSON");
		return (EBAD);
	}

	/* Now find the callbacks by ID */
	ctl = ctl_get();

	d = htable_find_by_int(ctl->dispatcher, disp_id);
	if (NULL == d) {
		DE("No handler is set for this type: %zd\n", disp_id);
		return (EBAD);
	}

	return (d->send(json));
}

/* This function fills dispatcher related fields in JSON message */
int mp_disp_prepare_request(void *json, char *target, app_type_e dest, app_type_e source, int ticket)
{
	int rc;
	TESTP_ASSERT(json, "Got NULL json object");

	rc = j_add_int(json, JK_DISP_SRC_APP, source);
	TESTI_ASSERT(rc, "Can't add JK_DISP_SRC_APP\n");

	rc = j_add_int(json, JK_DISP_TGT_APP, dest);
	TESTI_ASSERT(rc, "Can't add JK_DISP_TGT_APP\n");

	rc = j_add_int(json, JK_TICKET, ticket);
	TESTI_ASSERT(rc, "Can't add JK_TICKET\n");

	return (EOK);
}

/* This function fills dispatcher related fields in JSON message */
int mp_disp_prepare_response(void *json_req, void *json_resp)
{
	int rc;
	int var;
	int error = 0;
	TESTP_ASSERT(json_req, "Got NULL json request  object");
	TESTP_ASSERT(json_resp, "Got NULL json response object");

	/* 1. "Target app ID" of source is "Source app ID" in response */
	var = j_find_int(json_req, JK_DISP_TGT_APP, &error);
	TESTI_ASSERT(error, "Can't find JK_DISP_SRC_APP in request json\n");

	rc = j_add_int(json_resp, JK_DISP_SRC_APP, var);
	TESTI_ASSERT(rc, "Can't add JK_DISP_SRC_APP\n");

	/* 2. "Source app ID" in the request is "Target app ID" int the response */
	var = j_find_int(json_req, JK_DISP_SRC_APP, &error);
	TESTI_ASSERT(error, "Can't find JK_DISP_SRC_APP in request json\n");

	rc = j_add_int(json_resp, JK_DISP_TGT_APP, var);
	TESTI_ASSERT(rc, "Can't add JK_DISP_TGT_APP\n");

	/* Ticket is the same, we just copy it from request to response */
	var = j_find_int(json_req, JK_TICKET, &error);
	TESTI_ASSERT(error, "Can't find ticket in request json\n");
	rc = j_add_int(json_resp, JK_TICKET, var);
	TESTI_ASSERT(rc, "Can't add JK_TICKET\n");

	return (EOK);
}
/*** Tickets */

/* Save JSON */
int mp_disp_ticket_save(void *hash, void *json, void *priv)
{
	control_t *ctl    = NULL;
	ssize_t   disp_id = -1;
	disp_t    *d      = NULL;
	int       error   = 0;

	TESTP(json, EBAD);

	/* TODO: decide where to send it based on dest_id */
	disp_id = j_find_int(json, JK_DISP_TGT_APP, &error);

	if (EBAD == disp_id && EBAD == error) {
		DE("Can't find %s record in JSON\n", JK_DISP_SRC_APP);
		DE("JSON dump:\n");
		j_print(json, "No JK_DISP_SRC_APP in this JSON");
		return (EBAD);
	}

	/* Now find the callbacks by ID */
	ctl = ctl_get();

	d = htable_find_by_int(ctl->dispatcher, disp_id);
	if (NULL == d) {
		//DE("No handler is set for this type: %z\n", disp_id);
		DE("No handler is set fot this type: %zd\n", disp_id);
		return (EBAD);
	}

	return (d->send(json));
}