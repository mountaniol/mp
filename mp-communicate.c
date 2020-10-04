#include <stdio.h>
#include <stdarg.h>
#include <jansson.h>

#include "mosquitto.h"
#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-main.h"
#include "mp-memory.h"
#include "mp-requests.h"
#include "mp-jansson.h"
#include "mp-dict.h"
#include "mp-htable.h"
#include "mp-dispatcher.h"
/*@null@*/ buf_t *mp_communicate_forum_topic()
{
	const char *user = ctl_user_get();
	const char *uid  = ctl_uid_get();

	return (buf_sprintf("users/%s/forum/%s", user, uid));
}

/*@null@*/ buf_t *mp_communicate_forum_topic_all()
{
	const char *user = ctl_user_get();

	return (buf_sprintf("users/%s/forum/#", user));
}

/*@null@*/ buf_t *mp_communicate_private_topic()
{
	const char *user = ctl_user_get();
	const char *uid  = ctl_uid_get();

	return (buf_sprintf("users/%s/personal/%s", user, uid));
}

err_t mp_communicate_clean_missed_counters_hash(void)
{
	/*@shared@*/control_t *ctl;
	buf_t *buf;

	ctl = ctl_get_locked();
	do {
		buf = htable_extract_any(ctl->buf_hash);
		if (buf) {
			buf_free(buf);
		}
		
	} while (NULL != buf );
	ctl_unlock();
	return (EOK);
}

/*@null@*/ buf_t *mp_communicate_get_buf_t_from_hash(int counter)
{
	buf_t *buf;
	/*@shared@*/control_t *ctl = NULL;


	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (NULL);
	}


	ctl = ctl_get_locked();
	buf = htable_extract_by_int(ctl->buf_hash, counter);
	ctl_unlock();
	return (buf);
}

/* Save 'buf' ponter by key 'counter' in ctl->buffers.
   Used later in callback function mp_main_on_publish_cb
   to release the buf when mosq sent it */
static err_t mp_communicate_save_buf_t_to_hash(buf_t *buf, int counter)
{
	/*@shared@*/control_t *ctl = NULL;
	int rc;

	TESTP(buf, EBAD);
	if (counter < 0) {
		DE("Bad counter: %d\n", counter);
		return (EBAD);
	}

	DDD0("Got counter = %d\n", counter);

	ctl = ctl_get_locked(); 
	/* Save buffer into hash; the address of the buffer is the key */
	rc = htable_insert_by_int(ctl->buf_hash, (size_t)counter, NULL, buf);
	ctl_unlock();
	return (rc);
}


err_t mp_communicate_mosquitto_publish(/*@temp@*/const char *topic, /*@temp@*/buf_t *buf)
{
	int rc;
	int rc2;
	int counter = -1;
	/*@shared@*/control_t *ctl = ctl_get();
	rc = mosquitto_publish(ctl->mosq, &counter, topic, (int)buf_used(buf), buf->data, 0, false);
	// rc2 = mp_communicate_save_buf_t_to_ctl(buf, counter);
	rc2 = mp_communicate_save_buf_t_to_hash(buf, counter);
	if (EOK != rc2) {
		DE("Can't save buf_t to ctl\n");
	} else {
		DDD("Saved buffer %p to hash\n", buf);
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
#if 0 /* SEB DEADCODE 20/09/2020 13:38  */ 
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

	DDD("Going to send keepalive:\n%s\n", buf->data);

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
#endif /* SEB DEADCODE 20/09/2020 13:38 */
	int rc;
	control_t *ctl = ctl_get();
	j_t *root = mp_disp_create_request("ALL", MODULE_CONNECTION, MODULE_CONNECTION,0);
	TESTP_MES(root, EBAD, "Can't create request\n");
	j_merge(root, ctl->me);
	rc = mp_disp_send(root);
	if (EOK != rc) {
		DE("Can't send json\n");
	}
	return rc;
}

extern err_t send_keepalive_l_orig()
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

	DDD("Going to send keepalive:\n%s\n", buf->data);

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
#if 0 /* SEB DEADCODE 20/09/2020 13:32  */ 
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
#endif /* SEB DEADCODE 20/09/2020 13:32 */
	int rc;
	j_t *root = mp_disp_create_request("ALL", MODULE_CONNECTION, MODULE_CONNECTION,0);
	TESTP(root, EBAD);

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_REVEAL)) {
		j_rm(root);
		return EBAD;
	}
		rc = mp_disp_send(root);
	if (EOK != rc) {
		DE("Can't send json\n");
	}
	return rc;
}

err_t mp_communicate_send_request(j_t *root)
{
	int   rc           = EBAD;
	buf_t *buf         = NULL;
	//char  *forum_topic;
	buf_t *forum_topic;

	forum_topic = mp_communicate_forum_topic();
	TESTP(forum_topic, EBAD);

	DDD("Going to build request\n");
	buf = j_2buf(root);
	j_rm(root);

	TESTP_MES(buf, EBAD, "Can't build open port request");
	DDD0("Going to send request\n");
	j_print_v(root, "Sending requiest:", __FILE__, __LINE__);
	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	DDD("Send: \n%s\n", buf->data);
	buf_free(forum_topic);
	DDD("Sent request, status is %d\n", rc);
	return (rc);
}

/* TODO: this function must go */
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
	target_uid = j_find_ref(root, JK_DISP_SRC_UID);
	TESTP(target_uid, EBAD);

	ctl = ctl_get();

	forum_topic = mp_communicate_forum_topic();
	TESTP(forum_topic, EBAD);

	ctl_lock();
	if (j_count(ctl->tickets_out) == 1) {
		ctl_unlock();
		DD("No tickets to send\n");
		return (EOK);
	}
	ctl_unlock();

	resp = j_arr();
	if (NULL == resp) {
		buf_free(forum_topic);
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