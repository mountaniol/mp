#ifndef _SEC_CLIENT_MOSQ_CONFIG_H_
#define _SEC_CLIENT_MOSQ_CONFIG_H_
#include <jansson.h>

#include "mp-common.h"

/* If these names changed care about name lenghts as well*/
#define CONFIG_DIR_NAME ".mightypapa"
#define CONFIG_DIR_NAME_LEN (sizeof(CONFIG_DIR_NAME) - 1)

/* Config file name */
#define CONFIG_FILE_NAME "config.txt"
#define CONFIG_FILE_NAME_LEN (sizeof(CONFIG_FILE_NAME) - 1)

/* RSA private key */
#define RSA_PRIVATE_NAME "rsa.priv.pem"
#define RSA_PRIVATE_NAME_LEN (sizeof(RSA_PRIVATE_NAME) - 1)

/* RSA public key */
#define RSA_PUBLIC_NAME "rsa.pub.pem"
#define RSA_PUBLIC_NAME_LEN (sizeof(RSA_PUBLIC_NAME) - 1)

/* X509 certificate file name */
#define X509_CERT_NAME "x509.cert"
#define X509_CERT_NAME_LEN (sizeof(X509_CERT_NAME) - 1)

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
 * 	 new config object from content of ctl->me and saves it
 * 	 to config file
 * @func int mp_config_from_ctl(void *ctl)
 * @author se (06/05/2020)
 *
 * @param ctl
 *
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_from_ctl_l(void);

/**
 * @brief Load config from file, decode it into object and
 * 	 return this object. It also inits 'ctl->config' object
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
 * 	 into config file. If such a key already exists, it
 * 	 replaces the existing 'val' with new one
 * @func int mp_config_set(void *_ctl, const char *key, const char *val)
 * @author se (06/05/2020)
 *
 * @param _ctl
 * @param key
 * @param val  EOK on success, EBAD on error
 *
 * @return int EOK on success, EBAD on error
 */
extern err_t mp_config_set(const char *key, const char *val);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func int mp_config_save_rsa_keys(RSA *rsa)
 * @brief Save private and public RSA keys in pem format
 * @param RSA * rsa
 * @return int
 * @details
 */
extern int mp_config_save_rsa_keys(RSA *rsa);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func err_t mp_config_save_rsa_x509(X509 *x509)
 * @brief Save generated X509 certificate
 * @param X509 * x509
 * @return err_t
 * @details
 */
extern err_t mp_config_save_rsa_x509(X509 *x509);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func err_t mp_config_probe_x509(void)
 * @brief Probe is X509 certificate file exists
 * @param void
 * @return err_t
 * @details
 */
extern err_t mp_config_probe_x509(void);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func X509* mp_config_load_X509(void)
 * @brief Load X509 certificate 
 * @param void
 * @return X509*
 * @details
 */
extern X509 *mp_config_load_X509(void);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func RSA* mp_config_load_rsa_pub(void)
 * @brief Load RSA public key
 * @param void
 * @return RSA*
 * @details
 */
extern RSA *mp_config_load_rsa_pub(void);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func err_t mp_config_probe_rsa_priv(void)
 * @brief Probe if RSA private key file exists
 * @param void
 * @return err_t
 * @details
 */
err_t mp_config_probe_rsa_priv(void);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func RSA* mp_config_load_rsa_priv(void)
 * @brief Load RSA private key
 * @param void
 * @return RSA*
 * @details
 */
extern RSA *mp_config_load_rsa_priv(void);

#endif /* _SEC_CLIENT_MOSQ_CONFIG_H_ */
