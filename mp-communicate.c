/*@-skipposixheaders@*/
#include <string.h>
/*@=skipposixheaders@*/

#include "mosquitto.h"
#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-main.h"
#include "mp-requests.h"
#include "mp-jansson.h"
#include "mp-dict.h"

int send_keepalive_l(struct mosquitto *mosq)
{
	control_t *ctl = NULL;
	char forum_topic[TOPIC_MAX_LEN];
	buf_t *buf = NULL;
	int rc = EBAD;

	memset(forum_topic, 0, TOPIC_MAX_LEN);

	ctl = ctl_get();
	ctl_lock(ctl);
	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID));

	buf = mp_requests_build_keepalive();
	ctl_unlock(ctl);
	if (NULL == buf) {
		DE("can't build notification\n");
		return (EBAD);
	}

	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	if (MOSQ_ERR_SUCCESS == rc) {
		rc = EOK;
		goto end;
	}

	DE("Failed to send notification\n");
	rc = mosquitto_reconnect(mosq);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to reconnect\n");
		rc = EBAD;
		goto end;
	}

	DD("Reconnected\n");

	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send notification\n");
		rc = EBAD;
	}

end:
	buf_free_force(buf);
	return (rc);
}


int send_reveal_l(struct mosquitto *mosq)
{
	control_t *ctl = NULL;
	char forum_topic[TOPIC_MAX_LEN];
	buf_t *buf = NULL;
	int rc = EBAD;

	memset(forum_topic, 0, TOPIC_MAX_LEN);

	ctl = ctl_get_locked();
	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s", 
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID));

	buf = mp_requests_build_reveal(j_find_ref(ctl->me, JK_UID),
									j_find_ref(ctl->me, JK_NAME));
	ctl_unlock(ctl);

	TESTP_MES(buf, EBAD, "Can't build notification");

	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send reveal request\n");
		return (EBAD);
	}

	buf_free_force(buf);
	return (EOK);
}

int send_request_to_open_port(struct mosquitto *mosq, json_t *root)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char forum_topic[TOPIC_MAX_LEN];
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);

	ctl = ctl_get();

	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID));

	DDD("Going to build request\n");
	buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	buf_free_force(buf);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_to_open_port_old(struct mosquitto *mosq, char *target_uid, char *port, char *protocol)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char forum_topic[TOPIC_MAX_LEN];
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);
	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	ctl = ctl_get();

	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID));

	DDD("Going to build request\n");
	buf = mp_requests_open_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_to_close_port(struct mosquitto *mosq, char *target_uid, char *port, char *protocol)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char forum_topic[TOPIC_MAX_LEN];
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);
	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	ctl = ctl_get();

	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID));

	DDD("Going to build request\n");
	buf = mp_requests_close_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mosquitto_publish(mosq, 0, forum_topic, (int)buf->size, buf->data, 0, false);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

