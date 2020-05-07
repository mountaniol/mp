/*@-skipposixheaders@*/
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
/*@=skipposixheaders@*/

#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"

char *mp_os_get_hostname()
{
	struct addrinfo hints, *info;
	int gai_result = -1;

	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	char *ret = NULL;

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
const char charset_alpha[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK0123456789";
const char charset_num[] = "0123456789";

static char *_rand_string(size_t size, const char charset[], size_t charset_size)
{
	FILE *fd = NULL;
	char *str = NULL;
	size_t n = 0;
	int rc = -1;

	if (0 == size) return (NULL);

	str = zmalloc(size);
	TESTP_MES(str, NULL, "Can't allocate");

	fd = fopen("/dev/urandom", "r");
	TESTP_MES(fd, NULL, "Can't open /dev/urandom");

	rc = fread(str, 1, size, fd);
	fclose(fd);

	if ((int)size != rc) {
		DE("Can't read from /dev/urandom : asked %zu, read %d\n", size, rc);
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

char *mp_os_rand_string(size_t size)
{
	return (_rand_string(size, charset_alpha, sizeof(charset_alpha)));
}

static char *mp_os_rand_string_numeric(size_t size)
{
	return (_rand_string(size, charset_num, sizeof(charset_num)));
}

int mp_os_random_in_range(int lower, int upper)
{
	return ((rand() % (upper - lower + 1)) + lower);
}

char *mp_os_generate_uid(const char *name)
{
	char *str = NULL;
	char *part1;
	char *part2;
	char *part3;

	TESTP(name, NULL);

	part1 = mp_os_rand_string_numeric(4);
	TESTP_GO(part1, err);
	part2 = mp_os_rand_string_numeric(4);
	TESTP_GO(part2, err);
	part3 = mp_os_rand_string_numeric(4);
	TESTP_GO(part3, err);

	str = zmalloc(strlen(name) + 11 + 2);
	//DD("name: %s, part1: %s, part2: %s", name, part1, part2);
	/* SEB: TODO: Use snsprintf */
	sprintf(str, "%s-%s-%s-%s", name, part1, part2, part3);
err:
	TFREE(part1);
	TFREE(part2);
	TFREE(part3);
	return (str);
}

