#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern int send_keepalive_l(struct mosquitto *mosq);
extern int send_reveal_l(struct mosquitto *mosq);
extern int send_request_to_open_port(struct mosquitto *mosq, json_t *root);
extern int send_request_to_close_port(struct mosquitto *mosq, char *target_uid, char *port, char *protocol);
extern buf_t *mp_communicate_get_buf_t_from_ctl(int counter);
extern int mp_communicate_mosquitto_publish(struct mosquitto *mosq, const char *topic, buf_t *buf);
extern int mp_communicate_send_json(struct mosquitto *mosq, const char *forum_topic, json_t *root);
#endif /* MP_COMMUNICATE_H */
