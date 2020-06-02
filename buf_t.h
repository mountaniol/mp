#ifndef _BUF_T_H_
#define _BUF_T_H_

/* For uint32_t / uint8_t */
//#include <linux/types.h>
/*@-skipposixheaders@*/
#include <stdint.h>
/*@=skipposixheaders@*/
/* For err_t */
#include "mp-common.h"

/* Simple struct to hold a buffer / string and its size / lenght */
struct buf_t_struct
{
	uint32_t room;      /* Allocated size */
	uint32_t used;       /* Used size */
	uint8_t tp;        /* Buffer type. Optional. We may use it as we wish. */
	/*@temp@*/char *data;       /* Pointer to data */
};

typedef struct buf_t_struct buf_t;

/* Size of a regular buf_t structure */
#define BUF_T_STRUCT_SIZE (sizeof(buf_t))

/* Size of buf_t structure for network transmittion: without the last 'char *data' pointer */
#define BUF_T_STRUCT_NET_SIZE (sizeof(buf_t) - sizeof(char*))

/* How much bytes will be transmitted to send buf_t + its actual data */
#define BUF_T_NET_SEND_SIZE(b) (BUF_T_STRUCT_NET_SIZE + b->used)

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func err_t buf_set_data(buf_t *buf, char *data, size_t size, size_t len)
 * @brief Set new data into buffer. The buf_t *buf must be clean, i.e. buf->data == NULL and
 *  buf->room == 0; After the new buffer 'data' set, the buf->len also set to 'len'
 * @param buf_t  * buf 'buf_t' buffer to set new data 'data'
 * @param char   * data Data to set into the buf_t
 * @param size_t size Size of the new 'data'
 * @param size_t len Length of data in the buffer, user must provide it
 * @return err_t EOK on success, EBAD on failure
 * @details
 *
 */
err_t buf_set_data(/*@null@*/buf_t *buf, /*@null@*/char *data, size_t size, size_t len);

/**
 *
 * @func buf_t* buf_t_alloc(char *data, size_t size)
 * @brief Allocate buf_t. If 'data' is not null, the buf->data
 *    will be set to 'data'. If 'data' is NULL but 'size' >
 *    0, a new buffer of 'size' will be allocated. If 'data'
 *    != NULL the 'size' must be > 0
 *
 * @author se (03/04/2020)
 *
 * @param char   * data User's data, can be NULL.
 * @param size_t size Data buffer size, may be 0
 *
 * @return buf_t* New buf_t structure.
 */
/*@null@*/ buf_t *buf_new(/*@null@*/char *data, size_t size);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func void* buf_steal_data(buf_t *buf)
 * @brief 'Steal' data from buffer. After this operation the internal buffer 'data' returned to
 * 	 caller. After this function buf->data set to NULL, buf->len = 0, buf->size = 0
 *
 * @param buf_t * buf Buffer to extract data buffer
 *
 * @return void* Data buffer pointer on success, NULL on error. Warning: if the but_t did not have a
 *  	   buffer (i.e. buf->data was NULL) the NULL will be returned.
 * @details
 *
 */
/*@null@*/void *buf_steal_data(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func void* buf_2_data(buf_t *buf)
 * @brief Return data buffer from the buffer and release the buffer. After this operation the buf_t
 *  	  structure will be completly destroyed. WARNING: disregarding to the return value the buf_t
 *  	  will be destoyed!
 *
 * @param buf_t * buf Buffer to extract data
 *
 * @return void* Pointer to buffer on success (buf if the buffer is empty NULL will be returned),
 *  	   NULL on error (and the 'buf' destroyed).
 * @details
 *
 */
/*@null@*/void *buf_2_data(/*@null@*/buf_t *buf);

/**
 * @brief Remove data from buffer, set 0 buf->room and buf->len
 * @func err_t buf_free_room(buf_t *buf)
 * @author se (16/05/2020)
 *
 * @param buf Buffer to remove data in
 *
 * @return err_t EOK if all right, EBAD on error
 */
err_t buf_free_room(/*@only@*//*@null@*/buf_t *buf);

/**
 *
 * @func int buf_room(buf_t *buf, size_t size)
 * @brief Allocate additional 'size' in the tail of buf_t data
 *    buffer; existing content kept unchanged. The new
 *    memory will be cleaned. The 'size' argument must be >
 *    0. For removing buf->data use 'buf_free_force()'
 *
 * @author se (06/04/2020)
 *
 * @param buf_t  * buf Buffer to grow
 * @param size_t size How many byte to add
 *
 * @return int
 */
extern err_t buf_add_room(/*@null@*/buf_t *buf, size_t size);

/**
 *
 * @func int buf_test_room(buf_t *buf, size_t expect)
 * @brief The function accept size in bytes that caller wants to
 *    add into buf. It check if additional room needed. If
 *    yes, calls buf_room() to increase room. The 'expect'
 *    ca be == 0
 *
 * @author se (06/04/2020)
 *
 * @param buf_t  * buf Buffer to test
 * @param size_t expect How many bytes will be added
 *
 * @return int EOK on success, EBAD on failure
 */
extern err_t buf_test_room(/*@null@*/buf_t *buf, size_t expect);

/**
 *
 * @func int buf_t_free_force(buf_t *buf)
 * @brief Free buf; if buf->data is not empty, free buf->data
 *
 * @author se (03/04/2020)
 *
 * @param buf_t * buf Buffer to remove
 *
 * @return int EOK on success, EBAD on failure
 */
extern err_t buf_free(/*@only@*//*@null@*/buf_t *buf);

/**
 *
 * @func int buf_add(buf_t *b, const char *buf, const size_t size)
 * @brief Add buffer "buf" of size "size" to the tail of buf_t.
 *    The buf_t memory added if it needed.
 *
 * @author se (06/04/2020)
 *
 * @param buf_t * b
 * @param const char* buf
 * @param const size_t size
 *
 * @return int
 */
err_t buf_add(/*@null@*/buf_t *buf, /*@null@*/const char *new_data, const size_t size);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func err_t buf_add_null(buf_t *b)
 * @brief Add '\0' terminator to the buffer; for easier string manupulations
 *
 * @param buf_t * b Buffer containing data
 *
 * @return err_t EOK on success, EBAD on error
 * @details
 *
 */
err_t buf_add_null(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func err_t buf_pack(buf_t *buf)
 * @brief Shrink buf->data to buf->len. We may use this function when we finished with the buf_t and
 *  	  its size won't change. We release unused memory with this function.
 *
 * @param buf_t * buf Buffer to pack
 *
 * @return err_t EOK on success, EBAD on a failure. If EBAD returned the buf->data is untouched, we
 *  	   may use it.
 * @details
 *
 */
err_t buf_pack(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (02/06/2020)
 * @func buf_t* buf_sprintf(char *format, ...)
 * @brief sprintf into buf_t
 *
 * @param char * format Format (like in printf first argument )
 *
 * @return buf_t*
 * @details
 *
 */
buf_t *buf_sprintf(char *format, ...);
#endif /* _BUF_T_H_ */
