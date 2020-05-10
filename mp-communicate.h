#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern int send_keepalive_l(struct mosquitto *mosq);
extern int send_reveal_l(struct mosquitto *mosq);
extern int mp_communicate_send_request(struct mosquitto *mosq, json_t *root);
extern int send_request_to_open_port(struct mosquitto *mosq, json_t *root);
extern int send_request_to_close_port(struct mosquitto *mosq, char *target_uid, char *port, char *protocol);
extern char *mp_communicate_forum_topic(const char *user, const char *uid);
extern char *mp_communicate_private_topic(const char *user, const char *uid);
extern buf_t *mp_communicate_get_buf_t_from_ctl_l(int counter);
extern int mp_communicate_mosquitto_publish(struct mosquitto *mosq, const char *topic, buf_t *buf);
extern int mp_communicate_send_json(struct mosquitto *mosq, const char *forum_topic, json_t *root);
extern int send_request_return_tickets(struct mosquitto *mosq, json_t *root);
#endif /* MP_COMMUNICATE_H */
