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
#include <sys/sendfile.h>
/*@=skipposixheaders@*/

#include "mp-tunnel.h"

#define DL_PREFIX "mp-tunnel"
#define DDLOG(fmt, ...) do{syslog(LOG_ALERT, "%s +%d : ", __func__, __LINE__); printf(fmt, ##__VA_ARGS__); }while(0 == 1)

#include "mp-memory.h"
#include "mp-common.h"
#include "mp-tunnel.h"
#include "mp-debug.h"
#include "mp-limits.h"
#include "mp-net-utils.h"

#include "buf_t.h"

#define MAX_WINSIZE 512
/* TODO: Make this configurable */
//#define READ_BUF 4096
#define READ_BUF 16
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

static int mp_tunnel_get_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCGWINSZ, sz));
}

static int mp_tunnel_set_winsize(int fd, struct winsize *sz)
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

static int mp_tunnel_kill_pty(pid_t pid)
{
	int wstatus;
	DD("Asked to kill forked pty process\n\r");
	kill(pid, SIGKILL);
	waitpid(pid, &wstatus, 0);
	DD("waitpid returned %d\n\r", wstatus);
	return (EOK);
}

/* Copied as is from ssh */
static void enter_raw_mode(int quiet)
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

/* Implementations of connection operations */
static int conn_write_to_socket(int fd, char *buf, size_t sz)
{
	return (send(fd, buf, sz, 0));
}

static int conn_read_from_socket(int fd, char *buf, size_t sz)
{
	return (recv(fd, buf, sz, 0));
}

static int conn_write_to_std(int fd, char *buf, size_t sz)
{
	return (write(fd, buf, sz));
}

static int conn_read_from_std(int fd, char *buf, size_t sz)
{
	return (read(fd, buf, sz));
}

static int mp_tunnel_tunnel_t_init(tunnel_t *tunnel)
{
	memset(tunnel, 0, sizeof(tunnel_t));
	return (sem_init(&tunnel->lock, 0, 1));
}

static void mp_tunnel_lock(tunnel_t *tunnel)
{
	int rc;
	return;
	sem_getvalue(&tunnel->lock, &rc);
	if (rc > 1) {
		DE("Semaphor count is too high: %d > 1\n", rc);
		abort();
	}

	DD("Gettin sem\n");
	rc = sem_wait(&tunnel->lock);
	if (0 != rc) {
		DE("Can't wait on semaphore; abort\n");
		perror("Can't wait on semaphore; abort");
		abort();
	}
}

static void mp_tunnel_unlock(tunnel_t *tunnel)
{
	int rc;
	return;
	sem_getvalue(&tunnel->lock, &rc);
	if (rc > 0) {
		DE("Tried to unlock not locked semaphor\n");
		abort();
	}

	DD("Putting sem\n");
	rc = sem_post(&tunnel->lock);
	if (0 != rc) {
		DE("Can't unlock ctl->lock");
		perror("Can't unlock ctl->lock: abort");
		abort();
	}
}

static void mp_tunnel_tunnel_t_destroy(tunnel_t *tunnel)
{
	sem_wait(&tunnel->lock);
	/* TODO: close fds and free buffer*/
	TFREE_SIZE(tunnel->buf, tunnel->buf_size);
	sem_destroy(&tunnel->lock);
	memset(tunnel, 0, sizeof(tunnel_t));
}

/* TODO: tune it in a real tests */
/* Calculate new optimal buffer size */
static int mp_tunnel_resize(tunnel_t *tunnel)
{
	float  average_left  = 0;
	float  average_right = 0;
	float  average_max;
	float  total_writes;
	size_t new_size      = 0;

	/* What is max average? */
	if (tunnel->left_cnt_session_write_total > 0 && tunnel->left_num_session_writes > 0) {
		average_left = (float)tunnel->left_cnt_session_write_total / (float)tunnel->left_num_session_writes;
	}
	if (tunnel->right_cnt_session_write_total > 0 && tunnel->right_num_session_writes > 0) {
		average_right = (float)tunnel->right_cnt_session_write_total / (float)tunnel->right_num_session_writes;
	}

	average_max = MAX(average_left, average_right);

	/* Now it is a trick. How fast should we resize the buffer?
	 * Let's do it based on statistics. If the max buffer size
	 * used very often (80%+ percent of cases) - grow it fast,
	 * triple it. If is hit the maximum between 50-80% cases -
	 * double it. Else grow it slowly, 20% if its size */

	/* We consider all write operation - to read and to right file
	   descriptore */
	total_writes = tunnel->left_num_session_writes +
				   tunnel->right_num_session_writes;

	/* In 80%+ of writes the full buffer used. Triple  it*/
	if (tunnel->all_cnt_session_max_hits > total_writes * 0.8) {
		new_size = tunnel->buf_size * 3;

		/* In 50%-80% of writes the full buffer used. Double it*/
	} else if (tunnel->all_cnt_session_max_hits > total_writes * 0.5) {
		new_size = tunnel->buf_size * 2;
		/* In < 50% of writes the full buffer used. Double it*/
	} else {
		/* In this case size may be reduced */
		new_size = (float)(average_max * 1.2);
	}
	
	//size_t new_size = (float)(average_max * 1.2);
	if (new_size > MP_LIMIT_TUNNEL_BUF_SIZE) {
		new_size = MP_LIMIT_TUNNEL_BUF_SIZE;
	}

	if (new_size == tunnel->buf_size) {
		goto end;
	}

	DD("Going to resize tunnel buffer from %ld to %ld\n\r", tunnel->buf_size, new_size);
	DD("average left: %f average right: %f\n\r", average_left, average_right);
	mp_tunnel_lock(tunnel);
	TFREE_SIZE(tunnel->buf, tunnel->buf_size);
	tunnel->buf = zmalloc_any(new_size, &tunnel->buf_size);
	mp_tunnel_unlock(tunnel);
	DD("Resized tunnel buffer to %ld\n\r", tunnel->buf_size);

end:
	/* Save total writes for statistics */
	tunnel->right_cnt_write_total += tunnel->right_cnt_session_write_total;
	tunnel->right_num_session_writes = 0;
	tunnel->right_cnt_session_write_total = 0;

	/* Save total write for statistics */
	tunnel->left_cnt_write_total += tunnel->left_cnt_session_write_total;
	tunnel->left_num_session_writes = 0;
	tunnel->left_cnt_session_write_total = 0;

	tunnel->all_cnt_session_max_hits = 0;
	return (EOK);
}

/* We check optimal buffer size every 64 writes. If the buffer
  should be enlarged or shrank, we return 1, else 0 */
static int mp_tunnel_should_resize(tunnel_t *tunnel)
{
	/* Recalculate it after 16 operation done at least */
	if (tunnel->right_num_session_writes + tunnel->left_num_session_writes < 64) {
		return (0);
	}

	/* If from the last resize the 100% of buffer size used in >=
	   50% of writes - resize it */
	float total_writes = tunnel->left_num_session_writes +
						 tunnel->right_num_session_writes;
	if
		(tunnel->all_cnt_session_max_hits > total_writes * 0.5) {
		return (1);
	}

	/* Find the max average for both directions */
	float average_left  = (float)tunnel->left_cnt_session_write_total / tunnel->left_num_session_writes;
	float average_right = (float)tunnel->right_cnt_session_write_total / (float)tunnel->right_num_session_writes;
	float average_max   = MAX(average_left, average_right);

	/* If average transfer > 80% of the current buffer size */
	if ((float)average_max >= (float)tunnel->buf_size * 0.8) {
		return (1);
	}

	/* If average transfer < 50% of the current buffer size */
	if ((float)average_max <= (float)tunnel->buf_size * 0.5) {
		return (1);
	}

	return (0);
}

static int mp_tunnel_tunnel_fill_left(tunnel_t *tunnel, int left_fd, const char *left_name, conn_read_t left_read, conn_write_t left_write, conn_close_t left_close)
{
	TESTP(tunnel, EBAD);
	tunnel->left_fd = left_fd;
	tunnel->left_name = left_name;
	tunnel->left_read = left_read;
	tunnel->left_write = left_write;
	tunnel->left_close = left_close;
	return (EOK);
}

static int mp_tunnel_tunnel_fill_right(tunnel_t *tunnel, int right_fd, const char *right_name, conn_read_t right_read, conn_write_t right_write, conn_close_t right_close)
{
	TESTP(tunnel, EBAD);
	tunnel->right_fd = right_fd;
	tunnel->right_name = right_name;
	tunnel->right_read = right_read;
	tunnel->right_write = right_write;
	tunnel->right_close = right_close;
	return (EOK);
}


/* TCP max socket size is 1 GB */
#define MAX_SOCKET_SIZE (0x40000000)
/* Read data from conn1, send to conn2 */
static inline int mp_tunnel_x_conn_execute_l2r(tunnel_t *tunnel)
{
	ssize_t rr;
	ssize_t rs;

	mp_tunnel_lock(tunnel);
	rr = tunnel->left_read(tunnel->left_fd, tunnel->buf, tunnel->buf_size); mp_tunnel_lock(tunnel);
	if (rr < 0) {
		DE("Error on reading from %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		mp_tunnel_unlock(tunnel);
		return (EBAD);
	}

	if (0 == rr) {
		DE("Probably closed: %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		mp_tunnel_unlock(tunnel);
		return (EBAD);
	}

	rs = tunnel->right_write(tunnel->right_fd, tunnel->buf, rr);
	mp_tunnel_unlock(tunnel);

	if (rs < 0) {
		DE("Error on writing to %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		return (EBAD);
	}

	if (0 == rs) {
		DE("Probably file descriptor is closed: %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		return (EBAD);
	}

	/* One more read done */
	tunnel->right_num_writes++;
	tunnel->right_num_session_writes++;

	/* Count total number of read from left */
	tunnel->right_cnt_session_write_total += rs;

	if ((size_t)rr == tunnel->buf_size) {
		tunnel->all_cnt_session_max_hits++;
	}

	return (EOK);
}

static inline int mp_tunnel_x_conn_execute_r2l(tunnel_t *tunnel)
{
	ssize_t rr;
	ssize_t rs;

	mp_tunnel_lock(tunnel);
	rr = tunnel->right_read(tunnel->right_fd, tunnel->buf, tunnel->buf_size);
	if (rr < 0) {
		DE("Error on reading from %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		mp_tunnel_unlock(tunnel);
		return (EBAD);
	}

	if (0 == rr) {
		DE("Probably closed: %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		return (EBAD);
		mp_tunnel_unlock(tunnel);
	}

	rs = tunnel->left_write(tunnel->left_fd, tunnel->buf, rr);
	mp_tunnel_unlock(tunnel);

	if (rs < 0) {
		DE("Error on writing to %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		return (EBAD);
	}

	if (0 == rs) {
		DE("Probably file descriptor is closed: %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		return (EBAD);
	}

	/* One more read done */
	tunnel->left_num_writes++;
	tunnel->left_num_session_writes++;

	/* Count total number of read from left */
	tunnel->left_cnt_session_write_total += rs;

	if ((size_t)rr == tunnel->buf_size) {
		tunnel->all_cnt_session_max_hits++;
	}

	return (EOK);
}

/* 
 * This is a pretty universal function to 'pipe' two connection.
 * Doesn't matter what is the connection type, the most important, it must have an 'int' file descriptor. 
 * Because we use 'select' to watch the file descriptors.
 * It does its job until an error or until one ot the connections is disconnected
 */
static inline int mp_tunnel_run_x_conn(tunnel_t *tunnel)
{
	mp_tunnel_lock(tunnel);
	tunnel->buf = zmalloc_any(READ_BUF, &tunnel->buf_size);
	mp_tunnel_unlock(tunnel);

	while (1) {
		int    rc;
		fd_set read_set;
		fd_set ex_set;
		int    n;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(tunnel->right_fd, &read_set);
		FD_SET(tunnel->left_fd, &read_set);

		FD_SET(tunnel->left_fd, &ex_set);
		FD_SET(tunnel->right_fd, &ex_set);

		n = MAX(tunnel->left_fd, tunnel->right_fd) + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened */
		if (FD_ISSET(tunnel->left_fd, &ex_set) || FD_ISSET(tunnel->right_fd, &ex_set)) {
			DD("Some exception happened. Ending x_connect\n");
			return (EBAD);
		}

		/* Data is ready on connection 1: read from conn1, write to conn2 */
		if (FD_ISSET(tunnel->left_fd, &read_set)) {
			rc = mp_tunnel_x_conn_execute_l2r(tunnel);
			if (EOK != rc) goto end;
		}

		/* Data is ready on connection 2: read from conn2, write to conn1 */
		if (FD_ISSET(tunnel->right_fd, &read_set)) {
			rc = mp_tunnel_x_conn_execute_r2l(tunnel);
			if (EOK != rc) goto end;
		}

		if (tunnel->buf_size < MP_LIMIT_TUNNEL_BUF_SIZE && mp_tunnel_should_resize(tunnel)) {
			mp_tunnel_resize(tunnel);
		}
	}
end:
	//TFREE(tunnel->buf);
	//tunnel->buf_size = 0;
	return (EOK);
}

static void mp_tunnel_print_stat(tunnel_t *tunnel)
{
	DD("%s -> %s writes: %ld\n\r", tunnel->left_name, tunnel->right_name, tunnel->right_num_writes);
	DD("%s -> %s total bytes: %ld\n\r", tunnel->left_name, tunnel->right_name, tunnel->right_cnt_write_total);
	DD("%s -> %s average write: %f\n\r", tunnel->left_name, tunnel->right_name, (float)tunnel->right_cnt_write_total / tunnel->right_num_writes);

	DD("%s -> %s writes: %ld\n\r", tunnel->right_name, tunnel->left_name, tunnel->left_num_writes);
	DD("%s -> %s total bytes: %ld\n\r", tunnel->right_name, tunnel->left_name, tunnel->left_cnt_write_total);
	DD("%s -> %s average write: %f\n\r", tunnel->right_name, tunnel->left_name, (float)tunnel->left_cnt_write_total / tunnel->left_num_writes);

	DD("Buffer size: %ld\n\r", tunnel->buf_size);
}

static void *mp_tunnel_tty_server_go(void *v)
{
	int            terminal = -1;
	int            client   = *((int *)v);
	pid_t          pid;
	struct termios tparams;
	int            rc;
	struct winsize sz;
	struct termios tio;
	tunnel_t       tunnel;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	mp_tunnel_tunnel_t_init(&tunnel);

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

	mp_tunnel_tunnel_fill_external(&tunnel, client, "Server:Socket", conn_read_from_socket, conn_write_to_socket, NULL);
	mp_tunnel_tunnel_fill_internal(&tunnel, terminal, "Server:PTY", conn_read_from_std, conn_write_to_std, NULL);

	rc = mp_tunnel_run_x_conn(&tunnel);

	DD("Returned from mp_tunnel_run_x_conn\n\r");

	/* No need to join thread - they were dettached */

	mp_tunnel_print_stat(&tunnel);
	mp_tunnel_tunnel_t_destroy(&tunnel);
	close(client);
	close(terminal);
	DD("Close file descriptors\n\r");
	mp_tunnel_kill_pty(pid);
	DD("Finishing thread\n\r");
	pthread_exit(NULL);
}

void *mp_tunnel_tty_server_start(void *v)
{
	/* Socket initialization */
	int                sock;
	int                one       = 1;
	struct sockaddr_in serv_addr;
	// int *port = (int *)v;
	int                port      = SERVER_PORT;

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

		int       client  = accept(sock, (struct sockaddr *)&serv_addr, &addrlen);
		pthread_create(&p_go, NULL, mp_tunnel_tty_server_go, &client);
	}
}

void *mp_tunnel_tty_client_start(void *v)
{
	/* Socket initialization */
	int                sock;
	struct sockaddr_in serv_addr;
	int                port      = SERVER_PORT;
	int                rc;
	struct winsize     sz;
	struct termios     tio;
	tunnel_t           tunnel;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	mp_tunnel_tunnel_t_init(&tunnel);

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

	mp_tunnel_tunnel_fill_external(&tunnel, sock, "Client:Socket", conn_read_from_socket, conn_write_to_socket, NULL);
	mp_tunnel_tunnel_fill_internal(&tunnel, STDIN_FILENO, "Client:STDIN", conn_read_from_std, conn_write_to_std, NULL);

	rc = mp_tunnel_run_x_conn(&tunnel);

	DD("Finished mp_tunnel_run_x_conn\n\r");

	/* Set saved terminal flags */
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	close(sock);
	DD("Remote connection closed\n\r");
	mp_tunnel_print_stat(&tunnel);
	mp_tunnel_tunnel_t_destroy(&tunnel);
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
		printf("Usage:%s -c for client, -s for server\n", argv[0] );
	}
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);

	if (0 == strcmp(argv[1] , "-s")) {
		pthread_t pid;
		DD("Going to open server\n");
		// mp_tunnel_tty_server_start(NULL);
		pthread_create(&pid, NULL, mp_tunnel_tty_server_start, NULL);
		pthread_join(pid, NULL);
		DD("Finished\n\r");
		return (0);
	}

	if (0 == strcmp(argv[1] , "-c")) {
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
