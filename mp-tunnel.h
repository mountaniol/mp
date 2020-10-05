#ifndef MP_TUNNEL_H
#define MP_TUNNEL_H

#include <semaphore.h>
#include <linux/types.h>

/* These two function pointer are abstraction of read / write poerations */
typedef int (*conn_read_t) (int, char *, size_t);
typedef int (*conn_write_t) (int, char *, size_t);
typedef int (*conn_close_t) (int);

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

typedef struct {

	/*** TUNNEL MAIN */

	void *info;                     /* JSON object desctibing this tunnel */

	/*** Flags define the tunnel left size configuration, see TUN_* defines (like TUN_SERVER) */

	uint32_t flags[TUN_MAX];

	/*** BUFFERS */
	char *buf[TUN_MAX];                  /* Buffer used for read / write from left fd to right fd */
	size_t buf_size[TUN_MAX];            /* Size of the l2r (left to right)buffer */
	sem_t buf_lock[TUN_MAX];             /* The buffer lock */

	/*** File descriptors */
	int fd[TUN_MAX];

	/*** FILE OPERATIONS */
	conn_read_t do_read[TUN_MAX];          /* (Must) Read from fd */
	conn_write_t do_write[TUN_MAX];     /* (Must) Write to fd */
	conn_close_t do_close[TUN_MAX];     /* (Optional) Close fd */


	/*** SSL RELATED */

	void *ctx[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */
	void *rsa[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */
	void *x509[TUN_MAX];                /* (Optional) In case SSL used this won't be NULL; */
	void *ssl[TUN_MAX];                 /* (Optional) In case SSL used this won't be NULL; */

	char *name[TUN_MAX];                /* Name of fd - for debug and statistics printing */

	/* If server + port are given, the tunnel will resolve it */

	char *server[TUN_MAX];
	int port[TUN_MAX];


	/*** Staticstics */

	size_t cnt_write_total[TUN_MAX];            /* How many bytes passed to left fd in total */
	size_t num_writes[TUN_MAX];              /* How many write operation done to the left */
	size_t num_writes_ssl[TUN_MAX];              /* How many write operation done to the left */

	size_t cnt_session_write_total[TUN_MAX]; /* How many bytes passed to left fd after last buffer resize */
	size_t num_session_writes[TUN_MAX];      /* How many write operation done after last buffer resize */
	size_t all_cnt_session_max_hits[TUN_MAX];      /* How many times the max size buf_r2l used after last buffer resize */
} tun_t;

/*** API */

/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func tun_t *mp_tun_t_alloc(void)
 * @brief Allocate and init tunnel structure
 * @param void
 * @return tunnel_t* Allocated and inited tunnel struct on success, NULL on error
 * @details
 */
tun_t *mp_tun_t_alloc(void);

/* Allocate new tunnel structure, init semaphone */
/**
 * @author Sebastian Mountaniol (11/08/2020)
 * @func int mp_tun_t_init(tunnel_t *tunnel)
 * @brief Init tunnel structure
 * @param tunnel_t * tunnel Structure to init
 * @return int EOK on success, < 0 on error
 * @details You do not need it the stucture created
 * with mp_tunnel_tunnel_t_alloc() func. Only if you
 * allocated the structure manually you should init it
 * with this function
 */

int mp_tun_t_init(tun_t *tunnel);
/*
 * Close both descriptors, destroy semaphone, free memory
 * Attention: if there is no 'close' operation defined, the file descriptor must be closed and be < 0,
 * else this function makes nothing and returns error .
 */

/**
 * @author Sebastian Mountaniol (18/08/2020)
 * @func void mp_tun_t_destroy(tun_t *tunnel)
 * @brief Close all descriptors, destroy semaphores, free
 * the tunnel structure
 * @param tun_t * tunnel Tunnel to destroy
 * @details Attention: if there is no 'close' operation
 * defined, the file descriptor must be closed and be < 0,
 * else this function makes nothing and returns an error.
 */
void mp_tun_t_destroy(tun_t *tunnel);

int mp_tun_set_flags(tun_t *t, int direction, uint32_t flags);
uint32_t mp_tun_get_flags(tun_t *t, int direction);
int mp_tun_set_buf_size(tun_t *t, int direction, size_t size);
int mp_tun_set_fd(tun_t *t, int direction, int fd);
int mp_tun_get_fd(tun_t *t, int direction);
int mp_tun_set_name(tun_t *t, int direction, const char *name, size_t name_len);
char *mp_tun_get_name(tun_t *t, int direction);
int mp_tun_set_x509_rsa(tun_t *t, int direction, void *x509, void *rsa);
int mp_tun_set_server_port(tun_t *t, int direction, const char *server, int port);
char *mp_tun_get_server(tun_t *t, int direction);
int mp_tun_get_port(tun_t *t, int direction);
int mp_tun_fill(tun_t *tunnel, int direction, int fd, const char *name, conn_read_t do_read, conn_write_t do_write, conn_close_t do_close);

/**
 * @author Sebastian Mountaniol (09/08/2020)
 * @func int mp_tun_set_cert(SSL_CTX *ctx, void *x509, void *priv_rsa)
 * @brief Set tunel certificate, RSA and CTX
 * @param SSL_CTX * ctx - Inited CTX object
 * @param void * x509 - X509 certufucate
 * @param void * priv_rsa - Private RSA key (pair of public RSA used in the X509 certificate)
 * @return err_t EOK on success, < 0 on error
 * @details
 */
int mp_tun_set_cert(SSL_CTX *ctx, void *x509, void *priv_rsa);

/*@null@*/ SSL_CTX *mp_tun_init_server_tls_ctx(tun_t *t, int direction);
/*@null@*/ SSL_CTX *mp_tun_init_client_tls_ctx(tun_t *t, int direction);

int mp_tun_ssl_set_ctx(tun_t *t, int direction, void *ctx);
int mp_tun_ssl_set_rsa(tun_t *t, int direction, void *rsa);
int mp_tun_ssl_set_x509(tun_t *t, int direction, void *x509);

void *mp_tun_tty_server_start_thread(void *v);
void *mp_tun_tty_client_start_thread(void *v);

#endif /* MP_TUNNEL_H */
