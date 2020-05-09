/*@-skipposixheaders@*/
#include <netdb.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
/*@=skipposixheaders@*/
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-jansson.h"
#include "mp-network.h"
#include "mp-cli.h"
#include "mp-os.h"
#include "mp-dict.h"
#include "buf_t.h"
#include "mp-memory.h"

#ifndef S_SPLINT_S /* splint analyzer goes crazy if this included */
	#include "libfort/src/fort.h"
#else /* Mock for splint analyzer */
	#define ft_table_t void
	#define FT_ROW_HEADER (1)
	#define FT_CPROP_ROW_TYPE (1)
	#define FT_ANY_COLUMN (1)
	#define FT_ROW_HEADER (1)
	#define FT_ROW_HEADER (1)
	int ft_set_cell_prop(ft_table_t *table, size_t row, size_t col, uint32_t property, int value);
	void ft_destroy_table(ft_table_t *table);
	const char *ft_to_string(const ft_table_t *table);
	int ft_write_ln(void *, ...);
	ft_table_t *ft_create_table(void);
	extern const struct ft_border_style *const FT_PLAIN_STYLE;
	int ft_set_default_border_style(const struct ft_border_style *style);
#endif

#define SERVER_PATH     "/tmp/server"
#define BUFFER_LENGTH    250
#define FALSE              0

int status = 0;

/* Here we parse messages received from remote hosts.
   These messages are responces requests done from here */
int mp_shell_parse_in_command(json_t *root){
	TESTP(root, EBAD);
	j_print(root, "Received message from the CLI thread:");
	return EOK;
}

/* This thread accepts connection from CLI or from GUI client
   Only one client a time */
void *mp_shell_in_thread(void *arg __attribute__((unused)))
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
	strcpy(cli_addr.sun_path, CLI_SOCKET_PATH_CLI);
	unlink(CLI_SOCKET_PATH_CLI);

	rc = (ssize_t)bind(fd, (struct sockaddr *)&cli_addr, SUN_LEN(&cli_addr));
	if (rc < 0) {
		DE("bind failed\n");
		return (NULL);
	}

	/* Listen for incoming connection */
	rc = (ssize_t)listen(fd, 2);
	if (rc < 0) {
		DE("listen failed\n");
		return NULL;
	}

	do {
		int fd2 = -1;
		char *buf = NULL;
		//size_t len = CLI_BUF_LEN;
		json_t *root = NULL;

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

		TFREE(buf);
		/* That's it, we don't need request objext any more */
		rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object");
	} while (1);

	return (NULL);
}

json_t *execute_requiest(json_t *root)
{
	int sd = -1;
	ssize_t rc = -1;
	char buffer[CLI_BUF_LEN];
	struct sockaddr_un serveraddr;

	buf_t *buf = j_2buf(root);

	TESTP_MES(buf, NULL, "Can't encode JSON object\n");

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd < 0) {
		DE("Failed\n");
		perror("socket() failed");
		return (NULL);
	}

	DDD("Opened socket\n");

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, CLI_SOCKET_PATH_SRV);

	DDD("Before connect\n");
	rc = (ssize_t)connect(sd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (rc < 0) {
		DE("Failed\n");
		perror("connect() failed");
		return (NULL);
	}

	DDD("Connected\n");

	// memset(buf, '0', CLI_BUF_LEN);
	rc = send(sd, buf->data, buf->size, 0);
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (NULL);
	}

	DDD("Sent\n");

	rc = recv(sd, buffer, CLI_BUF_LEN, 0);
	if (rc < 0) {
		DE("Failed\n");
		perror("recv() failed");
		return (NULL);
	}

	if (rc == 0) {
		printf("The server closed the connection\n");
		return (NULL);
	}

	if (rc > CLI_BUF_LEN) {
		printf("The received buffer is too big. Expected max %d, received %zu\n", CLI_BUF_LEN, rc);
		return (NULL);
	}

	DDD("Received : %ld bytes\n", rc);

	buffer[rc] = '\0';
	if (sd != -1) {
		close(sd);
	}
	return (j_str2j(buffer));
}

static int mp_shell_watch_ticket(const char *ticket)
{
	json_t *ticket_req = NULL;
	json_t *ticket_resp = NULL;
	json_t *resp = NULL;
	int rc = EBAD;

	TESTP(ticket, EBAD);

	ticket_req = j_new();
	TESTP(ticket_req, EBAD);

	ticket_resp = j_new();
	TESTP(ticket_resp, EBAD);

	/* Now construct "give me my tickets" request and send it until received final ticket,
	   which is JV_STATUS_FAIL or JV_STATUS_SUCCESS */
	rc = j_add_str(ticket_req, JK_COMMAND, JV_TYPE_TICKET_REQ);
	TESTI_MES(rc, EBAD, "Can't add JK_COMMAND, JV_TYPE_TICKET");
	rc = j_add_str(ticket_req, JK_TYPE, JV_TYPE_TICKET_REQ);
	TESTI_MES(rc, EBAD, "Can't add JK_TYPE, JV_TYPE_TICKET");
	rc = j_add_str(ticket_req, JK_TICKET, ticket);
	TESTI_MES(rc, EBAD, "Can't add JK_TICKET, ticket");

	rc = j_add_str(ticket_resp, JK_COMMAND, JV_TYPE_TICKET_RESP);
	TESTI_MES(rc, EBAD, "Can't add JK_COMMAND, JV_TYPE_TICKET");
	rc = j_add_str(ticket_resp, JK_TYPE, JV_TYPE_TICKET_RESP);
	TESTI_MES(rc, EBAD, "Can't add JK_TYPE, JV_TYPE_TICKET");
	rc = j_add_str(ticket_resp, JK_TICKET, ticket);
	TESTI_MES(rc, EBAD, "Can't add JK_TICKET, ticket");
	
	do {
		if (resp) j_rm(resp);
		DD("starting: getting tickets\n");
		j_print(ticket_req, "Sending ticket request");
		resp = execute_requiest(ticket_req);
		/* TODO: test answer */
		if (resp) {
			j_print(resp, "Got responce:");
			j_rm(resp);
			resp = NULL;
		}

		sleep(1);

		j_print(ticket_req, "Sending get ticket request");
		resp = execute_requiest(ticket_resp);
		if (resp) {
			j_print(resp, "Got responce:");
		}
	} while (EOK != j_test(resp, JK_STATUS, JV_STATUS_FAIL) || EOK != j_test(resp, JK_STATUS, JV_STATUS_SUCCESS));



	return EOK;
}

static int mp_shell_ask_openport(json_t *args)
{
	int rc = EBAD;
	const char *uid_dst = NULL;
	const char *port = NULL;
	const char *protocol = NULL;
	json_t *resp = NULL;
	json_t *root = NULL;
	char *ticket = NULL;

	TESTP(args, EBAD);

	root = j_new();
	TESTP(root, EBAD);

	j_print(args, "args");

	uid_dst = j_find_ref(args, JK_UID_DST);
	TESTP_MES_GO(uid_dst, err, "Can't find uid");

	port = j_find_ref(args, JK_PORT_INT);
	TESTP_MES_GO(port, err, "Can't find port");

	protocol = j_find_ref(args, JK_PROTOCOL);
	TESTP_MES_GO(protocol, err, "Can't find protocol");

	rc = j_add_str(root, JK_COMMAND, JV_TYPE_OPENPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
	rc = j_add_str(root, JK_TYPE, JV_TYPE_OPENPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
#if 0
	rc = j_add_str(root, JK_UID_SRC, uid_dst);
	TESTI_MES_GO(rc, err, "Can't add 'uid' field");
#endif
	rc = j_add_str(root, JK_PORT_INT, port);
	TESTI_MES_GO(rc, err, "Can't add 'port' field");
	rc = j_add_str(root, JK_PROTOCOL, protocol);
	TESTI_MES_GO(rc, err, "Can't add 'protocol' field");
	//rc = j_add_str(root, JK_UID_DST, uid_dst);
	//TESTI_MES_GO(rc, err, "Can't add 'dest' field");

	ticket = mp_os_rand_string(TICKET_SIZE);
	TESTP(ticket, EBAD);
	rc = j_add_str(root, JK_TICKET, ticket);
	TESTI_MES_GO(rc, err, "Can't add 'ticket' field");

	DDD("Going to execute the request\n");

	printf("Please wait. Port remapping may take up to 10 seconds. Or more, who knows, kid.\n");
	resp = execute_requiest(root);
	TESTP_MES_GO(resp, err, "Responce is NULL\n");
	rc = j_rm(root);
	TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");

	if (j_test(resp, JK_STATUS, JV_OK)) {
		rc = EOK;
	}
	root = j_new();
	TESTP(root, EBAD);

	//return (mp_shell_watch_ticket(ticket));

	sleep(30);

	rc = EOK;

	/* Now receive tickets until requiest not done */
err:
	if (root) {
		rc = j_rm(root);
		TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");
	}
	if (resp) {
		rc = j_rm(resp);
		TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");
	}
	return (rc);
}

static int mp_shell_ask_closeport(json_t *args)
{
	int rc = EBAD;
	const char *uid = NULL;
	const char *port = NULL;
	const char *protocol = NULL;
	json_t *resp = NULL;
	json_t *root = NULL;
	char *ticket = NULL;

	TESTP(args, EBAD);

	root = j_new();
	TESTP(root, EBAD);

	j_print(args, "Closeport JSON");

	uid = j_find_ref(args, JK_UID_DST);
	TESTP_MES_GO(uid, err, "Can't find uid");

	port = j_find_ref(args, JK_PORT_INT);
	TESTP_MES_GO(port, err, "Can't find port");

	protocol = j_find_ref(args, JK_PROTOCOL);
	TESTP_MES_GO(protocol, err, "Can't find protocol");

	rc = j_add_str(root, JK_COMMAND, JV_TYPE_CLOSEPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
	rc = j_add_str(root, JK_UID_DST, uid);
	TESTI_MES_GO(rc, err, "Can't add 'uid' field");
	rc = j_add_str(root, JK_PORT_INT, port);
	TESTI_MES_GO(rc, err, "Can't add 'port' field");
	rc = j_add_str(root, JK_PROTOCOL, protocol);
	TESTI_MES_GO(rc, err, "Can't add 'protocol' field");

	ticket = mp_os_rand_string(TICKET_SIZE);
	TESTP(ticket, EBAD);
	rc = j_add_str(root, JK_TICKET, ticket);
	TESTI_MES_GO(rc, err, "Can't add 'ticket' field");

	DDD("Going to execute the request\n");

	printf("Please wait. Port remapping may take up to 10 seconds. Or more, who knows, kid.\n");
	resp = execute_requiest(root);
	TESTP_GO(resp, err);
	rc = j_rm(root);
	TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");
	if (j_test(resp, JK_STATUS, JV_OK)) {
		rc = EOK;
	}
err:
	if (root) {
		rc = j_rm(root);
		TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");
	}
	if (resp) {
		rc = j_rm(resp);
		TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");
	}
	return (rc);
}

static int mp_shell_get_info()
{
	json_t *root = j_new();
	json_t *resp = NULL;
	ft_table_t *table = NULL;
	int rc;

	TESTP(root, EBAD);

	rc = j_add_str(root, JK_COMMAND, JV_TYPE_ME);
	TESTI_MES(rc, EBAD, "Can't add JK_COMMAND, JV_TYPE_ME");
	resp = execute_requiest(root);
	if (NULL == resp) {
		printf("An error: can't bring information\n");
		return (EBAD);
	}

	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();

	ft_write_ln(table, "My Machine", j_find_ref(resp, JK_NAME));
	ft_write_ln(table, "My Username", j_find_ref(resp, JK_USER));
	ft_write_ln(table, "My UID", j_find_ref(resp, JK_UID_ME));
	ft_write_ln(table, "My External IP", j_find_ref(resp, JK_IP_EXT));
	ft_write_ln(table, "My Internal IP", j_find_ref(resp, JK_IP_INT));
	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);

	rc = j_rm(resp);
	TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");

	return (EOK);
}

static int mp_shell_ssh(json_t *args)
{
	json_t *root = j_new();
	json_t *resp = NULL;
	int rc;
	//ft_table_t *table = NULL;

	TESTP(root, EBAD);

	rc = j_add_str(root, JK_TYPE, JV_TYPE_SSH);
	TESTI_MES(rc, EBAD, "Can't add JK_TYPE, JV_TYPE_SSH");
	rc = j_cp(args, root, JK_UID_DST);
	TESTI_MES(rc, EBAD, "Can't add root, JK_UID");
	j_print(root, "Sending SSH command\n");
	resp = execute_requiest(root);
	if (NULL == resp) {
		printf("An error: can't bring information\n");
		return (EBAD);
	}

	rc = j_rm(resp);
	TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");
	return (EOK);
}

static int mp_shell_get_hosts()
{
	json_t *resp = NULL;
	json_t *root = j_new();
	const char *key;
	json_t *val = NULL;
	ft_table_t *table = NULL;
	int rc;

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_LIST)) {
		DE("No opened ports'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	rc = j_rm(root);
	TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");

	if (NULL == resp || 0 == j_count(resp)) {
		printf("No host in the list\n");
		rc = j_rm(resp);
		TESTI_MES(rc, EBAD, "Can't remove json object 'rest'\n");
		return (0);
	}

	printf("List of connected clients\n");
	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "UID", "External IP", "Internal IP", "Name");

	json_object_foreach(resp, key, val) {
		ft_write_ln(table, j_find_ref(val, JK_UID_ME), j_find_ref(val, JK_IP_EXT),
					j_find_ref(val, JK_IP_INT), j_find_ref(val, JK_NAME));
	}
	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);


	return (0);
}

static int mp_shell_get_ports()
{
	json_t *resp = NULL;
	json_t *root = j_new();
	size_t index = 0;
	json_t *val = NULL;
	ft_table_t *table = NULL;
	int rc;

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_PORTS)) {
		DE("Can't add 'command'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	rc = j_rm(root);
	TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");

	if (NULL == resp || 0 == json_array_size(resp)) {
		printf("No mapped ports\n");
		rc = j_rm(resp);
		TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");
		return (0);
	}

	printf("Ports mapped from router to this machine:\n");

	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "External", "Internal", "Protocol");

	json_array_foreach(resp, index, val) {
		ft_write_ln(table, j_find_ref(val, JK_PORT_EXT),
					j_find_ref(val, JK_PORT_INT),
					j_find_ref(val, JK_PROTOCOL));
	}

	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);
	return (0);
}

/* show opened remote ports */
static int mp_shell_get_remote_ports()
{
	json_t *resp = NULL;
	json_t *root = j_new();
	int index;
	const char *key;
	json_t *val = NULL;
	ft_table_t *table = NULL;
	int rc;

	D("Start\n");

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_LIST)) {
		DE("Can't create 'list' request for remote ports'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	rc = j_rm(root);
	TESTI_MES(rc, EBAD, "Can't remove json object 'root'\n");

	if (NULL == resp || 0 == j_count(resp)) {
		printf("No host in the list\n");
		rc = j_rm(resp);
		TESTI_MES(rc, EBAD, "Can't remove json object 'resp'\n");
		return (0);
	}

	j_print(resp, "resp");

	printf("List of connected clients\n");
	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "UID", "External Port", "Internal Port", "Protocol");

	json_object_foreach(resp, key, val) {
		/* Here we are inside of a host */
		json_t *host_ports;
		json_t *port;
		host_ports = j_find_j(val, "ports");
		json_array_foreach(host_ports, index, port) {
			ft_write_ln(table, j_find_ref(val, JK_UID_ME), j_find_ref(port, JK_PORT_EXT),
						j_find_ref(port, JK_PORT_INT), j_find_ref(port, JK_PROTOCOL));
		}
	}
	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);


	return (0);
}
static void mp_shell_usage(char *name)
{

	ft_table_t *table = NULL;
	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	/* Set "header" type for the first row */
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "Option", "Explanation");
	printf("Usage: %s -l -o -p -u -s\n", name);

	ft_write_ln(table, "-i", "information about this machine");
	ft_write_ln(table, "-l", "list connected machines");
	ft_write_ln(table, "-m", "print ports mapped from router to this machine");
	ft_write_ln(table, "-r", "print ports mapped on another hosts");
	ft_write_ln(table, "-o X", "open port X on the remote machine");
	ft_write_ln(table, "-c X", "close port X on the remote machine");
	ft_write_ln(table, "-u uid", "uid of the remote machine");
	ft_write_ln(table, "-p", "TCP or UDP - protocol");
	ft_write_ln(table, "-s", "open ssh connection to remote machine");
	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);

	printf("Examples:\n"
		   "%s -o 22 -u user-939-466-331 -p UDP\n"
		   "Open port 22 for TCP on machine user-939-466-331\n"
		   "%s -c 5001 -u user-939-466-331 -p UDP\n"
		   "Close port 5001 for UDP user-939-466-331\n"
		   , name, name);
}

int main(int argc, char *argv[])
{
	int opt;
	json_t *args;
	pthread_t in_thread_id;

	/* 
	 * i - info - print information about this machine 
	 * l - show list of remote hosts 
	 * m - show port maps on this machine 
	 * r - show port maps on a remote machin (if UID of remote given) / on all machines (if no UID given) 
	 * o - open port on remote machine
	 * c - close port on remote machine
	 * p - protocol, UDP or TCP  (for -o)
	 * u - uid (uid of the target)
	 * s - open ssh channel 
	*/

	if (argc < 2) {
		mp_shell_usage(argv[0]);
		return (-1);
	}

	pthread_create(&in_thread_id, NULL, mp_shell_in_thread, NULL);

	args = j_new();
	TESTP_MES(args, -1, "Can't allocate JSON object\n");

	while ((opt = getopt(argc, argv, ":limro:u:s:p:x:c:h")) != -1) {
		int rc;
		switch (opt) {
		case 'i': /* Show this machine info */
			rc = j_add_str(args, JK_SHOW_INFO, JV_YES);
			TESTI(rc, EBAD);
			break;
		case 'l': /* Show remote hosts */
			rc = j_add_str(args, JK_SHOW_HOSTS, JV_YES);
			TESTI(rc, EBAD);
			break;
		case 'o': /* Open port comand (open the port on remote machine UID */
			rc = j_add_str(args, JK_TYPE, JV_TYPE_OPENPORT);
			TESTI(rc, EBAD);
			rc = j_add_str(args, JK_PORT_INT, optarg);
			TESTI(rc, EBAD);
			D("Optarg is %s\n", optarg);
			break;
		case 'c': /* Close port comand (open the port on remote machine UID */
			rc = j_add_str(args, JK_TYPE, JV_TYPE_CLOSEPORT);
			TESTI(rc, EBAD);
			rc = j_add_str(args, JK_PORT_INT, optarg);
			TESTI(rc, EBAD);
			D("Optarg is %s\n", optarg);
			break;
		case 'u': /* UID of remote machine */
			rc = j_add_str(args, JK_UID_DST, optarg);
			TESTI(rc, EBAD);
			break;
		case 's': /* OPen ssh channel for communication */
			rc = j_add_str(args, JK_TYPE, JV_TYPE_SSH);
			TESTI(rc, EBAD);
			rc = j_add_str(args, JK_UID_DST, optarg);
			TESTI(rc, EBAD);
			break;
		case 'p': /* Protocol to use for port opening (-o command) */
			rc = j_add_str(args, JK_PROTOCOL, optarg);
			TESTI(rc, EBAD);
			break;
		case 'm': /* Show ports mapped on this machine */
			rc = j_add_str(args, JK_SHOW_PORTS, JV_YES);
			TESTI(rc, EBAD);
			break;
		case 'r': /* TODO: Show ports mapped on a remote machine (if UID given) / on all remotes (if UID is not specified) */
			rc = j_add_str(args, JK_SHOW_RPORTS, JV_YES);
			TESTI(rc, EBAD);
			break;
		case 'h': /* Print help */
			mp_shell_usage(argv[0]);
			break;
		case ':':
			printf("option needs a value\n");
			mp_shell_usage(argv[0]);
			return (EBAD);
		case '?':
			printf("unknown option:%c\n", optopt);
			mp_shell_usage(argv[0]);
			return (EBAD);
		}
	}

	if (0 == j_test(args, JK_SHOW_HOSTS, JV_YES)) {
		mp_shell_get_hosts(args);
	}

	if (0 == j_test(args, JK_SHOW_PORTS, JV_YES)) {
		mp_shell_get_ports(args);
	}

	if (0 == j_test(args, JK_SHOW_INFO, JV_YES)) {
		mp_shell_get_info(args);
	}

	if (0 == j_test(args, JK_TYPE, JV_TYPE_SSH)) {
		DDD("Founf SSH command\n");
		mp_shell_ssh(args);
	}

	if (0 == j_test(args, JK_TYPE, JV_TYPE_OPENPORT)) {
		mp_shell_ask_openport(args);
	}

	if (0 == j_test(args, JK_TYPE, JV_TYPE_CLOSEPORT)) {
		mp_shell_ask_closeport(args);
	}

	if (0 == j_test(args, JK_SHOW_RPORTS, JV_YES)) {
		D("Founf RPORTS command\n");
		mp_shell_get_remote_ports(args);
	}

	return (0);
}
