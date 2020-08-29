#ifndef MP_MQTT_H_
#define MP_MQTT_H_

err_t mp_mqtt_ticket_responce(const j_t *req, const char *status, const char *comment);
/*@null@*/ void *mp_mqtt_mosq_threads_manager_pthread(void *arg);
int mp_mqtt_start_app(void *cert);

#endif /* MP_MQTT_H_ */
