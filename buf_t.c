#include <stdlib.h>
#include <string.h>

#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"

/*@null@*/ buf_t *buf_new(/*@null@*/ char *data, size_t size)
{
	buf_t *buf = malloc(sizeof(buf_t));
	if (NULL == buf) {
		return (NULL);
	}

	buf->data = data;
	buf->size = size;
	buf->len = 0;

	return (buf);
}

int buf_room(buf_t *buf, size_t size)
{
	void *tmp;
	if (NULL == buf || 0 == size) {
		return (EBAD);
	}

	tmp = realloc(buf->data, buf->size + size);

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
	memset(buf->data + buf->size, 0, size);

	/* Case 3: realloc succidded, the same pointer - we do nothing */
	/* <Beeep> */

	buf->size += size;
	return (EOK);
}

int buf_test_room(buf_t *buf, size_t expect)
{
	if (NULL == buf) {
		DE("Got NULL\n");
		return (EBAD);
	}

	if (buf->len + expect <= buf->size) {
		return (EOK);
	}

	return (buf_room(buf, expect));
}

int buf_free(buf_t *buf)
{
	if (NULL != buf->data) {
		return (EBAD);
	}

	free(buf);
	return (EOK);
}

int buf_free_force(buf_t *buf)
{
	if (NULL != buf->data) {
		free(buf->data);
		buf->data = NULL;
	}

	return (buf_free(buf));
}

int buf_add(buf_t *b, const char *buf, const size_t size)
{
	if (NULL == b || NULL == buf || size < 1) {
		DE("Wrong params: b = %p, buf = %p, size = %zu\n", b, buf, size);
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

