/*@-skipposixheaders@*/
#include <netdb.h>
#include <sys/un.h>
#include <unistd.h>
/*@=skipposixheaders@*/
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-jansson.h"
#include "mp-network.h"
#include "mp-cli.h"
#include "mp-os.h"
#include "mp-dict.h"
#include "buf_t.h"
#include "libfort/src/fort.h"

#define SERVER_PATH     "/tmp/server"
#define BUFFER_LENGTH    250
#define FALSE              0

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
	} else if (rc == 0) {
		printf("The server closed the connection\n");
		return (NULL);
	}

	DDD("Received\n");;

	buffer[rc] = '\0';
	if (sd != -1) close(sd);
	return (j_str2j(buffer));
}

static int mp_shell_ask_openport(json_t *args)
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

	j_print(args, "args");

	uid = j_find_ref(args, JK_UID);
	TESTP_MES_GO(uid, err, "Can't find uid");

	port = j_find_ref(args, JK_PORT_INT);
	TESTP_MES_GO(port, err, "Can't find port");

	protocol = j_find_ref(args, JK_PROTOCOL);
	TESTP_MES_GO(protocol, err, "Can't find protocol");

	rc = j_add_str(root, JK_COMMAND, JV_TYPE_OPENPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
	rc = j_add_str(root, JK_TYPE, JV_TYPE_OPENPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
	rc = j_add_str(root, JK_UID, uid);
	TESTI_MES_GO(rc, err, "Can't add 'uid' field");
	rc = j_add_str(root, JK_PORT_INT, port);
	TESTI_MES_GO(rc, err, "Can't add 'port' field");
	rc = j_add_str(root, JK_PROTOCOL, protocol);
	TESTI_MES_GO(rc, err, "Can't add 'protocol' field");
	rc = j_add_str(root, JK_DEST, uid);
	TESTI_MES_GO(rc, err, "Can't add 'dest' field");

	ticket = mp_os_rand_string(TICKET_SIZE);
	TESTP(ticket, EBAD);
	rc = j_add_str(root, JK_TICKET, ticket);
	TESTI_MES_GO(rc, err, "Can't add 'ticket' field");

	DDD("Going to execute the request\n");

	printf("Please wait. Port remapping may take up to 10 seconds. Or more, who knows, kid.\n");
	resp = execute_requiest(root);
	j_rm(root);
	TESTP_GO(resp, err);
	if (j_test(resp, JK_STATUS, JV_OK)) {
		rc = EOK;
	}
err:
	if (root) j_rm(root);
	if (resp) j_rm(resp);
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

	uid = j_find_ref(args, JK_UID);
	TESTP_MES_GO(uid, err, "Can't find uid");

	port = j_find_ref(args, JK_PORT_INT);
	TESTP_MES_GO(port, err, "Can't find port");

	protocol = j_find_ref(args, JK_PROTOCOL);
	TESTP_MES_GO(protocol, err, "Can't find protocol");

	rc = j_add_str(root, JK_COMMAND, JV_TYPE_CLOSEPORT);
	TESTI_MES_GO(rc, err, "Can't add 'JK_COMMAND' field");
	rc = j_add_str(root, JK_UID, uid);
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
	j_rm(root);
	TESTP_GO(resp, err);
	if (j_test(resp, JK_STATUS, JV_OK)) {
		rc = EOK;
	}
err:
	if (root) j_rm(root);
	if (resp) j_rm(resp);
	return (rc);
}

static int mp_shell_get_info()
{
	json_t *root = j_new();
	json_t *resp = NULL;
	ft_table_t *table = NULL;

	j_add_str(root, JK_COMMAND, JV_TYPE_ME);
	resp = execute_requiest(root);
	if (NULL == resp) {
		printf("An error: can't bring information\n");
		return (EBAD);
	}

	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();

	ft_write_ln(table, "My Machine", j_find_ref(resp, JK_NAME));
	ft_write_ln(table, "My Username", j_find_ref(resp, JK_USER));
	ft_write_ln(table, "My UID", j_find_ref(resp, JK_UID));
	ft_write_ln(table, "My External IP", j_find_ref(resp, JK_IP_EXT));
	ft_write_ln(table, "My Internal IP", j_find_ref(resp, JK_IP_INT));
	printf("%s\n", ft_to_string(table));
	ft_destroy_table(table);

	j_rm(resp);

	return (EOK);
}

static int mp_shell_ssh(json_t *args)
{
	json_t *root = j_new();
	json_t *resp = NULL;
	ft_table_t *table = NULL;

	j_add_str(root, JK_TYPE, JV_TYPE_SSH);
	j_cp(args, root, JK_UID);
	j_print(root, "Sending SSH command\n");
	resp = execute_requiest(root);
	if (NULL == resp) {
		printf("An error: can't bring information\n");
		return (EBAD);
	}

	j_rm(resp);
	return (EOK);
}

static int mp_shell_get_hosts()
{
	json_t *resp = NULL;
	json_t *root = j_new();
	const char *key;
	json_t *val = NULL;
	ft_table_t *table = NULL;

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_LIST)) {
		DE("No opened ports'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	j_rm(root);

	if (NULL == resp || 0 == j_count(resp)) {
		printf("No host in the list\n");
		j_rm(resp);
		return (0);
	}

	printf("List of connected clients\n");
	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "UID", "External IP", "Internal IP", "Name");

	json_object_foreach(resp, key, val) {

		ft_write_ln(table, j_find_ref(val, JK_UID), j_find_ref(val, JK_IP_EXT),
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

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_PORTS)) {
		DE("Can't add 'command'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	j_rm(root);

	if (NULL == resp || 0 == json_array_size(resp)) {
		printf("No mapped ports\n");
		j_rm(resp);
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
	json_t *host_ports;
	int index;
	const char *key;
	json_t *val = NULL;
	ft_table_t *table = NULL;

	D("Start\n");

	TESTP_MES(root, -1, "Can't allocate JSON object\n");
	if (EOK != j_add_str(root, JK_COMMAND, JV_COMMAND_LIST)) {
		DE("Can't create 'list' request for remote ports'\n");
		return (EBAD);
	}

	resp = execute_requiest(root);
	j_rm(root);

	if (NULL == resp || 0 == j_count(resp)) {
		printf("No host in the list\n");
		j_rm(resp);
		return (0);
	}

	j_print(resp,"resp");

	printf("List of connected clients\n");
	ft_set_default_border_style(FT_PLAIN_STYLE);
	table = ft_create_table();
	ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	ft_write_ln(table, "UID", "External Port", "Internal Port", "Protocol");

	json_object_foreach(resp, key, val) {
		/* Here we are inside of a host */
		json_t *port;
		host_ports = j_find_j(val, "ports");
		json_array_foreach(host_ports, index, port) {
			ft_write_ln(table, j_find_ref(val, JK_UID), j_find_ref(port, JK_PORT_EXT),
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

	args = j_new();
	TESTP_MES(args, -1, "Can't allocate JSON object\n");

	while ((opt = getopt(argc, argv, ":limro:u:s:p:x:c:h")) != -1) {
		switch (opt) {
		case 'i': /* Show this machine info */
			j_add_str(args, JK_SHOW_INFO, JV_YES);
			break;
		case 'l': /* Show remote hosts */
			j_add_str(args, JK_SHOW_HOSTS, JV_YES);
			break;
		case 'o': /* Open port comand (open the port on remote machine UID */
			j_add_str(args, JK_TYPE, JV_TYPE_OPENPORT);
			j_add_str(args, JK_PORT_INT, optarg);
			D("Optarg is %s\n", optarg);
			break;
		case 'c': /* Close port comand (open the port on remote machine UID */
			j_add_str(args, JK_TYPE, JV_TYPE_CLOSEPORT);
			j_add_str(args, JK_PORT_INT, optarg);
			D("Optarg is %s\n", optarg);
			break;
		case 'u': /* UID of remote machine */
			j_add_str(args, JK_UID, optarg);
			break;
		case 's': /* OPen ssh channel for communication */
			j_add_str(args, JK_TYPE, JV_TYPE_SSH);
			j_add_str(args, JK_UID, optarg);
			break;
		case 'p': /* Protocol to use for port opening (-o command) */
			j_add_str(args, JK_PROTOCOL, optarg);
			break;
		case 'm': /* Show ports mapped on this machine */
			j_add_str(args, JK_SHOW_PORTS, JV_YES);
			break;
		case 'r': /* TODO: Show ports mapped on a remote machine (if UID given) / on all remotes (if UID is not specified) */
			j_add_str(args, JK_SHOW_RPORTS, JV_YES);
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
