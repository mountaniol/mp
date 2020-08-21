#ifndef MP_SECURITY_H
#define MP_SECURITY_H

/**
 * @author Sebastian Mountaniol (09/06/2020)
 * @func buf_t* mp_security_sha256_string(buf_t *buf)
 * @brief Calculate sha256 digest of the string
 *
 * @param buf_t * buf String to calculate digest for
 *
 * @return buf_t* Calculated sha256 digest; NULL on error
 * @details R
 *
 */
/*@null@*/ buf_t *mp_security_sha256_string(/*@null@*/buf_t *buf);


/**
 * @author Sebastian Mountaniol (09/06/2020)
 * @func buf_t* mp_security_system_footprint()
 * @brief Gather system information for sha256 digest
 * @param NO
 *
 * @return buf_t* String containing system information
 * @details
 *
 */
/*@null@*/buf_t *mp_security_system_footprint();

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func RSA* mp_security_generate_rsa_pem_RSA(const int kbits)
 * @brief Generate RDA key of 'kbits' lengh
 * @param const int kbits
 * @return RSA*
 * @details
 */
RSA *mp_security_generate_rsa_pem_RSA(const int kbits);

/**
 * @author Sebastian Mountaniol (14/07/2020)
 * @func X509* mp_security_generate_x509(RSA *rsa)
 * @brief Generate X509 certificate using RSA private key
 * @param RSA * rsa
 * @return X509*
 * @details
 */
/*@null@*/X509 *mp_security_generate_x509(RSA *rsa);


/**
 * @author Sebastian Mountaniol (21/08/2020)
 * @func SSL_CTX* mp_security_init_server_tls_ctx(void)
 * @brief Create CTX context
 * @param void
 * @return SSL_CTX* Created context
 * @details
 */
/*@null@*/ SSL_CTX *mp_security_init_server_tls_ctx(void);
#endif /* MP_SECURITY_H */
