/*@-skipposixheaders@*/
#include <string.h>
/*@=skipposixheaders@*/

#include "mosquitto.h"
#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-main.h"
#include "mp-memory.h"
#include "mp-requests.h"
#include "mp-jansson.h"
#include "mp-dict.h"

/*@null@*/ char *mp_communicate_forum_topic(const char *user, const char
											*uid)
{
	char *topic = zmalloc(TOPIC_MAX_LEN);
	TESTP(topic, NULL);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/forum/%s", user, uid);
	return (topic);
}

/*@null@*/ char *mp_communicate_forum_topic_all(const char *user)
{
	char *topic = zmalloc(TOPIC_MAX_LEN);
	TESTP(topic, NULL);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/forum/#", user);
	return (topic);
}

/*@null@*/ char *mp_communicate_private_topic(const char *user, const char *uid)
{
	char *topic = zmalloc(TOPIC_MAX_LEN);
	TESTP(topic, NULL);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/personal/%s", user, uid);
	return (topic);
}

/*@null@*/ char *mp_communicate_private_topic_all(const char *user)
{
	char *topic = zmalloc(TOPIC_MAX_LEN);
	TESTP(topic, NULL);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/personal/#", user);
	return (topic);
}


/* Find a buffer in ctl->buffers by vounter 'counter' */
int mp_communicate_clean_missed_counters(void)
{
	control_t *ctl;
	void *tmp;
	const char *key;
	json_t *val;

	ctl = ctl_get_locked();
	if (j_count(ctl->buf_missed) < 1) {
		DD("No missed counters\n");
		ctl_unlock(ctl);
		return (EOK);
	}

	json_object_foreach_safe(ctl->buf_missed, tmp, key, val) {
		buf_t *buf;
		size_t ret;

		ret = j_find_int(ctl->buffers, key);
		if (0XDEADBEEF == ret) {
			DE("Something wrong: can't find value for key %s\n", key);
			continue;
		}

		buf = (buf_t *)ret;
		buf_free_force(buf);
		DD("Found missed key, removing: %s\n", key);
		j_rm_key(ctl->buf_missed, key);
		j_rm_key(ctl->buffers, key);
	}
	ctl_unlock(ctl);

	return (EOK);
}

/* Find a buffer in ctl->buffers by vounter 'counter' */
/*@null@*/ buf_t *mp_communicate_get_buf_t_from_ctl_l(int counter)
{
	buf_t *buf_p;
	size_t ret;
	char *buf_counter_s;
	control_t *ctl;
	int rc;

	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (NULL);
	}

	buf_counter_s = (char *)zmalloc(32);
	TESTP(buf_counter_s, NULL);

	/* Transform counter to key (string) */
	snprintf(buf_counter_s, 32, "%d", counter);

	DD("Counter string = %s\n", buf_counter_s);

	ctl = ctl_get_locked();
	ret = j_find_int(ctl->buffers, buf_counter_s);
	ctl_unlock(ctl);

	if (0XDEADBEEF == ret) {
		DE("Can't get buffer\n");

		/* We can't get buffer it probably not set yet.
		   We should save this counter and try it later. */

		ctl_lock(ctl);
		j_add_int(ctl->buf_missed, buf_counter_s, counter);
		ctl_unlock(ctl);
		free(buf_counter_s);
		DD("Added to missed counters: %d\n", counter);
		j_print(ctl->buf_missed, "Now in missed  counters:");
		j_print(ctl->buffers, "Now in buffers counters:");
		return (NULL);
	}

	//DD("Got ret: %ld / %lx\n", ret, ret);
	buf_p = (buf_t *)ret;
	ctl_lock(ctl);
	rc = j_rm_key(ctl->buffers, buf_counter_s);
	ctl_unlock(ctl);
	if (EOK != rc) {
		DE("Can't remove key from json: ctl->buffers, buf_counter_s");
	}
	free(buf_counter_s);
	return (buf_p);
}


/* Save 'buf' ponter by key 'counter' in ctl->buffers.
   Used later in callback function mp_main_on_publish_cb
   to release the buf when mosq sent it */
int mp_communicate_save_buf_t_to_ctl(buf_t *buf, int counter)
{
	char *buf_counter_s;
	control_t *ctl;
	int rc;

	TESTP(buf, EBAD);
	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (EBAD);
	}

	DD("Got counter = %d\n", counter);

	buf_counter_s = (char *)zmalloc(32);
	TESTP(buf_counter_s, EBAD);

	/* Transfor counter to key (string) */
	snprintf(buf_counter_s, 32, "%d", counter);

	ctl = ctl_get_locked();
	rc = j_add_int(ctl->buffers, buf_counter_s, (size_t)buf);
	ctl_unlock(ctl);
	TESTI_MES(rc, EBAD, "Can't add int to json: buf_counter_s, (size_t) buf\n");
	free(buf_counter_s);

	return (EOK);
}

int mp_communicate_mosquitto_publish(struct mosquitto *mosq, const char *topic, buf_t *buf)
{
	int rc;
	int rc2;
	int counter = -1;
	rc = mosquitto_publish(mosq, &counter, topic, (int)buf->size, buf->data, 0, false);
	rc2 = mp_communicate_save_buf_t_to_ctl(buf, counter);
	if (EOK != rc2) {
		DE("Can't save buf_t to ctl\n");
	} else {
		control_t *ctl = ctl_get();
		j_print(ctl->buffers, "ctl->buffers: ");
	}

	return (rc);
}

int mp_communicate_send_json(struct mosquitto *mosq, const char *forum_topic, json_t *root)
{
	buf_t *buf;

	TESTP(mosq, EBAD);
	TESTP(forum_topic, EBAD);
	TESTP(root, EBAD);

	buf = j_2buf(root);
	/* We must save this buffer; we will free it later, in mp_main_on_publish_cb() */
	TESTP(buf, EBAD);

	return (mp_communicate_mosquitto_publish(mosq, forum_topic, buf));
}

int send_keepalive_l(struct mosquitto *mosq)
{
	control_t *ctl = NULL;
	//char forum_topic[TOPIC_MAX_LEN];
	char *forum_topic;
	buf_t *buf = NULL;
	int rc = EBAD;

	ctl = ctl_get();
	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());

	buf = mp_requests_build_keepalive();
	ctl_unlock(ctl);
	if (NULL == buf) {
		DE("can't build notification\n");
		return (EBAD);
	}

	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
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

	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send notification\n");
		rc = EBAD;
	}

end:
	TFREE(forum_topic);
	return (rc);
}

int send_reveal_l(struct mosquitto *mosq)
{
	control_t *ctl = NULL;
	char *forum_topic;
	buf_t *buf = NULL;
	int rc = EBAD;

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());
	TESTP(forum_topic, EBAD);

	buf = mp_requests_build_reveal();

	TESTP_MES(buf, EBAD, "Can't build notification");

	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	free(forum_topic);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send reveal request\n");
		return (EBAD);
	}

	return (EOK);
}

int mp_communicate_send_request(struct mosquitto *mosq, json_t *root)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());

	DDD("Going to build request\n");
	buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	//DDD("Going to send request\n");
	j_print(root, "Sending requiest:");
	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_to_open_port(struct mosquitto *mosq, json_t *root)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());

	DDD("Going to build request\n");
	buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	//DDD("Going to send request\n");
	j_print(root, "Sending requiest:");
	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_to_open_port_old(struct mosquitto *mosq, char *target_uid, char *port, char *protocol)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);
	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());

	DDD("Going to build request\n");
	buf = mp_requests_open_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_to_close_port(struct mosquitto *mosq, char *target_uid, char *port, char *protocol)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;
	control_t *ctl = NULL;

	TESTP(mosq, EBAD);
	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	ctl = ctl_get();
	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());


	DDD("Going to build request\n");
	buf = mp_requests_close_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_mosquitto_publish(mosq, forum_topic, buf);
	free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

int send_request_return_tickets(struct mosquitto *mosq, json_t *root)
{
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;
	control_t *ctl = NULL;
	json_t *resp = NULL;
	size_t index;
	json_t *val = NULL;
	const char *ticket = NULL;
	const char *target_uid = NULL;

	TESTP(mosq, EBAD);

	ticket = j_find_ref(root, JK_TICKET);
	TESTP(ticket, EBAD);
	target_uid = j_find_ref(root, JK_UID_SRC);
	TESTP(target_uid, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic(ctl_user_get(), ctl_uid_get());

	if (j_count(ctl->tickets_out) == 1) {
		DD("No tickets to send\n");
		return (EOK);
	}

	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s", ctl_user_get(), ctl_uid_get());

	resp = j_arr();
	if (NULL == resp) {
		free(forum_topic);
		return (EBAD);
	}

	json_array_foreach(ctl->tickets_out, index, val) {
		if (EOK == j_test(val, JK_TICKET, ticket)) {
			rc = j_arr_add(resp, val);
			/* TODO: Memory leak forum_topic */
			TESTI_MES(rc, EBAD, "Can't add ticket to responce");
		}
	}

	/* Build responce */
	/* TODO: Memory leak forum_topic */
	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_send_json(mosq, forum_topic, resp);
	free(forum_topic);
	rc = j_rm(resp);
	TESTI_MES(rc, EBAD, "Can't remove json object");
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

