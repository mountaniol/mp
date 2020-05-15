#ifndef MP_OS_H
#define MP_OS_H

extern /*@null@*/ char *mp_os_get_hostname(void);
extern /*@null@*/ char *mp_os_rand_string(size_t size);
extern int mp_os_random_in_range(int lower, int upper);
extern /*@null@*/ char *mp_os_generate_uid(const char *name);
extern err_t mp_os_usleep(int milliseconds);
#endif /* MP_OS_H */
