/*@-skipposixheaders@*/
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <fcntl.h>
/*@=skipposixheaders@*/

#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-limits.h"
#include "mp-os.h"

/* We open it once and keep opened */
static FILE *rand_device = NULL;

static int open_random_device(void)
{
	if (NULL != rand_device) {
		return (EOK);
	}

	/* Try to open a /dev/urandom device */
	if (NULL == rand_device) {
		//rand_device = fopen("/dev/urandom", "r");
		rand_device = mp_os_fopen_chardev("/dev/urandom", "r");
	}

	if (NULL != rand_device) {
		return (EOK);
	}

	/* Try to open /dev/random device */
	if (NULL == rand_device) {
		rand_device = mp_os_fopen_chardev("/dev/random", "r");
	}

	if (NULL != rand_device) {
		return (EOK);
	}

	return (EBAD);
}

/*@null@*/ char *mp_os_get_hostname(void)
{
	struct addrinfo hints, *info;
	int             gai_result     = -1;

	char            hostname[1024];
	hostname[1023] = '\0';
	char *ret = NULL;

	int  rc   = gethostname(hostname, 1023);
	if (0 != rc) {
		DE("Failed: gethostname\n");
		perror("Failed: gethostname\n");
		return (NULL);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
		return (NULL);
	}

	if (NULL != info->ai_canonname) {
		ret = strdup(info->ai_canonname);
	}

	freeaddrinfo(info);
	return (ret);
}

/* Fill the buffer with noise */
int mp_os_fill_random(void *buf, size_t buf_size)
{
	ssize_t rc;
	if (EOK != open_random_device()) {
		DE("Can't open random device!\n");
		abort();
	}

	TESTP_MES(buf, EBAD, "Got NULL");
	if (buf_size < 1) {
		DE("Length of buffer is invalid\n");
		return (EBAD);
	}

	rc = fread(buf, 1, buf_size, rand_device);
	if (rc < buf_size) {
		perror("Can't read from random device!\n");
		DE("Can't read from random device!\n");
		DE("Please check that you can read from /dev/random or /dev/urandom\n");
		abort();
	}

	if ((size_t)rc != buf_size) {
		DE("Can't read enough random data: asked %zu, got %zd\n", buf_size, rc);
		return (EBAD);
	}

	return (rc);
}

const char charset_alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK0123456789";
const char charset_num[]   = "0123456789";

/*@null@*/ static char *_rand_string(size_t size, const char charset[], size_t charset_size)
{
	//FILE   *fd  = NULL;
	char   *str = NULL;
	size_t n    = 0;
	int    rc   = -1;

	if (0 == size) return (NULL);

	if (EOK != open_random_device()) {
		DE("Can't open random device!\n");
		abort();
	}

	str = zmalloc(size);
	TESTP_MES(str, NULL, "Can't allocate");

	rc = mp_os_fill_random(str, size);

	if ((int)size != rc) {
		DE("Can't read from /dev/urandom : asked %zu, read %d\n", size, rc);
		free(str);
		return (NULL);
	}

	--size;

	for (n = 0; n < size; n++) {
		unsigned int key = (((unsigned int)str[n]) % (charset_size - 1));
		str[n] = charset[key];
	}

	str[size] = '\0';

	return (str);
}

/*@null@*/ char *mp_os_rand_string(size_t size)
{
	return (_rand_string(size, charset_alpha, sizeof(charset_alpha)));
}

/*@null@*/ static char *mp_os_rand_string_numeric(size_t size)
{
	return (_rand_string(size, charset_num, sizeof(charset_num)));
}

int mp_os_random_in_range(int lower, int upper)
{
	return ((rand() % (upper - lower + 1)) + lower);
}

/*@null@*/ char *mp_os_generate_uid(const char *name)
{
	char *str   = NULL;
	char *part1 = NULL;
	char *part2 = NULL;
	char *part3 = NULL;
	int  rc;

	TESTP(name, NULL);
	size_t len = 0;

	part1 = mp_os_rand_string_numeric(4);
	TESTP_GO(part1, err);
	len += 4;
	part2 = mp_os_rand_string_numeric(4);
	TESTP_GO(part2, err);
	len += 4;
	part3 = mp_os_rand_string_numeric(4);
	TESTP_GO(part3, err);
	len += 4;

	len += 2 + strnlen(name, MP_LIMIT_USER_MAX);

	str = zmalloc(len);
	//DD("name: %s, part1: %s, part2: %s", name, part1, part2);
	/* SEB: TODO: Use snsprintf */
	rc = snprintf(str, len, "%s-%s-%s-%s", name, part1, part2, part3);
	if (rc < 0) {
		DE("Can't generate uid\n");
		abort();
	}
err:
	TFREE_SIZE(part1, 4);
	TFREE_SIZE(part2, 4);
	TFREE_SIZE(part3, 4);
	return (str);
}

err_t mp_os_usleep(int milliseconds) // cross-platform sleep function
{
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	if (0 != nanosleep(&ts, NULL)) {
		DE("nanosleep returned an error\n");
		return (EBAD);
	}

	return (EOK);
}

/* Test that given file is kind of asked type, OK if yes */
static err_t mp_os_test_file_type(const char *file, int tp)
{
	struct stat st;

	if (0 != lstat(file, &st)) {
		DE("lstat failed, probably no such file\n");
		perror("lstat");
		return (EBAD);
	}

	/* Is this file a regular file? If not, report it and return with error */
	if (tp != (st.st_mode & S_IFMT)) {
		DE("Error on file [%s] opening: not a regular file, but ", file);
		switch (st.st_mode & S_IFMT) {
		case S_IFSOCK:
			printf("socket\n");
			break;
		case S_IFLNK:
			printf("symbolic link\n");
			break;
		case S_IFREG:
			printf("regular file\n");
			break;
		case S_IFBLK:
			printf("block device\n");
			break;
		case S_IFDIR:
			printf("directory\n");
			break;
		case S_IFCHR:
			printf("character device\n");
			break;
		case S_IFIFO:
			printf("FIFO\n");
			break;
		}

		return (EBAD);
	}
	return (EOK);
}

static int mp_os_open(const char *file, int flags, mode_t mode, int tp)
{
	if (EOK != mp_os_test_file_type(file, tp)) {
		DE("Wrong file\n");
		return (EBAD);
	}

	if (mode > 0) {
		return (open(file, flags, mode));
	} else {
		return (open(file, flags));
	}

	/* We should never be here */
	TESTP_ASSERT(NULL, "We should never be here!");
	/* To calm down compiled warnings */
	return (EBAD);
}

int mp_os_open_regular(const char *file, int flags, mode_t mode)
{
	return (mp_os_open(file, flags, mode, S_IFREG));
}

static FILE *mp_os_fopen(const char *file, const char *mode, int tp)
{
	if (EOK != mp_os_test_file_type(file, tp)) {
		DE("Wrong file\n");
		return (NULL);
	}

	return (fopen(file, mode));
}

FILE *mp_os_fopen_regular(const char *file, const char *mode)
{
	return (mp_os_fopen(file, mode, S_IFREG));
}

FILE *mp_os_fopen_chardev(const char *file, const char *mode)
{
	return (mp_os_fopen(file, mode, S_IFCHR));
}