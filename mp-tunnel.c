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

#define DL_PREFIX "mp-tunnel"
#define DDLOG(fmt, ...) do{syslog(LOG_ALERT, "%s +%d : ", __func__, __LINE__); printf(fmt, ##__VA_ARGS__); }while(0 == 1)

#include "mp-memory.h"
#include "mp-common.h"
#include "mp-tunnel.h"
#include "mp-debug.h"
#include "mp-net-utils.h"

#include "buf_t.h"

#define MAX_WINSIZE 512

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

/* For test only */
#define SERVER_PORT 2294

typedef struct fd_set_struct {
	int fd_in;
	int fd_out;
} fd_set_t;

#define MAX(a,b) ( a > b ? a : b)
/* This is a listener, for direct connection.
   Another function should serve reverse connection */


#if 0

/* Read from stdin, write to socket */
void *mp_tunnel_stdin_to_socket(void *v){
	int n;
	fd_set read_set;
	fd_set_t *fds = (fd_set_t *)v;

	/* Socket sets */
	FD_ZERO(&read_set);
	FD_SET(fds->fd_out, &read_set);
	FD_SET(STDIN_FILENO, &read_set);

	n = MAX(fds->fd_out, STDIN_FILENO) + 1;
	select(n, &read_set, NULL, NULL, NULL);

	/* Data is ready in terminal */
	if (FD_ISSET(STDIN_FILENO, &read_set)) {
		char buf[4096];
		int rr, rs;
		rr = read(STDIN_FILENO, buf, 4096);
		DD("Read %d\n", rr);
		if (rr < 0) {
			DE("Can't receive buffer\n");
			pthread_exit(NULL);
		}

		rs = send(STDIN_FILENO, buf, rr, 0);
		if (rr != rs) {
			DE("Expected to send %d, sent %d\n", rr, rs);
			pthread_exit(NULL);
		}
	}

	return (NULL);
}
#endif

int mp_tunnel_get_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCGWINSZ, sz));
}

int mp_tunnel_set_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCSWINSZ, &sz));
}

/* Ask user name, password. Check it and return answer: EOK if authorized, EBAD on fail */
err_t mp_tunnel_pam_auth(int socket)
{
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

static void mp_tunnel_tty_handler(int sig)
{
	//#if 0
	if (SIGUSR2 != sig) {
		DD("Got signal: %d, ignore\n", sig);
		return;
	}
	//#endif

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	DD("Found signal: %d, setting stop\n", sig);
	exit(0);
}

int mp_tunnel_kill_pty(pid_t pid)
{
	int wstatus;
	DD("Asked to kill forked pty process\n");
	// kill(pid, SIGTERM);
	kill(pid, SIGUSR2);
	kill(pid, SIGKILL);
	//waitpid(pid, &wstatus, WNOHANG);
	waitpid(pid, &wstatus, 0);
	DD("waitpid returned %d\n", wstatus);
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
}


static void *mp_tunnel_tty_server_go(void *v)
{
	int n;
	int terminal = -1;
	int client = *((int *)v);
	pid_t pid;
	struct termios tparams;
	int rc;
	struct winsize sz;
	struct termios tio;

	char *buf;
	size_t buf_size = 0;

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
		DD("Setting window size: col = %d, row = %d\n", sz.ws_col, sz.ws_row);
		pid = forkpty(&terminal, NULL, &tio, &sz);
	} else { /* Can't receive winsize setting from the client */
		pid = forkpty(&terminal, NULL, &tparams, NULL);
		//#if 0
		tcgetattr(terminal, &tparams);
		tparams.c_lflag |= EXTPROC;
		//tparams.c_iflag |= INLCR;
		tparams.c_lflag &= ~ECHO;
		tcsetattr(terminal, TCSANOW, &tparams);
		//#endif

	}

	//enter_raw_mode(terminal);

	if (pid < 0) {
		DE("Can't fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);
		syslog_terminal_flags();
		print_terminal_flags();
		signal(SIGINT, mp_tunnel_tty_handler);
		/* TODO: We may configure which shell to open and even receive it from the other side */
		execlp("/bin/bash", "/bin/login", NULL);
		//execlp("/bin/bash", "/bin/bash", "--login", NULL);
		//execlp("/bin/login", NULL, NULL);
		exit(0);
	}

	buf = zmalloc_any(4096, &buf_size);
	if (NULL == buf) {
		DE("Can't allocate buffer\n");
		mp_tunnel_kill_pty(pid);
		pthread_exit(NULL);
	}

	send(client, WELCOME, sizeof(WELCOME), 0);

	while (1) {
		int rr, rs;
		fd_set read_set;
		fd_set ex_set;
		int rc;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(client, &read_set);
		FD_SET(terminal, &read_set);

		FD_SET(client, &ex_set);
		FD_SET(terminal, &ex_set);

		n = MAX(client, terminal) + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened terminal */
		if (FD_ISSET(client, &ex_set) || FD_ISSET(terminal, &ex_set)) {
			DD("Looks like remote client closed connection\n");
			goto end;
		}

		/* Data is ready in terminal */
		if (FD_ISSET(terminal, &read_set)) {
			rr = read(terminal, buf, buf_size);
			if (NULL == buf) {
				DE("Can't receive buffer\n");
				goto end;
			}

			rs = send(client, (char *)buf, rr, 0);
			if (rs != rr) {
				DE("Can't send buffer: expected to send %d, sent %d\n", rr, rs);
				goto end;
			}
		}

		/* Data is ready on socket */
		if (FD_ISSET(client, &read_set)) {
			/* Read data from socket, write to terminal */
			rr = recv(client, buf, buf_size, 0);
			if (rr < 0) {
				DE("Can't receive buffer\n");
				goto end;
			}

			if (0 == rr) {
				DE("Looks like the socket closed\n");
				goto end;
			}

			/* Write data to the pty */
			rs = write(terminal, buf, rr);

			if (rr != rs) {
				DE("Expected to send %d, sent %d\n", rr, rs);
				goto end;
			}
		}
	}

end:
	close(client);
	close(terminal);
	mp_tunnel_kill_pty(pid);
	DD("Finishing thread and return\n");
	return (0);
}

void *mp_tunnel_tty_server_start(void *v)
{
	/* Socket initialization */
	int sock;
	int one = 1;
	struct sockaddr_in serv_addr;
	// int *port = (int *)v;
	int port = SERVER_PORT;

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
		DD("Going to listen for the next connection\n");
		if (listen(sock, 16)) {
			perror("listen()");
			pthread_exit(NULL);
		}

		socklen_t addrlen = sizeof(struct sockaddr);

		/* TODO: Here we may reject suspicious connection.
			The remote machine may declare preliminary about incoming connection.
			If we do not expect connection from this machine, we reject */

		int client = accept(sock, (struct sockaddr *)&serv_addr, &addrlen);
		dup2(client, 0);
		dup2(client, 1);
		dup2(client, 2);

		pthread_create(&p_go, NULL, mp_tunnel_tty_server_go, &client);
	}
}

void *mp_tunnel_tty_client_start(void *v)
{
	/* Socket initialization */
	int sock;
	//fd_set read_set;
	struct sockaddr_in serv_addr;
	// int *port = (int *)v;
	int port = SERVER_PORT;
	int rc;
	struct winsize sz;
	struct termios tio;
	char *buf;
	size_t buf_size = 0;

	// struct termios tparams;

	DD("Starting client\n");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
		exit(1);
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
	//#endif


	if (tcgetattr(fileno(stdin), &tio) == -1) {
		return;
	}

	rc = send(sock, &tio, sizeof(tio), 0);
	if (sizeof(tio) != rc) {
		DE("Can't sent tio\n");
	}

	/* TODO: From here: this code should be isolated into a separated function */
	print_terminal_flags();
	enter_raw_mode(1);
	write(STDOUT_FILENO, "\n", 1);
	/* Acept connection and create pty */

	buf = zmalloc_any(4096, &buf_size);
	if (NULL == buf) {
		DE("Can't allocate buffer\n");
		pthread_exit(NULL);
	}

	while (1) {
		int n;
		int terminal;
		//char t_count = 0;

		do {
			int rr, rs;
			fd_set read_set;
			fd_set ex_set;

			/* Socket sets */
			FD_ZERO(&read_set);
			FD_ZERO(&ex_set);
			FD_SET(sock, &read_set);
			FD_SET(STDIN_FILENO, &read_set);
			FD_SET(sock, &ex_set);
			FD_SET(STDIN_FILENO, &ex_set);

			n = MAX(sock, STDIN_FILENO) + 1;
			select(n, &read_set, NULL, &ex_set, NULL);

			/* Some exception happened on socket */
			//if (FD_ISSET(sock, &ex_set) || FD_ISSET(STDIN_FILENO, &ex_set)) {
			if (FD_ISSET(sock, &ex_set)) {
				DD("Looks like remote client or terminal closed connection\n");
				goto end;
			}

			/* Data is ready in terminal */
			if (FD_ISSET(STDIN_FILENO, &read_set)) {
				rr = read(STDIN_FILENO, buf, buf_size);
				if (rr < 0) {
					DE("Can't receive buffer\n");
					goto end;
				}

				if (rr >= 1 && 0 == strncmp(buf, "#$close", 7)) {
					DD("Mighty Papa: closing connection\n");
					goto end;
				}

				rs = send(sock, buf, rr, 0);
				if (rr != rs) {
					DE("Expected to send %d, sent %d\n", rr, rs);
					goto end; }
			}

			/* Data is ready on socket */
			if (FD_ISSET(sock, &read_set)) {
				rr = recv(sock, buf, buf_size, 0);
				if (rr < 0) {
					DE("Can't receive buffer\n");
					goto end;
				}

				if (0 == rr) {
					DE("Looks like the server closed the socket\n");
					goto end;
				}
				
				/* Write data to the pty */
				rs = write(STDOUT_FILENO, buf, rr);
				if (rr != rs) {
					DE("Expected to send %d, sent %d\n", rr, rs);
					goto end;
				}
			}
		} while (1);
	}
end:
	close(sock);
	free(buf);
	/* Set saved terminal flags */
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);
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
