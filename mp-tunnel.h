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

typedef struct tunnel_struct {
	int fd_left;                    /* (Must) File descriptor for read / write */
	int fd_right;                   /* (Must) File descriptor for read / write */

	/* Left socket operations */
	conn_read_t left_read;          /* (Must) Read from fd */
	conn_write_t left_write;        /* (Must) Write to fd */
	conn_close_t left_close;        /* (Optional) Close fd */

	/* Right socket operations */
	conn_read_t right_read;         /* (Must) Read from fd */
	conn_write_t right_write;       /* (Must) Write to fd */
	conn_close_t right_close;       /* (Optional) Close fd */

	void *info;                     /* JSON object desctibing this tunnel */
	size_t left_cnt_total;          /* How many bytes passed to left fd */
	size_t right_cnt_total;         /* How many bytes passed to right fd */

	/* These two counter can be used to resize buffer */
	size_t right_cnt_max;           /* Max count bytes passed to right fd at once */
	size_t left_cnt_max;            /* Max count bytes passed to left fd at once */

	char *buf;                      /* Buffer used for read / write */
	size_t buf_size;                /* Size of the buffer */
	sem_t lock;                     /* Lock */

} tunnel_t;

typedef struct conn2_struct {
	conn_t conn_in;
	conn_t conn_out;
	int status;
} conn2_t;

#endif /* MP_TUNNEL_H */
