/*@-skipposixheaders@*/
#include <string.h>
/*@=skipposixheaders@*/

#include "mp-common.h"
#include "mp-debug.h"
#include "mp-htable.h"
#include "mp-memory.h"
#include "mp-ctl.h"

#ifndef S_SPLINT_S
static inline uint32_t murmur_32_scramble(uint32_t k)
{
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return (k);
}
#endif

uint32_t murmur3_32(const uint8_t *key, size_t len)
{
#ifndef S_SPLINT_S
	uint32_t h = MURMUR_SEED;
	uint32_t k;
	/* Read in groups of 4. */

	for (size_t i = len >> 2; i; i--) {
		// Here is a source of differing results across endiannesses.
		// A swap here has no effects on hash properties though.
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= murmur_32_scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}
	/* Read the rest. */
	k = 0;

	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	// A swap is *not* necessary here because the preceding loop already
	// places the low bytes in the low places according to whatever endianness
	// we use. Swaps only apply when the memory is copied in a chunk.
	h ^= murmur_32_scramble(k);
	/* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return (h);
#endif /* S_SPLINT_S */
}

/* Allocate new hash table node */
/*@null@*/ static hnode_t *hnode_alloc()
{
	hnode_t *node = zmalloc(sizeof(hnode_t));
	TESTP_MES(node, NULL, "Can't alloc");
	return (node);
}

/* Allocate new hash table */
/*@null@*/ htable_t *htable_alloc(size_t size)
{
	htable_t *ht;

	if (size < 1) {
		DE("Size of table must be > 0\n");
		return (NULL);
	}

	ht = zmalloc(sizeof(htable_t));
	if (NULL == ht) {
		DE("Can't allocate hash table\n");
		return (NULL);
	}

	ht->nodes = zmalloc(sizeof(hnode_t *) * size);
	if (NULL == ht->nodes) {
		DE("Can't allocate array of pointers\n");
		TFREE(ht);
		return (NULL);
	}

	ht->size = size;
	return (ht);
}

/* Free hash table */
int htable_free(htable_t *ht)
{
	TESTP_MES(ht, -1, "Got NULL");
	/* TODO */
	TFREE(ht->nodes);
	TFREE(ht);
	return (0);
}

/* insert new key / value pair */
int htable_insert(htable_t *ht, char *key, void *data)
{
	hnode_t *node;
	hnode_t *node_p;
	uint32_t hash;
	size_t slot;

	TESTP_MES(ht, -1, "Got ht NULL");
	TESTP_MES(key, -1, "Got key NULL");
	TESTP_MES(data, -1, "Got data NULL");

	/* Calculate hash for the jey */
	hash = murmur3_32((uint8_t *)key, strlen(key));

	DDD("Counted hash for key %s : %X\n", key, hash);

	/* Find slot in the hash table for this data */
	slot = HTABLE_SLOT(ht, hash);

	DDD("Slot for key %s : %zu, size of htable: %zu\n", key, slot, ht->size);

	/* Allocate new hash node */
	node = hnode_alloc();
	TESTP_MES(node, -1, "Can't allocate node\n");
	node->data = data;
	node->key = strdup(key);
	node->hash = hash;

	DDD("Starting insert point search\n");
	/* If this slot in hash table is vacant - insert the hash node  */
	if (NULL == ht->nodes[slot]) {
		DDD("Slot is empty - insert and finish\n");
		ht->nodes[slot] = node;
		ht->members++;
		return (0);
	}

	DDD("Slot is not empty, starting linked list search\n");

	/* If in this slot already a node - insert into tail of this node */
	node_p = ht->nodes[slot];
	while (NULL != node_p->next) {
		DDD("Searchin tail: current node is key %s\n", node_p->key);
		node_p = node_p->next;
	}

	DDD("Found end of linked list\n");

	ht->members++;
	node_p->next = node;
	/* If we are here, it means a collition has place, the slot is occupied */
	ht->collisions++;
	return (0);
}

/* remove data for the given key; the data returned */
/*@null@*/ void *htable_delete(htable_t *ht, char *key)
{
	hnode_t *node;
	hnode_t *node_p;
//	hnode_t *node_r;
	uint32_t hash;
	size_t slot;
	void *data;

	TESTP_MES(ht, NULL, "Got NULL");
	TESTP_MES(key, NULL, "Got NULL");

	hash = murmur3_32((uint8_t *)key, strlen(key));
	slot = HTABLE_SLOT(ht, hash);

	node_p = node = ht->nodes[slot];

	/* Iterate over the linked list (if any) until NULL or until hash found */
	while ((NULL != node) && (node->hash != hash)) {
		node_p = node;
		node = node->next;
	}

	/* We can't find node with this key */
	if (NULL == node) {
		DE("Not found key %s\n", key);
		return (NULL);
	}

	/* We found the node. What now?*/

	/* If the found node and pointer to previous node are the same,
	   it means the found node is in the head of the list (if any) */

	/* The node is the only node in the slot:
	   the "next" if the first node in the slot is NULL */
	data = node->data;

	if (NULL == ht->nodes[slot]->next) {
		TFREE(node->key);
		TFREE(node);
		ht->nodes[slot] = NULL;
	} else {
		/* SEB: TODO: Fix here */
		node_p->next = node->next;
		TFREE(node->key);
		TFREE(node);
	}

	ht->members--;

	return (data);
}


/* Replace data for the given key; old datat returned */
#if 0
void *htable_replace(htable_t *ht, char *key, void *data){
	return (0);
}
#endif

/* find data for the given key */
/*@null@*/ void *htable_find(htable_t *ht, char *key)
{
	hnode_t *node;
	uint32_t hash;
	size_t slot;

	TESTP_MES(ht, NULL, "Got NULL");
	TESTP_MES(key, NULL, "Got NULL");

	hash = murmur3_32((uint8_t *)key, strlen(key));
	slot = HTABLE_SLOT(ht, hash);

	DDD("Search for key %s, hash %X, slot %zu\n", key, hash, slot);

	node = ht->nodes[slot];

	/* Iterate over the linked list (if any) until NULL or until hash found */
	while ((NULL != node) && (node->hash != hash)) {
		node = node->next;
	}

	if (node) {
		return (node->data);
	}

	return (NULL);
}


/* Test if this key already in the hash table. 0 for "yes", 1 for "no", -1 for error */
#if 0
int htable_test_key(htable_t *ht, char *key){
	void *data;
	TESTP(ht, -1, "Got NULL");
	TESTP(key, -1, "Got NULL");
	data = htable_find(ht, key);
	if (NULL == data) return (1);
	return (0);
}
#endif

/* remove first found key from hash table */
/*@null@*/ void *htable_first_key(htable_t *ht)
{
	hnode_t *node;
	size_t i;

	TESTP_MES(ht, NULL, "Got NULL");

	htable_each(ht, i, node) {
		return (node->key);
	}

	return (NULL);
}

/* Comparation function, used in htable_sort() */
static int htable_comp(const void *p1, const void *p2)
{
	hnode_t *h1 = *(hnode_t **)p1;
	hnode_t *h2 = *(hnode_t **)p2;
	DDD("Compare: |%s| |%s|\n", h1->key, h2->key);
	return (strcmp(h1->key, h2->key));
}

/* Prepare sorted array */
int htable_sort(htable_t *ht)
{
	size_t i = 0;
	int j = 0;
	hnode_t *node = NULL;
	hnode_t **sarr = NULL;
	TESTP_MES(ht, -1, "Got NULL");
	if (ht->members < 1) {
		DE("Not enough elements\n");
		return (-1);
	}

	sarr = zmalloc((sizeof(hnode_t *) * ht->members) + 1);
	TESTP_MES(sarr, -1, "Can't allocate array for sorted");

	DD("Starting copying\n");
	htable_each(ht, i, node) {
		DDD("Copying  node |%s|\n", node->key);
		sarr[j] = node;
		j++;
	}

	DDD("Finished copying\n");
	sarr[j] = NULL;

	DDD("Starting qsort\n");
	qsort(sarr, ht->members - 1, sizeof(hnode_t *), htable_comp);
	DDD("Finished qsort\n");
	ht->misc = sarr;
	return (0);
}

int htable_unsort(htable_t *ht)
{
	TESTP_MES(ht, -1, "Got NULL");
	TESTP_MES(ht->misc, -1, "misc is NULL");
	TFREE(ht->misc);
	return (0);
}

/*** Testing here ***/

#if 0
static int hrable_test_number(int table_size){
	char *keys[] = {"one", "two", "three", "four", "white", "black", "blue", "orange", NULL};
	char *key;
	size_t i;
	int rc;
	//	void *data;
	htable_t *ht = htable_alloc(table_size);
	if (NULL == ht) {
		DE("Can't allocate htable");
		return (-1);
	}

	D("Created htable - OK\n");

	/* Test insert */
	for (i = 0; keys[i] != NULL; i++) {
		key = keys[i];
		DDD("Starting insertion of key %s\n", key);
		rc = htable_insert(ht, key, strdup(key));
		if (0 != rc) {
			DE("Can't insert key %s\n", key);
			return (-1);
		}
		DDD("inserted key %s - OK\n", key);
	}


	D("---------------------------------------------\n");
	D("Inserted %zu members, collisions %zu, table size %zu\n", ht->members, ht->collisions, ht->size);
	D("---------------------------------------------\n");

	{
		int i;
		hnode_t *hn;

		htab_each(ht, i, hn) {

			printf("Key: %s val: %s\n", hn->key, (char *)hn->data);
		}

	}
	{
		hnode_t *hn;
		int i = 12;
		if (0 == htable_sort(ht)) {
			htab_each_sorted(ht, i, hn) {
				if (hn) {
					DDD("Sorted: key: %s, val: %s\n", hn->key, (char *)hn->data);
				} else DDD("Got NULL\n");
			}
			htable_unsort(ht);
		}
	}

	D("Starting find test\n");

	/* Test find */
	for (i = 0; keys[i] != NULL; i++) {
		char *found;
		key = keys[i];
		D("Starting search for key %s\n", key);
		found = htable_find(ht, key);
		if (NULL == found) {
			DE("Can't find key %s\n", key);
			return (-1);
		}
		DE("Found entry for key %s : %s\n", key, (char *)found);
	}
	D("---------------------------------------------\n");
	D("Starting delete test\n");

	/* Test find */
	for (i = 0; keys[i] != NULL; i++) {
		char *found;
		key = keys[i];

	#ifndef S_SPLINT_S
		D("Starting delete of key %s, members: %zu\n", key, ht->members);
	#endif
		found = htable_delete(ht, key);
		if (NULL == found) {
			DE("Can't find key %s\n", key);
			return (-1);
		}
		DE("Found entry for key %s : %s\n", key, (char *)found);
		TFREE(found);
	}

	htable_free(ht);
	return (0);
}


int hrable_test(void){
	int rc;
	int i;
	D("Testing htable with 0 entries\n");
	rc = hrable_test_number(0);
	if (rc) {
		D("Testing htable with 0 entries failed  - OK\n");
	} else {
		D("Testing htable with 0 entries success  - ERROR\n");
	}

	for (i = 1; i < 1024; i *= 2) {
		D("==============================================\n");
		D("Testing htable with %d entries\n", i);
		rc = hrable_test_number(i);
		if (rc) {
			D("Testing htable with %i entries failed  - ERROR\n", i);
		} else {
			D("Testing htable with %i entries success  - OK\n", i);
		}
		D("End of test of htable with %d entries\n", i);
		D("==============================================\n");
	}

	return (rc);
}
#endif


