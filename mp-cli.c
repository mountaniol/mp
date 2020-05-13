/*@-skipposixheaders@*/
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
/*@=skipposixheaders@*/
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-main.h"
#include "mp-cli.h"
#include "mp-requests.h"
#include "mp-communicate.h"
#include "mp-network.h"
#include "mp-dict.h"
#include "mp-ports.h"
#include "mp-ssh.h"

int mp_cli_send_to_cli(/*@temp@*/ const json_t *root)
{
	int sd = -1;
	ssize_t rc = -1;
	struct sockaddr_un serveraddr;
	buf_t *buf = NULL;

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

	rc = send(sd, buf->data, buf->size, 0);
	buf_free_force(buf);
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (EBAD);
	}

	if(0 != close(sd)) {
		DE("Can't close socket\n");
		return EBAD;
	}

	return EOK;
}


/* Get this machine info */
/*@null@*/ static json_t *mp_cli_get_self_info_l()
{
	/*@shared@*/control_t *ctl = NULL;
	DDD("Starting\n");
	ctl = ctl_get();
	return (j_dup(ctl->me));
}

/*@null@*/ static json_t *mp_cli_get_received_tickets_l(/*@temp@*/json_t *root)
{
	int rc = -1;
	/*@shared@*/control_t *ctl;
	json_t *arr = NULL;
	json_t *val = NULL;
	size_t index = 0;
	/*@only@*/ const char *ticket = NULL;
	DDD("Starting\n");
	//int rc;

	ticket = j_find_ref(root, JK_TICKET);
	TESTP(ticket, NULL);

	arr = j_arr();
	TESTP(arr, NULL);

	ctl = ctl_get_locked();

	json_array_foreach(ctl->tickets_in, index, val) {
		if (j_test(val, JK_TICKET, ticket)) {
			json_t * copied = j_dup(val);
			TESTP(copied, NULL);
			rc = j_arr_add(arr, copied);
			TESTI(rc, NULL);
			rc = json_array_remove(ctl->tickets_in, index);
			TESTI(rc, NULL);
		}
	}
	rc = mp_cli_send_to_cli(arr);
	if(EOK != rc) {
		DE("Can't send\n");
		j_rm(arr);
		return NULL;
	}

	//j_print(arr, "Sending to shell: ");
	return (arr);
}

/*@null@*/ static json_t *mp_cli_send_ticket_req(/*@temp@*/json_t *root)
{
	json_t *resp;
	int rc;

	DDD("Starting\n");

	rc = send_request_return_tickets(root);
	resp = j_new();
	TESTP(resp, NULL);
	if (0 == rc) {
		j_add_str(resp, JK_STATUS, JV_OK);
	} else {
		j_add_str(resp, JK_STATUS, JV_BAD);
	}

	return (resp);
}

/*@null@*/ static json_t *mp_cli_get_ports_l()
{
	/*@shared@*/control_t *ctl = NULL;
	json_t *resp;
	json_t *ports;
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
/*@null@*/ static json_t *mp_cli_get_list_l()
{
	/*@shared@*/control_t *ctl = NULL;
	json_t *resp;
	DDD("Starting\n");
	ctl = ctl_get_locked();
	resp = j_dup(ctl->hosts);
	ctl_unlock();
	return (resp);
}

/*@null@*/ static json_t *mp_cli_ssh_forward(/*@temp@*/json_t *root)
{
	//control_t *ctl = NULL;
	int rc = EBAD;
	json_t *resp = NULL;
	json_t *remote_host = NULL;

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
	j_print(remote_host, "Found remote host for ssh connection");

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
	j_print(root, "Params for ssh thread");
	ssh_thread_start(j_dup(root));

	resp = j_new();
	if (EOK == rc) {
		if (EOK != j_add_str(resp, JK_STATUS, JV_OK)) DE("Can't add JV_OK status\n");
	} else {
		if (EOK != j_add_str(resp, JK_STATUS, JV_BAD)) DE("Can't add JV_BAD status\n");
	}

	return (resp);
}

/*@null@*/ static json_t *mp_cli_execute_req(/*@temp@*/json_t *root)
{
	/*@shared@*/control_t *ctl = NULL;
	int rc = EBAD;
	json_t *resp = NULL;

	ctl = ctl_get_locked();
	j_add_str(root, JK_UID_SRC, ctl_uid_get());
	ctl_unlock();

	DDD("Calling send_request_to_open_port\n");
	j_print(root, "Sending request to open a port:");
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

/*@null@*/ static json_t *mp_cli_parse_command(/*@temp@*/json_t *root)
{
	TESTP_MES(root, NULL, "Got NULL");

	DD("Found '%s' type\n", j_find_ref(root, JK_TYPE));
	DD("Found '%s' command\n", j_find_ref(root, JK_COMMAND));

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_ME)) {
		return (mp_cli_get_self_info_l());
	}

	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_LIST)) {
		return (mp_cli_get_list_l());
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_CONNECT)) {
		return (NULL);
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_DISCONNECT)) {
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

	/* This is local request, we open a port on remote machine and connect */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_SSH)) {
		return (mp_cli_ssh_forward(root));
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_REQ)) {
		return (mp_cli_send_ticket_req(root));
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_RESP)) {
		return (mp_cli_get_received_tickets_l(root));
	}

	return (NULL);
}

/* This thread accepts connection from CLI or from GUI client
   Only one client a time */
/*@null@*/ void *mp_cli_thread(/*@temp@*/void *arg __attribute__((unused)))
{
	/* TODO: move it to common header */
	int fd = -1;
	struct sockaddr_un cli_addr;
	ssize_t rc = -1;

	DDD("CLI thread started\n");

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		DE("Can't open CLI socket\n");
		return (NULL);
	}

	memset(&cli_addr, 0, sizeof(cli_addr));
	cli_addr.sun_family = AF_UNIX;
	strcpy(cli_addr.sun_path, CLI_SOCKET_PATH_SRV);
	unlink(CLI_SOCKET_PATH_SRV);

	rc = (ssize_t)bind(fd, (struct sockaddr *)&cli_addr, SUN_LEN(&cli_addr));
	if (rc < 0) {
		DE("bind failed\n");
		return (NULL);
	}

	do {
		int fd2 = -1;
		char *buf = NULL;
		buf_t *buft = NULL;
		//size_t len = CLI_BUF_LEN;
		json_t *root = NULL;
		json_t *root_resp = NULL;

		/* Listen for incoming connection */
		rc = (ssize_t)listen(fd, 2);
		if (rc < 0) {
			DE("listen failed\n");
			break;
		}

		/* Connection is here, accept */
		fd2 = accept(fd, NULL, NULL);
		if (fd2 < 0) {
			DE("accept() failed\n");
			break;
		}

		/* Allocate buffer for reading */
		buf = zmalloc(CLI_BUF_LEN);
		TESTP_MES(buf, NULL, "Can't allocate buf");

		rc = (ssize_t)setsockopt(fd2, SOL_SOCKET, SO_RCVLOWAT, buf, CLI_BUF_LEN);
		if (rc < 0) {
			DE("setsockopt(SO_RCVLOWAT) failed\n");
			free(buf);
			break;
		}

		/* Receive buffer from cli */
		rc = recv(fd2, buf, CLI_BUF_LEN, 0);
		if (rc < 0) {
			DE("recv failed\n");
			free(buf);
			break;
		}

		/* Add 0 terminator, else json decoding will fail */
		*(buf + rc) = '\0';
		root = j_str2j(buf);
		free(buf);
		if (NULL == root) {
			DE("Can't decode buf to JSON object\n");
			break;
		}

		/* Now let's parse the command and receive from the parser an answer */
		root_resp = mp_cli_parse_command(root);

		/* That's it, we don't need request objext any more */
		rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object");

		if (NULL == root_resp) {
			DE("Can't create JSON object for respond (parse_cli_command failed)\n");
			break;
		}

		/* Encode response object into text buffer */
		buft = j_2buf(root_resp);
		rc = j_rm(root_resp);
		TESTI_MES(rc, NULL, "Can't remove json object");

		if (NULL == buft) {
			DE("Can't convert json to buf_t\n");
			break;
		}

		/* Send the encoded JSON to cli */
		rc = send(fd2, buft->data, buft->size, 0);
		if (rc != (ssize_t)buft->size) {
			DE("send() failed");
			buf_free_force(buft);
			break;
		}

		/* Free the buffer */
		buf_free_force(buft);
	} while (1);

	return (NULL);
}

