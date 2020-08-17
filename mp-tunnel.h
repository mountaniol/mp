#ifndef MP_TUNNEL_H
#define MP_TUNNEL_H

#include <semaphore.h>
#include <linux/types.h>

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

/* This pointer is a hook prototype used to register hook:
 * void * - the tunnel structure
 * char * - pointer to left or right buffer
 * size_t - current size of the buffer
 * size_t - how many bytes of data in the buffer now
 */
typedef int (*tunnel_hook_t) (void *, char *, size_t, size_t);

/* This defines in which mode works left / right part of tunnel */
/* Bits 0-7: configurations */
#define TUN_SERVER 		(1)		/* If set, the server part will be inited */
#define TUN_AUTOBUF		(1<<1)		/* If set, the tunnel will resize buffer size automatically */

/* Bits 8-15: tunnel mode */
#define TUN_FD 			(1<<8)	/* It is a file */
#define TUN_SOCKET_REG 	(1<<9)	/* It is a regular socket (no SSL) */
#define TUN_SOCKET_SSL 	(1<<10)	/* It is SSL socket */
#define TUN_SOCKET_UNIX	(1<<11)	/* It is UNIX socket */
#define TUN_TIPC 		(1<<12)	/* This is TIPC connection */
#define TUN_TTY_SERVER	(1<<13)	/* It is TTY */
#define TUN_TTY_CLIENT	(1<<14)	/* It is TTY */

typedef struct tunnel_args_struct {
	char *left_target;  /* In case of socket target is server name;
						 * in case of file it is file name
						 * In case of TIPC it is target name
						 */
	char *right_target;

	int left_port;      /* In case of socket / SSL socket port must be specified  (TIPC?) */
	int right_port;

	uint32_t left_flags;     /* Define what is the left part of tunnel  (see TUN_* flags) */
	uint32_t right_flags;    /* Define what is the right part of tunnel (see TUN_* flags) */

	/* If the left part of the tunnel is SSL: certificate + rsa for the SSL connection */
	void *right_rsa;    /* (Optional) In case SSL used this won't be NULL; */
	void *right_x509;   /* (Optional) In case SSL used this won't be NULL; */

	/* If the right part of the tunnel is SSL: certificate + rsa for the SSL connection */
	void *left_rsa;     /* (Optional) In case SSL used this won't be NULL; */
	void *left_x509;    /* (Optional) In case SSL used this won't be NULL; */

	/* Size of the buffer to use for the left part of the tunnel; 0 means "auto"  */
	size_t left_buf_size;

	/* Size of the buffer to use for the right part of the tunnel; 0 means "auto" */
	size_t right_buf_size;

	/* For internal usage - don't try to use it */
	void *priv;
} tunnel_args_t;

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

/* Define right (internal) and left (external) channels */
typedef enum tun_stream_enum {
	TUN_LEFT = 0,
	TUN_EXT = TUN_LEFT,
	TUN_RIGHT = 1,
	TUN_INTER = TUN_RIGHT,
	TUN_MAX = 2
} tun_stream_t;

typedef struct tunnel_struct {

	/*** TUNNEL MAIN */

	void *info;                     /* JSON object desctibing this tunnel */

	/*** Flags define the tunnel left size configuration, see TUN_* defines (like TUN_SERVER) */

	uint32_t flags[TUN_MAX];
	
	/*** LEFT (EXTERNAL) FD */

	/* Buffer: left buffer used to read from the left tunnel side and write to the right side */
	char *left_buf;                  /* Buffer used for read / write from left fd to right fd */
	size_t left_buf_size;            /* Size of the l2r (left to right)buffer */
	sem_t left_buf_lock;             /* The buffer lock */

	int left_fd;                     /* (Must) File descriptor for read / write */

	/* Left socket operations */
	conn_read_t left_read;          /* (Must) Read from fd */
	conn_write_t left_write;        /* (Must) Write to fd */
	conn_close_t left_close;        /* (Optional) Close fd */
	const char *left_name;          /* Name of the left fd - for debug */

	/*** SSL RELATED */

	void *ctx[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */
	void *rsa[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */
	void *x509[TUN_MAX];                /* (Optional) In case SSL used this won't be NULL; */
	void *ssl[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */

	/*** RIGHT (INTERNAL) FD */
	/* Buffer: right buffer used to read from the right side and to write to left size */
	char *right_buf;                  /* Buffer used for read / write from right fd to left fd */
	size_t right_buf_size;            /* Size of the r2l (right to left) buffer */
	sem_t right_buf_lock;                 /* The buffer Lock */

	int right_fd;                   /* (Must) File descriptor for read / write */

	/* Right socket operations */
	conn_read_t right_read;         /* (Must) Read from fd */
	conn_write_t right_write;       /* (Must) Write to fd */
	conn_close_t right_close;       /* (Optional) Close fd */
	/* TODO: replace it with buf_t */
	const char *right_name;         /* Name of the right fd - for debug */

	/*** Staticstics */

	/*** LEFT (EXTERNAL) STATS */

	size_t left_cnt_write_total;            /* How many bytes passed to left fd in total */
	size_t left_num_writes;                 /* How many write operation done to the left */
	size_t left_num_writes_ssl;                 /* How many write operation done to the left */

	size_t left_cnt_session_write_total;    /* How many bytes passed to left fd after last buffer resize */
	size_t left_num_session_writes;         /* How many write operation done after last buffer resize */
	size_t left_all_cnt_session_max_hits;         /* How many times the max size buf_r2l used after last buffer resize */


	/*** RIGHT (INTERNAL) STATS */

	size_t right_cnt_write_total;           /* How many bytes passed to left fd in total */
	size_t right_num_writes;                /* How many write operation done to the right */
	size_t right_num_writes_ssl;            /* How many write operation done to the right */

	size_t right_cnt_session_write_total;   /* How many bytes passed to left fd after last buffer resize */
	size_t right_num_session_writes;        /* How many write operation done to the right after last buffer resize */
	size_t right_all_cnt_session_max_hits;        /* How many times the max size buf_l2r used after last buffer resize */

	/*** Optional params **/
	/* If given the server + port, the tunnel will resolve it */
	
	char *left_server;
	int left_port;
	char *right_server;
	int right_port;

} tunnel_t;

/*** API */

/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func tunnel_t* mp_tunnel_tunnel_t_alloc(void)
 * @brief Allocate and init tunnel structure
 *
 * @param void
 *
 * @return tunnel_t* Allocated and inited tunnel struct on success, NULL on error
 * @details
 */
tunnel_t *mp_tunnel_tunnel_t_alloc(void);

/* Allocate new tunnel structure, init semaphone */
/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func int mp_tunnel_tunnel_t_alloc(tunnel_t *tunnel)
 * @brief Init tunnel structure
 *
 * @param tunnel_t * tunnel Structure to init
 *
 * @return int EOK on success, < 0 on error
 * @details You do not need it the stucture created
 * with mp_tunnel_tunnel_t_alloc() func. Only if you
 * allocated the structure manually you should init it
 * with this function
 */
int mp_tunnel_tunnel_t_init(tunnel_t *tunnel);
/*
 * Close both descriptors, destroy semaphone, free memory
 * Attention: if there is no 'close' operation defined, the file descriptor must be closed and be < 0,
 * else this function makes nothing and returns error .
 */

/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func void mp_tunnel_tunnel_t_destroy(tunnel_t *tunnel)
 * @brief Close all descriptors, destroy semaphores, free
 * the tunnel structure
 *
 * @param tunnel_t * tunnel Tunnel to destroy
 * @details Attention: if there is no 'close' operation
 * defined, the file descriptor must be closed and be < 0,
 * else this function makes nothing and returns an error.
 */
void mp_tunnel_tunnel_t_destroy(tunnel_t *tunnel);

#if 0
/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func int mp_tunnel_set_left_flags(tunnel_t *t, uint32_t flags)
 * @brief Set configuration flags of the tunnel left side
 * @param tunnel_t * t Tunnel to set flags
 * @param uint32_t flags Flags to set
 * @return int New set of flags returned. In case of error 0xFFFFFFFF value returned
 * @details The new det of flags completely replace the old set of flags. The error is possible is pointer to tunnel is NULL
 */
int mp_tun_set_flags_left(tunnel_t *t, uint32_t flags);


/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func int mp_tunnel_set_right_flags(tunnel_t *t, uint32_t flags)
 * @brief Set configuration flags of the tunnel right side
 *
 * @param tunnel_t * t Tunnel to set flags
 * @param uint32_t flags Flags to set
 *
 * @return int New set of flags returned. In case of error 0xFFFFFFFF value returned
 * @details The new set of flags completely replace the old set of flags. The error is possible is pointer to tunnel is NULL
 */
int mp_tun_set_flags_right(tunnel_t *t, uint32_t flags);
#endif

/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func uint32_t mp_tunnel_get_left_flags(tunnel_t *t)
 * @brief Return left flags
 *
 * @param tunnel_t * t Current set of flags returned. In case of error 0xFFFFFFFF value returned
 *
 * @return uint32_t Flags
 * @details The error is possible if pointer of the tunnel struct is NULL
 */
uint32_t mp_tun_get_flags_left(tunnel_t *t);

#if 0
/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func uint32_t mp_tunnel_get_right_flags(tunnel_t *t)
 * @brief Return left flags
 * @param tunnel_t * t Current set of flags returned. In case of error 0xFFFFFFFF value returned
 * @return uint32_t Flags
 * @details The error is possible if pointer of the tunnel struct is NULL
 */
uint32_t mp_tun_get_flags_right(tunnel_t *t);
#endif

static int mp_tunnel_tunnel_fill_left(tunnel_t *tunnel, int left_fd, const char *left_name, conn_read_t left_read, conn_write_t left_write, conn_close_t left_close);
static int mp_tunnel_tunnel_fill_right(tunnel_t *tunnel, int right_fd, const char *right_name, conn_read_t right_read, conn_write_t right_write, conn_close_t right_close);

/* These are aliases in case the tunnel used for external-to-internal file descriptors */
#define mp_tunnel_tunnel_fill_external(x1,x2,x3,x4,x5,x6) mp_tunnel_tunnel_fill_left(x1,x2,x3,x4,x5,x6)
#define mp_tunnel_tunnel_fill_internal(x1,x2,x3,x4,x5,x6) mp_tunnel_tunnel_fill_right(x1,x2,x3,x4,x5,x6)

/**
 * @author Sebastian Mountaniol (09/08/2020)
 * @func int mp_tunnel_set_certs(SSL_CTX *ctx, void *x509, void *priv_rsa)
 * @brief Set tunel certificate, RSA and CTX
 * @param SSL_CTX * ctx - Inited CTX object
 * @param void * x509 - X509 certufucate
 * @param void * priv_rsa - Private RSA key (pair of public RSA used in the X509 certificate)
 * @return err_t EOK on success, < 0 on error
 * @details
 */
int mp_tunnel_set_cert(SSL_CTX *ctx, void *x509, void *priv_rsa);

/* This function starts */
void *mp_tunnel_tty_server_start_thread(void *v);

typedef struct conn2_struct {
	conn_t conn_in;
	conn_t conn_out;
	int status;
} conn2_t;

#endif /* MP_TUNNEL_H */
