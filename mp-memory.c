#include <stdlib.h>
#include <string.h>


/*@null@*/ /*@shared@*/void *zmalloc(size_t sz)
{
	/*@shared@*/void *ret = malloc(sz);
	if (NULL == ret) return NULL;
	memset(ret, 0, sz);
	return ret;
}

