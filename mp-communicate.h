#ifndef MP_COMMUNICATE_H
#define MP_COMMUNICATE_H

extern int send_keepalive_l(struct mosquitto *mosq);
extern int send_reveal_l(struct mosquitto *mosq);
extern int send_request_to_open_port(struct mosquitto *mosq, json_t *root);
extern int send_request_to_close_port(struct mosquitto *mosq, char *target_uid, char *port, char *protocol);

#endif /* MP_COMMUNICATE_H */
