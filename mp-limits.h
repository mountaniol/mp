#ifndef MP_LIMITS_H
#define MP_LIMITS_H

/* Max user name length */
#define MP_LIMIT_USER_MAX 64
/* Max uid length = 'user_name-123-456-789' */
#define MP_LIMIT_UID_MAX (MP_LIMIT_USER_MAX + 12)

/* Max buffer size for tcp channel */
#define MP_LIMIT_TUNNEL_BUF_SIZE (4096*128)

/* Maximal topic string length */
#define TOPIC_MAX_LEN 1024

/* Maximal length of string in buf_sprintf() function */
#define MP_LIMIT_BUF_STRING_LEN (4096*16) 

#endif /* MP_LIMITS_H */
