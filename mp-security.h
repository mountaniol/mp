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
#endif /* MP_SECURITY_H */
