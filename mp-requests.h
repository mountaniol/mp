#ifndef _SEC_BUILD_REQUESTS_H_
#define _SEC_BUILD_REQUESTS_H_

extern buf_t *mp_requests_build_connect(const char *uid, const char *name);
extern buf_t *mp_requests_build_last_will(void);
extern buf_t *mp_requests_build_reveal(void);
extern buf_t *mp_requests_build_ssh(const char *uid);
extern buf_t *mp_requests_build_ssh_done(const char *uid, const char *ip, const char *port);
extern buf_t *mp_requests_build_sshr(const char *uid, const char *ip, const char *port);
extern buf_t *mp_requests_build_sshr_done(const char *uid, const char *localport, const char *status);
extern buf_t *mp_requests_build_keepalive(void);
extern buf_t *mp_requests_open_port(const char *uid, const char *port, const char *protocol);
extern buf_t *mp_requests_close_port(const char *uid, const char *port, const char *protocol);


#endif /* _SEC_BUILD_REQUESTS_H_ */
