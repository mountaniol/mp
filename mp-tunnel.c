/*@-skipposixheaders@*/
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <pty.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <syslog.h>
/*@=skipposixheaders@*/

#include "mp-tunnel.h"

#define DL_PREFIX "mp-tunnel"
#define DDLOG(fmt, ...) do{syslog(LOG_ALERT, "%s +%d : ", __func__, __LINE__); printf(fmt, ##__VA_ARGS__); }while(0 == 1)

#include "mp-memory.h"
#include "mp-common.h"
#include "mp-tunnel.h"
#include "mp-debug.h"
#include "mp-net-utils.h"

#include "buf_t.h"

#define MAX_WINSIZE 512
/* TODO: Make this configurable */
#define READ_BUF 4096
/* For test only */
#define SERVER_PORT 2294
#define MAX(a,b) ( a > b ? a : b)

#define WELCOME \
"+-+-+-+-+-+-+ +-+-+-+-+\n\
|M|i|g|h|t|y| |P|a|p|a|\n\
+-+-+-+-+-+-+ +-+-+-+-+\n"

#if 0 /* SEB 22/05/2020 15:32  */

	#define WELCOME \
".  .    .   ,      .__\n\
|\/|* _ |_ -+-  .  [__) _.._  _.\n\
|  ||(_][ ) | \_|  |   (_][_)(_]\n\
     ._|      ._|         |     \n"

	#define WELCOME "Mighty Papa welcomes you\n"
#endif /* SEB 22/05/2020 15:32 */

int mp_tunnel_get_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCGWINSZ, sz));
}

int mp_tunnel_set_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCSWINSZ, &sz));
}

/* Ask user name, password. Check it and return answer: EOK if authorized, EBAD on fail */
#if 0
err_t mp_tunnel_pam_auth(int socket){
	struct pam_conv conv;
	pam_handle_t *pamh = NULL;
	int retval;
	const char *user = "nobody";

	retval = pam_start("check_user", user, &conv, &pamh);

	if (retval == PAM_SUCCESS) retval = pam_authenticate(pamh, 0);    /* is user really user? */

	if (retval == PAM_SUCCESS) retval = pam_acct_mgmt(pamh, 0);       /* permitted access? */

	/* This is where we have been authorized or not. */

	if (retval == PAM_SUCCESS) {
		fprintf(stdout, "Authenticated\n");
	} else {
		fprintf(stdout, "Not Authenticated\n");
	}

	if (pam_end(pamh, retval) != PAM_SUCCESS) {     /* close Linux-PAM */
		pamh = NULL;
		fprintf(stderr, "check_user: failed to release authenticator\n");
		exit(1);
	}

	return (retval == PAM_SUCCESS ? 0 : 1);       /* indicate success */
}
#endif

static void mp_tunnel_tty_handler(int sig)
{
	//#if 0
	if (SIGUSR2 != sig) {
		DD("Got signal: %d, ignore\n\r", sig);
		return;
	}
	//#endif

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	DD("Found signal: %d, setting stop\n\r", sig);

	return;
}

int mp_tunnel_kill_pty(pid_t pid)
{
	int wstatus;
	DD("Asked to kill forked pty process\n\r");
	kill(pid, SIGKILL);
	waitpid(pid, &wstatus, 0);
	DD("waitpid returned %d\n\r", wstatus);
	return (EOK);
}

/* Copied as is from ssh */
void enter_raw_mode(int quiet)
{
	struct termios tio;

	if (tcgetattr(fileno(stdin), &tio) == -1) {
		if (!quiet) perror("tcgetattr");
		return;
	}

	cfmakeraw(&tio);

#if 0

	//_saved_tio = tio;
	tio.c_iflag |= IGNPAR;
	//tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
	tio.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | IXON | IXANY | IXOFF);
	#ifdef IUCLC
	tio.c_iflag &= ~IUCLC;
	#endif
	tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
	#ifdef IEXTEN
	tio.c_lflag &= ~IEXTEN;
	#endif
	tio.c_oflag &= ~OPOST;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
#endif


	/* SEB: Now add needed flags */
	//tio.c_iflag |= ICRNL | IXON | ICRNL | ICANON;
	//tio.c_lflag |= ICANON;

	if (tcsetattr(fileno(stdin), TCSADRAIN, &tio) == -1) {
		if (!quiet) perror("tcsetattr");
	} //else _in_raw_mode = 1;
}

/* Copied as is from ssh */
void print_terminal_flags()
{
	struct termios tio;

	if (tcgetattr(fileno(stdin), &tio) == -1) {
		return;
	}

#define PRINT_I_FLAG(t, f) if (t.c_iflag & f) printf("%s\tYES\n", #f); else printf("%s\tNO\n", #f);
#define PRINT_L_FLAG(t, f) if (t.c_iflag & f) printf("%s\tYES\n", #f); else printf("%s\tNO\n", #f);
#define PRINT_O_FLAG(t, f) if (t.c_iflag & f) printf("%s\tYES\n", #f); else printf("%s\tNO\n", #f);

	PRINT_I_FLAG(tio, IGNPAR);
	PRINT_I_FLAG(tio, ISTRIP);
	PRINT_I_FLAG(tio, INLCR);
	PRINT_I_FLAG(tio, IGNCR);
	PRINT_I_FLAG(tio, ICRNL);
	PRINT_I_FLAG(tio, IXON);
	PRINT_I_FLAG(tio, IXANY);
	PRINT_I_FLAG(tio, ICRNL);
	PRINT_I_FLAG(tio, IXOFF);
#ifdef IUCLC
	PRINT_L_FLAG(tio, IUCLC);
#endif
	PRINT_L_FLAG(tio, ISIG);
	PRINT_L_FLAG(tio, ICANON);
	PRINT_L_FLAG(tio, ECHO);
	PRINT_L_FLAG(tio, ECHOE);
	PRINT_L_FLAG(tio, ECHOK);
	PRINT_L_FLAG(tio, ECHONL);
#ifdef IEXTEN
	PRINT_I_FLAG(tio, IEXTEN);
#endif
	PRINT_O_FLAG(tio, OPOST);
	printf("tio.c_cc[VMIN] = %d\n", tio.c_cc[VMIN]);
	printf("tio.c_cc[VTIME] = %d\n", tio.c_cc[VTIME]);
#undef PRINT_I_FLAG
#undef PRINT_L_FLAG
#undef PRINT_O_FLAG
}

/* Implementations of connection operations */
int conn_write_to_socket(int fd, char *buf, size_t sz)
{
	return (send(fd, buf, sz, 0));
}

int conn_read_from_socket(int fd, char *buf, size_t sz)
{
	return (recv(fd, buf, sz, 0));
}

int conn_write_to_std(int fd, char *buf, size_t sz)
{
	return (write(fd, buf, sz));
}

int conn_read_from_std(int fd, char *buf, size_t sz)
{
	return (read(fd, buf, sz));
}

void conn_t_fill(conn_t *conn, int fd, conn_read_t read_fd, conn_write_t write_fd, conn_close_t close_fd, const char *name)
{
	conn->fd = fd;
	conn->close_fd = close_fd;
	conn->read_fd = read_fd;
	conn->write_fd = write_fd;
	conn->name = name;
}

void sig_hnd(int sig)
{
	//(void)sig;
	printf("Got signal %d\n", sig);
}

/* Read data from conn1, send to conn2 */
static inline int mp_tunnel_x_conn_execute(conn_t *conn1, conn_t *conn2, char *buf, const size_t buf_size)
{
	int rr, rs;
	rr = conn1->read_fd(conn1->fd, buf, buf_size);
	if (rr < 0) {
		DE("Error on reading from %s\n\r", conn1->name ? conn1->name : "conn1");
		return (EBAD);
	}

	if (0 == rr) {
		DE("Probably closed: %s\n", conn1->name ? conn1->name : "conn1");
		return (EBAD);
	}

	/* Write buffer to another connection */
	rs = conn2->write_fd(conn2->fd, buf, rr);

	/* Is there an error on writing? */
	if (rs < 0) {
		DE("Error (return < 0) on write to %s\n", conn2->name ? conn2->name : "conn2");
		return (EBAD);
	}

	/* We can't write asked anount of data */
	if (rs != rr) {
		DE("Error (write != read) on write to %s\n", conn2->name ? conn2->name : "conn2");
		return (EBAD);
	}
	/* All good */
	return (EOK);
}

/* This thread watch connection 1, read from it and writes to connection 2 */
/* For every connection we run two such a threads, one for each direction */
void *mp_tunnel_run_x_one_direction_conn(void *v)
{
	//(SIGCHLD, SIG_IGN);
	/* In this structure we get 2 connection:
	   in connection - read from
	   out connection - write to */
	conn2_t *cons = (conn2_t *)v;

	size_t buf_size;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	char *buf = zmalloc_any(READ_BUF, &buf_size);
	if (NULL == buf) {
		DE("Can't allocate buffer\n");
		pthread_exit(NULL);
		//return (EBAD);
	}

	while (1) {
		int rc;
		fd_set read_set;
		fd_set ex_set;
		int n;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(cons->conn_in.fd, &read_set);
		FD_SET(cons->conn_in.fd, &ex_set);

		n = cons->conn_in.fd + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened */
		if (FD_ISSET(cons->conn_in.fd, &ex_set)) {
			DD("Some exception happened. Ending x_connect\n");
			cons->status = EBAD;
			free(buf);
			pthread_exit(NULL);
			//return (NULL);
		}

		/* Data is ready on connection 1: read from conn1, write to conn2 */
		if (FD_ISSET(cons->conn_in.fd, &read_set)) {
			rc = mp_tunnel_x_conn_execute(&cons->conn_in, &cons->conn_out, buf, buf_size);
			if (EOK != rc) {
				cons->status = EBAD;
				free(buf);
				DE("Finishing\n");
				pthread_exit(NULL);
			}
		}
	}

	/* Should never be here*/
	pthread_exit(NULL);
	//return (NULL);
}

/* 
 * This is a pretty universal function to 'pipe' two connection.
 * Doesn't matter what is the connection type, the most important, it must have an 'int' file descriptor. 
 * Because we use 'select' to watch the file descriptors.
 * It does its job until an error or until one ot the connections is disconnected
 */
static inline int mp_tunnel_run_x_conn(conn_t *conn1, conn_t *conn2)
{
	size_t buf_size;
	char *buf = zmalloc_any(READ_BUF, &buf_size);
	if (NULL == buf) {
		DE("Can't allocate buffer\n");
		return (EBAD);
	}

	while (1) {
		int rc;
		fd_set read_set;
		fd_set ex_set;
		int n;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(conn1->fd, &read_set);
		FD_SET(conn2->fd, &read_set);

		FD_SET(conn1->fd, &ex_set);
		FD_SET(conn2->fd, &ex_set);

		n = MAX(conn1->fd, conn2->fd) + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened */
		if (FD_ISSET(conn1->fd, &ex_set) || FD_ISSET(conn2->fd, &ex_set)) {
			DD("Some exception happened. Ending x_connect\n");
			return (EBAD);
		}

		/* Data is ready on connection 1: read from conn1, write to conn2 */
		if (FD_ISSET(conn1->fd, &read_set)) {
			rc = mp_tunnel_x_conn_execute(conn1, conn2, buf, buf_size);
			if (EOK != rc) goto end;
		}

		/* Data is ready on connection 2: read from conn2, write to conn1 */
		if (FD_ISSET(conn2->fd, &read_set)) {
			rc = mp_tunnel_x_conn_execute(conn2, conn1, buf, buf_size);
			if (EOK != rc) goto end;
		}
	}
end:
	free(buf);
	return (EOK);
}

/* Copied as is from ssh */
void syslog_terminal_flags()
{
	struct termios tio;

	if (tcgetattr(fileno(stdin), &tio) == -1) {
		return;
	}

#define PRINT_I_FLAG(t, f) if (t.c_iflag & f) DDLOG("%s\tYES\n", #f); else printf("%s\tNO\n", #f);
#define PRINT_L_FLAG(t, f) if (t.c_iflag & f) DDLOG("%s\tYES\n", #f); else printf("%s\tNO\n", #f);
#define PRINT_O_FLAG(t, f) if (t.c_iflag & f) DDLOG("%s\tYES\n", #f); else printf("%s\tNO\n", #f);

	PRINT_I_FLAG(tio, IGNPAR);
	PRINT_I_FLAG(tio, ISTRIP);
	PRINT_I_FLAG(tio, INLCR);
	PRINT_I_FLAG(tio, IGNCR);
	PRINT_I_FLAG(tio, ICRNL);
	PRINT_I_FLAG(tio, IXON);
	PRINT_I_FLAG(tio, IXANY);
	PRINT_I_FLAG(tio, ICRNL);
	PRINT_I_FLAG(tio, IXOFF);
#ifdef IUCLC
	PRINT_L_FLAG(tio, IUCLC);
#endif
	PRINT_L_FLAG(tio, ISIG);
	PRINT_L_FLAG(tio, ICANON);
	PRINT_L_FLAG(tio, ECHO);
	PRINT_L_FLAG(tio, ECHOE);
	PRINT_L_FLAG(tio, ECHOK);
	PRINT_L_FLAG(tio, ECHONL);
#ifdef IEXTEN
	PRINT_I_FLAG(tio, IEXTEN);
#endif
	PRINT_O_FLAG(tio, OPOST);
	DDLOG("tio.c_cc[VMIN] = %d\n", tio.c_cc[VMIN]);
	DDLOG("tio.c_cc[VTIME] = %d\n", tio.c_cc[VTIME]);
#undef PRINT_I_FLAG
#undef PRINT_L_FLAG
#undef PRINT_O_FLAG
}


static void *mp_tunnel_tty_server_go(void *v)
{
	int terminal = -1;
	int client = *((int *)v);
	pid_t pid;
	struct termios tparams;
	int rc;
	struct winsize sz;
	struct termios tio;
	conn_t conn_terminal;
	conn_t conn_socket;

	//conn2_t conn_one_direction;
	//conn2_t conn_second_direction;

	//pthread_t pid_one;
	//pthread_t pid_two;

	char *buf;
	size_t buf_size;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	if (client < 0) {
		DE("Can't accept connection\n");
		/* TODO: What should we do here? New iteration? Break? */
		pthread_exit(0);
	}

	/* Create PTY */

	/* Should receive termios setting from the client*/
	rc = recv(client, &sz, sizeof(struct winsize), 0);
	rc += recv(client, &tio, sizeof(tio), 0);

	if (sizeof(struct winsize) + sizeof(tio) == rc) {
		pid = forkpty(&terminal, NULL, &tio, &sz);
	} else { /* Can't receive winsize setting from the client */
		pid = forkpty(&terminal, NULL, &tparams, NULL);
		tcgetattr(terminal, &tparams);
		tparams.c_lflag |= EXTPROC;
		tparams.c_lflag &= ~ECHO;
		//tparams.c_cc[VEOF] = 3; // ^C
		//tparams.c_cc[VINTR] = 4; // ^D
		tcsetattr(terminal, TCSANOW, &tparams);
	}

	if (pid < 0) {
		DE("Can't fork");
		pthread_exit(NULL);
		//exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);

		printf("%s", WELCOME);
		signal(SIGUSR2, mp_tunnel_tty_handler);
		//(SIGINT, mp_tunnel_tty_handler);
		/* TODO: We may configure which shell to open and even receive it from the other side */
		execlp("/bin/bash", "/bin/bash", NULL);
		pthread_exit(NULL);
		//exit(0);
	}

	buf = zmalloc_any(READ_BUF, &buf_size);
	if (NULL == buf) {
		DE("Can't allocate buffer\n");
		mp_tunnel_kill_pty(pid);
		pthread_exit(NULL);
	}

	/* All done, start x_conn - read-write from one descriptor to another unbtil an error or disconnect */
	conn_t_fill(&conn_socket, client, conn_read_from_socket, conn_write_to_socket, NULL, "Server:Socket");
	conn_t_fill(&conn_terminal, terminal, conn_read_from_std, conn_write_to_std, NULL, "Server:PTY");

	rc = mp_tunnel_run_x_conn(&conn_socket, &conn_terminal);

	DD("Returned from mp_tunnel_run_x_conn\n\r");

#if 0

	conn_one_direction.conn_in = conn_socket;
	conn_one_direction.conn_out = conn_terminal;

	conn_second_direction.conn_in = conn_terminal;
	conn_second_direction.conn_out = conn_socket;

	pthread_create(&pid_one, NULL, mp_tunnel_run_x_one_direction_conn, &conn_one_direction);
	pthread_create(&pid_two, NULL, mp_tunnel_run_x_one_direction_conn, &conn_second_direction);

	while (1) {
		rc = pthread_kill(pid_one, 0);
		if (0 != rc) {
			pthread_kill(pid_two, SIGTERM);
			break;
		}
		rc = pthread_kill(pid_two, 0);
		if (0 != rc) {
			pthread_kill(pid_one, SIGTERM);
			break;
		}
		mp_os_usleep(200);
	}
#endif

	/* No need to join thread - they were dettached */

	// rc = mp_tunnel_run_x_conn(&conn_socket, &conn_terminal);

	free(buf);
	close(client);
	close(terminal);
	DD("Close file descriptors\n\r");
	mp_tunnel_kill_pty(pid);
	DD("Finishing thread\n\r");
	//return (0);
	pthread_exit(NULL);
}

void *mp_tunnel_tty_server_start(void *v)
{
	/* Socket initialization */
	int sock;
	int one = 1;
	struct sockaddr_in serv_addr;
	// int *port = (int *)v;
	int port = SERVER_PORT;

	//signal(SIGCHLD, SIG_IGN);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("server socket()");
		pthread_exit(NULL);
	}

	/* allow later reuse of the socket (without timeout) */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int))) {
		perror("setsockopt()");
		pthread_exit(NULL);
	}

	if (bind(sock, (const struct sockaddr *)&serv_addr, sizeof(struct sockaddr))) {
		perror("bind()");
		pthread_exit(NULL);
	}

	/* TODO: From here: this code should be isolated into a separated function */

	/* Acept connection and create pty */
	while (1) {
		pthread_t p_go;

		/* TODO: This number should not be hardcoded */
		if (listen(sock, 16)) {
			perror("listen()");
			pthread_exit(NULL);
		}

		socklen_t addrlen = sizeof(struct sockaddr);

		/* TODO: Here we may reject suspicious connection.
			The remote machine may declare preliminary about incoming connection.
			If we do not expect connection from this machine, we reject */

		int client = accept(sock, (struct sockaddr *)&serv_addr, &addrlen);
		//dup2(client, 0);
		//dup2(client, 1);
		//dup2(client, 2);
#if 0
		signal(SIGINT, sig_hnd);
		signal(SIGTERM, sig_hnd);
		signal(SIGHUP, sig_hnd);
		signal(SIGTSTP, sig_hnd);
		signal(SIGTTIN, sig_hnd);
		signal(SIGTTOU, sig_hnd);
		signal(SIGCHLD, sig_hnd);
#endif
		pthread_create(&p_go, NULL, mp_tunnel_tty_server_go, &client);
	}
}

void *mp_tunnel_tty_client_start(void *v)
{
	/* Socket initialization */
	int sock;
	struct sockaddr_in serv_addr;
	int port = SERVER_PORT;
	int rc;
	struct winsize sz;
	struct termios tio;
	conn_t conn_stdin;
	conn_t conn_socket;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}
	
	DD("Starting client\n");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
		pthread_exit(NULL);
		//exit(1);
	}

	if (INADDR_NONE == serv_addr.sin_addr.s_addr) {
		DE("Can't create inet addr\n");
		perror("inet_addr");
		return (NULL);
	}

	rc = connect(sock, (const struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
	if (0 != rc) {
		perror("connect()");
		pthread_exit(NULL);
	}

	//#if 0
	rc = mp_tunnel_get_winsize(STDOUT_FILENO, &sz);
	if (0 != rc) {
		DE("mp_tunnel_get_winsize failed: status is %d\n", rc);
	}

	rc = send(sock, &sz, sizeof(struct winsize), 0);
	if (sizeof(struct winsize) != rc) {
		DE("Can't sent winsize\n");
	}

	DD("Sent window size: col = %d, row = %d\n", sz.ws_col, sz.ws_row);

	if (tcgetattr(fileno(stdin), &tio) == -1) {
		return (NULL);
	}

	rc = send(sock, &tio, sizeof(tio), 0);
	if (sizeof(tio) != rc) {
		DE("Can't sent tio\n");
		pthread_exit(NULL);
	}

	/* TODO: From here: this code should be isolated into a separated function */
	enter_raw_mode(1);

	/* Acept connection and create pty */

	conn_t_fill(&conn_socket, sock, conn_read_from_socket, conn_write_to_socket, NULL, "Client:Socket");
	conn_t_fill(&conn_stdin, STDIN_FILENO, conn_read_from_std, conn_write_to_std, NULL, "Client:STDIN");

	rc = mp_tunnel_run_x_conn(&conn_socket, &conn_stdin);

	printf("Finished mp_tunnel_run_x_conn\n");

	/* Set saved terminal flags */
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	close(sock);
	printf("Remote cnnection closed\n");
	pthread_exit(NULL);
}

#if 0

/* 
 * Start the tunnel server and wait for connection from the tunnel client 
 * Listen for incoming connection on port 'port_to_listen' 
 * When connection s established, read configuration and do what the client wants. 
 * The tunnel server should support theses options: 
 *  
 * 1. Open unencrypted connection and forward all traffic to local port X . 
 *    We may use it for SSH connection running inside this tunel; in this case we do not need the encryption,
 *    SSH will do it.
 * 2. Open encrypted tunnel and forward all traffic to port X
 * 3. Open encrypted tunnel and connect to local PTY 
 * 4. Open encrypted connection and work as FUSE filesystem 
 *  
 */

err_t mp_tunnel_server(int port_to_listen, int connection_type){

	return (EOK);
}

/* 
 * Start the tunnel client and connect to the remote server on target host 
 * We may use this connection same way as server connection, see the comment for mp_tunnel_server() 
 *  
 * However, the client also listens on local port 'local_port' and forward all data 
 * received from 'local_port' to remote server 
 *  
 */
err_t mp_tunnel_client(char *target_host, int target_port, int local_port, int connection_type){
	return (EOK);
}
#endif


#ifdef STANDALONE
int main(int argi, char *argv[])
{
	if (argi < 2) {
		printf("Usage:%s -c for client, -s for server\n", argv[0]);
	}
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);

	#if 0
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	#endif

	if (0 == strcmp(argv[1], "-s")) {
		pthread_t pid;
		DD("Going to open server\n");
		// mp_tunnel_tty_server_start(NULL);
		pthread_create(&pid, NULL, mp_tunnel_tty_server_start, NULL);
		pthread_join(pid, NULL);
		DD("Finished\n");
		return (0);
	}

	if (0 == strcmp(argv[1], "-c")) {
		pthread_t pid;
		DD("Going to open client\n");
		pthread_create(&pid, NULL, mp_tunnel_tty_client_start, NULL);
		// mp_tunnel_tty_client_start(NULL);
		pthread_join(pid, NULL);
		DD("Finished\n");
		return (0);
	}

	return (0);
}
#endif /* STANDALONE */
