#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern err_t send_keepalive_l(void);
extern err_t send_reveal_l(void);
extern err_t mp_communicate_send_request(/*@temp@*/j_t *root);
extern /*@null@*/ buf_t *mp_communicate_forum_topic(void);
extern /*@null@*/ buf_t *mp_communicate_forum_topic_all(void);
extern /*@null@*/ buf_t *mp_communicate_private_topic(void);
extern err_t mp_communicate_clean_missed_counters_hash(void);
extern /*@null@*/ buf_t *mp_communicate_get_buf_t_from_hash(int counter); 
extern err_t mp_communicate_mosquitto_publish(/*@temp@*/const char *topic, /*@temp@*/buf_t *buf);
extern err_t mp_communicate_send_json(/*@temp@*/const char *forum_topic, /*@temp@*/j_t *root);
#endif /* MP_COMMUNICATE_H */
