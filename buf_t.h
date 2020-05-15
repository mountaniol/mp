#ifndef _BUF_T_H_
#define _BUF_T_H_

/* For err_t */
#include "mp-common.h"

/* Simple struct to hold a buffer / string and its size / lenght */
struct buf_t_struct {
	char *data;		/* Pointer to data */
	size_t size;	/* Allocated size */
	size_t len;		/* Used size */
};

typedef struct buf_t_struct buf_t;

/**
 * 
 * @func buf_t* buf_t_alloc(char *data, size_t size)
 * @brief Allocate buf_t
 * 
 * @author se (03/04/2020)
 * 
 * @param char* data 
 * @param size_t size 
 * 
 * @return buf_t* 
 */
extern /*@null@*/ buf_t *buf_new(/*@temp@*/ char *data, size_t size);

/**
 * 
 * @func int buf_t_free(buf_t *buf)
 * @brief Free buf_t; Returns 0 on success, -1 if buf->data != 
 *  	  NULL
 * 
 * @author se (03/04/2020)
 * 
 * @param buf_t* buf 
 * 
 * @return int 
 */
extern err_t buf_free(buf_t *buf);

/**
 * 
 * @func int buf_room(buf_t *buf, size_t size)
 * @brief Allocate additional 'size' in the tail of buf_t; 
 *  	  content kept unchanged. The new memory will be
 *  	  cleaned.
 * 
 * @author se (06/04/2020)
 * 
 * @param buf_t* buf 
 * @param size_t size 
 * 
 * @return int 
 */
extern err_t buf_room(/*@temp@*/buf_t *buf, size_t size);

/**
 * 
 * @func int bud_test_room(buf_t *buf, size_t expect)
 * @brief Check if additional room needed. If yes, calls 
 *  	  buf_room()
 * 
 * @author se (06/04/2020)
 * 
 * @param buf_t* buf 
 * @param size_t expect 
 * 
 * @return int 
 */
extern err_t buf_test_room(buf_t *buf, size_t expect);

/**
 * 
 * @func int buf_t_free_force(buf_t *buf)
 * @brief Free buf; if not empty, free buf->data
 * 
 * @author se (03/04/2020)
 * 
 * @param buf_t* buf 
 * 
 * @return int 
 */
extern err_t buf_free_force(/*@only@*/buf_t *buf);

/**
 * 
 * @func int buf_add(buf_t *b, const char *buf, const size_t size)
 * @brief Add buffer "buf" of size "size" to the tail of buf_t. 
 *  	  The buf_t memory added if it needed.
 * 
 * @author se (06/04/2020)
 * 
 * @param buf_t* b 
 * @param const char* buf 
 * @param const size_t size 
 * 
 * @return int 
 */
extern err_t buf_add(buf_t *b, const char *buf, const size_t size);

#endif /* _BUF_T_H_ */
