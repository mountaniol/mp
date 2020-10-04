#ifndef MP_OS_H
#define MP_OS_H

extern /*@null@*/ char *mp_os_get_hostname(void);
extern /*@null@*/ char *mp_os_rand_string(size_t size);
extern int mp_os_random_in_range(int lower, int upper);
extern /*@null@*/ char *mp_os_generate_uid(const char *name);
extern err_t mp_os_usleep(int milliseconds);
extern err_t mp_os_test_if_file_exists(const char *file);

/**
 * @author Sebastian Mountaniol (09/06/2020)
 * @func int mp_os_open(const char *file, int flags, mode_t mode)
 * @brief Secure open regular file: test that it is not a link or whatever
 *
 * @param char   * file Filename
 * @param int    flags Flags for open()
 * @param mode_t mode Mode for open()
 *
 * @return int File descriptor on success, < 0 on an error 
 * @details
 *
 */
int mp_os_open_regular(const char *file, int flags, mode_t mode);

/**
 * @author Sebastian Mountaniol (09/06/2020)
 * @func FILE* mp_os_fopen(const char *file, const char *mode)
 * @brief Secure open a regular file, and only regular file 
 *
 * @param char  * file Filename to open
 * @param const char* mode Mode to pass to fopen()
 *
 * @return FILE* File descriptor pointer on success, NULL on failure
 * @details
 *
 */
FILE *mp_os_fopen_regular(const char *file, const char *mode);

/**
 * @author se (04/09/2020)
 * @brief Secure open a character device
 * @param file Name of character device to open
 * @param mode Opening mode, like "fopen" function accepts
 * @return FILE* Pointer to opened file descriptor; NULL on
 *  	   error
 */
FILE *mp_os_fopen_chardev(const char *file, const char *mode);

/**
 * @author Sebastian Mountaniol (21/08/2020)
 * @func int mp_os_fill_random(void *buf, size_t buf_size)
 * @brief Fille given buffer with random noise
 * @param void * buf Buffer to fill with random data
 * @param size_t buf_size Buffer size
 * @return int EOK on success, EBAD on error
 * @details
 */
int mp_os_fill_random(void *buf, size_t buf_size);
#endif /* MP_OS_H */
