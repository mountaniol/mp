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

int mp_cli_send_to_cli(json_t *root)
{
	int sd = -1;
	ssize_t rc = -1;
	struct sockaddr_un serveraddr;

	DDD("start");
	buf_t *buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't encode JSON object\n");

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

	// memset(buf, '0', CLI_BUF_LEN);
	rc = send(sd, buf->data, buf->size, 0);
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (EBAD);
	}

	close(sd);

	return EOK;
}


/* Get this machine info */
static json_t *mp_cli_get_self_info_l()
{
	control_t *ctl = NULL;
	DDD("Starting\n");
	ctl = ctl_get();
	return (j_dup(ctl->me));
}

static json_t *mp_cli_get_received_tickets(json_t *root)
{
	control_t *ctl;
	json_t *arr;
	json_t *val;
	int index;
	const char *ticket;
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
			j_arr_add(arr, copied);
			json_array_remove(ctl->tickets_in, index);
		}
	}

	j_print(arr, "Sending to shell: ");
	mp_cli_send_to_cli(arr);
	return (arr);
}

static json_t *mp_cli_send_ticket_req(json_t *root)
{
	control_t *ctl = ctl_get();
	json_t *resp;
	//json_t *ports;
	int rc;

	DDD("Starting\n");

	rc = send_request_return_tickets(ctl->mosq, root);
	resp = j_new();
	if (0 == rc) {
		j_add_str(resp, JK_STATUS, JV_OK);
	} else {
		j_add_str(resp, JK_STATUS, JV_BAD);
	}

	return (resp);
}

static json_t *mp_cli_get_ports_l()
{
	control_t *ctl = NULL;
	json_t *resp;
	json_t *ports;
	DDD("Starting\n");

	ctl = ctl_get_locked();
	ports = j_find_j(ctl->me, "ports");
	if (NULL == ports) {
		DE("Can't extract array 'ports'\n");
		ctl_unlock(ctl);
		return (NULL);
	}
	resp = j_dup(ports);
	ctl_unlock(ctl);
	return (resp);
}

/* Get list of all sources and targets */
static json_t *mp_cli_get_list_l()
{
	control_t *ctl = NULL;
	json_t *resp;
	DDD("Starting\n");
	ctl = ctl_get_locked();
	resp = j_dup(ctl->hosts);
	ctl_unlock(ctl);
	return (resp);
}

static json_t *mp_cli_ssh_forward(json_t *root)
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


/* SEB:TODO: We should form JSON in the mp-shell and here we should just send the request */
/* The CLI asked to open a port on remote machine. */
static json_t *mp_cli_openport_l(json_t *root)
{
	control_t *ctl = NULL;
	int rc = EBAD;
	json_t *resp = NULL;

	ctl = ctl_get_locked();
	j_add_str(root, JK_UID_SRC, j_find_ref(ctl->me, JK_UID_ME));
	ctl_unlock(ctl);

	DDD("Calling send_request_to_open_port\n");
	j_print(root, "Sending request to open a port:");
	if (NULL != ctl->mosq) {
		rc = send_request_to_open_port(ctl->mosq, root);
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

static json_t *mp_cli_closeport_l(json_t *root)
{
	char *uid_src = NULL;
	char *port = NULL;
	char *protocol = NULL;
	control_t *ctl = NULL;
	int rc = EBAD;
	json_t *resp = NULL;

	TESTP_MES(root, NULL, "Got NULL");

	/* Get uid of remmote client */
	uid_src = j_find_dup(root, JK_UID_SRC);
	TESTP_MES(uid_src, NULL, "Not found 'uid' field");

	/* Get port which should be opened in the remote client */
	/* We mean "internal" port, for example port "22" for ssh.*/
	port = j_find_dup(root, JK_PORT_INT);
	TESTP_MES_GO(port, err, "Not found 'port' field");

	/* And this port should be opened for TCP or UDP? */
	protocol = j_find_dup(root, JK_PROTOCOL);
	TESTP_MES_GO(port, err, "Not found 'protocol' field");

	ctl = ctl_get_locked();
	DDD("Calling send_request_to_open_port\n");
	if (NULL != ctl->mosq) {
		rc = send_request_to_close_port(ctl->mosq, uid_src, port, protocol);
	}
	ctl_unlock(ctl);

	resp = j_new();
	TESTP_MES_GO(resp, err, "Can't allocate JSON object");

	if (EOK == rc) {
		if (EOK != j_add_str(resp, JK_STATUS, JV_OK)) DE("Can't add 'status'\n");
	} else {
		if (EOK != j_add_str(resp, JK_STATUS, JV_BAD)) DE("Can't add 'name'\n");
	}

err:
	TFREE(uid_src);
	TFREE(port);
	TFREE(protocol);
	return (resp);
}

static json_t *mp_cli_parse_command(json_t *root)
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

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_OPENPORT)) {
		return (mp_cli_openport_l(root));
	}

	if (EOK == j_test(root, JK_COMMAND, JV_TYPE_CLOSEPORT)) {
		return (mp_cli_closeport_l(root));
	}

	if (EOK == j_test(root, JK_COMMAND, JV_COMMAND_PORTS)) {
		return (mp_cli_get_ports_l());
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_SSH)) {
		return (mp_cli_ssh_forward(root));
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_REQ)) {
		return (mp_cli_send_ticket_req(root));
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_RESP)) {
		return (mp_cli_get_received_tickets(root));
	}

	return (NULL);
}

/* This thread accepts connection from CLI or from GUI client
   Only one client a time */
void *mp_cli_thread(void *arg __attribute__((unused)))
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

