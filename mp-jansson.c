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
#include "mp-memory.h"

/*@null@*//*@only@*/ json_t *j_str2j(/*@null@*/const char *str)
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
/*@null@*//*@only@*/ json_t *j_strn2j(/*@null@*/const char *str, size_t len)
{
	/*only*/char *buf = NULL;
	/*temp*/ json_t *root = NULL;
	TESTP(str, NULL);
	if (len < 1) return (NULL);
	buf = zmalloc(len + 1);
	if (NULL == buf) {
		return (NULL);
	}

	memcpy(buf, str, len);
	root = j_str2j(buf);
	free(buf);
	return (root);
}

/*@null@*//*@only@*/ json_t *j_buf2j(/*@null@*/const buf_t *buf)
{
	/*@temp@*/json_t *root = NULL;

	TESTP(buf, NULL);
	TESTP(buf->data, NULL);
	root = json_loads(buf->data, 0, NULL);
	TESTP(root, NULL);
	return (root);
}

/*@null@*//*@only@*/ buf_t *j_2buf(/*@null@*/const json_t *j_obj)
{
	buf_t *buf = NULL;
	char *jd = NULL;

	TESTP(j_obj, NULL);

	jd = json_dumps(j_obj, (size_t)JSON_INDENT(4));
	TESTP_MES(j_obj, NULL, "Can't transform JSON to string");

	buf = buf_new(jd, strlen(jd));
	TESTP_MES_GO(jd, err, "Can't allocate buf_t");

	buf->len = buf->room;

	return (buf);
err:
	TFREE(jd);
	if (buf) {
		if (EOK != buf_free(buf)) {
			DE("Can't remove buf_t: probably passed NULL pointer?\n");
		}
	}
	return (NULL);
}

err_t j_arr_add(/*@null@*/json_t *arr, /*@null@*/json_t *obj)
{
	TESTP(arr, EBAD);
	TESTP(obj, EBAD);
	return (json_array_append_new(arr, obj));
}

/*@null@*//*@only@*/ void *j_arr()
{
	return (json_array());
}

/*@null@*//*@only@*/ void *j_new()
{
	return (json_object());
}

err_t j_add_j2arr(/*@null@*/json_t *root, /*@null@*/const char *arr_name, /*@null@*/json_t *obj)
{
	json_t *arr = NULL;

	TESTP(root, EBAD);
	TESTP(arr_name, EBAD);
	TESTP(obj, EBAD);

	arr = j_find_j(root, arr_name);
	TESTP(arr, EBAD);
	return (j_arr_add(arr, obj));
}

err_t j_add_j(/*@null@*/json_t *root, /*@null@*/const char *key, /*@null@*/json_t *obj)
{
	TESTP(root, EBAD);
	TESTP(key, EBAD);
	TESTP(obj, EBAD);

	return (json_object_set_new(root, key, obj));
}

err_t j_add_str(/*@null@*/json_t *root, /*@null@*/const char *key, /*@null@*/const char *val)
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
	return (EOK);
}

err_t j_add_int(/*@null@*/json_t *root, /*@null@*/const char *key, json_int_t val)
{
	int rc;
	json_t *j_int;

	TESTP(root, EBAD);
	TESTP(key, EBAD);

	j_int = json_integer(val);
	TESTP(j_int, EBAD);

	rc = json_object_set_new(root, key, j_int);
	TESTI_MES(rc, EBAD, "Can't set new string to json object");
	return (EOK);
}


err_t j_cp(/*@null@*/const json_t *from, /*@null@*/json_t *to, /*@null@*/const char *key)
{
	json_t *j_obj = NULL;
	json_t *j_obj_copy = NULL;
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

err_t j_cp_val(/*@null@*/const json_t *from, /*@null@*/json_t *to, /*@null@*/const char *key_from, /*@null@*/const char *key_to)
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
err_t j_test(/*@null@*/const json_t *root, /*@null@*/const char *type_name, /*@null@*/const char *expected_val)
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

err_t j_test_key(/*@null@*/const json_t *root, /*@null@*/const char *key)
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

/*@temp@*//*@null@*/ json_t *j_find_j(/*@null@*/const json_t *root, /*@null@*/const char *key)
{
	TESTP(root, NULL);
	TESTP(key, NULL);
	return (json_object_get(root, key));
}

/*@temp@*//*@null@*/ const char *j_find_ref(/*@null@*/const json_t *root, /*@null@*/const char *key)
{
	json_t *j_obj;
	TESTP(root, NULL);
	TESTP(key, NULL);

	if (EOK != j_test_key(root, key)) {
		return (NULL);
	}

	j_obj = json_object_get(root, key);
	TESTP(j_obj, NULL);
	return (json_string_value(j_obj));
}

json_int_t j_find_int(/*@null@*/const json_t *root, /*@null@*/const char *key)
{
	/*@temp@*/json_t *j_obj;
	TESTP(root, 0XDEADBEEF);
	TESTP(key, 0XDEADBEEF);

	if (EOK != j_test_key(root, key)) {
		return (0XDEADBEEF);
	}

	j_obj = json_object_get(root, key);
	TESTP(j_obj, 0XDEADBEEF);
	return (json_integer_value(j_obj));
}

/*@null@*//*@only@*/char *j_find_dup(/*@null@*/const json_t *root, /*@null@*/const char *key)
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

err_t j_count(/*@null@*/const json_t *root)
{
	TESTP(root, EBAD);
	return (json_object_size(root));
}

err_t j_replace(/*@null@*/json_t *root, /*@null@*/const char *key, /*@null@*/json_t *j_new)
{
	TESTP(root, EBAD);
	TESTP(key, EBAD);
	TESTP(j_new, EBAD);
	return (json_object_set_new(root, key, j_new));
}

/*@null@*//*@only@*/ json_t *j_dup(/*@null@*/const json_t *root)
{
	TESTP(root, NULL); 
	return (json_deep_copy(root));
}

err_t j_rm_key(/*@null@*/json_t *root, /*@null@*/const char *key)
{
	TESTP(root, EBAD);
	TESTP(key, EBAD);
	return (json_object_del(root, key));
}

void j_rm(/*@keep@*//*@null@*/json_t *root)
{
	const char *key = NULL;
	void *tmp = NULL;
	json_t *value = NULL;

	if (NULL == root) return;

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
}

void j_print(/*@null@*/const json_t *root, /*@null@*/const char *prefix)
{

	buf_t *buf;
	if (NULL == root || NULL == prefix) {
		DE("NULL arg\n");
		return;
	}

	buf = j_2buf(root);
	if (NULL == buf) {
		DE("Got NULL\n");
		return;
	}

	if (prefix) D("%s :\n", prefix);
	printf("%s\n", buf->data);
	if (EOK != buf_free(buf)) {
		DE("Can't remove buf_t: probably passed NULL pointer?\n");
	}
}

