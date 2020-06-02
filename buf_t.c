#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-limits.h"


/*@null@*/ buf_t *buf_new(/*@null@*/char *data, size_t size)
{
	/*@temp@*/buf_t *buf;

	if (NULL != data && 0 == size) {
		DE("The 'size' must be > 0 it 'data' given\n");
		return (NULL);
	}

	buf = zmalloc(sizeof(buf_t));
	if (NULL == buf) {
		return (NULL);
	}

	/* If buffer is passed to this function - use it*/
	if (NULL != data) {
		buf->data = data;
	} else {
		/* If no buffer passed, but size given - allocate new buffer */
		buf->data = zmalloc(size);
		TESTP_ASSERT(buf->data, "Can't allocate buf->data");
	}
	buf->room = size;
	buf->used = 0;

	return (buf);
}

err_t buf_set_data(/*@null@*/buf_t *buf, /*@null@*/char *data, size_t size, size_t len)
{
	TESTP(buf, EBAD);
	TESTP(data, EBAD);
	buf->data = data;
	buf->room = size;
	buf->used = len;
	return (EOK);
}

/*@null@*/void *buf_steal_data(/*@null@*/buf_t *buf)
{
	/*@temp@*/void *data;
	TESTP(buf, NULL);
	data = buf->data;
	buf->data = NULL;
	buf->room = 0;
	buf->used = 0;
	return (data);
}

/*@null@*/void *buf_2_data(/*@null@*/buf_t *buf)
{
	void *data;
	TESTP(buf, NULL);
	data = buf_steal_data(buf);
	buf_free(buf);
	return (data);
}

err_t buf_add_room(/*@null@*/buf_t *buf, size_t size)
{
	void *tmp;
	if (NULL == buf || 0 == size) {
		return (EBAD);
	}

	tmp = realloc(buf->data, buf->room + size);

	/* Case 1: realloc can't reallocate */
	if (NULL == tmp) {
		DE("Realloc failed\n");
		return (EBAD);
	}

	/* Case 2: realloc succidded, new memory returned */
	/* No need to clean the old memory - done by realloc */
	if (NULL != tmp) {
		buf->data = tmp;
	}

	/* Clean newely allocated memory */
	memset(buf->data + buf->room, 0, size);

	/* Case 3: realloc succidded, the same pointer - we do nothing */
	/* <Beeep> */

	buf->room += size;
	return (EOK);
}

err_t buf_test_room(/*@null@*/buf_t *buf, size_t expect)
{
	if (NULL == buf) {
		DE("Got NULL\n");
		return (EBAD);
	}

	if (buf->used + expect <= buf->room) {
		return (EOK);
	}

	return (buf_add_room(buf, expect));
}

err_t buf_free_room(/*@only@*//*@null@*/buf_t *buf)
{
	TESTP(buf, EBAD);
	if (buf->data) {
		/* Security: zero memory before it freed */
		memset(buf->data, 0, buf->room);
		free(buf->data);
	}
	buf->used = buf->room = 0;
	buf->tp = 0;
	return (EOK);
}

err_t buf_free(/*@only@*//*@null@*/buf_t *buf)
{
	TESTP(buf, EBAD);
	buf_free_room(buf);
	TFREE_SIZE(buf, sizeof(buf_t));
	return (EOK);
}

err_t buf_add(/*@null@*/buf_t *buf, /*@null@*/const char *new_data, const size_t size)
{
	if (NULL == buf || NULL == new_data || size < 1) {
		/*@ignore@*/
		DE("Wrong params: b = %p, buf = %p, size = %zu\n", buf, new_data, size);
		/*@end@*/
		return (EBAD);
	}

	if (0 != buf_test_room(buf, size)) {
		DE("Can't add room into buf_t\n");
		return (EBAD);
	}

	memcpy(buf->data + buf->used, new_data, size);
	buf->used += size;
	return (EOK);
}

err_t buf_add_null(/*@null@*/buf_t *buf)
{
	if (NULL == buf) {
		/*@ignore@*/
		DE("Wrong params: b = %p\n", buf);
		/*@end@*/
		return (EBAD);
	}

	if (0 != buf_test_room(buf, buf->used + 1)) {
		DE("Can't add room into buf_t\n");
		return (EBAD);
	}

	memset(buf->data + buf->used, '\0', 1);
	buf->used++;
	return (EOK);
}

err_t buf_pack(/*@null@*/buf_t *buf)
{
	TESTP(buf, EBAD);
	if (NULL == buf->data) {

		/* Sanity check */
		if (buf->used > 0 || buf->room > 0) {
			DE("WARNING! buf->data == NULL, buf room or len not: len = %u, room = %u\n",
			   buf->used, buf->room);
			buf->used = buf->room = 0;
		}
		return (EOK);
	}

	/* Sanity check */
	if (buf->used > buf->room) {
		DE("ERROR! buf->len (%u) > buf->room (%u)\n", buf->used, buf->room);
		return (EBAD);
	}

	/* Here we should dhring the buffer */
	if (buf->used < buf->room) {
		DD("Packing buf_t: %u -> %u\n", buf->room, buf->used);
		void *tmp = realloc(buf->data, buf->used);

		/* Case 1: realloc can't reallocate */
		if (NULL == tmp) {
			DE("Realloc failed\n");
			return (EBAD);
		}

		/* Case 2: realloc succeeded, new memory returned */
		/* No need to clean the old memory - done by realloc */
		if (NULL != tmp) {
			buf->data = tmp;
		}

		buf->room = buf->used;
		return (EOK);
	}

	/* Here we are if buf->len == buf->room */
	return (EOK);
}


#if 0
err_t buf_str_concat(buf_t *buf, ...){
	va_list p_str;
	va_start(p_str, nHowMany);
	for (int i = 0; i < nHowMany; i++) nSum += va_arg(p_str, char *);
	va_end(intArgumentPointer);

}
#endif

buf_t *buf_sprintf(char *format, ...)
{
	va_list args;
	buf_t   *buf = NULL;
	int     rc   = -1;
	TESTP(format, NULL);

	/* Create buf_t with reserved room for the string */
	buf = buf_new(NULL, 16);
	TESTP(buf, NULL);

	va_start(args, format);
	/* Printf string into the buf_t */
	/* Even if there not enough space in the buffer, this function returns length of full string. So
	   if rc > buf->room means that the buffer is too small for this string */
	rc = vsnprintf(buf->data, buf->room - 1, format, args);
	va_end(args);


	if (rc > MP_LIMIT_BUF_STRING_LEN - 1) {
		DE("The string too long - can't print\n");
		buf_free(buf);
		return (NULL);
	}

	if ((uint32_t)rc > buf->room - 1) {
		rc = buf_add_room(buf, rc - buf->room + 1);
		va_start(args, format);
		/* Printf string into the buf_t */
		rc = vsnprintf(buf->data, buf->room - 1, format, args);
		va_end(args);
	}

	if (rc < 0) {
		DE("Can't print string\n");
		buf_free(buf);
		return (NULL);
	}

	buf->used = rc + 1;
	buf_pack(buf);
	DD("Returning string: |%s|\n", buf->data);
	return (buf);
}
