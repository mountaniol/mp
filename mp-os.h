#ifndef MP_OS_H
#define MP_OS_H

extern char *mp_os_get_hostname(void);
extern char *mp_os_rand_string(size_t size);
extern int mp_os_random_in_range(int lower, int upper);
char *mp_os_generate_uid(const char *name);
#endif /* MP_OS_H */
