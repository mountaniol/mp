#define _GNU_SOURCE             /* See feature_test_macros(7) */
#ifndef S_SPLINT_S
	#include <pthread.h>
	#include <sys/prctl.h>

	#include <sys/socket.h>
	#include <sys/un.h>
	#include <unistd.h>
#endif

#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-cli.h"
#include "mp-communicate.h"
#include "mp-net-utils.h"
#include "mp-dict.h"
#include "mp-ports.h"
#include "mp-ssh.h"
#include "buf_t/buf_t.h"

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
/*@null@*/ static j_t *mp_cli_dup_self_info_l()
{
	/*@temp@*/control_t *ctl = NULL;
	/*@temp@*/j_t *ret = NULL;

	DDD("Starting\n");
	ctl = ctl_get_locked();
	ret = j_dup(ctl->me);
	ctl_unlock();
	return (ret);
}

/*@null@*/ static j_t *mp_cli_get_received_tickets_l(/*@temp@*/j_t *root)
{
	int    rc    = -1;
	/*@temp@*/control_t *ctl;
	/*@temp@*/j_t *arr = NULL;
	/*@temp@*/j_t *val = NULL;
	size_t index = 0;
	/*@temp@*/ const char *ticket = NULL;
	DDD("Starting\n");

	ticket = j_find_ref(root, JK_TICKET);
	TESTP(ticket, NULL);

	arr = j_arr();
	TESTP(arr, NULL);

	ctl = ctl_get_locked();

	j_arr_foreach(ctl->tickets_in, index, val) {
		if (j_test(val, JK_TICKET, ticket)) {
			/*@temp@*/j_t *copied = j_dup(val);
			if (NULL == copied) {
				j_rm(arr);
				return (NULL);
			}

			if (EOK != rc) {
				j_rm(copied);
				j_rm(arr);
				return (NULL);
			}

			rc = j_arr_rm(ctl->tickets_in, index);
			if (EOK != rc) {
				j_rm(copied);
				j_rm(arr);
				return (NULL);
			}
		}
	}

	rc = mp_cli_send_to_cli(arr);
	if (EOK != rc) {
		DE("Can't send\n");
		j_rm(arr);
		return (NULL);
	}

	//j_print(arr, "Sending to shell: ");
	return (arr);
}

/*@null@*/ static j_t *mp_cli_send_ticket_req(/*@temp@*/j_t *root)
{
	/*@temp@*/j_t *resp;
	int rc;

	DDD("Starting\n");

	rc = send_request_return_tickets_l(root);
	resp = j_new();
	TESTP(resp, NULL);
	if (0 == rc) {
		rc = j_add_str(resp, JK_STATUS, JV_OK);
	} else {
		rc = j_add_str(resp, JK_STATUS, JV_BAD);
	}

	if (EOK != rc) {
		DE("Can't add status into JSON\n");
		j_rm(root);
		return (NULL);
	}

	return (resp);
}

/*@null@*/ static j_t *mp_cli_get_ports_l()
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

/* Get list of all sources and targets */
/*@null@*/ static j_t *mp_cli_get_list_l()
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

#if 0
/*@null@*/ static j_t *mp_cli_ssh_forward(/*@temp@*/j_t *root){
	//control_t *ctl = NULL;
	int rc = EBAD;
	/*@temp@*/j_t *resp = NULL;
	/*@temp@*/j_t *remote_host = NULL;

	DDD("Start\n");

	/* Now: Here we should:
	   1. Open port on remote machine UID
	   2. Build json and start "forward ssh" thread
	   3. (TODO) If the remote machine can not open port -  we should open port here and
	   4. (TODO) start reverse ssh channel thread
	   5. Also we need: private key, public key adn all other parameters for ssh opening
	   6. (TODO) All this should be configured by user OR
	   7. (TODO) We may generate SSH key for us and use it for communication
	   */

	/*
	server_ip = j_find_ref(root, JK_SSH_SERVER);
	remote_destport_src = j_find_ref(root, JK_SSH_DESTPORT);
	remote_destport = atoi(remote_destport_src);
	local_listenport_str = j_find_ref(root, JK_SSH_LOCALPORT);
	local_listenport = atoi(local_listenport_str);
	pub_key_name = j_find_ref(root, JK_SSH_PUBKEY);
	priv_key_name = j_find_ref(root, JK_SSH_PRIVKEY);
	username = j_find_ref(root, JK_SSH_USERNAME);
	password = "";
	*/

	TESTP(root, NULL);

	remote_host = mp_ports_ssh_port_for_uid(j_find_ref(root, JK_UID_SRC));
	TESTP(remote_host, NULL);
	//j_print(remote_host, "Found remote host for ssh connection");

	rc = j_add_str(root, JK_SSH_SERVER, j_find_ref(remote_host, JK_IP_EXT));
	TESTI_MES(rc, NULL, "can't find / add JK_IP_EXT -> JK_SSH_SERVER");

	rc = j_add_str(root, JK_SSH_DESTPORT, j_find_ref(remote_host, JK_PORT_EXT));
	TESTI_MES(rc, NULL, "can't find / add JK_PORT_EXT -> JK_SSH_DESTPORT");

	rc = j_add_str(root, JK_SSH_LOCALPORT, "2222");
	TESTI_MES(rc, NULL, "can't add port 222 -> JK_SSH_LOCALPORT");

	rc = j_add_str(root, JK_SSH_PUBKEY, "/home/se/.ssh/id_rsa.pub");
	TESTI_MES(rc, NULL, "can't add port /home/se/.ssh/id_rsa.pub -> JK_SSH_PUBKEY");

	rc = j_add_str(root, JK_SSH_PRIVKEY, "/home/se/.ssh/id_rsa");
	TESTI_MES(rc, NULL, "can't add port /home/se/.ssh/id_rsa -> JK_SSH_PRIVKEY");

	rc = j_add_str(root, JK_SSH_USERNAME, "se");
	TESTI_MES(rc, NULL, "can't add port JK_SSH_USERNAME 'se'");

	DDD("Going to start SSH thread\n");
	//j_print(root, "Params for ssh thread");
	rc = ssh_thread_start(j_dup(root));

	resp = j_new();
	TESTP(resp, NULL);
	if (EOK == rc) {
		if (EOK != j_add_str(resp, JK_STATUS, JV_OK)) DE("Can't add JV_OK status\n");
	} else {
		if (EOK != j_add_str(resp, JK_STATUS, JV_BAD)) DE("Can't add JV_BAD status\n");
	}

	return (resp);
}
#endif

/*@null@*/ static j_t *mp_cli_execute_req(/*@temp@*/j_t *root)
{
	/*@shared@*/control_t *ctl = NULL;
	int rc = EBAD;
	/*@temp@*/j_t *resp = NULL;

	ctl = ctl_get_locked();
	rc = j_add_str(root, JK_UID_SRC, ctl_uid_get());
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

/*@null@*/ static j_t *mp_cli_parse_command(/*@temp@*/j_t *root)
{
	TESTP_MES(root, NULL, "Got NULL");

	DD("Found '%s' type\n", j_find_ref(root, JK_TYPE));
	DD("Found '%s' command\n", j_find_ref(root, JK_COMMAND));

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_ME)) {
		return (mp_cli_dup_self_info_l());
	}

	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_LIST)) {
		return (mp_cli_get_list_l());
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_CONNECT)) {
		return (NULL);
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_DISCONNECTED)) {
		return (NULL);
	}

	/* We forward this request to the remote client */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_OPENPORT)) {
		return (mp_cli_execute_req(root));
	}

	/* We forward this request to the remote client */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_CLOSEPORT)) {
		return (mp_cli_execute_req(root));
	}

	/* This is a local reuqest, extract data and return to shell */
	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_PORTS)) {
		return (mp_cli_get_ports_l());
	}

	#if 0
	/* This is local request, we open a port on remote machine and connect */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_SSH)) {
		return (mp_cli_ssh_forward(root));
	}
	#endif

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_REQ)) {
		return (mp_cli_send_ticket_req(root));
	}


	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_RESP)) {
		return (mp_cli_get_received_tickets_l(root));
	}

	return (NULL);
}

/* This thread accepts connection from shell or from GUI client
   Only one client a time */
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

	//rc = pthread_setname_np(pthread_self(), "mp_cli_thread");
	rc = prctl(PR_SET_NAME, "mp_cli_thread");

	if (0 != rc) {
		DE("Can't set pthread name\n");
	}

	fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd_socket < 0) {
		DE("Can't open CLI socket\n");
		return (NULL);
	}

	memset(&cli_addr, 0, sizeof(cli_addr));
	cli_addr.sun_family = AF_UNIX;
	strcpy(cli_addr.sun_path, CLI_SOCKET_PATH_SRV);
	rc = unlink(CLI_SOCKET_PATH_SRV);

	/* Ignore -1 status of unlink: it happens if no such file which is valid situation */
	if ((0 != rc) && (-1 != rc)) {
		DE("Can't remove the file %s\n", CLI_SOCKET_PATH_SRV);
	}

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

		/* That's it, we don't need request object anymore */
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