#include <stdlib.h>
#include <string.h>

#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"

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
	buf->len = 0;

	return (buf);
}

err_t buf_set_data(/*@null@*/buf_t *buf, /*@null@*/char *data, size_t size, size_t len)
{
	TESTP(buf, EBAD);
	TESTP(data, EBAD);
	buf->data = data;
	buf->room = size;
	buf->len = len;
	return (EOK);
}

/*@null@*/void *buf_steal_data(/*@null@*/buf_t *buf)
{
	/*@temp@*/void *data;
	TESTP(buf, NULL);
	data = buf->data;
	buf->data = NULL;
	buf->room = 0;
	buf->len = 0;
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

	if (buf->len + expect <= buf->room) {
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
	buf->len = buf->room = 0;
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

	memcpy(buf->data + buf->len, new_data, size);
	buf->len += size;
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

	if (0 != buf_test_room(buf, buf->len + 1)) {
		DE("Can't add room into buf_t\n");
		return (EBAD);
	}

	memset(buf->data + buf->len, '\0', 1);
	buf->len++;
	return (EOK);
}

err_t buf_pack(/*@null@*/buf_t *buf)
{
	TESTP(buf, EBAD);
	if (NULL == buf->data) {

		/* Sanity check */
		if (buf->len > 0 || buf->room > 0) {
			DE("WARNING! buf->data == NULL, buf room or len not: len = %u, room = %u\n",
			   buf->len, buf->room);
			buf->len = buf->room = 0;
		}
		return (EOK);
	}

	/* Sanity check */
	if (buf->len > buf->room) {
		DE("ERROR! buf->len (%u) > buf->room (%u)\n", buf->len, buf->room);
		return (EBAD);
	}

	/* Here we should dhring the buffer */
	if (buf->len < buf->room) {
		DD("Packing buf_t: %u -> %u\n", buf->room, buf->len);
		void *tmp = realloc(buf->data, buf->len);

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

		buf->room = buf->len;
		return (EOK);
	}

	/* Here we are if buf->len == buf->room */
	return (EOK);
}