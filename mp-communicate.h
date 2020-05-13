#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern int send_keepalive_l(void);
extern int send_reveal_l(void);
extern int mp_communicate_send_request(/*@temp@*/const json_t* root);
extern int send_request_to_open_port(/*@temp@*/const json_t* root);
extern int send_request_to_close_port(/*@temp@*/const char* target_uid, /*@temp@*/const char* port, /*@temp@*/const char* protocol);
extern /*@null@*/ char *mp_communicate_forum_topic(void);
extern /*@null@*/ char *mp_communicate_forum_topic_all(void);
extern /*@null@*/ char *mp_communicate_private_topic(void);
extern /*@null@*/ char *mp_communicate_private_topic_all(void);
extern int mp_communicate_clean_missed_counters(void);
extern /*@null@*/ buf_t *mp_communicate_get_buf_t_from_ctl_l(int counter);
extern int mp_communicate_mosquitto_publish(/*@temp@*/const char *topic, /*@temp@*/buf_t *buf);
extern int mp_communicate_send_json(/*@temp@*/const char *forum_topic, /*@temp@*/json_t *root);
extern int send_request_return_tickets(/*@temp@*/json_t *root);
#endif /* MP_COMMUNICATE_H */
