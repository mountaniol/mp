#define _GNU_SOURCE             /* See feature_test_macros(7) */
#ifndef S_SPLINT_S
	#include <pthread.h>
	#include <sys/prctl.h>

	#include <sys/socket.h>
	#include <sys/un.h>
	#include <unistd.h>
#endif

#include "mp-debug.h"
#include "mp-dispatcher.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-cli.h"
#include "mp-communicate.h"
#include "mp-net-utils.h"
#include "mp-dict.h"
#include "mp-ports.h"
#include "buf_t/buf_t.h"

/*
 * README
 *
 * This code serves requests from shell application.
 * The shell application doesn't have access to the infrastructure of MPD:
 * it is an external application.
 * So it forms its own special requests and sends it using UNIX socket.
 *
 *
 * Such a request:
 *
 * 1. Doesn not have UID (because only MPD knows the UID)
 * (MPD - MP Daemon [this file is part of MPD])
 * 2. Doesn't have direct access to information about connected hosts (the shell application asks this information from the local MPD)
 * 3. It may reuest information from the local MPD, or may ask to request the information from a remote machine
 *
 * So the role of CLI it to accept the request from the shell application,
 * parse it and either return information from the local machine,
 * or send a requrst to a remote machine, receive answer and return this
 * answer to the shell application.
 *
 * TICKET
 *
 * A "ticket" used to identify a request. Let's see what happens when the shell application request
 * intformation from a remote machine:
 *
 * [shell] : REQUEST --> [cli] REQUEST --> [dispatcher] REQUEST --> [remote machine]
 *
 * and back:
 *
 * [remote machine] RESPONSE --> [this machine dispatcher] RESPONSE --> [cli] RESPONCE --> [shell application] RESPONCE
 *
 * We should associate REQUEST and REPONCE; to do so we use the ticket.
 * Ticket is an unique integer ID.
 * We know that we have reponce to a specific request by this ID.
 *
 */

err_t mp_cli_send_to_cli(/*@temp@*/const j_t *root)
{
	int                sd         = -1;
	ssize_t            rc         = -1;
	struct sockaddr_un serveraddr;
	buf_t              *buf       = NULL;

	DDD("start");

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd < 0) {
		DE("Failed\n");
		perror("socket() failed");
		return (EBAD);
	}

	DDD("Opened socket\n");

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, CLI_SOCKET_PATH_CLI);

	DDD("Before connect\n");
	rc = (ssize_t)connect(sd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (rc < 0) {
		DE("Failed\n");
		perror("connect() failed");
		return (EBAD);
	}

	DDD("Connected\n");

	buf = j_2buf(root);
	TESTP_MES(buf, EBAD, "Can't encode JSON object\n");

	rc = send(sd, buf->data, buf_used(buf), 0);
	if (EOK != buf_free(buf)) {
		DE("Can't remove buf_t: probably passed NULL pointer?\n");
	}
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (EBAD);
	}

	if (0 != close(sd)) {
		DE("Can't close socket\n");
		return (EBAD);
	}

	return (EOK);
}

/* Get this machine info */
/*@null@*/ static j_t *mp_cli_dup_self_info_l(void)
{
	/*@temp@*/control_t *ctl = NULL;
	/*@temp@*/j_t *ret = NULL;

	DDD("Starting\n");
	ctl = ctl_get_locked();
	ret = j_dup(ctl->me);
	ctl_unlock();
	return (ret);
}

/* Copy this machine + this router ports configuration  only */
/*@null@*/ static j_t *mp_cli_get_ports_l(void)
{
	/*@shared@*/control_t *ctl = NULL;
	/*@temp@*/j_t *resp;
	/*@temp@*/j_t *ports;
	DDD("Starting\n");

	ctl = ctl_get_locked();
	ports = j_find_j(ctl->me, "ports");
	if (NULL == ports) {
		DE("Can't extract array 'ports'\n");
		ctl_unlock();
		return (NULL);
	}
	resp = j_dup(ports);
	ctl_unlock();
	return (resp);
}

/* Get this machine list of all sources and targets */
/*@null@*/ static j_t *mp_cli_get_list_l(void)
{
	/*@shared@*/control_t *ctl = NULL;
	/*@temp@*/j_t *resp;
	DDD("Starting\n");
	ctl = ctl_get_locked();
	resp = j_dup(ctl->hosts);
	ctl_unlock();
	//j_print(resp, "Copied ctl->hosts");
	//j_print(resp, "Orig   ctl->hosts");
	return (resp);
}

/* Send the shell request to remote machine */
/* This function sends only; the response is asynchronous */
/*@null@*/ static j_t *mp_cli_execute_req(/*@temp@*/j_t *root)
{
	/*@shared@*/control_t *ctl = NULL;
	int rc = EBAD;
	/*@temp@*/j_t *resp = NULL;

	ctl = ctl_get_locked();
	rc = j_add_str(root, JK_DISP_SRC_UID, ctl_uid_get());
	ctl_unlock();
	TESTI_MES(rc, NULL, "Can't add my UID into JSON");

	DDD("Calling send_request_to_open_port\n");
	//j_print(root, "Sending request to open a port:");
	if (NULL != ctl->mosq) {
		rc = mp_communicate_send_request(root);
	}

	resp = j_new();
	TESTP(resp, NULL);

	if (EOK == rc) {
		if (EOK != j_add_str(resp, JK_STATUS, JV_OK)) DE("Can't add JV_OK status\n");
	} else {
		if (EOK != j_add_str(resp, JK_STATUS, JV_BAD)) DE("Can't add JV_BAD status\n");
	}

	return (resp);
}

/*
 * The parser:
 * We received a JSON request from a shell client;
 * Here we detect what the client wants.
 * We execute action / send the request to remote, etc.
 * Then we create a response and send it back to the client.
 */
/*@null@*/ static j_t *mp_cli_parse_command(/*@temp@*/j_t *root)
{
	TESTP_MES(root, NULL, "Got NULL");

	//DD("Found '%s' type\n", j_find_ref(root, JK_TYPE));
	DD("Found '%s' command\n", j_find_ref(root, JK_COMMAND));

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_ME)) {
		return (mp_cli_dup_self_info_l());
	}

	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_LIST)) {
		return (mp_cli_get_list_l());
	}

	/* TODO: JV_TYPE_CONNECT used only in this line - looks like a bug */
	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_CONNECT)) {
		return (NULL);
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_DISCONNECTED)) {
		return (NULL);
	}

	/* We forward this request to the remote client */
	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_OPENPORT)) {
		return (mp_cli_execute_req(root));
	}

	/* We forward this request to the remote client */
	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_CLOSEPORT)) {
		return (mp_cli_execute_req(root));
	}

	/* This is a local reuqest, extract data and return to shell */
	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_PORTS)) {
		return (mp_cli_get_ports_l());
	}

	return (NULL);
}

/* This thread accepts connection from shell or from GUI client
   Only one client a time */
/* TODO: Run each client in separate thread */
/*@null@*/ void *mp_cli_pthread(/*@unused@*/void *arg __attribute__((unused)))
{
	int                fd_socket = -1;
	struct sockaddr_un cli_addr;
	ssize_t            rc        = -1;
	control_t          *ctl;

	DDD("CLI thread started\n");

	rc = pthread_detach(pthread_self());
	if (0 != rc) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	/* Set name of this thread */
	rc = prctl(PR_SET_NAME, "mp-cli-module");

	if (0 != rc) {
		DE("Can't set pthread name\n");
	}

	/* Create a new UNIX socket */
	fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd_socket < 0) {
		DE("Can't open CLI socket\n");
		return (NULL);
	}

	/* Configure IN the socket: use predefined name CLI_SOCKET_PATH_SRV */
	memset(&cli_addr, 0, sizeof(cli_addr));
	cli_addr.sun_family = AF_UNIX;
	strcpy(cli_addr.sun_path, CLI_SOCKET_PATH_SRV);
	rc = unlink(CLI_SOCKET_PATH_SRV);

	/* Ignore -1 status of unlink: it happens if no such file which is valid situation */
	if ((0 != rc) && (-1 != rc)) {
		DE("Can't remove the file %s\n", CLI_SOCKET_PATH_SRV);
	}

	/* Bind the IN socket */
	rc = (ssize_t)bind(fd_socket, (struct sockaddr *)&cli_addr, SUN_LEN(&cli_addr));
	if (rc < 0) {
		DE("bind failed\n");
		return (NULL);
	}

	ctl = ctl_get();

	/* Now listen (blocking) -> accept -> receive a buffer -> process -> return responce -> repeat */
	do {
		int fd_connection = -1;
		/*@only@*/buf_t *buft = NULL;

		/*@only@*/j_t *root = NULL;
		/*@only@*/j_t *root_resp = NULL;

		/* Listen for incoming connection (blocking) */
		rc = (ssize_t)listen(fd_socket, 2);
		if (rc < 0) {
			DE("listen failed\n");
			break;
		}

		/* TODO: Move it to a thread? */

		/* Connection is here, accept */
		fd_connection = accept(fd_socket, NULL, NULL);
		if (fd_connection < 0) {
			DE("accept() failed\n");
			break;
		}

		/* We receive the buffer in form of buf_t and convert it into JSON object */
		root = mp_net_utils_receive_json(fd_connection);

		if (NULL == root) {
			DE("Can't receive buf to JSON object\n");
			if (EOK != buf_free(buft)) {
				DE("Failed to release buf_t\n");
				abort();
			}
			break;
		}

		/* Now let's parse the command and receive from the parser an answer */
		root_resp = mp_cli_parse_command(root);

		/* That's it, we don't need the request object anymore */
		j_rm(root);
		TESTP(root_resp, NULL);

		if (NULL == root_resp) {
			DE("Can't create JSON object for respond (parse_cli_command failed)\n");
			break;
		}

		//j_print(root_resp, "Got responce, goung to send");

		rc = mp_net_utils_send_json(fd_connection, root_resp);
		if (EOK != rc) {
			DE("Can't send JSON response\n");
		}

		j_rm(root_resp);

	} while (ST_STOP != ctl->status && ST_STOPPED != ctl->status);

	return (NULL);
}


/***************** Module implementation *****************/

/* Received json just to be forwarded to the shell app */
int mp_module_cli_recv(void *root)
{
	int        rc       = EBAD;
	//const char *command;

	rc = mp_cli_send_to_cli(root);
	j_rm(root);
	return rc;
}

/* Send: on send we certainly want to send it to a remote host;
   The local request should be cared before this function called */

int mp_module_cli_send(void *root)
{
	int        rc      = -1;
	const char *uid_me = NULL;
	j_print(root, "Sending a message");

	/* JK_DISP_TGT_UID must be set*/
	if (EOK != j_test_key(root, JK_DISP_TGT_UID)) {
		DE("JK_DISP_TGT_UID is not set\n");
		j_print_v(root, "Wrong json: no JK_DISP_TGT_UID", __FILE__, __LINE__);
		j_rm(root);
		return EBAD;
	}
	/* JK_DISP_SRC_MODULE must be set*/
	if (EOK != j_test_key(root, JK_DISP_SRC_MODULE)) {
		DE("JK_DISP_SRC_MODULE is not set\n");
		j_print_v(root, "Wrong json: no JK_DISP_SRC_MODULE", __FILE__, __LINE__);
		j_rm(root);
		return EBAD;
	}

	/* JK_DISP_TGT_MODULE must be set*/
	if (EOK != j_test_key(root, JK_DISP_TGT_MODULE)) {
		DE("JK_DISP_TGT_MODULE is not set\n");
		j_print_v(root, "Wrong json: no JK_DISP_TGT_MODULE", __FILE__, __LINE__);
		j_rm(root);
		return EBAD;
	}

	/* JK_TICKET must be set*/
	if (EOK != j_test_key(root, JK_TICKET)) {
		DE("JK_TICKET is not set\n");
		j_print_v(root, "Wrong json: no JK_TICKET", __FILE__, __LINE__);
		j_rm(root);
		return EBAD;
	}

	/* TODO: In the future, if we plan to support several shell clients,
	   we should check ticket and forward the JSON by ticket */

	/* The Shell app itself should design the packet. We only need to add our UID */
	uid_me = ctl_uid_get();
	rc = j_add_str(root, JK_DISP_SRC_UID, uid_me);

	TESTI_ASSERT(rc, "Can't add JK_UID_SRC\n");
	return mp_communicate_send_request(root);
}

int mp_module_cli_init(void)
{
	int rc;

	/* Register this module in dispatcher */
	rc = mp_disp_register(MODULE_SHELL, mp_module_cli_send, mp_module_cli_recv);

	if (EOK != rc) {
		DE("Can't register in dispatcher\n");
		return (EBAD);
	}
	return (EOK);
}
