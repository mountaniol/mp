#ifndef MP_LIMITS_H
#define MP_LIMITS_H

/* Max user name length */
#define MP_LIMIT_USER_MAX 64
/* Max uid length = 'user_name-123-456-789' */
#define MP_LIMIT_UID_MAX (MP_LIMIT_USER_MAX + 12)

/* Max buffer size for tcp channel */
#define MP_LIMIT_TUNNEL_BUF_SIZE_MAX (4096*128)
#define MP_LIMIT_TUNNEL_BUF_SIZE_MIN (16)

/* Maximal topic string length */
#define TOPIC_MAX_LEN 1024

/* Maximal length of string in buf_sprintf() function */
#define MP_LIMIT_BUF_STRING_LEN (4096*16) 

/* Maximal legth of interface name (like 'eth0') */
#define MP_LIMIT_INTERFACE_NAME_MAX (32)

/* Maximal length of JSON buffer length: 256 KB */
#define MP_LIMIT_JSON_TEXT_BUF_LEN (1024 * 256)

#endif /* MP_LIMITS_H */
