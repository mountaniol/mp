#ifndef _SEC_BUILD_REQUESTS_H_
#define _SEC_BUILD_REQUESTS_H_

extern /*@null@*/ buf_t *mp_requests_build_connect(const char *uid, const char *name);
extern /*@null@*/ buf_t *mp_requests_build_last_will(void);
extern /*@null@*/ buf_t *mp_requests_build_reveal(void);
extern /*@null@*/ buf_t *mp_requests_build_ssh(const char *uid);
extern /*@null@*/ buf_t *mp_requests_build_ssh_done(const char *uid, const char *ip, const char *port);
extern /*@null@*/ buf_t *mp_requests_build_sshr(const char *uid, const char *ip, const char *port);
extern /*@null@*/ buf_t *mp_requests_build_sshr_done(const char *uid, const char *localport, const char *status);
extern /*@null@*/ buf_t *mp_requests_build_keepalive(void);
extern /*@null@*/ buf_t *mp_requests_open_port(const char *uid, const char *port, const char *protocol);
extern /*@null@*/ buf_t *mp_requests_close_port(const char *uid, const char *port, const char *protocol);


#endif /* _SEC_BUILD_REQUESTS_H_ */
