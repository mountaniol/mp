#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "buf_t.h"
#include "mp-debug.h"

#define PSPLITTER()  do{printf("++++++++++++++++++++++++++++\n");} while(0)
#define PSTART(x)     do{printf("Running  test: [%s]\n", x);} while(0)
#define PSTEP(x)      do{printf("Passed   step: [%s] +%d\n", x, __LINE__);} while(0)
#define PSUCCESS(x) do{printf("Finished test: [%s] : OK\n", x);} while(0)
#define PFAIL(x)   do{printf("Finished test: [%s] : FAIL [line +%d]\n", x, __LINE__);} while(0)

/* Create buffer with 0 size data */
void test_buf_new_zero_size()
{
	buf_t *buf = NULL;
	PSPLITTER();

	PSTART("allocate 0 size buffer");
	buf = buf_new(0);
	if (NULL == buf) {
		PFAIL("Cant allocate 0 size buf");
		abort();
	}

	if (buf->used != 0 || buf->room != 0) {
		printf("0 size buffer: used (%d) or room (%d) != 0\n", buf->used, buf->room);
		PFAIL("0 size buffer");
		abort();
	}

	if (buf->data != NULL) {
		printf("0 size buffer: data != NULL (%p)\n", buf->data);
		PFAIL("0 size buffer");
		abort();
	}

	PSUCCESS("allocate 0 size buffer");
}

/* Create buffers with increasing size */
void test_buf_new_increasing_size()
{
	buf_t  *buf = NULL;
	size_t size = 64;
	int    i;
	PSPLITTER();

	PSTART("increasing size buffer");
	for (i = 1; i < 16; i++) {
		size = size << 1;

		buf = buf_new(size);
		if (NULL == buf) {
			PFAIL("increasing size buffer");
			printf("Tryed to allocate: %zu size\n", size);
			abort();
		}

		if (buf->used != 0 || buf->room != size) {
			printf("increasing size buffer: used (%d) !=0 or room (%d) != %zu\n", buf->used, buf->room, size);
			PFAIL("increasing size buffer");
			abort();
		}

		if (NULL == buf->data) {
			printf("increasing size buffer: data == NULL (%p), asked size: %zu, iteration: %d\n", buf->data, size, i);
			PFAIL("increasing size buffer");
			abort();
		}

		buf_free(buf);
	}

	printf("[Allocated up to %zu bytes buffer]\n", size);
	PSUCCESS("increasing size buffer");
}

void test_buf_string(size_t buffer_init_size)
{
	buf_t      *buf  = NULL;
	const char *str  = "Jabala Labala Hoom";
	const char *str2 = " Lalala";

	PSPLITTER();

	PSTART("buf_string");
	printf("[Asked string size: %zu]\n", buffer_init_size);

	buf = buf_string(buffer_init_size);
	if (NULL == buf) {
		PFAIL("buf_string: Can't allocate buf");
		abort();
	}

	PSTEP("Allocated buffer");

	if (EOK != buf_add(buf, str, strlen(str))) {
		PFAIL("buf_string: can't add");
		abort();
	}

	PSTEP("Adding str");

	#if 0
	if (buf->used != (buf->room - 1)) {
		printf("[After buf_add: wrong buf->used or buf->room]\n");
		printf("[buf->used = %d, buf->room = %d]\n", buf->used, buf->room);
		printf("[bif->used should be = (buf->room - 1)]\n");
		PFAIL("buf_string failed");
		abort();
	}
	#endif

	if (strlen(buf->data) != strlen(str)) {
		printf("[After buf_add: wrong string len of buf->data]\n");
		printf("[Added string len = %zu]\n", strlen(str));
		printf("[buf->data len = %zu]\n", strlen(buf->data));
		PFAIL("buf_string");
		abort();
	}

	#if 0
	printf("After string: |%s|, str: |%s|\n", buf->data, str);
	printf("After first string: buf->room = %d, buf->used = %d\n", buf->room, buf->used);
	#endif

	PSTEP("Tested str added");

	if (EOK != buf_add(buf, str2, strlen(str2))) {
		printf("[Can't add string into buf]\n");
		PFAIL("buf_string");
		abort();
	}

	if (buf->used != strlen(str) + strlen(str2)) {
		printf("After buf_add: wrong buf->used\n");
		printf("Expected: buf->used = %zu\n", strlen(str) + strlen(str2));
		printf("Current : buf->used = %d\n", buf->used);
		printf("str = |%s| len = %ld\n", str, strlen(str));
		printf("str2 = |%s| len = %ld\n", str2, strlen(str2));

		PFAIL("buf_string");
		abort();
	}

	if (strlen(buf->data) != (strlen(str) + strlen(str2))) {
		printf("[buf->used != added strings]\n");
		printf("[buf->used = %zu, added strings len = %zu]\n", strlen(buf->data), strlen(str) + strlen(str2));
		printf("[String is: |%s|, added strings: |%s%s|]\n", buf->data, str, str2);
		printf("str = |%s| len = %ld\n", str, strlen(str));
		printf("str2 = |%s| len = %ld\n", str2, strlen(str2));
		PFAIL("buf_string");
		abort();
	}

	//printf("%s\n", buf->data);
	buf_free(buf);

	PSUCCESS("buf_string");
}

/* Allocate string buffer. Add several strings. Pack it. Test that after the packing the buf is
   correct. Test that the string in the buffer is correct. */
void test_buf_pack_string(void)
{
	buf_t      *buf     = NULL;
	const char *str     = "Jabala Labala Hoom";
	const char *str2    = " Lalala";
	char       *con_str = NULL;
	size_t     len;
	size_t     len2;

	PSPLITTER();

	PSTART("buf_pack_string");

	buf = buf_string(1024);
	if (NULL == buf) {
		PFAIL("buf_string: Can't allocate buf");
		abort();
	}

	PSTEP("Allocated buffer");

	len = strlen(str);

	if (EOK != buf_add(buf, str, len)) {
		PFAIL("buf_pack_string: can't add");
		abort();
	}

	PSTEP("Adding str");

	if (strlen(buf->data) != strlen(str)) {
		PFAIL("buf_pack_string");
		abort();
	}

	PSTEP("Tested str added");


	len2 = strlen(str2);
	if (EOK != buf_add(buf, str2, len2)) {
		printf("[Can't add string into buf]\n");
		PFAIL("buf_pack_string");
		abort();
	}

	if (buf->used != (len + len2)) {
		PFAIL("buf_pack_string");
		abort();
	}

	if (strlen(buf->data) != (len + len2)) {
		PFAIL("buf_pack_string");
		abort();
	}

	/* Now we pack the buf */
	if (EOK != buf_pack(buf)) {
		PFAIL("buf_pack_string");
		abort();
	}

	/* Test that the packed buffer has the right size */
	if (buf->used != (len + len2)) {
		PFAIL("buf_pack_string");
		abort();
	}

	/* Test that buf->room = buf->used + 1 */
	if (buf->used != buf->room - 1) {
		PFAIL("buf_pack_string");
		abort();
	}

	con_str = malloc(len + len2 + 1);
	if (NULL == con_str) {
		printf("Error: can't allocate memory\n");
		abort();
	}

	memset(con_str, 0, len + len2 + 1);
	sprintf(con_str, "%s%s", str, str2);

	if (0 != strcmp(buf->data, con_str)) {
		PFAIL("buf_pack_string");
		abort();
	}
	//printf("%s\n", buf->data);
	buf_free(buf);
	free(con_str);

	PSUCCESS("buf_pack_string");
}

void test_buf_pack(void)
{
	buf_t  *buf          = NULL;
	char   *buf_data     = NULL;
	size_t buf_data_size = 256;
	int    i;
	time_t current_time  = time(0);
	srandom((unsigned int)current_time);

	PSPLITTER();

	PSTART("buf_pack");

	buf = buf_new(1024);
	if (NULL == buf) {
		PFAIL("buf_string: Can't allocate buf");
		abort();
	}

	PSTEP("Allocated buffer");

	buf_data = malloc(256);
	if (NULL == buf_data) {
		printf("Can't allocate buffer\n");
		abort();
	}

	PSTEP("Allocated local buffer for random data");

	for (i = 0; i < buf_data_size; i++) {
		char randomNumber = (char)random();
		buf_data[i] = randomNumber;
	}


	PSTEP("Filled local buffer with random data");

	/* Make sure that this buffer ended not with 0 */
	//buf_data[buf_data_size - 1] = 9;

	if (EOK != buf_add(buf, buf_data, buf_data_size)) {
		PFAIL("buf_pack: can't add");
		abort();
	}

	PSTEP("Added buffer into buf_t");

	if (buf->used != buf_data_size) {
		PFAIL("buf_pack");
		abort();
	}

	/* Compare memory */
	if (0 != memcmp(buf->data, buf_data, buf_data_size)) {
		PFAIL("buf_pack");
		abort();
	}
	
	PSTEP("Compared memory");

	/* Now we pack the buf */
	if (EOK != buf_pack(buf)) {
		PFAIL("buf_pack");
		abort();
	}
	PSTEP("Packed buf_t");

	/* Test that the packed buffer has the right size */
	if (buf->used != buf_data_size) {
		PFAIL("buf_pack");
		abort();
	}
	PSTEP("That buf->used is right");

	/* Test that buf->room = buf->used + 1 */
	if (buf->used != buf->room) {
		printf("buf->room (%d) != buf->used (%d)\n", buf->room, buf->used);
		PFAIL("buf_pack");
		abort();
	}
	PSTEP("Tested room and used");

	//printf("%s\n", buf->data);
	buf_free(buf);
	free(buf_data);

	PSUCCESS("buf_pack");
}

int main()
{
	/* Abort on any error */
	DD("This is regular print\n");
	DDD("This is extended print\n");
	DE("This is error print\n");
	DDE("This is extended error print\n");

	buf_set_abort();
	test_buf_new_zero_size();
	test_buf_new_increasing_size();
	test_buf_string(0);
	test_buf_string(1);
	test_buf_string(32);
	test_buf_string(1024);
	test_buf_pack_string();
	test_buf_pack();

	return (0);
}