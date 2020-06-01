#include <jansson.h>

#include "mosquitto.h"
#include "buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-main.h"
#include "mp-memory.h"
#include "mp-requests.h"
#include "mp-jansson.h"
#include "mp-dict.h"

/*@null@*/ buf_t *mp_communicate_forum_topic()
{
	const char *user = ctl_user_get();
	const char *uid  = ctl_uid_get();
	buf_t      *buf  = buf_new(NULL, TOPIC_MAX_LEN);
	int        rc;
	TESTP(buf, NULL);
	rc = snprintf(buf->data, TOPIC_MAX_LEN, "users/%s/forum/%s", user, uid);
	if (rc < 0) {
		DE("Can't create topic\n");
		buf_free(buf);
		return (NULL);
	}
	buf->len = rc;
	buf_pack(buf);
	return (buf);
}

/*@null@*/ buf_t *mp_communicate_forum_topic_all()
{
	const char *user  = ctl_user_get();
	buf_t      *topic = buf_new(NULL, TOPIC_MAX_LEN);
	int        rc;
	TESTP(topic, NULL);
	rc = snprintf(topic->data, TOPIC_MAX_LEN, "users/%s/forum/#", user);
	if (rc < 0) {
		DE("Can't create topic\n");
		buf_free(topic);
		return (NULL);
	}
	topic->len = rc;
	buf_pack(topic);
	return (topic);
}

/*@null@*/ buf_t *mp_communicate_private_topic()
{
	const char *user  = ctl_user_get();
	const char *uid   = ctl_uid_get();
	int        rc;
	buf_t       *topic = buf_new(NULL, TOPIC_MAX_LEN);
	TESTP(topic, NULL);
	rc = snprintf(topic->data, TOPIC_MAX_LEN, "users/%s/personal/%s", user, uid);
	if (rc < 0) {
		DE("Can't create topic\n");
		TFREE_SIZE(topic, TOPIC_MAX_LEN);
		return (NULL);
	}
	topic->len = rc;
	buf_pack(topic);
	return (topic);
}

#if 0 /* SEB DEADCODE 14/05/2020 21:33  */
/*@null@*/ char *mp_communicate_private_topic_all(){
	const char *user = ctl_user_get();
	char *topic = zmalloc(TOPIC_MAX_LEN);
	int rc;
	TESTP(topic, NULL);
	rc = snprintf(topic, TOPIC_MAX_LEN, "users/%s/personal/#", user);
	if (rc < 0) {
		DE("Can't create topic\n");
		TFREE(topic);
		return (NULL);
	}
	return (topic);
}
#endif /* SEB DEADCODE 14/05/2020 21:33 */


/* Find a buffer in ctl->buffers by counter 'counter' */
err_t mp_communicate_clean_missed_counters(void)
{
	/*@shared@*/control_t *ctl;
	/*@shared@*/void *tmp;
	/*@shared@*/const char *key;
	/*@shared@*/j_t *val;

	ctl = ctl_get_locked();
	if (j_count(ctl->buf_missed) < 1) {
		DDD0("No missed counters\n");
		ctl_unlock();
		return (EOK);
	}

	json_object_foreach_safe(ctl->buf_missed, tmp, key, val) {
		buf_t  *buf;
		size_t ret;
		err_t  rc;

		ret = j_find_int(ctl->buffers, key);
		if (0XDEADBEEF == ret) {
			DE("Something wrong: can't find value for key %s\n", key);
			ctl_unlock();
			return (EBAD);
		}

		buf = (buf_t *)ret;
		if (EOK != buf_free(buf)) {
			DE("Can't remove buf_t: probably passed NULL pointer?\n");
			ctl_unlock();
			return (EBAD);
		}

		DDD0("Found missed key, removing: %s\n", key);
		rc = j_rm_key(ctl->buf_missed, key);
		if (EOK != rc) {
			DE("Can't remove counter from missed keys: %s\n", key);
			ctl_unlock();
			return (EBAD);
		}
		rc = j_rm_key(ctl->buffers, key);
		if (EOK != rc) {
			DE("Can't remove counter from buffers: %s\n", key);
			ctl_unlock();
			return (EBAD);
		}
	}
	ctl_unlock();

	return (EOK);
}

/* Find a buffer in ctl->buffers by counter 'counter' */
/*@null@*/ buf_t *mp_communicate_get_buf_t_from_ctl_l(int counter)
{
	/*@shared@*/buf_t *buf_p;
	size_t ret;
	/*@only@*/char *buf_counter_s;
	/*@shared@*/control_t *ctl;
	int    rc;

	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (NULL);
	}

	buf_counter_s = (char *)zmalloc(32);
	TESTP(buf_counter_s, NULL);

	/* Transform counter to key (string) */
	rc = snprintf(buf_counter_s, 32, "%d", counter);
	if (rc < 0) {
		DE("Can't transform buffer counter to string\n");
		TFREE_SIZE(buf_counter_s, 32);
		return (NULL);
	}

	DDD0("Counter string = %s\n", buf_counter_s);

	ctl = ctl_get_locked();
	ret = j_find_int(ctl->buffers, buf_counter_s);
	ctl_unlock();

	if (0XDEADBEEF == ret) {
		DE("Can't get buffer\n");

		/* We can't get buffer it probably not set yet.
		   We should save this counter and try it later. */

		ctl_lock();
		rc = j_add_int(ctl->buf_missed, buf_counter_s, counter);
		ctl_unlock();
		TFREE_SIZE(buf_counter_s, 32);
		if (EOK != rc) {
			DE("Can't add counter into ctl->buf_missed\n");
		}
		DDD0("Added to missed counters: %d\n", counter);
		//j_print(ctl->buf_missed, "Now in missed  counters:");
		//j_print(ctl->buffers, "Now in buffers counters:");
		return (NULL);
	}

	//DD("Got ret: %ld / %lx\n", ret, ret);
	buf_p = (buf_t *)ret;
	ctl_lock();
	rc = j_rm_key(ctl->buffers, buf_counter_s);
	ctl_unlock();
	if (EOK != rc) {
		DE("Can't remove key from json: ctl->buffers, buf_counter_s");
	}
	TFREE_SIZE(buf_counter_s, 32);
	return (buf_p);
}


/* Save 'buf' ponter by key 'counter' in ctl->buffers.
   Used later in callback function mp_main_on_publish_cb
   to release the buf when mosq sent it */
static err_t mp_communicate_save_buf_t_to_ctl(buf_t *buf, int counter)
{
	/*@only@*/char *buf_counter_s = NULL;
	/*@shared@*/control_t *ctl;
	int rc;

	TESTP(buf, EBAD);
	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (EBAD);
	}

	DDD0("Got counter = %d\n", counter);

	buf_counter_s = (char *)zmalloc(32);
	TESTP(buf_counter_s, EBAD);

	/* Transfor counter to key (string) */
	rc = snprintf(buf_counter_s, 32, "%d", counter);
	if (rc < 0) {
		DE("Can't convert counter to string\n");
		TFREE_SIZE(buf_counter_s, 32);
		return (EBAD);
	}

	ctl = ctl_get_locked();
	rc = j_add_int(ctl->buffers, buf_counter_s, (size_t)buf);
	ctl_unlock();
	TFREE_SIZE(buf_counter_s, 32);
	TESTI_MES(rc, EBAD, "Can't add int to json: buf_counter_s, (size_t) buf\n");

	return (EOK);
}

static err_t mp_communicate_mosquitto_publish(/*@temp@*/const char *topic, /*@temp@*/buf_t *buf)
{
	int rc;
	int rc2;
	int counter = -1;
	/*@shared@*/control_t *ctl = ctl_get();
	rc = mosquitto_publish(ctl->mosq, &counter, topic, (int)buf->len, buf->data, 0, false);
	rc2 = mp_communicate_save_buf_t_to_ctl(buf, counter);
	if (EOK != rc2) {
		DE("Can't save buf_t to ctl\n");
	} else {
		//control_t *ctl = ctl_get();
		//j_print(ctl->buffers, "ctl->buffers: ");
	}

	return (rc);
}

err_t mp_communicate_send_json(/*@temp@*/const char *forum_topic, /*@temp@*/j_t *root)
{
	buf_t *buf;

	TESTP(forum_topic, EBAD);
	TESTP(root, EBAD);

	buf = j_2buf(root);
	/* We must save this buffer; we will free it later, in mp_main_on_publish_cb() */
	TESTP(buf, EBAD);

	return (mp_communicate_mosquitto_publish(forum_topic, buf));
}

extern err_t send_keepalive_l()
{
	//char  *forum_topic;
	buf_t *forum_topic;
	buf_t *buf         = NULL;
	int   rc           = EBAD;

	/*@shared@*/control_t *ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic();
	TESTP(forum_topic, EBAD);

	buf = mp_requests_build_keepalive();

	if (NULL == buf) {
		DE("can't build notification\n");
		return (EBAD);
	}

	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	if (MOSQ_ERR_SUCCESS == rc) {
		rc = EOK;
		goto end;
	}

	DE("Failed to send notification\n");
	rc = mosquitto_reconnect(ctl->mosq);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to reconnect\n");
		rc = EBAD;
		goto end;
	}

	DD("Reconnected\n");

	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send notification\n");
		rc = EBAD;
	}

end:
	buf_free(forum_topic);
	return (rc);
}

err_t send_reveal_l()
{
	//char  *forum_topic;
	buf_t *forum_topic;
	buf_t *buf         = NULL;
	int   rc           = EBAD;

	forum_topic = mp_communicate_forum_topic();
	TESTP(forum_topic, EBAD);

	buf = mp_requests_build_reveal();

	TESTP_MES(buf, EBAD, "Can't build notification");

	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	buf_free(forum_topic);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Failed to send reveal request\n");
		return (EBAD);
	}

	return (EOK);
}

err_t mp_communicate_send_request(const j_t *root)
{
	int   rc           = EBAD;
	buf_t *buf         = NULL;
	//char  *forum_topic;
	buf_t *forum_topic;

	forum_topic = mp_communicate_forum_topic();

	DDD("Going to build request\n");
	buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD0("Going to send request\n");
	//j_print(root, "Sending requiest:");
	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	buf_free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

#if 0 /* SEB DEADCODE 14/05/2020 21:32  */
int send_request_to_open_port(/*@temp@*/const j_t *root){
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;

	forum_topic = mp_communicate_forum_topic();

	DDD("Going to build request\n");
	buf = j_2buf(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD0("Going to send request\n");
	//j_print(root, "Sending requiest:");
	rc = mp_communicate_mosquitto_publish(forum_topic, buf);
	TFREE(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}
#endif /* SEB DEADCODE 14/05/2020 21:32 */

err_t send_request_to_open_port_old(struct mosquitto *mosq, char *target_uid, char *port, char *protocol)
{
	int   rc           = EBAD;
	buf_t *buf         = NULL;
	//char  *forum_topic;
	buf_t *forum_topic;

	TESTP(mosq, EBAD);
	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	forum_topic = mp_communicate_forum_topic();

	DDD("Going to build request\n");
	buf = mp_requests_open_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	buf_free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

#if 0 /* SEB DEADCODE 14/05/2020 21:33  */
int send_request_to_close_port(/*@temp@*/const char *target_uid, /*@temp@*/const char *port, /*@temp@*/const char *protocol){
	int rc = EBAD;
	buf_t *buf = NULL;
	char *forum_topic;

	TESTP(target_uid, EBAD);
	TESTP(port, EBAD);
	TESTP(protocol, EBAD);

	forum_topic = mp_communicate_forum_topic();

	DDD("Going to build request\n");
	buf = mp_requests_close_port(target_uid, port, protocol);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_mosquitto_publish(forum_topic, buf);
	TFREE(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}
#endif /* SEB DEADCODE 14/05/2020 21:33 */

err_t send_request_return_tickets_l(/*@temp@*/j_t *root)
{
	int        rc           = EBAD;
	buf_t      *buf         = NULL;
	//char       *forum_topic;
	buf_t      *forum_topic;
	/*@shared@*/control_t *ctl = NULL;
	j_t        *resp        = NULL;
	size_t     index;
	j_t        *val         = NULL;
	const char *ticket      = NULL;
	const char *target_uid  = NULL;

	ticket = j_find_ref(root, JK_TICKET);
	TESTP(ticket, EBAD);
	target_uid = j_find_ref(root, JK_UID_SRC);
	TESTP(target_uid, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic();

	ctl_lock();
	if (j_count(ctl->tickets_out) == 1) {
		ctl_unlock();
		DD("No tickets to send\n");
		return (EOK);
	}
	ctl_unlock();

	//rc = snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s", ctl_user_get(), ctl_uid_get());

	resp = j_arr();
	if (NULL == resp) {
		TFREE_SIZE(forum_topic->data, TOPIC_MAX_LEN);
		return (EBAD);
	}

	ctl_lock();
	json_array_foreach(ctl->tickets_out, index, val) {
		if (EOK == j_test(val, JK_TICKET, ticket)) {
			rc = j_arr_add(resp, val);
			/* TODO: Memory leak forum_topic */
			if (EBAD == rc) {
				DE("Can't add ticket to responce\n");
				ctl_unlock();
				return (EBAD);
			}
		}
	}
	ctl_unlock();

	/* Build responce */
	/* TODO: Memory leak forum_topic */
	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD("Going to send request\n");
	rc = mp_communicate_send_json(forum_topic->data, resp);
	buf_free(forum_topic);
	j_rm(resp);
	TESTI_MES(rc, EBAD, "Can't remove json object");
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}