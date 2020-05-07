#include <stdlib.h>
#include <string.h>


void *zmalloc(size_t sz)
{
	void *ret = malloc(sz);
	if (NULL == ret) return NULL;
	memset(ret, 0, sz);
	return ret;
}

