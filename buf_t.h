#ifndef _BUF_T_H_
#define _BUF_T_H_

#define BUF_DEBUG
#define BUF_NOISY

/*
 * buf_t is an implementation of abstract buffer.
 * This buffer keeps data and its size.
 * buf->data - data.
 * buf->room - allocated memory
 * buf->used - size of used memory
 * 
 * A set of function help to manipulate the buffer: allocate / delete / add data and more.
 * Here is several examples:
 * 
 * ==== Example 1 ====
 * // Allocate buffer
 * buf_t *buf = buf_new(NULL, 0);
 * 
 * // Add data to the buffer
 * buf_add(buf, "Aloha", 5);
 * buf_add(buf, "/", 1);
 * buf_add(buf, "Shalom", 6);
 * 
 * // Now buffer contains string "Aloha/Shalom" without null terminator.
 * // The buf->room == 12
 * // The buf->used == 12
 * 
 * // Destroy buffer
 * buf_free(buf);
 * 
 * // The buffer and its memory securely destroyed: all data filled with 0 before it releaased.
 * // The 'buf' structure as well filled with '0' before destroyed.
 * 
 * ==== Example 2 ====
 * 
 * // Allocate buffer with memory == 32 bytes
 * buf_t = buf_new(NULL, 32);
 * // buf->data is buffer 32 bytes filled with '0'
 * 
 * // Add data
 * buf_add(buf, "Aloha", 5);
 * // buf->data contains "Aloha"
 * // buf->used == 5
 * // buf->room == 32
 * 
 * // Shrink memory to size of used area
 * buf_pack(buf)
 * // buf->data contains "Aloha"
 * // buf->used == 5
 * // buf->room == 5
 * 
 * buf_free(buf);
 * 
 * ==== Example 3 ====
 * 
 * // print string into buf_t
 * buf_t *buf = buf_sprintf("%s %s %s", "Lemon", "is", "yellow"); // buf_t allocated; // string
 * length measured and buf->data allocated, including terminating '\0'
 * 
 * 
 * // Now buf->data contains "Lemon is yellow\0"
 * // buf->used == 16
 * // buf->room == 16
 * // The used length contains terminating \0
 * 
 * // Print this string
 * printf("buf->data == %s; buf->used = %u\n", buf->data, buf->used);
 * 
 * // Release buffer
 * buf_free(buf);
 * 
 * ==== Example 4 ====
 * 
 * // print string into buf_t
 * buf_t *buf = buf_sprintf("%s %s %s", "Lemon", "is", "yellow"); // buf_t allocated; // string
 * length measured and buf->data allocated, including terminating '\0'
 * 
 * 
 * // Now buf->data contains "Lemon is yellow\0"
 * // buf->used == 16
 * // buf->room == 16
 * // The used length contains terminating \0
 * 
 * // Steal string from buffer
 * char *str = buf_steal_data(buf);
 * // Now str == "Lemon is yellow\0"
 * // buf->used == 0
 * // buf->room == 0
 * // buf->data == NULL
 * 
 * // Print this string
 * printf("string == %s\n", str);
 * 
 * // Release buffer
 * buf_free(buf);
 * 
 * // Release string
 * free(str);
 * 
 */


/* For uint32_t / uint8_t */
//#include <linux/types.h>
/*@-skipposixheaders@*/
#include <stdint.h>
/*@=skipposixheaders@*/
/* For err_t */
#include "mp-common.h"

typedef uint8_t buf_t_flags_t;

/* Simple struct to hold a buffer / string and its size / lenght */
struct buf_t_struct
{
	uint32_t room;              /* Allocated size */
	uint32_t used;              /* Used size */
	buf_t_flags_t flags;        /* Buffer flags. Optional. We may use it as we wish. */
	/*@temp@*/char *data;       /* Pointer to data */
	#ifdef BUF_DEBUG
	/* Where this buffer allocated: function */
	const char *func;
	/* Where this buffer allocated: file */
	const char *filename;
	/* Where this buffer allocated: line */
	int line;
	#endif
};

typedef struct buf_t_struct buf_t;

/* buf_t flags */

/* We may mark the buffer as string buffer. In this case, additional test enabled */
#define BUF_T_STRING      (1)

/* Buffer is read only; for example you may keep a static char * / const char * in buf_t */
#define BUF_T_READONLY     (1<<1)

/* Buffer is compressed */
#define BUF_T_COMPRESSED (1<<2)

/* Buffer is enctypted */
#define BUF_T_ENCRYPTED  (1<<3)

/* Buffer is enctypted */
#define BUF_T_CANARY  (1<<4)

/* Buffer is crc32 protected */
#define BUF_T_CRC  (1<<5)

#define IS_BUF_STRING(buf) (buf->flags & BUF_T_STRING)
#define IS_BUF_RO(buf) (buf->flags & BUF_T_READONLY)
#define IS_BUF_COMPRESSED(buf) (buf->flags & BUF_T_COMPRESSED)
#define IS_BUF_ENCRYPTED(buf) (buf->flags & BUF_T_ENCRYPTED)
#define IS_BUF_CANARY(buf) (buf->flags & BUF_T_CANARY)
#define IS_BUF_CRC(buf) (buf->flags & BUF_T_CRC)

#define SET_BUF_STRING(buf) (buf->flags |= BUF_T_STRING)
#define SET_BUF_RO(buf) (buf->flags |= BUF_T_READONLY)
#define SET_BUF_COMPRESSED(buf) (buf->flags |= BUF_T_COMPRESSED)
#define SET_BUF_ENCRYPTED(buf) (buf->flags |= BUF_T_ENCRYPTED)

/* CANARY: Set a mark after allocated buffer*/
/* PRO and CONTRA of this method:*/
/* PRO: It can help to catch memory problems */
/* Contras: The buffer increased, and buffer validation should be run on every buffer operation */
/* The mark we set at the end of the buf if PROTECTED flag is enabled */

/* Size of canary */
//typedef uint32_t buf_t_canary_t;


//typedef uint32_t buf_t_canary_t;
/* We use 2 characters as canary tail = 1 short */
//typedef uint16_t buf_t_canary_t;
typedef uint8_t buf_t_canary_t;

#define BUF_T_CANARY_SIZE (sizeof(buf_t_canary_t))

//#define BUF_T_CANARY_WORD ((buf_t_canary_t) 0xFEE1F4EE)
#define BUF_T_CANARY_WORD ((buf_t_canary_t) 0x31415926)

// The CANARY char pattern is : 10101010 = 0XAA
#define BUF_T_CANARY_CHAR_PATTERN 0XAA
#define BUF_T_CANARY_SHORT_PATTERN 0XAAAA

//0x12345678)

/* Size of a regular buf_t structure */
#define BUF_T_STRUCT_SIZE (sizeof(buf_t))

/* Size of buf_t structure for network transmittion: without the last 'char *data' pointer */
#define BUF_T_STRUCT_NET_SIZE (sizeof(buf_t) - sizeof(char*))

/* How much bytes will be transmitted to send buf_t + its actual data */
#define BUF_T_NET_SEND_SIZE(b) (BUF_T_STRUCT_NET_SIZE + b->used)


/**
 * @author Sebastian Mountaniol (16/06/2020)
 * @func void buf_set_abort(void)
 * @brief set "abort on errors" state
 *
 * @param void
 * @details
 *
 */
void buf_set_abort(void);


/**
 * @author Sebastian Mountaniol (16/06/2020)
 * @func void buf_unset_abort(void)
 * @brief Unset "abort on errors" state
 *
 * @param void
 * @details
 *
 */
void buf_unset_abort(void);

/**
 * @author Sebastian Mountaniol (14/06/2020)
 * @func void buf_default_flags(buf_t_flags_t f)
 * @brief Set default buf_t flags. Will be applied for every allocated new buf_t struct.
 *
 * @param buf_t_flags_t f
 * @details
 *
 */
void buf_default_flags(buf_t_flags_t f);

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
 * @author Sebastian Mountaniol (15/06/2020)
 * @func err_t buf_is_valid(buf_t *buf)
 * @brief Test bufer validity
 *
 * @param buf_t * buf
 *
 * @return err_t
 * @details
 *
 */
err_t buf_is_valid(buf_t *buf);

/**
 * @func buf_t* buf_new(size_t size)
 * @brief Allocate buf_t. A new  buffer of 'size' will be
 *    allocated.
 * @author se (03/04/2020)
 * @param size_t size Data buffer size, may be 0
 * @return buf_t* New buf_t structure.
 */

#ifdef BUF_DEBUG
/*@null@*/ buf_t *_buf_new(size_t size, const char *func, const char *file, int line);
#define buf_new(a) _buf_new(a, __func__, __FILE__, __LINE__)
#else
/*@null@*/ buf_t *buf_new(size_t size);
#endif

/**
 * @author Sebastian Mountaniol (16/06/2020)
 * @func buf_t* buf_string(size_t size)
 * @brief Allocate buffer and mark it as STRING 
 *
 * @param size_t size
 *
 * @return buf_t*
 * @details
 *
 */
#ifdef BUF_DEBUG
/*@null@*/ buf_t *_buf_string(size_t size, const char *func, const char *file, int line);
#define buf_string(a) _buf_string(a, __func__, __FILE__, __LINE__)
#else
/*@null@*/ buf_t *buf_string(size_t size);
#endif


/**
 * @author Sebastian Mountaniol (16/06/2020)
 * @func err_t buf_set_data_ro(buf_t *buf, char *data, size_t size)
 * @brief Set a data into buffer and lock the buffer, i.e. turn the buf into "read-only".
 *
 * @param buf_t  * buf
 * @param char   * data
 * @param size_t size
 *
 * @return err_t
 * @details
 *
 */
err_t buf_set_data_ro(buf_t *buf, char *data, size_t size);
/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func void* buf_steal_data(buf_t *buf)
 * @brief 'Steal' data from buffer. After this operation the internal buffer 'data' returned to
 *    caller. After this function buf->data set to NULL, buf->len = 0, buf->size = 0
 *
 * @param buf_t * buf Buffer to extract data buffer
 *
 * @return void* Data buffer pointer on success, NULL on error. Warning: if the but_t did not have a
 * 			 buffer (i.e. buf->data was NULL) the NULL will be returned.
 * @details
 *
 */
/*@null@*/void *buf_steal_data(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func void* buf_2_data(buf_t *buf)
 * @brief Return data buffer from the buffer and release the buffer. After this operation the buf_t
 *    structure will be completly destroyed. WARNING: disregarding to the return value the buf_t
 *    will be destoyed!
 *
 * @param buf_t * buf Buffer to extract data
 *
 * @return void* Pointer to buffer on success (buf if the buffer is empty NULL will be returned),
 * 			 NULL on error (and the 'buf' destroyed).
 * @details
 *
 */
/*@null@*/void *buf_2_data(/*@null@*/buf_t *buf);

/**
 * @brief Remove data from buffer, set buf->room = buf->len = 0
 * @func err_t buf_clean(buf_t *buf)
 * @author se (16/05/2020)
 *
 * @param buf Buffer to remove data in
 *
 * @return err_t EOK if all right, EBAD on error
 */
err_t buf_clean(/*@only@*//*@null@*/buf_t *buf);

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

#if 0
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
#endif

/**
 * @author Sebastian Mountaniol (14/06/2020)
 * @func uint32_t buf_used(buf_t *buf)
 * @brief Return value of buf->used
 *
 * @param buf_t * buf
 *
 * @return uint32_t buf->used; (uint32_t) -1 on error
 * @details
 *
 */
uint32_t buf_used(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (14/06/2020)
 * @func uint32_t buf_room(buf_t *buf)
 * @brief Value of buf->room
 *
 * @param buf_t * buf
 *
 * @return uint32_t
 * @details of buf->room, (uint32_t) -1 on error
 *
 */
uint32_t buf_room(/*@null@*/buf_t *buf);

/**
 * @author Sebastian Mountaniol (01/06/2020)
 * @func err_t buf_pack(buf_t *buf)
 * @brief Shrink buf->data to buf->len. We may use this function when we finished with the buf_t and
 *    its size won't change. We release unused memory with this function.
 *
 * @param buf_t * buf Buffer to pack
 *
 * @return err_t EOK on success, EBAD on a failure. If EBAD returned the buf->data is untouched, we
 * 			 may use it.
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
buf_t *buf_sprintf(const char *format, ...);


/* Additional defines */
#ifdef BUF_DEBUG

#define BUF_TEST(buf) do {if (0 != buf_is_valid(buf)){fprintf(stderr, "######>>> Buffer invalid here: func: %s file: %s + %d [allocated here: %s +%d %s()]\n", __func__, __FILE__, __LINE__, buf->filename, buf->line, buf->func);}} while (0)
#define BUF_DUMP(buf) do {DD("[BUFDUMP]: [%s +%d] buf = %p, data = %p, room = %u, used = %u [allocated here: %s +%d %s()]\n", __func__, __LINE__, buf, buf->data, buf->room, buf->used, buf->filename, buf->line, buf->func);} while(0)
#define BUF_DUMP_ERR(buf) do {DD("[BUFDUMP]: [%s +%d] buf = %p, data = %p, room = %u, used = %u [allocated here: %s +%d %s()]\n", __func__, __LINE__, buf, buf->data, buf->room, buf->used, buf->filename, buf->line, buf->func);} while(0)

#else

#define BUF_TEST(buf) do {if (0 != buf_is_valid(buf)){fprintf(stderr, "######>>> Buffer test invalid here: func: %s file: %s + %d\n", __func__, __FILE__, __LINE__);}} while (0)
#define BUF_DUMP(buf) do {DD("[BUFDUMP]: [%s +%d] buf = %p, data = %p, room = %u, used = %u\n", __func__, __LINE__, buf, buf->data, buf->room, buf->used);} while(0)
#define BUF_DUMP_ERR(buf) do {DD("[BUFDUMP]: [%s +%d] buf = %p, data = %p, room = %u, used = %u\n", __func__, __LINE__, buf, buf->data, buf->room, buf->used);} while(0)
#endif

#ifndef BUF_NOISY
#undef BUF_DUMP
#define BUF_DUMP(buf) do{}while(0)
#endif

#endif /* _BUF_T_H_ */
