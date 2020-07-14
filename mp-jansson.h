#ifndef _SEC_SERVER_JANSSON_H_
#define _SEC_SERVER_JANSSON_H_

#include "mp-common.h"
#include "buf_t/buf_t.h"

/* Use it instead of json_int_t, to remove dependency of jansson.h */
typedef long long int j_int_t;
typedef void j_t;
/**
 * @brief Get string with JSON object, return json_t object
 * 
 * @author se (14/04/2020)
 * 
 * @param str Input string, must be 0 terminated
 * 
 * @return json_t* JSON objectm can return NULL if allocation failed
 */
/*@null@*//*@only@*/ j_t *j_str2j(/*@null@*/const char *str);

/**
 * @brief Convert string 'str' of length 'len' into json object
 * @func json_t* j_strn2j(const char *str, size_t len)
 * @author se (14/05/2020)
 * 
 * @param str Input string, not must be not 0 terminated
 * @param len String length
 * 
 * @return json_t* JSON object, can return NULL  if allocation 
 *  	   failed
 */
/*@null@*//*@only@*/ j_t *j_strn2j(/*@null@*/const char *str, size_t len);
/**
 * @brief Get buf_t containing JSON in text form. Creates and 
 *  	  returns JSON object
 * 
 * @author se (06/04/2020)
 * 
 * @param buf 
 * 
 * @return json_t* 
 */
/*@null@*//*@only@*/ j_t *j_buf2j(/*@null@*/const buf_t *buf);

/**
 * @func buf_t* encode_json(const json_t *j_obj)
 * @brief Transform JSON object into text form
 * @author se (07/04/2020)
 * 
 * @param j_obj 
 * 
 * @return buf_t* 
 */
/*@null@*//*@only@*/ buf_t *j_2buf(/*@null@*/const j_t *j_obj);

/**
 * @brief Allocate new json array
 * @author se (01/05/2020)
 * @param void 
 * @return void* 
 */
/*@null@*//*@only@*/ j_t *j_arr();


/**
 * @brief Get arriy size (how many elements in the array)
 * @author se (26/05/2020)
 * @param const j_t *arr Pointer to array object
 * @return size_t Array size
 */
size_t j_arr_size(const j_t *arr);

/**
 * @brief Get arriy element by index
 * @author se (26/05/2020)
 * @param const j_t *arr Pointer to array object
 * @return size_t Array size
 */
/*@null@*/j_t *j_arr_get(/*@null@*/const j_t *arr, size_t index);
/**
 *  
 * @brief Add new object into array
 * @func void* j_arr_add(json_t *arr, json_t *obj)
 * @author se (04/05/2020)
 * 
 * @param arr 
 * @param obj 
 * 
 * @return int EOK on success 
 */
err_t j_arr_add(/*@null@*/j_t *arr, /*@null@*/j_t *obj);

/**
 * @brief Allocate new json object
 * 
 * @author se (01/05/2020)
 * 
 * @param void 
 * 
 * @return void* 
 */
/*@null@*//*@only@*/ j_t *j_new();


/**
 * @brief json 'array' included into json 'root'. Add new 'obj' 
 *  	  element into this 'arr'
 * @func int j_add_j2arr(json_t *root, const char *arr_name, 
 *  	 json_t *obj)
 * @author se (05/05/2020)
 * 
 * @param root 
 * @param arr_name 
 * @param obj 
 * 
 * @return int EOK on success, EBAD on failure
 */
err_t j_add_j2arr(/*@null@*/j_t *root, /*@null@*/const char *arr_name, /*@null@*/j_t *obj);

/**
 * @brief Add json 'obj' into json 'root' by key 'key'
 * @func int j_add_j(json_t *root, char *key, json_t *obj)
 * @author se (05/05/2020)
 * 
 * @param root json object to add to
 * @param key key
 * @param obj json object to be added
 * 
 * @return int EOK on success, EBAD on failure
 */
err_t j_add_j(/*@null@*/j_t *root, /*@null@*/const char *key, /*@null@*/j_t *obj);

/**
 * @func int json_add_string_string(json_t *root, char *key, char *val)
 * @brief Add into JSON object string "val" for key "key"
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param key 
 * @param val 
 * 
 * @return int 
 */
err_t j_add_str(/*@null@*/j_t *root, /*@null@*/const char *key, const char *val);

/**
 * @brief Add integer value to 'root' by key 'key'
 * @func int j_add_int(json_t *root, const char *key, long val)
 * @author se (07/05/2020)
 * 
 * @param root json object
 * @param key ket to use
 * @param val json_int_t integer
 * 
 * @return int EOK on success, EBAD on failure
 */
err_t j_add_int(/*@null@*/j_t *root, /*@null@*/const char *key, j_int_t val);

#if 0
/**
 * @func int json_add_string_int(json_t *root, char *key, int val)
 * @brief Add into JSON object integer "val" for key "key"
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param key 
 * @param val 
 * 
 * @return int 
 */
int json_add_string_int(json_t *root, char *key, int val);
#endif


/**
 * @brief Copy one field from 'from' to 'to' by key 'key'
 * 
 * @author se (01/05/2020)
 * 
 * @param from 
 * @param to 
 * @param key 
 * 
 * @return int 
 */
err_t j_cp(/*@null@*/const j_t *from, /*@null@*/j_t *to, /*@null@*/const char *key);

/**
 * @brief Copy string value from 'from' to 'to'. THe value found 
 *  	  by key 'key_from' and inserted by 'key_to'
 * 
 * @author se (01/05/2020)
 * 
 * @param from 
 * @param to 
 * @param key 
 * 
 * @return int 
 */
err_t j_cp_val(/*@null@*/const j_t *from, /*@null@*/j_t *to, /*@null@*/const char *key_from, /*@null@*/const char *key_to);


/**
 * @brief Find json in json by key
 * @func json_t* j_find_j(json_t *root, const char *key)
 * @author se (05/05/2020)
 * 
 * @param root 
 * @param key 
 * 
 * @return json_t* on success, NULL otherwise
 */
/*@temp@*//*@null@*/ j_t *j_find_j(/*@null@*/const j_t *root, /*@null@*/const char *key);

/**
 * @func char *j_find_dup(json_t *root, const char *key)
 * @brief Return reference to value for key "key". The reference
 *  	  should not be freed.
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param key 
 * 
 * @return char* 
 */
/*@temp@*//*@null@*/ const char *j_find_ref(/*@null@*/const j_t *root, /*@null@*/const char *key);

/**
 * @brief Find long value saved by 'key'
 * @func long j_find_int(json_t *root, const char *key)
 * @author se (07/05/2020)
 * 
 * @param root json object containing value
 * @param key key to search
 * 
 * @return json_int_t value if found, 0XDEADBEEF on error
 */
j_int_t j_find_int(/*@null@*/const j_t *root, /*@null@*/const char *key);
/**
 * @func char *j_find_dup(json_t *root, const char *key)
 * @brief Extract from JSON object string value for key "key"
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param key 
 * 
 * @return char* 
 */
/*@null@*//*@only@*/char *j_find_dup(/*@null@*/const j_t *root, /*@null@*/const char *key);

/**
 * @brief Test that given key exists
 * 
 * @author se (30/04/2020)
 * 
 * @param root 
 * @param key 
 * 
 * @return int EOK if exists, EBAF if not 
 */
err_t j_test_key(/*@null@*/const j_t *root, /*@null@*/const char *key);

/**
 * @func int json_validate_field(json_t *root, const char *type_name, const char *expected_val)
 * @brief Validate that key "type_name" exist in JSON and value 
 *  	  for this key is "expected_val"
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param type_name 
 * @param expected_val 
 * 
 * @return int 
 */
err_t j_test(/*@null@*/const j_t *root, /*@null@*/const char *type_name, /*@null@*/const char *expected_val);

/**
 * @func int j_count(const json_t *root);
 * @brief Count number of elements in 'root' json object
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */

err_t j_count(/*@null@*/const j_t *root);

/**
 * @func j_replace(json_t *root, const char *key, const json_t 
 *  	  j_new);
 * @brief Replace JSON object in 'root' with the new object 
 *  	  'j_new' for the key 'key'
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
err_t j_replace(/*@null@*/j_t *root, /*@null@*/const char *key, /*@null@*/j_t *j_new);

/**
 * @func json_t *j_dup(const json_t *root)
 * @brief Duplicate JSON object and all it includes
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
/*@null@*//*@only@*/ j_t *j_dup(/*@null@*/const j_t *root);

/**
 * @func int j_rm_key(hson_t *root, const char *key)
 * @brief Remove key from 'root'
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
err_t j_rm_key(/*@null@*/j_t *root, /*@null@*/const char *key);

/**
 * @func int json_remove_it(json_t *root)
 * @brief Remove JSON object completely
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return void 
 */
void j_rm(/*@keep@*//*@null@*/j_t *root);

/**
 * @func int j_arr_rm(j_t *arr, size_t index)
 * @brief Remove object from json array by index
 * @author se (26/05/2020)
 * 
 * @param j_t *arr Array to remove from
 * @param size_t index Index of element to remove in the array
 * @return in 0 on success 
 */

int j_arr_rm(j_t *arr, size_t index);

/**
 * @func int json_print_all(json_t *root, const char *prefix)
 * @brief Print content of JSON object. Before JSON dump 
 *  	  'prefix' printed
 * @author se (07/04/2020)
 * 
 * @param root 
 * @param prefix - will be printed before json dump 
 * 
 * @return int 
 */
void j_print(/*@null@*/const j_t *root, /*@null@*/const char *prefix);
#endif /* _SEC_SERVER_JANSSON_H_ */

/*** Self implementation of interators ***/

#define j_arr_foreach(array, index, value) \
	for(index = 0; \
		index < j_arr_size(array) && (value = j_arr_get(array, index)); \
		index++)
