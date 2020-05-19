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
		free(buf->data);
	}

	free(buf);

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

/* Not tested */
err_t buf_network_prepare(buf_t *buf)
{
	ssize_t to_copy = sizeof(buf_t) - sizeof(char *);;
	TESTP(buf, EBAD);

	if (EOK != buf_test_room(buf, to_copy)) {
		DE("Can't add room to the buffer\n");
		return (EBAD);
	}

	/* We copy the content of buf_t except pointer */

	return (buf_add(buf, (char *)buf, to_copy));
}

/* Not tested */
/*@null@*/ buf_t *buf_network_restore(char *data, size_t size)
{
	buf_t *buf;
	size_t buf_t_struct_size = sizeof(buf_t) - sizeof(char *);;
	TESTP(data, NULL);
	if (size < buf_t_struct_size) {
		DE("The 'data' is too small, less then buf_t size\n");
		return (NULL);
	}

	buf = buf_new(NULL, 0);
	TESTP(buf, NULL);


	/* Now we copy data and cut out from data the buf_t header */
	if (size == buf_t_struct_size) {
		DDD("Nothing to copy");
		return (buf);
	}

	/* If the buffer containf only the buf_t header - finish here */
	if (size == buf->room) {
		DD("Probably wrong buf size: received %zu, buf->size = %d", size, buf->room);
		return (buf);
	}

	/* Restore buf_t header. The buf_t header placed at the end of buf->data, see buf_network_prepare() */
	memcpy(buf, data + size - buf_t_struct_size, buf_t_struct_size);

	buf->data = data;

	/* Clean buf_t at the end */
	memset(buf->data + size - buf_t_struct_size, 0, buf_t_struct_size);

	/* Decrease buf->len*/
	buf->len -= buf_t_struct_size;

	/* And done */

	return (buf);
}

