#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern int send_keepalive_l(/*@only@*/const struct mosquitto *mosq);
extern int send_reveal_l(/*@only@*/const struct mosquitto *mosq);
extern int mp_communicate_send_request(/*@only@*/const struct mosquitto *mosq, /*@only@*/const json_t *root);
extern int send_request_to_open_port(/*@only@*/const struct mosquitto *mosq, /*@only@*/const json_t *root);
extern int send_request_to_close_port(/*@only@*/const struct mosquitto *mosq, /*@only@*/const char *target_uid, /*@only@*/const char *port, /*@only@*/const char *protocol);
extern /*@null@*/ char *mp_communicate_forum_topic(/*@only@*/const char *user, /*@only@*/const char *uid);
extern /*@null@*/ char *mp_communicate_forum_topic_all(/*@only@*/const char *user);
extern /*@null@*/ char *mp_communicate_private_topic(/*@only@*/const char *user, /*@only@*/const char *uid);
extern /*@null@*/ char *mp_communicate_private_topic_all(/*@only@*/const char *user);
extern int mp_communicate_clean_missed_counters(void);
extern /*@null@*/ buf_t *mp_communicate_get_buf_t_from_ctl_l(int counter);
extern int mp_communicate_mosquitto_publish(/*@only@*/const struct mosquitto *mosq, /*@only@*/const char *topic, /*@only@*/buf_t *buf);
extern int mp_communicate_send_json(/*@only@*/const struct mosquitto *mosq, /*@only@*/const char *forum_topic, /*@only@*/json_t *root);
extern int send_request_return_tickets(/*@only@*/const struct mosquitto *mosq, /*@only@*/json_t *root);
#endif /* MP_COMMUNICATE_H */
