#include <stdlib.h>
#include <string.h>


/*@null@*/ /*@only@*/void *zmalloc(size_t sz)
{
	/*@shared@*/void *ret = malloc(sz);
	if (NULL == ret) return (NULL);
	memset(ret, 0, sz);
	return (ret);
}


/*@null@*/ /*@only@*/void *zmalloc_any(size_t asked, size_t *allocated)
{
	/*@shared@*/void *ret = NULL;

	while (NULL == ret && asked > 0) {
		ret = zmalloc(asked);
		if (NULL != ret) {
			*allocated = asked;
			return (ret);
		}
		asked /= 2;
	}
	return NULL;
}

