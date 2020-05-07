/* 
 * Copyright Sebastian Mountaniol 
 * All rights reserved 
 * 03/2020 
 * This file inplements JSON encoding and decoding, 
 * JSON objects build and parse 
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE        /* or any value < 500 */

#include <jansson.h>
#include <string.h>
#include "mp-common.h"
#include "buf_t.h"
#include "mp-debug.h"
#include "mp-jansson.h"

/*@null@*/ json_t *j_str2j(char *str)
{
	json_error_t error;
	json_t *root;

	TESTP_MES(str, NULL, "Got NULL\n");

	root = json_loads(str, 0, &error);

	if (NULL == root) {
		DE("Can't decore JSON string to object\n");
		DE("String is: \n%s\n", str);
		DE("Error text is  : %s\n", error.text);
		DE("Error source is: %s\n", error.source);
		DE("Error is in line: %i\n", error.line);
		DE("Error is in col: %i\n", error.column);
		DE("Error is in pos: %i\n", error.position);
		return (NULL);
	}

	return (root);
}

/*@null@*/ json_t *j_buf2j(const buf_t *buf)
{
	json_t *root = NULL;

	TESTP(buf, NULL);
	TESTP(buf->data, NULL);
	//D("Got buffer: %s\n", buf->data);
	root = json_loads(buf->data, 0, NULL);
	TESTP(root, NULL);
	return (root);
}

/*@null@*/ buf_t *j_2buf(const json_t *j_obj)
{
	buf_t *buf = NULL;
	char *jd = NULL;

	TESTP(j_obj, NULL);

	//jd = json_dumps(j_obj, JSON_COMPACT);
	jd = json_dumps(j_obj, (size_t)JSON_INDENT(4));
	TESTP_MES(j_obj, NULL, "Can't transform JSON to string");

	buf = buf_new(jd, strlen(jd));
	TESTP_MES_GO(jd, err, "Can't allocate buf_t");

	buf->len = buf->size;

	return (buf);
err:
	TFREE(jd);
	if (buf) buf_free_force(buf);
	return (NULL);
}

int j_arr_add(json_t *arr, json_t *obj)
{
	return (json_array_append_new(arr, obj));
}

void *j_arr()
{
	return (json_array());
}

void *j_new()
{
	return (json_object());
}

int j_add_j2arr(json_t *root, const char *arr_name, json_t *obj)
{
	json_t *arr = j_find_j(root, arr_name);
	TESTP(arr, EBAD);
	return j_arr_add(arr, obj);
}

int j_add_j(json_t *root, const char *key, json_t *obj)
{
	return json_object_set_new(root, key, obj);
}

int j_add_str(json_t *root, const char *key, const char *val)
{
	int rc;
	json_t *j_str = NULL;

	TESTP(root, EBAD);
	TESTP(key, EBAD);
	if (NULL == val) {
		DE("Error: val == NULL for key '%s'\n", key);
	}
	TESTP(val, EBAD);

	j_str = json_string(val);
	TESTP(j_str, EBAD);

	rc = json_object_set_new(root, key, j_str);
	TESTI_MES(rc, EBAD, "Can't set new string to json object");
	return (0);
}

int j_add_int(json_t *root, const char *key, json_int_t val)
{
	int rc;
	json_t *j_int;

	TESTP(root, EBAD);

	j_int = json_integer(val);
	TESTP(j_int, EBAD);

	rc = json_object_set_new(root, key, j_int);
	TESTI_MES(rc, EBAD, "Can't set new string to json object");
	return (0);
}


int j_cp(json_t *from, json_t *to, const char *key)
{
	json_t *j_obj = NULL;
	json_t *j_obj_copy  = NULL;
	int rc = EBAD;
	TESTP(from, EBAD);
	TESTP(to, EBAD);
	TESTP(key, EBAD);

	j_obj = json_object_get(from, key);
	TESTP(j_obj, EBAD);

	j_obj_copy = json_deep_copy(j_obj);
	TESTP(j_obj_copy, EBAD);

	rc = json_object_set_new(to, key, j_obj_copy);
	if (0 != rc) {
		return (EBAD);
	}
	return (EOK);
}

int j_cp_val(json_t *from, json_t *to, char *key_from, char *key_to)
{
	json_t *j_obj;
	json_t *j_obj_copy;
	int rc;
	TESTP(from, EBAD);
	TESTP(to, EBAD);
	TESTP(key_from, EBAD);
	TESTP(key_to, EBAD);

	j_obj = json_object_get(from, key_from);
	TESTP(j_obj, EBAD);
	j_obj_copy = json_deep_copy(j_obj);
	TESTP(j_obj_copy, EBAD);
	rc = json_object_set_new(to, key_to, j_obj_copy);
	if (0 != rc) {
		return (EBAD);
	}
	return (EOK);
}


#if 0
int json_add_string_int(json_t *root, char *key, int val){
	json_t *j_int = NULL;

	if (NULL == root) {
		DE("Got root == NULL\n");
		return (-1);
	}

	if (NULL == key) {
		DE("Got key == NULL\n");
		return (-1);
	}

	j_int = json_integer(val);

	if (NULL == j_int) {
		DE("Can't allocate json object\n");
		return (-1);
	}

	if (0 != json_object_set_new(root, key, j_int)) {
		DE("Can't set new pait into json object\n");
		//json_decref(j_int);
		return (-1);
	}

	return (0);
}
#endif

/* Get JSON object, get  field name and expected value of this field.
   Return EOK if this is true, return EBAD is no match */
int j_test(json_t *root, const char *type_name, const char *expected_val)
{
	const char *val = NULL;
	size_t val_len = 0;

	if (NULL == root || NULL == type_name || NULL == expected_val) {
		DE("Got NULL: root = %p, type_name == %p, expected_val = %p\n", root, type_name, expected_val);
		DE("Got NULL: type_name == %s, expected_val = %s\n", type_name, expected_val);
		return (EBAD);
	}

	/* SEB: TODO: Max length of the string should be defined and documented! */
	val_len = strnlen(expected_val, 32);

	/* Get and check type: we should be sure we have one  */
	val = j_find_ref(root, type_name);
	if (NULL == val) {
		//DE("Parsing error: can't get value for type %s\n", type_name);
		return (EBAD);
	}

	if (0 != strncmp(expected_val, val, val_len)) {
		//DE("Value of type %s is %s instead of %s\n", type_name, val, expected_val);
		return (EBAD);
	}

	return (EOK);
}


/*@null@*/ int j_test_key(json_t *root, const char *key)
{
	json_t *j_string = NULL;

	TESTP(root, EBAD);
	TESTP(key, EBAD);

	j_string = json_object_get(root, key);
	if (NULL == j_string) {
		return (EBAD);
	}

	return (EOK);
}

/*@null@*/ json_t *j_find_j(json_t *root, const char *key)
{
	TESTP(root, NULL);
	TESTP(key, NULL);
	return json_object_get(root, key);
}

/*@null@*/ const char *j_find_ref(json_t *root, const char *key)
{
	json_t *j_obj;
	TESTP(root, NULL);
	TESTP(key, NULL);

	j_obj = json_object_get(root, key);
	TESTP(j_obj, NULL);
	return (json_string_value(j_obj));
}

json_int_t j_find_int(json_t *root, const char *key)
{
	json_t *j_obj;
	TESTP(root, 0XDEADBEEF);
	TESTP(key, 0XDEADBEEF);

	j_obj = json_object_get(root, key);
	TESTP(j_obj, 0XDEADBEEF);
	return (json_integer_value(j_obj));
}

/*@null@*/ char *j_find_dup(json_t *root, const char *key)
{
	json_t *j_string = NULL;
	size_t len = 0;
	const char *s_string;

	TESTP(root, NULL);
	TESTP(key, NULL);

	j_string = json_object_get(root, key);
#if 0 /* SEB 30/04/2020 18:36  */

	if (NULL == j_string) {
		DE("Can't find str for key: %s\n", key);
	}
#endif /* SEB 30/04/2020 18:36 */
	TESTP(j_string, NULL);

	len = json_string_length(j_string);
	if (len < 1) {
		DE("Value string length for key '%s' is wrong: %zu\n", key, len);
		return (NULL);
	}

	s_string = json_string_value(j_string);
	TESTP(s_string, NULL);

	return (strndup(s_string, len));
}

int j_count(const json_t *root)
{
	TESTP(root, EBAD);
	return (json_object_size(root));
}

int j_replace(json_t *root, const char *key, json_t *j_new)
{
	TESTP(root, EBAD);
	TESTP(key, EBAD);
	TESTP(j_new, EBAD);
	return (json_object_set_new(root, key, j_new));
}

json_t *j_dup(const json_t *root)
{
	return (json_deep_copy(root));
}

int j_rm_key(json_t *root, const char *key)
{
	return (json_object_del(root, key));
}

int j_rm(json_t *root)
{
	const char *key = NULL;
	void *tmp = NULL;
	json_t *value = NULL;

	TESTP(root, EBAD);

	json_object_foreach_safe(root, tmp, key, value) {

		if (JSON_ARRAY == json_typeof(value)) {
			if (0 != json_array_clear(value)) {
				DE("Error on jsom arrary cleaning\n");
			}
		}

		if (0 != json_object_del(root, key)) {
			DE("Can't delete key '%s' from JSON object\n", key);
		}
	}

	if (json_object_size(root) != 0) {
		DE("json_object_foreach_safe failed to iterate all key-value pairs");
	}

	json_decref(root);
	return (0);
}

int j_print(json_t *root, const char *prefix)
{

	buf_t *buf = j_2buf(root);
	TESTP(buf, EBAD);
	if (prefix) D("%s :\n", prefix);
	printf("%s\n", buf->data);
	buf_free_force(buf);
	return EOK;
	
#if 0
	const char *key;
	json_t *val;

	json_object_foreach(root, key, val) {
		printf("%s: ", key);
		switch (json_typeof(val)) {
			case JSON_OBJECT:
			printf("err: type of key is JSON object\n");
			break;
			case JSON_ARRAY:
			printf("err: type of key is JSON array (not supported)\n");
			break;
			case JSON_STRING:
			printf("%s\n", json_string_value(val));
			break;
			case JSON_INTEGER:
			printf("%lli\n", json_integer_value(val));
			break;
			case JSON_REAL:
			printf("%f\n", json_real_value(val));
			break;
			case JSON_TRUE:
			case JSON_FALSE:
			printf("err: type of key is JSON true/false\n");
			break;
			case JSON_NULL:
			printf("NULL\n");
			break;
		}
	}
	return (0);
#endif
}

#if 0 /* SEB 26/04/2020 18:33  */
static int release_dnode(void *v){
	if (NULL == v) {
		return (-1);
	}

	free(v);
	return (0);
}


/*@null@*/ json_t *json_array_from_dholder(dholder_t *dh){
	json_t *arr = NULL;
	json_t *val = NULL;
	dnode_t *dn = NULL;

	TESTP(dh, NULL, "Got NULL\n");

	arr = json_array();
	if (NULL == arr) {
		DE("Can't create new json array\n");
		goto err;
	}

	while (dh->members > 0) {
		DD("Building names array\n");
		dn = dnode_extract_first(dh);
		if (dn) {

			DD("Extracted name: %s\n", dn->var);
			val = json_string(dn->var);

			if (NULL == val) {
				DE("Can't create json string from the string %s\n", dn->var);
				dnode_free(dn, release_dnode);
				goto err;
			}

			DD("Adding json string %s into array \n", dn->var);

			if (0 != json_array_append_new(arr, val)) {
				DE("Failed to add json string to array\n");
				dnode_free(dn, release_dnode);
				goto err;
			}

			dnode_free(dn, release_dnode);
		} /* if (dn) */
	} /* while */

	return (arr);

	err:
	if (NULL != dh) dholder_destroy(dh, release_dnode);
	if (NULL != dn) dnode_free(dn, release_dnode);
	return (NULL);
}

#endif /* SEB 26/04/2020 18:33 */
