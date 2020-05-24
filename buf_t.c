#include <stdlib.h>
#include <string.h>

#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"

/*@null@*/ buf_t *buf_new(/*@temp@*//*@null@*/char *data, size_t size)
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

	if (NULL != data) {
		buf->data = data;
	} else {
		buf->data = zmalloc(size);
		TESTP_ASSERT(buf->data, "Can't allocate buf->data");
	}
	buf->room = size;
	buf->len = 0;

	return (buf);
}

err_t buf_set_type(buf_t *buf, uint8_t tp)
{
	TESTP(buf, EBAD);
	buf->tp = tp;
	return (EOK);
}

err_t buf_add_room(buf_t *buf, size_t size)
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

err_t buf_test_room(buf_t *buf, size_t expect)
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

int buf_get_room(buf_t *buf)
{
	if (NULL == buf) {
		DE("Got NULL\n");
		return (EBAD);
	}

	return ((buf->room) - buf->len);
}

err_t buf_free_room(/*@only@*/buf_t *buf)
{
	TESTP(buf, EBAD);
	TFREE(buf->data);
	buf->len = buf->room = 0;
	buf->tp = 0;
	return (EOK);
}

err_t buf_free(/*@only@*/buf_t *buf)
{
	TESTP(buf, EBAD);

	if (NULL != buf->data) {
		TFREE(buf->data);
	}

	TFREE(buf);

	return (EOK);
}

err_t buf_add(buf_t *b, const char *buf, const size_t size)
{
	if (NULL == b || NULL == buf || size < 1) {
		/*@ignore@*/
		DE("Wrong params: b = %p, buf = %p, size = %zu\n", b, buf, size);
		/*@end@*/
		return (EBAD);
	}

	if (0 != buf_test_room(b, size)) {
		DE("Can't add room into buf_t\n");
		return (EBAD);
	}

	memcpy(b->data + b->len, buf, size);
	b->len += size;
	return (EOK);
}
