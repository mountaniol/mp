#ifndef _BUF_T_H_
#define _BUF_T_H_

/* For uint32_t / uint8_t */
//#include <linux/types.h>
#include <stdint.h>
/* For err_t */
#include "mp-common.h"

/* Simple struct to hold a buffer / string and its size / lenght */
struct buf_t_struct {
	uint32_t room;      /* Allocated size */
	uint32_t len;       /* Used size */
	uint8_t tp;        /* Buffer type. Optional. We may use it as we wish. */
	/*@temp@*/char *data;       /* Pointer to data */
};

typedef struct buf_t_struct buf_t;

/* Size of a regular buf_t structure */
#define BUF_T_STRUCT_SIZE (sizeof(buf_t))

/* Size of buf_t structure for network transmittion: without the last 'char *data' pointer */
#define BUF_T_STRUCT_NET_SIZE (sizeof(buf_t) - sizeof(char*))

/* How much bytes will be transmitted to send buf_t + its actual data */
#define BUF_T_NET_SEND_SIZE(b) (BUF_T_STRUCT_NET_SIZE + b->len)

/**
 * 
 * @func buf_t* buf_t_alloc(char *data, size_t size)
 * @brief Allocate buf_t. If 'data' is not null, the buf->data 
 *  	  will be set to 'data'. If 'data' is NULL but 'size' >
 *  	  0, a new buffer of 'size' will be allocated. If 'data'
 *  	  != NULL the 'size' must be > 0
 * 
 * @author se (03/04/2020)
 * 
 * @param char* data User's data, can be NULL.
 * @param size_t size Data buffer size, may be 0
 * 
 * @return buf_t* New buf_t structure.
 */
/*@null@*/ buf_t *buf_new(/*@temp@*//*@null@*/char *data, size_t size);

/**
 * @func err_t buf_set_type(buf_t *buf, buf_type_t tp)
 * @brief Set buffer type. Buffer type is optional. The only 
 *  	  spetiall case for now is BUF_T_NETWORK which reserves
 *  	  room fo the 'buf_t' structure in the buffer head
 * @author se (19/05/2020)
 * 
 * @param buf 
 * @param tp 
 * 
 * @return err_t 
 */
err_t buf_set_type(buf_t *buf, uint8_t tp);
/**
 * @brief Remove data from buffer, set 0 buf->room and buf->len 
 * @func err_t buf_free_room(buf_t *buf)
 * @author se (16/05/2020)
 * 
 * @param buf Buffer to remove data in
 * 
 * @return err_t EOK if all right, EBAD on error
 */
err_t buf_free_room(/*@only@*/buf_t *buf);

/**
 * 
 * @func int buf_room(buf_t *buf, size_t size)
 * @brief Allocate additional 'size' in the tail of buf_t data 
 *  	  buffer; existing content kept unchanged. The new
 *  	  memory will be cleaned. The 'size' argument must be >
 *  	  0. For removing buf->data use 'buf_free_force()'
 * 
 * @author se (06/04/2020)
 * 
 * @param buf_t* buf Buffer to grow
 * @param size_t size How many byte to add
 * 
 * @return int 
 */
extern err_t buf_add_room(buf_t *buf, size_t size);

/**
 * 
 * @func int buf_test_room(buf_t *buf, size_t expect)
 * @brief The function accept size in bytes that caller wants to
 *  	  add into buf. It check if additional room needed. If
 *  	  yes, calls buf_room() to increase room. The 'expect'
 *  	  ca be == 0
 * 
 * @author se (06/04/2020)
 * 
 * @param buf_t* buf Buffer to test
 * @param size_t expect How many bytes will be added
 * 
 * @return int EOK on success, EBAD on failure
 */
extern err_t buf_test_room(buf_t *buf, size_t expect);

/**
 * @func int buf_get_room(buf_t *buf)
 * @brief Return available room in the buffer
 * @author se (16/05/2020)
 * 
 * @param buf_t *buf Buffer to test
 * 
 * @return int Available room in the buffer
 */
int buf_get_room(buf_t *buf);

/**
 * 
 * @func int buf_t_free_force(buf_t *buf)
 * @brief Free buf; if buf->data is not empty, free buf->data
 * 
 * @author se (03/04/2020)
 * 
 * @param buf_t* buf Buffer to remove
 * 
 * @return int EOK on success, EBAD on failure
 */
extern err_t buf_free(/*@only@*/buf_t *buf);

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

/**
 * @brief Serialization: Prepare buffer for sending through 
 *  	  socket: add buf_t structure to the end of the
 *  	  buf->data. On the other side of the socket we will cut
 *  	  if off
 * @func err_t buf_network_prepare(buf_t *buf)
 * @author se (19/05/2020)
 * 
 * @param buf_t *buf Buffer to prepare (serialize) 
 * 
 * @return err_t OEK on success, EBAD on an error
 */
err_t buf_network_prepare(buf_t *buf);

/**
 * @brief Deserialization: Restore buf_t from received data 
 *  	  buffer. Used to restore buf_t after it received from
 *  	  socket.
 * @func buf_t* buf_network_restore(char *data, size_t size)
 * @author se (19/05/2020)
 * 
 * @param char *data Data recived from socket 
 * @param size Size of data received from socket
 * 
 * @return buf_t* Restored buf_t on success; NULL on failure
 */
/*@null@*/ buf_t *buf_network_restore(char *data, size_t size);
#endif /* _BUF_T_H_ */
