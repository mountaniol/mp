#ifndef _SEC_HTABLE_H_
#define _SEC_HTABLE_H_
/*@-skipposixheaders@*/
#include <stdint.h>
/*@=skipposixheaders@*/

#define MURMUR_SEED 17
uint32_t murmur3_32(const uint8_t *key, size_t len);

typedef struct hnode_struct {
	size_t hash;
	char *key;
	char *data;
	struct hnode_struct *next;
} hnode_t;

typedef struct htable_struct {
	hnode_t **nodes;
	size_t size;
	size_t members;
	size_t collisions;
	void *misc;
} htable_t;

/* accepts: trable_t, hash */
#define HTABLE_SLOT(htbl, hsh) (hsh % (htbl->size))

// #define htab_each(htable_t *htab, int index, hnode_t *node)
#define htable_each(htab, index, hnode) \
	for (i = 0, hnode = htab->nodes[i] ; i < htab->size; i++) \
		for (hnode = htab->nodes[i]; hnode != NULL ; hnode = hnode->next)

#define htable_each_sorted(htab, index, hnode) \
	for(index = 0, hnode = ((hnode_t **)htab->misc)[index] ; index < htab->members - 1; hnode = ((hnode_t **)htab->misc)[++index]) 


extern /*@null@*/ htable_t *htable_alloc(size_t size);
extern int htable_free(htable_t *ht);
extern int htable_insert_by_int(htable_t *ht, size_t hash, char *key, void *data);
extern int htable_insert_by_string(htable_t *ht, char *key, void *data);
extern /*@null@*/ void *htable_extract_any(htable_t *ht);
/*@null@*/ void *htable_extract_by_int(htable_t *ht, size_t key);
/*@null@*/ void *htable_extract_by_string(htable_t *ht, char *key);
//extern void *htable_replace(htable_t *ht, char *key, void *data);
extern /*@null@*/ void *htable_find(htable_t *ht, char *key);
//extern int htable_test_key(htable_t *ht, char *key);
extern /*@null@*/ void *htable_first_key(htable_t *ht);
extern int htable_sort(htable_t *ht);
extern int htable_unsort(htable_t *ht);

extern int hrable_test(void);
#endif /* _SEC_HTABLE_H_ */
