/*@-skipposixheaders@*/
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <pty.h>
//#include <security/pam_appl.h>
//#include <security/pam_misc.h>
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
#include <openssl/ssl.h>
#include <openssl/err.h>
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
#include "mp-config.h"

#include "buf_t/buf_t.h"

#define MAX_WINSIZE 512
/* TODO: Make this configurable */
#define READ_BUF 4096
//#define READ_BUF 16
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

/* If tunnel runs in standalone mode, we shoud implement certificate loading and CTX creation
   here. */

static int mp_tunnel_get_winsize(int fd, struct winsize *sz)
{
	return (ioctl(fd, TIOCGWINSZ, sz));
}

#if 0
static int mp_tunnel_set_winsize(int fd, struct winsize *sz){
	return (ioctl(fd, TIOCSWINSZ, &sz));
}
#endif

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
	if (SIGUSR2 != sig) {
		DD("Got signal: %d, ignore\n\r", sig);
		return;
	}

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

/* Init tunnel */
int mp_tunnel_tunnel_t_init(tunnel_t *tunnel)
{
	memset(tunnel, 0, sizeof(tunnel_t));
	tunnel->fd[TUN_RIGHT] = -1;
	tunnel->fd[TUN_LEFT] = -1;

	return (sem_init(&tunnel->buf_lock[TUN_RIGHT], 0, 1) + sem_init(&tunnel->buf_lock[TUN_LEFT], 0, 1));
}

/* Alloc and init the tunnel structure */
tunnel_t *mp_tunnel_tunnel_t_alloc(void)
{
	tunnel_t *tunnel = malloc(sizeof(tunnel_t));
	TESTP(tunnel, NULL);

	if (EOK != mp_tunnel_tunnel_t_init(tunnel)) {
		DE("Can't init tunnel");
		free(tunnel);
		return (NULL);
	}
	return (tunnel);
}

/*** LOCKS */

static void mp_tunnel_lock_right(tunnel_t *tunnel)
{
	int rc;
	return;

	if (NULL == tunnel) {
		DE("tunnel is NULL\n\r");
		return;
	}

	sem_getvalue(&tunnel->buf_lock[TUN_RIGHT], &rc);
	if (rc > 1) {
		DE("Semaphor (left) count is too high: %d > 1\n\r", rc);
		abort();
	}

	DD("Gettin sem\n\r");
	rc = sem_wait(&tunnel->buf_lock[TUN_RIGHT]);
	if (0 != rc) {
		DE("Can't wait on left semaphore; abort\n\r");
		perror("Can't wait on left semaphore; abort");
		abort();
	}

	DD("Locked right buf\n\r");
}

static void mp_tunnel_lock_left(tunnel_t *tunnel)
{
	int rc;
	return;

	if (NULL == tunnel) {
		DE("tunnel is NULL\n\r");
		return;
	}

	sem_getvalue(&tunnel->buf_lock[TUN_LEFT], &rc);
	if (rc > 1) {
		DE("Semaphor (right) count is too high: %d > 1\n\r", rc);
		abort();
	}

	DD("Gettin sem\n\r");
	rc = sem_wait(&tunnel->buf_lock[TUN_LEFT]);
	if (0 != rc) {
		DE("Can't wait on right semaphore; abort\n\r");
		perror("Can't wait on right semaphore; abort");
		abort();
	}

	DD("Locked left buf\n\r");
}

static void mp_tunnel_unlock_right(tunnel_t *tunnel)
{
	int rc;
	return;

	if (NULL == tunnel) {
		DE("tunnel is NULL\n\r");
		return;
	}

	sem_getvalue(&tunnel->buf_lock[TUN_RIGHT], &rc);
	if (rc > 0) {
		DE("Tried to unlock not locked left semaphor\n\r");
		abort();
	}

	DD("Putting sem\n\r");
	rc = sem_post(&tunnel->buf_lock[TUN_RIGHT]);
	if (0 != rc) {
		DE("Can't unlock ctl->left_lock\n\r");
		perror("Can't unlock ctl->left_lock: abort");
		abort();
	}

	DD("Unlocked right buf\n\r");
}

static void mp_tunnel_unlock_left(tunnel_t *tunnel)
{
	int rc;
	return;

	if (NULL == tunnel) {
		DE("tunnel is NULL\n\r");
		return;
	}

	sem_getvalue(&tunnel->buf_lock[TUN_LEFT], &rc);
	if (rc > 0) {
		DE("Tried to unlock not locked right semaphor\n\r");
		abort();
	}

	DD("Putting sem\n\n");
	rc = sem_post(&tunnel->buf_lock[TUN_LEFT]);
	if (0 != rc) {
		DE("Can't unlock ctl->right_lock\n\r");
		perror("Can't unlock ctl->right_lock: abort");
		abort();
	}

	DD("Unlocked left buf\n\r");
}

void mp_tunnel_tunnel_t_destroy(tunnel_t *tunnel)
{
	int i = 0;
	if (NULL == tunnel) {
		DE("tunnel is NULL\n");
		return;
	}

	DE("Destroying tunnel\n");

	sem_wait(&tunnel->buf_lock[TUN_LEFT]);
	TFREE_SIZE(tunnel->buf[TUN_LEFT], tunnel->buf_size[TUN_LEFT]);
	sem_destroy(&tunnel->buf_lock[TUN_LEFT]);
	DE("Destroied left lock\n");

	sem_wait(&tunnel->buf_lock[TUN_RIGHT]);
	TFREE_SIZE(tunnel->buf[TUN_RIGHT], tunnel->buf_size[TUN_RIGHT]);
	sem_destroy(&tunnel->buf_lock[TUN_RIGHT]);
	DE("Destroied right lock\n");

	DE("Destroied locks\n");

	for (i = 0; i < TUN_MAX; i++) {
		if (NULL != tunnel->ssl[i] ) {
			SSL_free(tunnel->ssl[i]);
		}

		if (NULL != tunnel->ctx[i] ) {
			SSL_CTX_free(tunnel->ctx[i] );
		}

		if (NULL != tunnel->rsa[i] ) {
			RSA_free(tunnel->rsa[i] );
		}

		if (NULL != tunnel->x509[i]) {
			X509_free(tunnel->x509[i]);
		}
	}

	if (NULL != tunnel->left_server) {
		free(tunnel->left_server);
	}

	if (NULL != tunnel->right_server) {
		free(tunnel->right_server);
	}

	if (NULL != tunnel->buf[TUN_RIGHT]) {
		free(tunnel->buf[TUN_RIGHT]);
	}

	if (NULL != tunnel->buf[TUN_LEFT]) {
		free(tunnel->buf[TUN_LEFT]);
	}

	memset(tunnel, 0, sizeof(tunnel_t));

	DE("Finished\n");
}

/*** TUNNEL CONFIGURATION API */

/*** Configure tunnel flags */

int mp_tun_set_flags(tunnel_t *t, int direction, uint32_t flags)
{
	TESTP(t, 0xFFFFFFFF);
	t->flags[direction] = flags;
	return (t->flags[direction]);
}

uint32_t mp_tun_get_flags(tunnel_t *t, int direction)
{
	TESTP(t, 0xFFFFFFFF);
	return (t->flags[direction]);
}

/*** Left / right buffer size */

int mp_tun_get_set_buf_size_left(tunnel_t *t, size_t size)
{
	TESTP(t, -1);
	/* TODO: lock left buf when set size */
	t->buf_size[TUN_LEFT] = size;
	return (EOK);
}

int mp_tun_get_set_buf_size_ext(tunnel_t *t, size_t size)
{
	return (mp_tun_get_set_buf_size_left(t, size));
}

int mp_tun_get_set_buf_size_right(tunnel_t *t, size_t size)
{
	TESTP(t, -1);
	/* TODO: lock left buf when set size */
	t->buf_size[TUN_RIGHT] = size;
	return (EOK);
}

int mp_tun_get_set_buf_size_intern(tunnel_t *t, size_t size)
{
	return (mp_tun_get_set_buf_size_right(t, size));
}


/*** Set / get tunnel file descriptors */

int mp_tun_get_set_fd_left(tunnel_t *t, int fd)
{
	TESTP(t, -1);
	t->fd[TUN_LEFT] = fd;
	return (EOK);
}

int mp_tun_get_set_fd_right(tunnel_t *t, int fd)
{
	TESTP(t, -1);
	t->fd[TUN_RIGHT] = fd;
	return (EOK);
}

int mp_tun_get_get_fd_left(tunnel_t *t)
{
	TESTP(t, -1);
	return (t->fd[TUN_LEFT]);
}

int mp_tun_get_get_fd_right(tunnel_t *t)
{
	TESTP(t, -1);
	return (t->fd[TUN_RIGHT]);
}

/*** Set / get right / left side name (used for debug and info) */

int mp_tun_get_set_name_left(tunnel_t *t, const char *name, size_t name_len)
{
	TESTP(t, -1);
	t->left_name = strndup(name, name_len);
	return (EOK);
}

char *mp_tun_get_get_name_left(tunnel_t *t)
{
	TESTP(t, NULL);
	TESTP(t->right_name, NULL);
	return (strdup(t->left_name));
}

int mp_tun_get_set_name_right(tunnel_t *t, const char *name, size_t name_len)
{
	TESTP(t, -1);
	t->right_name = strndup(name, name_len);
	return (EOK);
}

char *mp_tun_get_get_name_right(tunnel_t *t)
{
	TESTP(t, NULL);
	TESTP(t->right_name, NULL);
	return (strdup(t->right_name));
}

/*** Set / get right / left X509 and private RSA key */

int mp_tun_set_x509_rsa(tunnel_t *t, int direction, void *x509, void *rsa)
{
	TESTP(t, -1);
	t->rsa[direction] = rsa;
	t->x509[direction] = x509;
	return (EOK);
}

/*** Set / get left / right server name + port */

int mp_tun_set_server_port_left(tunnel_t *t, const char *server, int port)
{
	TESTP(t, -1);

	if (NULL != t->left_server) {
		free(t->left_server);
		t->left_server = NULL;
	}
	if (NULL != server) {
		t->left_server = strdup(server);
	}

	t->left_port = port;
	return (EOK);
}

char *mp_tun_get_server_left(tunnel_t *t)
{
	char *ret = NULL;
	TESTP(t, NULL);

	if (NULL != t->left_server) {
		ret = strdup(t->left_server);
	}
	return (ret);
}

int mp_tun_get_port_left(tunnel_t *t)
{
	TESTP(t, -1);
	return (t->left_port);
}


int mp_tun_set_server_port_right(tunnel_t *t, const char *server, int port)
{
	TESTP(t, -1);

	if (NULL != t->right_server) {
		free(t->right_server);
		t->right_server = NULL;
	}
	if (NULL != server) {
		t->right_server = strdup(server);
	}

	t->right_port = port;
	return (EOK);
}

char *mp_tun_get_server_right(tunnel_t *t)
{
	char *ret = NULL;
	TESTP(t, NULL);

	if (NULL != t->right_server) {
		ret = strdup(t->right_server);
	}
	return (ret);
}

int mp_tun_get_port_right(tunnel_t *t)
{
	TESTP(t, -1);
	return (t->right_port);
}

/*** TUN2: Set / get right / left X509 and private RSA key */

int mp_tun2_set_x509_rsa(tunnel_t *t, tun_stream_t stream, void *x509, void *rsa)
{
	TESTP(t, -1);
	t->rsa[stream] = rsa;
	t->x509[stream] = x509;
	return (EOK);
}

char *mp_tun2_get_x509(tunnel_t *t, tun_stream_t stream)
{
	TESTP(t, NULL);
	return (t->x509[stream] );
}

char *mp_tun2_get_rsa(tunnel_t *t, tun_stream_t stream)
{
	TESTP(t, NULL);
	return (t->rsa[stream] );
}


/* TODO: tune it in a real tests */
/* Calculate new optimal buffer size */
static int mp_tunnel_resize_right_buf(tunnel_t *tunnel)
{
	float  average_left = 0;
	size_t new_size     = 0;

	TESTP_MES(tunnel, EBAD, "tunnel is NULL");

	/* What is max average? */
	if (tunnel->left_cnt_session_write_total > 0 && tunnel->left_num_session_writes > 0) {
		average_left = (float)tunnel->left_cnt_session_write_total / (float)tunnel->left_num_session_writes;
	}

	/* Now it is a trick. How fast should we resize the buffer?
	 * Let's do it based on statistics. If the max buffer size
	 * used very often (80%+ percent of cases) - grow it fast,
	 * triple it. If is hit the maximum between 50-80% cases -
	 * double it. Else grow it slowly, 20% if its size */

	/* We consider all write operation - to read and to right file
	   descriptore */

	/* In 80%+ of writes the full buffer used. Triple  it*/
	if (tunnel->left_all_cnt_session_max_hits > tunnel->left_num_session_writes * 0.8) {
		new_size = tunnel->buf_size[TUN_RIGHT] * 3;

		/* In 50%-80% of writes the full buffer used. Double it*/
	} else
	if (tunnel->left_all_cnt_session_max_hits > tunnel->left_num_session_writes * 0.5) {
		new_size = tunnel->buf_size[TUN_RIGHT] * 2;
		/* In < 50% of writes the full buffer used. Double it*/
	} else {
		/* In this case size may be reduced */
		new_size = (float)(average_left * 1.2);
	}

	if (new_size > MP_LIMIT_TUNNEL_BUF_SIZE_MAX) {
		new_size = MP_LIMIT_TUNNEL_BUF_SIZE_MAX;
	}

	if (new_size < MP_LIMIT_TUNNEL_BUF_SIZE_MIN) {
		new_size = MP_LIMIT_TUNNEL_BUF_SIZE_MIN;
	}

	if (new_size == tunnel->buf_size[TUN_RIGHT]) {
		goto end;
	}

	DD("Going to resize r2l tunnel buffer from %ld to %ld\n\r", tunnel->buf_size[TUN_RIGHT], new_size);
	DD("average left: %f\n\r", average_left);

	mp_tunnel_lock_right(tunnel);
	TFREE_SIZE(tunnel->buf[TUN_RIGHT], tunnel->buf_size[TUN_RIGHT]);
	tunnel->buf[TUN_RIGHT] = zmalloc_any(new_size, &tunnel->buf_size[TUN_RIGHT]);
	mp_tunnel_unlock_right(tunnel);

	DD("Resized r2l tunnel buffer to %ld\n\r", tunnel->buf_size[TUN_RIGHT]);

end:
	/* Save total write for statistics */
	tunnel->left_cnt_write_total += tunnel->left_cnt_session_write_total;
	tunnel->left_num_session_writes = 0;
	tunnel->left_cnt_session_write_total = 0;

	tunnel->left_all_cnt_session_max_hits = 0;
	return (EOK);
}

/* TODO: tune it in a real tests */
/* r2l means "right to left", it means we write to the left buffer*/
/* So we measure and change left beffer (r2l*/
/* Calculate new optimal buffer size */
static int mp_tunnel_resize_left_buf(tunnel_t *tunnel)
{
	float  average_right = 0;
	size_t new_size      = 0;

	TESTP_MES(tunnel, EBAD, "tunnel is NULL");

	/* What is max average? */
	if (tunnel->right_cnt_session_write_total > 0 && tunnel->right_num_session_writes > 0) {
		average_right = (float)tunnel->right_cnt_session_write_total / (float)tunnel->right_num_session_writes;
	}

	/* Now it is a trick. How fast should we resize the buffer?
	 * Let's do it based on statistics. If the max buffer size
	 * used very often (80%+ percent of cases) - grow it fast,
	 * triple it. If is hit the maximum between 50-80% cases -
	 * double it. Else grow it slowly, 20% if its size */

	/* We consider all write operation - to read and to right file
	   descriptore */

	/* In 80%+ of writes the full buffer used. Triple  it*/
	if (tunnel->right_all_cnt_session_max_hits > tunnel->right_num_session_writes * 0.8) {
		new_size = tunnel->buf_size[TUN_LEFT] * 3;
		/* In 50%-80% of writes the full buffer used. Double it*/
	} else
	if (tunnel->right_all_cnt_session_max_hits > tunnel->right_num_session_writes * 0.5) {
		new_size = tunnel->buf_size[TUN_LEFT] * 2;
		/* In < 50% of writes the full buffer used. Double it*/
	} else {
		/* In this case size may be reduced */
		new_size = (float)(average_right * 1.2);
	}

	//size_t new_size = (float)(average_max * 1.2);
	if (new_size > MP_LIMIT_TUNNEL_BUF_SIZE_MAX) {
		new_size = MP_LIMIT_TUNNEL_BUF_SIZE_MAX;
	}

	if (new_size < MP_LIMIT_TUNNEL_BUF_SIZE_MIN) {
		new_size = MP_LIMIT_TUNNEL_BUF_SIZE_MIN;
	}

	if (new_size == tunnel->buf_size[TUN_RIGHT]) {
		goto end;
	}


	DD("Going to resize l2r tunnel buffer from %ld to %ld\n\r", tunnel->buf_size[TUN_LEFT], new_size);
	DD("average left: %f\n\r", average_right);
	mp_tunnel_lock_left(tunnel);
	TFREE_SIZE(tunnel->buf[TUN_LEFT], tunnel->buf_size[TUN_LEFT]);
	tunnel->buf[TUN_LEFT] = zmalloc_any(new_size, &tunnel->buf_size[TUN_LEFT]);
	mp_tunnel_unlock_left(tunnel);
	DD("Resized l2r tunnel buffer to %ld\n\r", tunnel->buf_size[TUN_LEFT]);

end:
	/* Save total write for statistics */
	tunnel->right_cnt_write_total += tunnel->right_cnt_session_write_total;
	tunnel->right_num_session_writes = 0;
	tunnel->right_cnt_session_write_total = 0;

	tunnel->right_all_cnt_session_max_hits = 0;
	return (EOK);
}

/* We check optimal buffer size every 64 writes. If the buffer
  should be enlarged or shrank, we return 1, else 0 */
static int mp_tunnel_should_resize_l2r(tunnel_t *tunnel)
{
	TESTP_MES(tunnel, 0, "tunnel is NULL");
	/* Recalculate it after 16 operation done at least */
	if (tunnel->right_num_session_writes < 64) {
		return (0);
	}

	/* If from the last resize the 100% of buffer size used in >=
	   50% of writes - resize it */
	if (tunnel->right_all_cnt_session_max_hits > tunnel->right_num_session_writes * 0.5) {
		return (1);
	}

	/* Find the max average for both directions */
	float average_right = (float)tunnel->right_cnt_session_write_total / (float)tunnel->right_num_session_writes;

	/* If average transfer > 80% of the current buffer size */
	if ((float)average_right >= (float)tunnel->buf_size[TUN_LEFT] * 0.8) {
		return (1);
	}

	/* If average transfer < 50% of the current buffer size */
	if (((float)average_right <= (float)tunnel->buf_size[TUN_LEFT] * 0.5) &&
		average_right >= MP_LIMIT_TUNNEL_BUF_SIZE_MIN) {
		return (1);
	}

	return (0);
}

static int mp_tunnel_should_resize_r2l(tunnel_t *tunnel)
{
	TESTP_MES(tunnel, 0, "tunnel is NULL");
	/* Recalculate it after 16 operation done at least */
	if (tunnel->left_num_session_writes < 64) {
		return (0);
	}

	/* If from the last resize the 100% of buffer size used in >=
	   50% of writes - resize it */
	if (tunnel->left_all_cnt_session_max_hits > tunnel->left_num_session_writes * 0.5) {
		return (1);
	}

	/* Find the max average for both directions */
	float average_left = (float)tunnel->left_cnt_session_write_total / (float)tunnel->left_num_session_writes;

	/* If average transfer > 80% of the current buffer size */
	if ((float)average_left >= (float)tunnel->buf_size[TUN_RIGHT] * 0.8) {
		return (1);
	}

	/* If average transfer < 50% of the current buffer size */
	if (((float)average_left <= (float)tunnel->buf_size[TUN_RIGHT] * 0.5) &&
		average_left >= MP_LIMIT_TUNNEL_BUF_SIZE_MIN) {
		return (1);
	}

	return (0);
}

static int mp_tunnel_tunnel_fill_left(tunnel_t *tunnel, int fd, const char *left_name, conn_read_t left_read, conn_write_t left_write, conn_close_t left_close)
{
	TESTP(tunnel, EBAD);
	tunnel->fd[TUN_LEFT] = fd;
	tunnel->left_name = left_name;
	tunnel->left_read = left_read;
	tunnel->left_write = left_write;
	tunnel->left_close = left_close;
	return (EOK);
}

static int mp_tunnel_tunnel_fill_right(tunnel_t *tunnel, int fd, const char *right_name, conn_read_t right_read, conn_write_t right_write, conn_close_t right_close)
{
	TESTP(tunnel, EBAD);
	tunnel->fd[TUN_RIGHT] = fd;
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

	TESTP_MES(tunnel, EBAD, "tunnel is NULL");

	mp_tunnel_lock_left(tunnel);

	/* If 'left_ssl()' is not we use the SSL version*/
	if (NULL != tunnel->ssl[TUN_LEFT] ) {
		int read_blocked = 0;
		int ssl_error    = 0;

		rr = SSL_read(tunnel->ssl[TUN_LEFT] , tunnel->buf[TUN_LEFT], tunnel->buf_size[TUN_LEFT]);

		/*** TODO: We should handle here SSL errors */
		//check SSL errors
		switch (ssl_error = SSL_get_error(tunnel->ssl[TUN_LEFT] , rr)) {
		case SSL_ERROR_NONE: /* All right */
			break;
		case SSL_ERROR_ZERO_RETURN: /* Connection closed by other side */
			return (EBAD);
			break;
		case SSL_ERROR_WANT_READ: /* Operation is not completed, enable "blocked" mode */
			read_blocked = 1;
			break;
		case SSL_ERROR_WANT_WRITE: /* The operation is not completed */
			break;
		case SSL_ERROR_SYSCALL: /* Some error, the operation should be terminated and the client should be disconnected */
			return (EBAD);
			break;
		default: /* Some error, clean up and close connection */
			return (EBAD);
			break;
		} while (SSL_pending(tunnel->ssl[TUN_LEFT] ) && !read_blocked);
		tunnel->right_num_writes_ssl++;
	}  /* End of the SSL handler */
	else {
		assert(NULL != tunnel->left_read);
		rr = tunnel->left_read(tunnel->fd[TUN_LEFT], tunnel->buf[TUN_LEFT], tunnel->buf_size[TUN_LEFT]);
	}

	if (rr < 0) {
		DE("Error on reading from %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		mp_tunnel_unlock_left(tunnel);
		return (EBAD);
	}

	if (0 == rr) {
		DE("Probably closed: %s\n\r", tunnel->left_name ? tunnel->left_name : "Left fd");
		mp_tunnel_unlock_left(tunnel);
		return (EBAD);
	}

	if (NULL != tunnel->ssl[TUN_RIGHT]) {
		rs = SSL_write(tunnel->ssl[TUN_RIGHT], tunnel->buf[TUN_LEFT], rr);
		/*** TODO: We should handle here SSL errors */
	} else {
		assert(NULL != tunnel->right_write);
		rs = tunnel->right_write(tunnel->fd[TUN_RIGHT], tunnel->buf[TUN_LEFT], rr);
	}

	mp_tunnel_unlock_left(tunnel);

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

	if ((size_t)rr == tunnel->buf_size[TUN_LEFT]) {
		tunnel->left_all_cnt_session_max_hits++;
	}

	return (EOK);
}

static inline int mp_tunnel_x_conn_execute_r2l(tunnel_t *tunnel)
{
	ssize_t rr;
	ssize_t rs;

	TESTP_MES(tunnel, EBAD, "tunnel is NULL");

	mp_tunnel_lock_right(tunnel);

	if (NULL != tunnel->ssl[TUN_RIGHT]) {
		int ssl_error    = 0;
		int read_blocked = 0;
		rr = SSL_read(tunnel->ssl[TUN_RIGHT], tunnel->buf[TUN_RIGHT], tunnel->buf_size[TUN_RIGHT]);
		/*** TODO: Handle SSL errors */
		//check SSL errors
		switch (ssl_error = SSL_get_error(tunnel->ssl[TUN_RIGHT] , rr)) {
		case SSL_ERROR_NONE: /* All right */
			break;
		case SSL_ERROR_ZERO_RETURN: /* Connection closed by other side */
			return (EBAD);
			break;
		case SSL_ERROR_WANT_READ: /* Operation is not completed, enable "blocked" mode */
			read_blocked = 1;
			break;
		case SSL_ERROR_WANT_WRITE: /* The operation is not completed */
			break;
		case SSL_ERROR_SYSCALL: /* Some error, the operation should be terminated and the client should be disconnected */
			return (EBAD);
			break;
		default: /* Some error, clean up and close connection */
			return (EBAD);
			break;
		  } while (SSL_pending(tunnel->ssl[TUN_RIGHT]) && !read_blocked);

	} /* End of SSL handler */
	else {
		assert(NULL != tunnel->right_read);
		rr = tunnel->right_read(tunnel->fd[TUN_RIGHT], tunnel->buf[TUN_RIGHT], tunnel->buf_size[TUN_RIGHT]);
	}

	if (rr < 0) {
		DE("Error on reading from %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		mp_tunnel_unlock_right(tunnel);
		return (EBAD);
	}

	if (0 == rr) {
		DE("Probably closed: %s\n\r", tunnel->right_name ? tunnel->right_name : "Right fd");
		return (EBAD);
		mp_tunnel_unlock_right(tunnel);
	}

	/* If 'left_ssl' is not NULL - use SSL operation */
	if (NULL != tunnel->ssl[TUN_LEFT] ) {
		rs = SSL_write(tunnel->ssl[TUN_LEFT] , tunnel->buf[TUN_RIGHT], rr);
		tunnel->left_num_writes++;
		/*** TODO: Handle SSL errors */
	} else {
		assert(NULL != tunnel->left_write);
		rs = tunnel->left_write(tunnel->fd[TUN_LEFT], tunnel->buf[TUN_RIGHT], rr);
	}

	mp_tunnel_unlock_right(tunnel);

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

	if ((size_t)rr == tunnel->buf_size[TUN_LEFT]) {
		tunnel->left_all_cnt_session_max_hits++;
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
	TESTP_MES(tunnel, EBAD, "tunnel is NULL");

	mp_tunnel_lock_left(tunnel);
	tunnel->buf[TUN_LEFT] = zmalloc_any(READ_BUF, &tunnel->buf_size[TUN_LEFT]);
	mp_tunnel_unlock_left(tunnel);
	TESTP(tunnel->buf[TUN_LEFT], EBAD);

	mp_tunnel_lock_right(tunnel);
	tunnel->buf[TUN_RIGHT] = zmalloc_any(READ_BUF, &tunnel->buf_size[TUN_RIGHT]);
	mp_tunnel_unlock_right(tunnel);
	TESTP(tunnel->buf[TUN_RIGHT], EBAD);

	while (1) {
		int    rc;
		fd_set read_set;
		fd_set ex_set;
		int    n;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(tunnel->fd[TUN_RIGHT], &read_set);
		FD_SET(tunnel->fd[TUN_LEFT], &read_set);

		FD_SET(tunnel->fd[TUN_LEFT], &ex_set);
		FD_SET(tunnel->fd[TUN_RIGHT], &ex_set);

		n = MAX(tunnel->fd[TUN_LEFT], tunnel->fd[TUN_RIGHT]) + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened */
		if (FD_ISSET(tunnel->fd[TUN_LEFT], &ex_set) || FD_ISSET(tunnel->fd[TUN_RIGHT], &ex_set)) {
			DD("Some exception happened. Ending x_connect\n");
			return (EBAD);
		}

		/* Data is ready on left file descriptor: read from conn1, write to conn2 */
		if (FD_ISSET(tunnel->fd[TUN_LEFT], &read_set)) {
			rc = mp_tunnel_x_conn_execute_l2r(tunnel);
			if (EOK != rc) goto end;
		}
		/* Data is ready on the right file descriptor: read from conn2, write to conn1 */
		if (FD_ISSET(tunnel->fd[TUN_RIGHT], &read_set)) {
			rc = mp_tunnel_x_conn_execute_r2l(tunnel);
			if (EOK != rc) goto end;
		}

		if (tunnel->buf_size[TUN_LEFT] < MP_LIMIT_TUNNEL_BUF_SIZE_MAX && mp_tunnel_should_resize_l2r(tunnel)) {
			mp_tunnel_resize_left_buf(tunnel);
		}

		if (tunnel->buf_size[TUN_RIGHT] < MP_LIMIT_TUNNEL_BUF_SIZE_MAX && mp_tunnel_should_resize_r2l(tunnel)) {
			mp_tunnel_resize_right_buf(tunnel);
		}
	}
end:
	return (EOK);
}

/* Like the previous function mp_tunnel_run_x_conn() but use ssl file handlers */
static inline int mp_tunnel_run_x_conn_ssl(tunnel_t *tunnel)
{
	TESTP_MES(tunnel, EBAD, "tunnel is NULL");
	mp_tunnel_lock_left(tunnel);
	tunnel->buf[TUN_LEFT] = zmalloc_any(READ_BUF, &tunnel->buf_size[TUN_LEFT]);
	mp_tunnel_unlock_left(tunnel);

	mp_tunnel_lock_right(tunnel);
	tunnel->buf[TUN_RIGHT] = zmalloc_any(READ_BUF, &tunnel->buf_size[TUN_RIGHT]);
	mp_tunnel_unlock_right(tunnel);

	while (1) {
		int    rc;
		fd_set read_set;
		fd_set ex_set;
		int    n;

		/* Socket sets */
		FD_ZERO(&read_set);
		FD_ZERO(&ex_set);

		FD_SET(tunnel->fd[TUN_RIGHT], &read_set);
		FD_SET(tunnel->fd[TUN_LEFT], &read_set);

		FD_SET(tunnel->fd[TUN_LEFT], &ex_set);
		FD_SET(tunnel->fd[TUN_RIGHT], &ex_set);

		n = MAX(tunnel->fd[TUN_LEFT], tunnel->fd[TUN_RIGHT]) + 1;

		rc = select(n, &read_set, NULL, &ex_set, NULL);

		/* Some exception happened */
		if (FD_ISSET(tunnel->fd[TUN_LEFT], &ex_set) || FD_ISSET(tunnel->fd[TUN_RIGHT], &ex_set)) {
			DD("Some exception happened. Ending x_connect\n");
			return (EBAD);
		}

		/* Data is ready on connection 1: read from conn1, write to conn2 */
		if (FD_ISSET(tunnel->fd[TUN_LEFT], &read_set)) {
			rc = mp_tunnel_x_conn_execute_l2r(tunnel);
			if (EOK != rc) goto end;
		}

		/* Data is ready on connection 2: read from conn2, write to conn1 */
		if (FD_ISSET(tunnel->fd[TUN_RIGHT], &read_set)) {
			rc = mp_tunnel_x_conn_execute_r2l(tunnel);
			if (EOK != rc) goto end;
		}

		if (tunnel->buf_size[TUN_LEFT] < MP_LIMIT_TUNNEL_BUF_SIZE_MAX && mp_tunnel_should_resize_l2r(tunnel)) {
			mp_tunnel_resize_left_buf(tunnel);
		}

		if (tunnel->buf_size[TUN_RIGHT] < MP_LIMIT_TUNNEL_BUF_SIZE_MAX && mp_tunnel_should_resize_r2l(tunnel)) {
			mp_tunnel_resize_right_buf(tunnel);
		}
	}
end:
	return (EOK);
}

static void mp_tunnel_print_stat(tunnel_t *tunnel)
{
	DD("%s -> %s writes: %ld (ssl: %ld)\n\r", tunnel->left_name, tunnel->right_name, tunnel->right_num_writes, tunnel->right_num_writes_ssl);
	DD("%s -> %s total bytes: %ld\n\r", tunnel->left_name, tunnel->right_name, tunnel->right_cnt_write_total);
	DD("%s -> %s average write: %f\n\r", tunnel->left_name, tunnel->right_name, (float)tunnel->right_cnt_write_total / tunnel->right_num_writes);

	DD("%s -> %s writes: %ld (ssl: %ld)\n\r", tunnel->right_name, tunnel->left_name, tunnel->left_num_writes, tunnel->left_num_writes_ssl);
	DD("%s -> %s total bytes: %ld\n\r", tunnel->right_name, tunnel->left_name, tunnel->left_cnt_write_total);
	DD("%s -> %s average write: %f\n\r", tunnel->right_name, tunnel->left_name, (float)tunnel->left_cnt_write_total / tunnel->left_num_writes);

	DD("Buffer size: %ld\n\r", tunnel->buf_size[TUN_LEFT]);
}


/* Don't load certificates from the file, use in-memory */
int mp_tunnel_set_cert(SSL_CTX *ctx, void *x509, void *priv_rsa)
{
	TESTP_MES(ctx, EBAD, "CTX is NULL");
	TESTP_MES(x509, EBAD, "X509 is NULL");
	TESTP_MES(priv_rsa, EBAD, "RSA private key is NULL");

	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate(ctx, x509) <= 0) {
		DE("Can't use ctl->x509 certificate\n");
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}

	/* set the private key from KeyFile (may be the same as CertFile) */
	if (SSL_CTX_use_RSAPrivateKey(ctx, priv_rsa) <= 0) {
		DE("Can't use ctl->rsa private key\n");
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}

	/* verify private key */
	if (1 != SSL_CTX_check_private_key(ctx)) {
		DE("Private key does not match the public certificate\n");
		return (EBAD);
	}

	//DD("Success: created OpenSSL CTX object\n");
	return (EOK);
}

#ifdef STANDALONE
/* Create SSL context (CTX) */
/*@null@*/ SSL_CTX *mp_tunnel_init_server_tls_ctx(tunnel_t *t, int direction)
{
	const SSL_METHOD *method;
	SSL_CTX          *ctx;

	/* TODO: Do we really need all algorythms? */
	/* No return value */
	OpenSSL_add_all_algorithms();      /* load & register all cryptos, etc. */

	/* No return value */
	SSL_load_error_strings();       /* load all error messages */
	method = TLS_server_method();
	if (NULL == method) {
		DE("Can't create 'method' for CTX context");
		return (NULL);
	}

	ctx = SSL_CTX_new(method);       /* create new context from method */
	if (ctx == NULL) {
		DE("Can't create OpenSSL 'ctx' object\n");
		ERR_print_errors_fp(stderr);
		return (NULL);
	}

	/* Establish connection automatically; enable partial write */
	/* The return value of this function is useless for us */
	(void)SSL_CTX_set_mode(ctx, (long int)(SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE));

	/* Load certificates */
	/* Before the CTX set to use X509 cert and RSA private key, they should be created and loaded  */
	if (NULL == t->x509[direction]) {
		DE("Can not proceed - no X509 certificate loaded\n");

	}
	if (EOK != mp_tunnel_set_cert(ctx, t->x509[direction], t->rsa[direction])) {
		SSL_CTX_free(ctx);
		return (NULL);
	}

	return (ctx);
}

/*@null@*/ SSL_CTX *mp_tunnel_init_client_tls_ctx(tunnel_t *t, int direction)
{
	const SSL_METHOD *method;
	SSL_CTX          *ctx;

	/* TODO: Do we really need all algorythms? */
	/* No return value */
	OpenSSL_add_all_algorithms();      /* load & register all cryptos, etc. */

	/* No return value */
	SSL_load_error_strings();       /* load all error messages */
	method = TLS_client_method();
	if (NULL == method) {
		DE("Can't create 'method' for CTX context");
		return (NULL);
	}

	ctx = SSL_CTX_new(method);       /* create new context from method */
	if (ctx == NULL) {
		DE("Can't create OpenSSL 'ctx' object\n");
		ERR_print_errors_fp(stderr);
		return (NULL);
	}

	/* Establish connection automatically; enable partial write */
	/* THe return value of this function is useless for us */
	(void)SSL_CTX_set_mode(ctx, (long int)(SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE));

	/* Load certificates */
	/* Before the CTX set to use X509 cert and RSA private key, they should be created and loaded  */
	if (NULL == t->x509[direction] ) {
		DE("Can not proceed - no X509 certificate loaded\n");

	}
	if (EOK != mp_tunnel_set_cert(ctx, t->x509[direction], t->rsa[direction])) {
		SSL_CTX_free(ctx);
		return (NULL);
	}

	return (ctx);
}

/* Create SSL context (CTX) */
int mp_tunnel_ssl_set_ctx(tunnel_t *t, int direction, void *ctx)
{
	TESTP(t, EBAD);
	TESTP(ctx, EBAD);
	t->ctx[direction] = ctx;
	return (EOK);
}

int mp_tunnel_ssl_set_rsa(tunnel_t *t, int direction, void *rsa)
{
	TESTP(t, EBAD);
	TESTP(rsa, EBAD);
	t->rsa[direction] = rsa;
	return (EOK);
}

int mp_tunnel_ssl_set_x509(tunnel_t *t, int direction, void *x509)
{
	TESTP(t, EBAD);
	TESTP(x509, EBAD);
	t->x509[direction] = x509;
	return (EOK);
}

/* Load SSL certificates / keys */

static int mp_tunnel_init_server_ssl(tunnel_t *t, int direction)
{
	void *rsa  = NULL;
	void *ctx  = NULL;
	void *x509 = NULL;
	int  rc    = -1;

	rsa = mp_config_load_rsa_priv();
	if (NULL == rsa) {
		DE("Can't load RSA private\n");
		return (EBAD);
	}

	rc = mp_tunnel_ssl_set_rsa(t, direction, rsa);
	if (EOK != rc) {
		DE("Can't set RSA private left\n");
		return (EBAD);
	}

	x509 = mp_config_load_X509();
	if (NULL == x509) {
		DE("Can't load X509 cetrificate\n");
		return (EBAD);
	}
	rc = mp_tunnel_ssl_set_x509(t, direction, x509);
	if (EOK != rc) {
		DE("Can't set X509 left\n");
		return (EBAD);
	}

	ctx = mp_tunnel_init_server_tls_ctx(t, direction);
	if (NULL == ctx) {
		DE("Can't create CTX\n");
		return (EBAD);
	}

	rc = mp_tunnel_ssl_set_ctx(t, direction, ctx);
	if (EOK != rc) {
		DE("Can't set CTX\n");
		return (EBAD);
	}

	return (EOK);
}

static int mp_tunnel_init_client_ssl(tunnel_t *t, int direction)
{
	t->rsa[direction] = mp_config_load_rsa_priv();
	if (NULL == t->rsa[direction]) {
		DE("Can't load RSA private\n");
		return (EBAD);
	}

	t->x509[direction] = mp_config_load_X509();
	if (NULL == t->x509[direction]) {
		DE("Can't load X509 cetrificate\n");
		return (EBAD);
	}

	t->ctx[direction] = mp_tunnel_init_client_tls_ctx(t, direction);
	if (NULL == t->ctx[direction]) {
		DE("Can't create CTX\n");
		return (EBAD);
	}

	return (EOK);
}

#endif /* STANDALONE */

/* 
 * Create SSL connection from opened socket.
 * The left fd must be already opened socket must
 * be set: t->ssl != NULL Warning: tunnel->lef_fd
 * must be a socket!
 */
static int mp_tunnel_start_ssl_conn_left(tunnel_t *t){
	TESTP(t, EBAD);

	if (NULL == t->ctx[TUN_LEFT]) {
		DE("Tunnel left CTX is NULL\n");
		return (EBAD);
	}

	if (t->fd[TUN_LEFT] < 0) {
		DE("Left fd is not a valid file descriptor\n");
		return (EBAD);
	}

	t->ssl[TUN_LEFT] = SSL_new(t->ctx[TUN_LEFT]);
	TESTP_MES(t->ssl[TUN_LEFT] , EBAD, "Can't allocate left SSL");

	if (1 != SSL_set_fd(t->ssl[TUN_LEFT] , t->fd[TUN_LEFT])) {
		DE("Can't set left fd into *SSL");
		SSL_free(t->ssl[TUN_LEFT] );
		t->ssl[TUN_LEFT] = NULL;
		return (EBAD);
	}
	return (EOK);
}

static int mp_tunnel_start_ssl_conn_right(tunnel_t *t)
{
	TESTP(t, EBAD);

	if (NULL == t->ctx[TUN_RIGHT]) {
		DE("Tunnel right CTX is NULL\n");
		return (EBAD);
	}

	if (t->fd[TUN_RIGHT] < 0) {
		DE("Right fd is not a valid file descriptor\n");
		return (EBAD);
	}

	t->ssl[TUN_RIGHT] = SSL_new(t->ctx[TUN_RIGHT]);
	TESTP_MES(t->ssl[TUN_RIGHT], EBAD, "Can't allocate right SSL");

	if (1 != SSL_set_fd(t->ssl[TUN_RIGHT], t->fd[TUN_RIGHT])) {
		DE("Can't set right fd into *SSL");
		SSL_free(t->ssl[TUN_RIGHT]);
		t->ssl[TUN_RIGHT] = NULL;
		return (EBAD);
	}
	return (EOK);
}

static int mp_tunnel_start_ssl_conn_internal(tunnel_t *t)
{
	return (mp_tunnel_start_ssl_conn_right(t));
}

static void *mp_tunnel_tty_server_go(void *v)
{
	pid_t          pid;
	struct termios tparams;
	int            rc;
	struct winsize sz;
	struct termios tio;
	tunnel_t       *tunnel = v;

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	if (tunnel->fd[TUN_LEFT] < 0) {
		DE("Can't accept connection\n");
		/* TODO: What should we do here? New iteration? Break? */
		pthread_exit(0);
	}

	/* Before we start the connection: accept information aboit TTY and configure it */

	/* Should receive termios setting from the client*/

	rc = recv(tunnel->fd[TUN_LEFT], &sz, sizeof(struct winsize), 0);
	rc += recv(tunnel->fd[TUN_LEFT], &tio, sizeof(tio), 0);

	if (sizeof(struct winsize) + sizeof(tio) == rc) {
		pid = forkpty(&tunnel->fd[TUN_RIGHT], NULL, &tio, &sz);
	} else {     /* Can't receive winsize setting from the client */
		pid = forkpty(&tunnel->fd[TUN_RIGHT], NULL, &tparams, NULL);
		tcgetattr(tunnel->fd[TUN_RIGHT], &tparams);
		tparams.c_lflag |= EXTPROC;
		tparams.c_lflag &= ~ECHO;
		//tparams.c_cc[VEOF] = 3; // ^C
		//tparams.c_cc[VINTR] = 4; // ^D
		tcsetattr(tunnel->fd[TUN_RIGHT], TCSANOW, &tparams);
	}

	if (pid < 0) {
		DE("Can't fork");
		pthread_exit(NULL);
	}
	if (pid == 0) {
		char user[64]    = {0};
		char command[128] = {0};

		if (0 != getlogin_r(user, 64)) {
			DE("Can't get username\n\r");
			return (NULL);
		}
		/* Ttis is the terminal part; execute the bash */
		openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);

		printf("%s", WELCOME);
		signal(SIGUSR2, mp_tunnel_tty_handler);
		//(SIGINT, mp_tunnel_tty_handler);
		/* TODO: We may configure which shell to open and even receive it from the other side */
		execlp("/bin/bash", "/bin/bash", NULL);
		//execlp("/bin/bash", "--login", NULL);
		//sprintf(command, "/usr/bin/su - %s", user);
		//execlp("/bin/bash", "/usr/bin/su", "-", user,  NULL);
		pthread_exit(NULL);
	}

	mp_tunnel_tunnel_fill_external(tunnel, tunnel->fd[TUN_LEFT], "Server:Socket", conn_read_from_socket, conn_write_to_socket, NULL);
	mp_tunnel_tunnel_fill_internal(tunnel, tunnel->fd[TUN_RIGHT], "Server:PTY", conn_read_from_std, conn_write_to_std, NULL);

	/* Start SSL connection */
	mp_tunnel_init_server_ssl(tunnel, TUN_LEFT);
	rc = mp_tunnel_start_ssl_conn_left(tunnel);
	
	if (EOK != rc) {
		DE("Can't start SSL\n");
		return (NULL);
	}
	rc = SSL_accept(tunnel->ssl[TUN_LEFT] );
	if (1 != rc) {
		DE("SSL: Can't accept connection\n");
		return (NULL);
	}

	rc = mp_tunnel_run_x_conn(tunnel);

	DD("Returned from mp_tunnel_run_x_conn\n\r");

	/* No need to join thread - they were dettached */

	/* If it SSL connection, we should dhutdown it */
	if (tunnel->ssl[TUN_LEFT] ) {
		SSL_shutdown(tunnel->ssl[TUN_LEFT] );
	}

	mp_tunnel_print_stat(tunnel);
	//mp_tunnel_tunnel_t_destroy(tunnel);
	close(tunnel->fd[TUN_LEFT]);
	close(tunnel->fd[TUN_RIGHT]);
	DD("Close file descriptors\n\r");
	mp_tunnel_kill_pty(pid);
	DD("Finishing thread\n\r");
	pthread_exit(NULL);
}

void *mp_tunnel_tty_server_start_thread(void *v)
{
	/* Socket initialization */
	tunnel_t           *t        = v;
	int                sock;
	int                one       = 1;
	struct sockaddr_in serv_addr;

	//signal(SIGCHLD, SIG_IGN);

	/* TODO: check here the mode, the ssl, the fd */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(t->left_port);
	serv_addr.sin_addr.s_addr = inet_addr(t->left_server);

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

		t->fd[TUN_LEFT] = accept(sock, (struct sockaddr *)&serv_addr, &addrlen);
		pthread_create(&p_go, NULL, mp_tunnel_tty_server_go, t);
	}
}

void *mp_tunnel_tty_server_start_thread_(void *v)
{
	/* Socket initialization */
	tunnel_t           *t        = v;
	int                sock;
	int                one       = 1;
	struct sockaddr_in serv_addr;

	//signal(SIGCHLD, SIG_IGN);

	/* TODO: check here the mode, the ssl, the fd */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(t->left_port);
	serv_addr.sin_addr.s_addr = inet_addr(t->left_server);

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
		int       client;

		/* TODO: This number should not be hardcoded */
		if (listen(sock, 16)) {
			perror("listen()");
			pthread_exit(NULL);
		}

		socklen_t addrlen = sizeof(struct sockaddr);

		/* TODO: Here we may reject suspicious connection.
			The remote machine may declare preliminary about incoming connection.
			If we do not expect connection from this machine, we reject */

		client = accept(sock, (struct sockaddr *)&serv_addr, &addrlen);
		pthread_create(&p_go, NULL, mp_tunnel_tty_server_go, &client);
	}
}

void *mp_tunnel_tty_client_start_thread(void *v)
{
	/* Socket initialization */
	int                sock;
	struct sockaddr_in serv_addr;
	int                rc;
	struct winsize     sz;
	struct termios     tio;
	tunnel_t           *tunnel   = v;

	/* Detach this thread */

	if (0 != pthread_detach(pthread_self())) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	//DD("Starting client\n");

	/* Open the socket */

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(tunnel->left_port);
	serv_addr.sin_addr.s_addr = inet_addr(tunnel->left_server);

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		perror("socket");
		pthread_exit(NULL);
	}

	if (INADDR_NONE == serv_addr.sin_addr.s_addr) {
		DE("Can't create inet addr\n");
		perror("inet_addr");
		return (NULL);
	}

	/* Connect the socket */

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

	//DD("Sent window size: col = %d, row = %d\n", sz.ws_col, sz.ws_row);

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

	mp_tunnel_tunnel_fill_external(tunnel, sock, "Client:Socket", conn_read_from_socket, conn_write_to_socket, NULL);
	mp_tunnel_tunnel_fill_internal(tunnel, STDIN_FILENO, "Client:STDIN", conn_read_from_std, conn_write_to_std, NULL);

	/* Set SSL tunnel for external connection */
	mp_tunnel_init_client_ssl(tunnel, TUN_LEFT);

	rc = mp_tunnel_start_ssl_conn_left(tunnel);
	if (EOK != rc) {
		DE("Can't init SSL\n");
		return (NULL);
	}

	rc = SSL_connect(tunnel->ssl[TUN_LEFT] );
	if (1 != rc) {
		DE("SSL: Can't handshake with server\n");
		return (NULL);
	}

	rc = mp_tunnel_run_x_conn(tunnel);

	/* Set saved terminal flags */
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);
	close(sock);
	DD("Remote connection closed\n\r");
	mp_tunnel_print_stat(tunnel);

	if (tunnel->ssl[TUN_LEFT] ) {
		SSL_shutdown(tunnel->ssl[TUN_LEFT] );
	}

	pthread_exit(NULL);
}

#ifdef STANDALONE
int main(int argi, char *argv[])
{
	tunnel_t *tunnel = mp_tunnel_tunnel_t_alloc();

	if (argi < 2) {
		printf("Usage:%s -c for client, -s for server\n", argv[0] );
	}

	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DL_PREFIX, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_SYSLOG);

	/* Run as a "server" applience */
	if (0 == strcmp(argv[1] , "-s")) {
		pthread_t pid;

		/* In case we run as "server" (-s) flag: configure server tunnel */
		/* LEFT (external) part of the tunnel */
		/* Tunnel left (external) side is SSL server mode socket */

		mp_tun_set_flags(tunnel, TUN_LEFT, TUN_SOCKET_SSL | TUN_SERVER | TUN_AUTOBUF);
		mp_tun_set_server_port_left(tunnel, "127.0.0.1", 3318);

		/* RIGHT (internal) size of the tunnel: it is TTY */
		mp_tun_set_flags(tunnel, TUN_RIGHT, TUN_TTY_SERVER | TUN_AUTOBUF);

		pthread_create(&pid, NULL, mp_tunnel_tty_server_start_thread, tunnel);
		pthread_join(pid, NULL);
		DD("Finished\n\r");
	}

	/* Run as a "client" applience */
	if (0 == strcmp(argv[1] , "-c")) {
		pthread_t pid;

		/* LEFT side og the tunnel: SSL client connection */
		mp_tun_set_flags(tunnel, TUN_LEFT, TUN_SOCKET_SSL | TUN_AUTOBUF);
		mp_tun_set_server_port_left(tunnel, "127.0.0.1", 3318);
		/* RIGHT (internal) size of the tunnel: it is TTY */
		mp_tun_set_flags(tunnel, TUN_RIGHT, TUN_TTY_CLIENT | TUN_AUTOBUF);

		pthread_create(&pid, NULL, mp_tunnel_tty_client_start_thread, tunnel);
		pthread_join(pid, NULL);
		DD("Finished\n");
	}

	mp_tunnel_tunnel_t_destroy(tunnel);
	tunnel = NULL;
	return (0);
}
#endif /* STANDALONE */
