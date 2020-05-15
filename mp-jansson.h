#ifndef _SEC_SERVER_JANSSON_H_
#define _SEC_SERVER_JANSSON_H_

#include <jansson.h>
#include "buf_t.h"
//#include "sec-linkedlist.h"

/**
 * @brief Get string with JSON object, return json_t object
 * 
 * @author se (14/04/2020)
 * 
 * @param str Input string, must be 0 terminated
 * 
 * @return json_t* JSON objectm can return NULL if allocation failed
 */
/*@null@*//*@only@*/ json_t *j_str2j(const char *str);

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
/*@null@*//*@only@*/ json_t *j_strn2j(const char *str, size_t len);
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
/*@null@*//*@only@*/ json_t *j_buf2j(const buf_t *buf);

/**
 * @func buf_t* encode_json(const json_t *j_obj)
 * @brief Transform JSON object into text form
 * @author se (07/04/2020)
 * 
 * @param j_obj 
 * 
 * @return buf_t* 
 */
/*@null@*//*@only@*/ buf_t *j_2buf(const json_t *j_obj);

/**
 * @brief Allocate new json array
 * @author se (01/05/2020)
 * @param void 
 * @return void* 
 */
/*@null@*//*@only@*/ void *j_arr();

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
err_t j_arr_add(json_t *arr, json_t *obj);

/**
 * @brief Allocate new json object
 * 
 * @author se (01/05/2020)
 * 
 * @param void 
 * 
 * @return void* 
 */
/*@null@*//*@only@*/ void *j_new();


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
err_t j_add_j2arr(json_t *root, const char *arr_name, json_t *obj);

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
err_t j_add_j(json_t *root, const char *key, json_t *obj);

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
err_t j_add_str(json_t *root, const char *key, const char *val);

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
err_t j_add_int(json_t *root, const char *key, json_int_t val);

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
err_t j_cp(const json_t *from, json_t *to, const char *key);

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
err_t j_cp_val(const json_t *from, json_t *to, const char *key_from, const char *key_to);


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
/*@temp@*//*@null@*/ json_t *j_find_j(const json_t *root, const char *key);

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
/*@temp@*//*@null@*/ const char *j_find_ref(const json_t *root, const char *key);

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
json_int_t j_find_int(const json_t *root, const char *key);
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
/*@null@*//*@only@*/char *j_find_dup(const json_t *root, const char *key);

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
err_t j_test_key(const json_t *root, const char *key);

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
err_t j_test(const json_t *root, const char *type_name, const char *expected_val);

/**
 * @func int j_count(const json_t *root);
 * @brief Count number of elements in 'root' json object
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */

err_t j_count(const json_t *root);

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
err_t j_replace(json_t *root, const char *key, json_t *j_new);

/**
 * @func json_t *j_dup(const json_t *root)
 * @brief Duplicate JSON object and all it includes
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
/*@null@*//*@only@*/ json_t *j_dup(const json_t *root);

/**
 * @func int j_rm_key(hson_t *root, const char *key)
 * @brief Remove key from 'root'
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
err_t j_rm_key(json_t *root, const char *key);

/**
 * @func int json_remove_it(json_t *root)
 * @brief Remove JSON object completely
 * @author se (07/04/2020)
 * 
 * @param root 
 * 
 * @return int 
 */
err_t j_rm(/*@only@*/json_t *root);

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
void j_print(const json_t *root, const char *prefix);
#endif /* _SEC_SERVER_JANSSON_H_ */
