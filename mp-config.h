#ifndef _SEC_CLIENT_MOSQ_CONFIG_H_
#define _SEC_CLIENT_MOSQ_CONFIG_H_
#include <jansson.h>

#define CONFIG_DIR_NAME ".mightypapa"
#define CONFIG_FILE_NAME "config"

/**
 * @brief Save ctl->config into config file
 * @func int mp_config_save(void *_ctl)
 * @author se (06/05/2020)
 * 
 * @param _ctl Pointer to control_t structure
 * 
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_save(void);

/**
 * @brief Used in the first run, when no config file. Creates 
 *  	  new config object from content of ctl->me and saves it
 *  	  to config file
 * @func int mp_config_from_ctl(void *ctl)
 * @author se (06/05/2020)
 * 
 * @param ctl 
 * 
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_from_ctl(void);

/**
 * @brief Load config from file, decode it into object and 
 *  	  return this object. It also inits 'ctl->config' object
 * @func int mp_config_load(void *ctl)
 * @author se (06/05/2020)
 * 
 * @param ctl 
 * 
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_load(void);

/**
 * @brief Adds new pair 'key' = 'val' into ctl->config and also 
 *  	  into config file. If such a key already exists, it
 *  	  replaces the existing 'val' with new one
 * @func int mp_config_set(void *_ctl, const char *key, const char *val)
 * @author se (06/05/2020)
 * 
 * @param _ctl 
 * @param key 
 * @param val EOK on success, EBAD on error
 * 
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_set(void *_ctl, /*@only@*/const char *key, /*@only@*/const char *val);
#endif /* _SEC_CLIENT_MOSQ_CONFIG_H_ */
