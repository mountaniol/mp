#ifndef MP_TUNNEL_H
#define MP_TUNNEL_H

#include <semaphore.h>

/* These two function pointer are abstraction of read / write poerations */
typedef int (*conn_read_t) (int, char *, size_t);
typedef int (*conn_write_t) (int, char *, size_t);
typedef int (*conn_close_t) (int);

/* This is a connection abstraction.
   The connection defined as a file descriptor and 3 operation - read, write and (optional) close.
   The connection may have a name (optional) - for debug prints */
typedef struct connection_struct {
	int fd;                     /* (Must) File descriptor for read / write */
	conn_read_t read_fd;        /* (Must) Read from fd */
	conn_write_t write_fd;      /* (Must) Write to fd */
	conn_close_t close_fd;      /* (Optional) Close fd */
	const char *name;           /* (Optional) Name of the connection (optional) */
} conn_t;

/* 
 * Tunnel structure used to connect two file descriptors and
 * transfer data between them.
 * 
 * 1. LEFT and RIGHT fds
 * We connect two file descriptors. ONe we call 'left,,
 * another 'right'. It it up to us how to interpter these
 * terms.
 * 
 * For example, if the tunnel works as a bridge:
 * [ socket ] <--> tunnel <--> [ socket ]
 * 
 * It this structure we defined 'int left_fd' and
 * 'int right_fd'. Also there set of operations for 'left'
 * and 'right' descriptors.
 * 
 * 2. EXTERNAL and INTERNAL fds
 * In case we connect a socket with an internal file
 * descriptor (for example, socket to PTY), we call 'left'
 * socket 'external', and the 'right' socket 'internal':
 * 
 * [ socket ] <--> tunnel <--> [ tty / file / whatever ]
 * 
 * 3. SSL socket
 * 
 * The SSL socket file descriptor isn't a standard 'int fd'
 * file descriptor. It works on top of 'int fd' but it also
 * use 'SSL *' file descriptor on top of it.
 * 
 * For this case we have additional 'void *left_ssl' and
 * 'void *right_ssl'.
 * 
 * The logic of these tow is simple: if we have
 * 'left_ssl != NULL' - we use this one, else we use
 * 'int left_fd'.
 * 
 * 4. Tunnel buffer
 * In the tunnel structure we keep a pointer to allocated
 * buffer and to the buffer size. This buffer used for
 * transfer data from left to right and from right to left.
 * We can resize this buffer depending on the read/right
 * sizes.
 * 
 * 5. Lock
 * The 'lock' semaphore used to prevent race conditions.
 * Right now (28/05/2020) we do not have any code where it is
 * really the case, however we may have such a code in the
 * future.
 * 
 * So: every action of read / write / tunnel structure
 * modification (except statistics updates / reads) must be
 * protected.
 * 
 * 6. Statistics and calibration
 * We use statistics to calibrate size of the tunnel buffer.
 * See functions:
 * - mp_tunnel_should_resize()
 * - mp_tunnel_resize()
 * 
 */

typedef struct tunnel_struct {

	/*** TUNNEL MAIN */

	char *buf;                      /* Buffer used for read / write */
	size_t buf_size;                /* Size of the buffer */

	void *info;                     /* JSON object desctibing this tunnel */
	sem_t lock;                     /* Lock */

	/*** LEFT (EXTERNAL) FD */

	int left_fd;                    /* (Must) File descriptor for read / write */
	/* Left socket operations */
	conn_read_t left_read;          /* (Must) Read from fd */
	conn_write_t left_write;        /* (Must) Write to fd */
	conn_close_t left_close;        /* (Optional) Close fd */
	const char *left_name;          /* Name of the left fd - for debug */

	/*** LEFT (EXTERNAL) SSL RELATED */

	void *left_ssl;                 /* (Optional) In case SSL used this won't be NULL; */
	void *left_ctx;                 /* (Optional) SSL_CTX structure for left conection */

	/*** RIGHT (INTERNAL) FD */

	int right_fd;                   /* (Must) File descriptor for read / write */
	/* Right socket operations */
	conn_read_t right_read;         /* (Must) Read from fd */
	conn_write_t right_write;       /* (Must) Write to fd */
	conn_close_t right_close;       /* (Optional) Close fd */
	const char *right_name;         /* Name of the right fd - for debug */

	/*** RIGH (EXTERNAL) SSL RELATED */

	void *right_ssl;                /* (Optional) In case SSL used this won't be NULL; */
	void *right_ctx;                /* (Optional) SSL_CTX structure for left conection */

	/*** Staticstics */

	/*** LEFT (EXTERNAL) STATS */

	size_t left_cnt_write_total;            /* How many bytes passed to left fd in total */
	size_t left_num_writes;                 /* How many write operation done to the left */

	size_t left_cnt_session_write_total;    /* How many bytes passed to left fd after last buffer resize */
	size_t left_num_session_writes;         /* How many write operation done after last buffer resize */


	/*** RIGHT (INTERNAL) STATS */

	size_t right_cnt_write_total;           /* How many bytes passed to left fd in total */
	size_t right_num_writes;                /* How many write operation done to the right */

	size_t right_cnt_session_write_total;   /* How many bytes passed to left fd after last buffer resize */
	size_t right_num_session_writes;        /* How many write operation done to the right after last buffer resize */

	/*** COMMON (LEFT + RIGHT) STATS */

	size_t all_cnt_session_max_hits;        /* How many times the max size buffer used after last buffer resize */
} tunnel_t;

/* Allocate new tunnel structure, init semaphone */
static tunnel_t *mp_tunnel_tunnel_t_alloc(void);
static int mp_tunnel_tunnel_t_init(tunnel_t *tunnel);
/*
 * Close both descriptors, destroy semaphone, free memory
 * Attention: if there is no 'close' operation defined, the file descriptor must be closed and be < 0,
 * else this function makes nothing and returns error .
 */
static void mp_tunnel_tunnel_t_destroy(tunnel_t *tunnel);
static void mp_tunnel_lock(tunnel_t *tunnel);
static void mp_tunnel_unlock(tunnel_t *tunnel);

static int mp_tunnel_tunnel_fill_left(tunnel_t *tunnel, int left_fd, const char *left_name, conn_read_t left_read, conn_write_t left_write, conn_close_t left_close);
static int mp_tunnel_tunnel_fill_right(tunnel_t *tunnel, int right_fd, const char *right_name, conn_read_t right_read, conn_write_t right_write, conn_close_t right_close);

/* These are aliases in case the tunnel used for external-to-internal file descriptors */
#define mp_tunnel_tunnel_fill_external(x1,x2,x3,x4,x5,x6) mp_tunnel_tunnel_fill_left(x1,x2,x3,x4,x5,x6)
#define mp_tunnel_tunnel_fill_internal(x1,x2,x3,x4,x5,x6) mp_tunnel_tunnel_fill_right(x1,x2,x3,x4,x5,x6)

typedef struct conn2_struct {
	conn_t conn_in;
	conn_t conn_out;
	int status;
} conn2_t;

#endif /* MP_TUNNEL_H */
