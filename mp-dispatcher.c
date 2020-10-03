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
 *
 * Pattern of usage:
 *
 * 
 */

/* The callback every active side should registr to receive responces */

/* This function translate MODULE ID to a string. For debug prints. */
static const char *mp_disp_module_name(module_type_e module_id)
{
	const char *module_names[] = {
		"MODULE_CONNECTION",
		"MODULE_REMOTE",
		"MODULE_CONFIG",
		"MODULE_PORTS",
		"MODULE_SHELL",
		"MODULE_TUNNEL",
		"MODULE_GUI",
		"MODULE_SECURITY",
		"MODULE_MPFS",
	};
	return (module_names[module_id]);
}

static disp_t *disp_t_alloc(void)
{
	disp_t *d = malloc(sizeof(disp_t));
	TESTP_MES(d, NULL, "Can't allocate dip_t\n");

	memset(d, 0, sizeof(disp_t));
	return (d);
}

int mp_disp_register(size_t module_id, mp_disp_cb_t func_send, mp_disp_cb_t func_recv)
{
	control_t *ctl = NULL;
	disp_t    *d   = NULL;

	TESTP_MES(func_send, EBAD, "Got NULL as dispatcher callback");
	TESTP_MES(func_recv, EBAD, "Got NULL as dispatcher callback");

	d = disp_t_alloc();
	TESTP(d, EBAD);

	d->disp_id = module_id;
	d->recv = func_recv;
	d->send = func_send;

	ctl = ctl_get_locked();
	htable_insert_by_int(ctl->dispatcher, module_id, NULL, d);
	ctl_unlock();

	DDD("Registred handlers for module %zu : %s\n", module_id, mp_disp_module_name(module_id));
	return (EOK);
}

/* This function analyses the JSOM message and detects 
   whether this message dedicated to this host or to a remote.
   Return:
   MES_DEST_ME - for this host
   MES_DEST_REMOTE  - for a remote
   MES_DEST_ERR on error */
static int mp_disp_is_mes_for_me(void *json)
{
	//control_t  *ctl    = NULL;
	const char *target = NULL;
	const char *my_uid = NULL;
	TESTP(json, MES_DEST_ERR);

	target = j_find_ref(json, JK_DISP_TGT_UID);
	TESTP(target, MES_DEST_ERR);

	/* If this message dedicated to ALL hosts, we accept it */
	if (0 == strncmp(target, "ALL", 3)) {
		return (MES_DEST_ME);
	}

	//ctl = ctl_get();
	my_uid = ctl_uid_get();

	if (0 == strncmp(target, my_uid, MP_LIMIT_UID_MAX)) {
		return (MES_DEST_ME);
	}

	return (MES_DEST_REMOTE);
}

/* This function analyses the JSOM message and detects 
   whether this message sent from this host or from a remote.
   Return:
   MES_SRC_ME - from this host
   MES_SRC_REMOTE  - from a remote host
   MES_DEST_ERR on error */
static int mp_disp_is_mes_from_me(void *json)
{
	const char *source = NULL;
	const char *my_uid = NULL;
	TESTP(json, MES_DEST_ERR);

	source = j_find_ref(json, JK_DISP_SRC_UID);
	TESTP(source, MES_DEST_ERR);

	my_uid = ctl_uid_get();

	if (0 == strncmp(source, my_uid, MP_LIMIT_UID_MAX)) {
		return (MES_SRC_ME);
	}

	return (MES_SRC_REMOTE);
}

/* Send a message: the message is running from its source to destination */
int mp_disp_send(void *json)
{
	control_t *ctl    = NULL;
	ssize_t   disp_id = -1;
	disp_t    *d      = NULL;
	int       error   = 0;

	TESTP(json, EBAD);

	error = mp_disp_is_mes_for_me(json);
	if (MES_DEST_ERR == error) {
		DE("Can't detect target host of the message\n");
		j_print(json, "The JSON without target host");
		j_rm(json);
		return (EBAD);
	}

	/* If this message is not for us (not for this machine) the MODULE is REMOTE - send it to remote host */
	if (MES_DEST_REMOTE == error) {
		disp_id = MODULE_REMOTE;
	}

	/* This message is for us, so find callbacks of target module */
	if (MES_DEST_ME == error) {

		/* Extract from JSON the target module ID */
		disp_id = j_find_int(json, JK_DISP_TGT_MODULE, &error);

		/* If we can't find it - return with error */
		if (EBAD == disp_id && EBAD == error) {
			DE("Can't find %s record in JSON\n", JK_DISP_SRC_MODULE);
			DE("JSON dump:\n");
			j_print(json, "No JK_DISP_SRC_MODULE in this JSON");
			return (EBAD);
		}
	}

	/* Now find the callbacks by ID */
	ctl = ctl_get();

	d = htable_find_by_int(ctl->dispatcher, disp_id);
	if (NULL == d) {
		DE("No handler is set for this type: %zd : %s\n", disp_id, mp_disp_module_name(disp_id));
		j_print(json, "The JSON failed is:");
		j_rm(json);
		return (EBAD);
	}

	/* If the MODULE registered the "send" function, we use it.
	   Else we drop the JSON and return error */
	if (NULL == d->send) {
		DE("Can't find 'send' handler for the module %s\n", mp_disp_module_name(disp_id));
		j_rm(json);
	}

	/* If we can't extract MODULE remote we are dead, this is an illigal situation */
	TESTP_ASSERT(d, "Can't find MODULE_REMOTE!\n");

	DDD("The request dedicated to module: %zd : %s\n", disp_id, mp_disp_module_name(disp_id));

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

	DDD("Received response message\n");

	/* If this message is not for us, or not to ALL we drop it */
	if (EOK != mp_disp_is_mes_for_me(json)) {
		DDD("Found a 'not for me' message - drop it\n");
		j_rm(json);
		return (EOK);
	}

	/* If we are here, this message is for us;
	*  however, we receive also messages from ourseves,
	*  if we send a message to ALL */
	if (MES_SRC_ME == mp_disp_is_mes_from_me(json)) {
		DDD("Found a 'from me' message - drop it\n");
		j_rm(json);
		return (EOK);
	}

	/* TODO: decide where to send it based on dest_id.
	*  When an module sends the answer, it set SOURCE MODULE ID
	*  of the request as TARGET MODULE ID as the reponce */
	disp_id = j_find_int(json, JK_DISP_TGT_MODULE, &error);

	if (EBAD == disp_id && EBAD == error) {
		DE("Can't find %s record in JSON\n", JK_DISP_SRC_MODULE);
		DE("JSON dump:\n");
		j_print(json, "No JK_DISP_SRC_MODULE in this JSON");
		j_rm(json);
		return (EBAD);
	}

	/* Now find the callbacks by ID */
	ctl = ctl_get();

	d = htable_find_by_int(ctl->dispatcher, disp_id);
	if (NULL == d) {
		DE("No handler is set for this type: %zd : %s\n", disp_id, mp_disp_module_name(disp_id));
		j_rm(json);
		return (EBAD);
	}

	DDD("The response dedicated to module: %zd : %s\n", disp_id, mp_disp_module_name(disp_id));

	return (d->send(json));
}

/* This function fills dispatcher related fields in JSON message */
int mp_disp_prepare_request(void *json, const char *target_host, module_type_e dest_module, module_type_e src_module, ticket_t ticket)
{
	int        rc      = EBAD;
	const char *uid_me = NULL;

	TESTP_ASSERT(json, "Got NULL json object");

	/* Add source host UID (this host) */
	uid_me = ctl_uid_get();
	DD("My uid is: %s\n", uid_me);
	rc = j_add_str(json, JK_DISP_SRC_UID, uid_me);
	TESTI_ASSERT(rc, "Can't add JK_UID_SRC\n");

	/* Add target host UID */
	rc = j_add_str(json, JK_DISP_TGT_UID, target_host);
	TESTI_ASSERT(rc, "Can't add JK_UID_DST\n");

	/* Add source module ID */
	rc = j_add_int(json, JK_DISP_SRC_MODULE, src_module);
	TESTI_ASSERT(rc, "Can't add JK_DISP_SRC_MODULE\n");

	/* Add target module ID */
	rc = j_add_int(json, JK_DISP_TGT_MODULE, dest_module);
	TESTI_ASSERT(rc, "Can't add JK_DISP_TGT_MODULE\n");

	/* If the ticket is not specified, we generate if for the module */
	if (0 == ticket) {
		mp_os_fill_random(&ticket, sizeof(ticket));
	}

	rc = j_add_int(json, JK_TICKET, ticket);
	TESTI_ASSERT(rc, "Can't add JK_TICKET\n");

	return (EOK);
}

j_t *mp_disp_create_request(const char *target_host, module_type_e dest_module, module_type_e source_module, ticket_t ticket)
{
	void *json = j_new();
	int  rc;

	TESTP_ASSERT(json, "Can't allocate json object");
	rc = mp_disp_prepare_request(json, target_host, dest_module, source_module, ticket);
	if (EOK != rc) {
		DE("Can't fill the request\n");
		j_rm(json);
		return (NULL);
	}

	return (json);
}

/* This function fills dispatcher related fields in JSON message */
int mp_disp_prepare_response(const void *json_req, void *json_resp)
{
	int        rc;
	int        var;
	int        error = 0;
	const char *uid;
	TESTP_ASSERT(json_req, "Got NULL json request  object");
	TESTP_ASSERT(json_resp, "Got NULL json response object");

	/* 1. Set the response target host UID - it is "source UID" of the request */
	uid = j_find_ref(json_req, JK_DISP_SRC_UID);
	TESTP_ASSERT(uid, "Can't find JK_UID_SRC in the request json\n");
	rc = j_add_str(json_resp, JK_DISP_TGT_UID, uid);
	TESTI_ASSERT(rc, "Can't find JK_UID_DST in request json\n");

	/* 2. Set source host UID - it is UID of this host */
	uid = ctl_uid_get();
	rc = j_add_str(json_resp, JK_DISP_SRC_UID, uid);
	TESTI_ASSERT(rc, "Can't find JK_UID_SRC in request json\n");

	/* 3. "Target module ID" of source is "Source module ID" in response */
	var = j_find_int(json_req, JK_DISP_TGT_MODULE, &error);
	TESTI_ASSERT(error, "Can't find JK_DISP_SRC_MODULE in request json\n");
	rc = j_add_int(json_resp, JK_DISP_SRC_MODULE, var);
	TESTI_ASSERT(rc, "Can't add JK_DISP_SRC_MODULE\n");

	/* 4. "Source module ID" in the request is "Target module ID" int the response */
	var = j_find_int(json_req, JK_DISP_SRC_MODULE, &error);
	TESTI_ASSERT(error, "Can't find JK_DISP_SRC_MODULE in request json\n");
	rc = j_add_int(json_resp, JK_DISP_TGT_MODULE, var);
	TESTI_ASSERT(rc, "Can't add JK_DISP_TGT_MODULE\n");

	/* 5. Ticket is the same, we copy it from the request to the response */
	var = j_find_int(json_req, JK_TICKET, &error);
	TESTI_ASSERT(error, "Can't find ticket in request json\n");
	rc = j_add_int(json_resp, JK_TICKET, var);
	TESTI_ASSERT(rc, "Can't add JK_TICKET\n");

	return (EOK);
}

/* This function cretes a new JSON 'response' message, and fills it using information from 'request' message */
j_t *mp_disp_create_response(const void *json_req)
{
	j_t *resp = j_new();
	TESTP(resp, NULL);
	if (EOK != mp_disp_prepare_response(json_req, resp)) {
		DE("Can't prepare response\n");
		j_rm(resp);
		return (NULL);
	}

	return (resp);
}

/* This function creates a new JSON 'ticket notification' message, and fills it using information from 'request' message */
j_t *mp_disp_create_ticket_answer(void *json_req)
{
	j_t *resp = j_new();
	TESTP(resp, NULL);
	if (EOK != mp_disp_prepare_response(json_req, resp)) {
		DE("Can't prepare response\n");
		j_rm(resp);
		return (NULL);
	}

	if (EOK != j_add_str(resp, JK_STATUS, JV_STATUS_WORK)) {
		DE("Can't add JV_STATUS_WORK into the ricket response");
		j_rm(resp);
		return (NULL);
	}

	return (resp);
}

/*** Tickets */

/* TODO. Not done, not tested.
   Save JSON: we save JSON struct, private data and callback pointer by ticket.
   We need it in module like MODULE_SHELL or MODULE_GUI, where we may receive
   the JSON struct from several external module, and when a response received
   we should know where to return it.
 */
#if 0 /* Not used yet 03/10/2020 17:16  */ 
int mp_disp_ticket_save(void *hash, void *json, void *priv){
	control_t *ctl    = NULL;
	ssize_t   disp_id = -1;
	disp_t    *d      = NULL;
	int       error   = 0;

	TESTP(json, EBAD);

	/* TODO: decide where to send it based on dest_id */
	disp_id = j_find_int(json, JK_DISP_TGT_MODULE, &error);

	if (EBAD == disp_id && EBAD == error) {
		DE("Can't find %s record in JSON\n", JK_DISP_SRC_MODULE);
		DE("JSON dump:\n");
		j_print(json, "No JK_DISP_SRC_MODULE in this JSON");
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
#endif /* SEB DEADCODE 03/10/2020 17:16 */
