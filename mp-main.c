/*@-skipposixheaders@*/
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
/*@=skipposixheaders@*/

#include "mosquitto.h"
#include "buf_t.h"
#include "mp-common.h"
#include "mp-main.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-main.h"
#include "mp-cli.h"
#include "mp-config.h"
#include "mp-network.h"
#include "mp-requests.h"
#include "mp-communicate.h"
#include "mp-os.h"
#include "mp-dict.h"

/* If we got request with a ticket, it means that the scond part (remote client)
   waits for a responce.
   We test the client's request, and if find a ticket - we create the
   ticket reponce with 'status'
   It is up to the client to ask again for the ticket status */
/* TODO: add time to ticket such a way we may scan all tickets and remove old ones */

/* Parameters:
   req - request which must contain JK_TICKET with ticket id
   status - operation status: must be JV_STATUS_STARTED, JV_STATUS_UPDATE, JV_STATUS_DONE
   comment (optional) - free form test explaining what happens. THis text will be displeyed to user */
int mp_main_ticket_responce(json_t *req, const char *status, const char *comment)
{
	json_t *root = NULL;
	const char *ticket = NULL;
	const char *uid = NULL;
	json_t *j_ticket;
	control_t *ctl = NULL;
	int rc;
	//time_t t;
	char *forum;

	DD("Start\n");
	TESTP(req, EBAD);
	DD("Here\n");
	TESTP(status, EBAD);
	DD("Here\n");
	j_ticket = j_new();
	TESTP(j_ticket, EBAD);
	DD("Here\n");

	uid = j_find_ref(req, JK_UID_SRC);
	TESTP(uid, EBAD);
	DD("Here\n");

	ticket = j_find_ref(req, JK_TICKET);
	if (NULL == ticket) {
		DD("No ticket\n");
		j_print(req, "req is:");
		return (EOK);
	}

	DD("Found ticket :%s\n", ticket);
	root = j_new();
	TESTP(root, EBAD);
	DD("Here\n");

	rc = j_add_str(root, JK_TYPE, JV_TYPE_TICKET_RESP);
	TESTI(rc, EBAD);
	DD("Here\n");
	rc = j_add_str(root, JK_TICKET, ticket);
	TESTI(rc, EBAD);
	DD("Here\n");
	rc = j_add_str(root, JK_STATUS, status);
	TESTI(rc, EBAD);
	DD("Here\n");

	if (NULL != comment) {
		rc = j_add_str(root, JK_REASON, comment);
		TESTI(rc, EBAD);
	}

	/* Send it */

	ctl = ctl_get();
	DD("Here\n");
	forum = mp_communicate_forum_topic(j_find_ref(ctl->me, JK_USER), j_find_ref(ctl->me, JK_UID_ME));
	DD("Here\n");
	TESTP(forum, EBAD);
	DD("Here\n");
	//mp_main_mosq_thread(arg);

	DD("forum is : %s\n", forum);
	j_print(root, "Ticket update is:");
	rc = mp_communicate_send_json(ctl->mosq, forum, root);
	free(forum);
	return (rc);

#if 0
	/***TODO:  Add time of the ticket creation ***/

	t = time(NULL);
	if ((time_t)-1 != t) {
		rc = j_add_int(root, JK_TIME, t);
		TESTI_MES(rc, EBAD, "Can't add int to json: JK_TIME, t");
	} else {
		DE("Can't add time to ticket");
	}

	DD("Created ticket json resp:\n");
	j_print(root, "root");

	ctl = ctl_get_locked();
	DD("Going to add ticket to tickets\n");
	rc = j_arr_add(ctl->tickets_out, root);
	TESTI_MES(rc, EBAD, "Can't add ticket to array");

	DD("Added ticket to tickets\n");
	ctl_unlock(ctl);

	j_print(ctl->tickets_out, "ctl->tickets");
	return (rc);
#endif
}

static int mp_main_save_tickets(json_t *root)
{
	int index;
	json_t *ticket;
	control_t *ctl;
	TESTP(root, EBAD);

	ctl = ctl_get_locked();
	json_array_foreach(root, index, ticket) {
		j_arr_add(ctl->tickets_in, ticket);
	}
	ctl_unlock(ctl);
	return (EOK);
}

static int mp_main_remove_host_l(json_t *root)
{
	control_t *ctl = NULL;
	char *uid_src = NULL;
	int rc;

	TESTP(root, EBAD);
	uid_src = j_find_dup(root, JK_UID_SRC);
	TESTP_MES(uid_src, EBAD, "Can't extract uid from json\n");

	ctl = ctl_get_locked();
	rc = j_rm_key(ctl->hosts, uid_src);
	ctl_unlock(ctl);
	if (rc) {
		DE("Cant remove key from ctl->hosts:\n");
		DE("UID_SRC = |%s|\n", uid_src);
		j_print(ctl->hosts, "Hosts in ctl->hosts:");
	}
	return (EOK);
}

/* This function is called when remote machine asks to open port for imcoming connection */
static int mp_main_do_open_port_l(json_t *root)
{
	control_t *ctl = ctl_get();
	json_t *mapping = NULL;
	const char *asked_port = NULL;
	const char *protocol = NULL;
	const char *uid = NULL;
	//const char *ticket = NULL;
	//port_t *port = NULL;
	json_t *val = NULL;
	json_t *ports = NULL;
	int index = 0;
	int rc;

	TESTP(root, EBAD);

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	ctl_lock(ctl);
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_IP_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			ctl_unlock(ctl);
			DD("Already mapped port\n");
			return (EOK);
		}
	}

	ctl_unlock(ctl);

	/* this function probes the internal port. Is it alreasy mapped, it returns the mapping */
	mapping = mp_ports_if_mapped_json(root, asked_port, j_find_ref(ctl->me, JK_IP_INT), protocol);

	/* Found existing mapping */
	if (NULL != mapping) {
		DD("Found existing mapping: %s -> %s | %s\n",
		   j_find_ref(mapping, JK_PORT_EXT), j_find_ref(mapping, JK_PORT_INT), j_find_ref(mapping, JK_PROTOCOL));
		/* Add this mapping to table */
		ctl_lock(ctl);
		rc = j_arr_add(ports, mapping);
		ctl_unlock(ctl);
		TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
		return (EOK);
	}

	/* If we here it means no such mapping exists. Let's map it */
	mapping = mp_ports_remap_any(root, asked_port, protocol);
	TESTP_MES(mapping, EBAD, "Can't map port");

	/* Ok, port mapped. Now we should update ctl->ports hash table */

	ctl_lock(ctl);
	rc = j_arr_add(ports, mapping);
	ctl_unlock(ctl);
	TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
	return (EOK);
}

/* This function is called when remote machine asks to open port for imcoming connection */
static int mp_main_do_close_port_l(json_t *root)
{
	control_t *ctl = ctl_get();
	const char *asked_port = NULL;
	const char *protocol = NULL;
	json_t *val = NULL;
	json_t *ports = NULL;
	int index = 0;
	int index_save = 0;
	const char *external_port = NULL;
	int rc = EBAD;

	TESTP(root, EBAD);

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	ctl_lock(ctl);
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_PORT_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			external_port = j_find_ref(val, JK_PORT_EXT);
			index_save = index;
			D("Found opened port: %s -> %sd %s\n", asked_port, external_port, protocol);
			ctl_unlock(ctl);
		}
	}

	ctl_unlock(ctl);

	if (NULL == external_port) {
		DE("No such a open port\n");
		return (EBAD);
	}

	mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Starting port removing");
	/* this function probes the internal port. If it alreasy mapped, it returns the mapping */
	rc = mp_ports_unmap_port(root, asked_port, external_port, protocol);

	if (0 != rc) {
		DE("Can'r remove port \n");
		return (EBAD);
	}

	ctl_lock(ctl);
	json_array_remove(ports, index_save);
	ctl_unlock(ctl);
	return (EOK);
}

/* 
 * Here we may receive several types of the request: 
 * type: "keepalive" - a source sends its status 
 * type: "reveal" - a new host asks all clients to send information
 */
static int mp_main_parse_message_l(struct mosquitto *mosq, char *uid, json_t *root)
{
	int rc = EBAD;
	char *tp = NULL;
	control_t *ctl = ctl_get();

	TESTP(root, EBAD);
	TESTP(uid, EBAD);
	TESTP(mosq, EBAD);

	if (EOK != j_test_key(root, JK_TYPE)) {
		DE("No type in the message\n");
		j_print(root, "root");
		rc = j_rm(root);
		TESTI_MES(rc, EBAD, "Can't remove json object");
		free(tp);
		return (EBAD);
	}

	/*** Message "keepalive" ***/
	/* 
	 * "keepalive" is a regular message every
	 * that every client send once a while 
	 * This is a broadcast message, everyone receive it  
	 * This is aa status update.
	 */

	/* This is "me" object sent from remote host */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_ME)) {

		/* Find uid of this remote host */
		const char *uid_src = j_find_ref(root, JK_UID_ME);
		TESTP(uid_src, EBAD);
		/* Is this host already in the list? Just for information */
		//j_print(root, "Received ME from remote host:");

		ctl_lock(ctl);
		rc = j_replace(ctl->hosts, uid_src, root);
		ctl_unlock(ctl);
		return (rc);
	}

	/*** Message "reveal" ***/
	/*
	 * "reveal" is a message that every client sends after connect. 
	 * All other clients respond with "keepalive" 
	 * This way the new client build a list of all other clients 
	 * This if a broadcast message, everyone receive it  
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_REVEAL)) {
		send_keepalive_l(mosq);
		DD("Found reveal\n");
		rc = EOK;
		goto end;
	}

	/*** Message "disconnect" ***/
	/*
	 * Message "disconect" sent by broker. 
	 * This is the "last will" message. 
	 * By this message we remove the client with an uid from our lists. 
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_DISCONNECT)) {
		DD("Found disconnected client\n");
		rc = mp_main_remove_host_l(root);
		goto end;
	}


	/** All mesages except above should be dedicated to us ***/
	if (EOK != j_test(root, JK_DEST, j_find_ref(ctl->me, JK_UID_ME))) {
		rc = 0;
		goto end;
	}

	/*
	 * Message "openport" sent remote client. 
	 * The remote client wants to connect to this machine.
	 * We open a port and notify the remote machine about it
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_OPENPORT)) {

		DD("Got 'openport' request\n");

		rc = mp_main_do_open_port_l(root);
		/* 
		 * When the port opened, it added ctl global control_t structure 
		 * We don't need to know what port exactly opened, 
		 * we just send update to all listeners
		 */

		/*** TODO: SEB: Send update to all */
		if (EOK == rc) {
			mp_main_ticket_responce(root, JV_STATUS_SUCCESS, "Port opening finished OK");
			send_keepalive_l(mosq);
		} else {
			mp_main_ticket_responce(root, JV_STATUS_FAIL, "Port opening failed");
		}

		j_print(ctl->tickets_out, "After opening port: tickets: ");

		/*** TODO: SEB: After keepalive send report of "openport" is finished */
		goto end;
	}

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_CLOSEPORT)) {
		DD("Got 'closeport' request\n");

		rc = mp_main_do_close_port_l(root);
		/* 
		 * When the port opened, it added ctl global control_t structure 
		 * We don't need to know what port exactly opened, 
		 * we just send update to all listeners
		 */

		/*** TODO: SEB: Send update to all */
		if (EOK == rc) {
			mp_main_ticket_responce(root, JV_STATUS_SUCCESS, "Port closing finished OK");
			send_keepalive_l(mosq);
		} else {
			mp_main_ticket_responce(root, JV_STATUS_FAIL, "Port closing failed");
		}

		j_print(ctl->tickets_out, "After closing port: tickets: ");

		/*** TODO: SEB: After keepalive send report of "openport" is finished */
		goto end;
	}


	/*** Message "ticket" ***/

	/* The user asks for his tickets */
	/* This command we receive from outside */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_REQ)) {
		DD("Got 'JV_TYPE_TICKET_REQ' request\n");
		send_request_return_tickets(mosq, root);
		goto end;
	}

	/* We received a ticket responce. We should keep it localy until shell client grab it */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_RESP)) {
		DD("Got 'JV_TYPE_TICKET_RESP' request\n");
		mp_main_save_tickets(root);
		goto end;
	}

	/*** Message "ssh-done" ***/
	/*
	 * A client responds that "ssh" command executed, ready for ssh connection 
	 */

	/*** Message "sshr" ***/
	/*
	 * A client wants to connect to us using reversed ssh channel. 
	 * We should open a port and establish reversed SSH connection with the 
	 * senser; then we send notification "sshr-done" about it 
	 */

	/*** Message "sshr-done ***/
	/*
	 * Responce to "sshr" message, see above
	 */

	if (rc) DE("Unknown type: %s\n", tp);

end:
	if (root) {
		rc = j_rm(root);
		TESTI_MES(rc, EBAD, "Can't remove json object");
	}
	TFREE(tp);
	return (rc);
}

static int mp_main_on_message_processor(struct mosquitto *mosq, void *topic_v, void *data_v)
{
	char **topics;
	int topics_count = 0;
	char *topic = (char *)topic_v;
	int rc = EBAD;
	char *uid = NULL;
	json_t *root = NULL;

	TESTP(topic, EBAD);

	rc = mosquitto_sub_topic_tokenise(topic, &topics, &topics_count);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't tokenize topic\n");
		mosquitto_sub_topic_tokens_free(&topics, topics_count);
		return (EBAD);
	}

	/* TODO: SEB: Move numner of levels intopic to a define */
	if (topics_count < 4) {
		DE("Expected at least 4 levels of topic, got %d\n", topics_count);
		return (EBAD);
	}

	/* This define is a splint fix - splint parsing fails here */
#ifndef S_SPLINT_S
	uid = mosquitto_userdata(mosq);
	TESTP_MES(uid, EBAD, "Can't extract my uid");
#endif

	if (0 == strcmp(uid, topics[3])) {
		mosquitto_sub_topic_tokens_free(&topics, topics_count);
		return (EOK);
	}

	root = j_str2j((char *)data_v);
	TESTP_GO(root, err);

	/* The client uid always 4'th param */
	mp_main_parse_message_l(mosq, topics[3], root);
	mosquitto_sub_topic_tokens_free(&topics, topics_count);
	return (rc);
err:
	if (root) {
		rc = j_rm(root);
		TESTI_MES(rc, EBAD, "Can't remove json object");
	}

	mosquitto_sub_topic_tokens_free(&topics, topics_count);
	return (rc);
}

static void mp_main_on_message_cl(struct mosquitto *mosq, void *userdata __attribute__((unused)), const struct mosquitto_message *msg)
{
	mp_main_on_message_processor(mosq, msg->topic, msg->payload);
}

static void connect_callback_l(struct mosquitto *mosq, void *obj __attribute__((unused)), int result __attribute__((unused)))
{
	control_t *ctl = ctl_get();
	printf("connected!\n");
	send_reveal_l(mosq);
	ctl_lock(ctl);
	ctl->status = ST_CONNECTED;
	ctl_unlock(ctl);
}

static void mp_main_on_disconnect_l_cl(struct mosquitto *mosq __attribute__((unused)), void *data __attribute__((unused)), int reason)
{
	control_t *ctl = NULL;
	int rc;
	if (0 != reason) {
		switch (reason) {
		case MOSQ_ERR_NOMEM:
			DE("Memeory error: no memory\n");
			break;
		case MOSQ_ERR_PROTOCOL:
			DE("Protocol error\n");
			/* SEB: TODO: May we switch protocol?*/
			_exit(EBAD);
			break;
		case MOSQ_ERR_INVAL:
			DE("Invalid message?\n");
			break;
		case MOSQ_ERR_NO_CONN:
			DE("No connection\n");
			/* SEB: TODO: what should we do here?*/
			break;
		case MOSQ_ERR_CONN_REFUSED:
			DE("Connection is refused\n");
			/* Here we should terminate */
			_exit(EBAD);
			break;
		case MOSQ_ERR_NOT_FOUND:
			DE("MOSQ_ERR_NOT_FOUND: What not found? Guys. I love you, shhh\n");
			break;
		case MOSQ_ERR_CONN_LOST:
			DE("Connection lost\n");
			break;
		case MOSQ_ERR_TLS:
			DE("SSL connection error\n");
			break;
		case MOSQ_ERR_PAYLOAD_SIZE:
			DE("Wrong payload size, the message is too long?\n");
			break;
		case MOSQ_ERR_NOT_SUPPORTED:
			DE("MOSQ_ERR_NOT_SUPPORTED: Not supported WHAT?\n");
			break;
		case MOSQ_ERR_AUTH:
			DE("Auth problem: exit\n");
			_exit(EBAD);
		case MOSQ_ERR_ACL_DENIED:
			DE("Access denied: exit\n");
			_exit(EBAD);
		case MOSQ_ERR_UNKNOWN:
			DE("Unknown error. Lovely\n");
			break;
		case MOSQ_ERR_ERRNO:
			DE("MOSQ_ERR_ERRNO: Really?\n");
		}
	}

	ctl = ctl_get_locked();
	ctl->status = ST_DISCONNECTED;
	rc = j_rm(ctl->me);
	ctl->me = j_new();
	ctl_unlock(ctl);
	if (NULL == ctl->me) {
		DE("Can't allocate ctl->me\n");
		return;
	}
	if (rc) {
		DE("Error: couldn't remove json object ctl->me\n");
	}
	DDD("Exit from function\n");
}

void mp_main_on_publish_cb(struct mosquitto *mosq __attribute__((unused)),
						   void *data __attribute__((unused)), int buf_id)
{
	buf_t *buf = mp_communicate_get_buf_t_from_ctl(buf_id);
	if (NULL == buf) {
		DE("Can't find buffer\n");
		return;
	}

	//DD("Found buffer: \n%s\n", buf->data);
	buf_free_force(buf);
}


#define SERVER "185.177.92.146"
#define PORT 8883
#define PASS "asasqwqw"

/* This thread is responsible for connection to the broker */
static void *mp_main_mosq_thread(void *arg)
{
	control_t *ctl = NULL;
	int rc = EBAD;
	int i;
	char *cert = (char *)arg;
	char topic[TOPIC_MAX_LEN];
	char forum_topic[TOPIC_MAX_LEN];
	char personal_topic[TOPIC_MAX_LEN];
	buf_t *buf = NULL;

	/* Client ID, should be assigned on registartion and gotten from config file */
	char clientid[24] = "seb";
	/* Instance ID, we dont care, it always random */
	int counter = 0;

	TESTP(cert, NULL);

	mosquitto_lib_init();

	ctl = ctl_get_locked();

	memset(forum_topic, 0, TOPIC_MAX_LEN);
	memset(personal_topic, 0, TOPIC_MAX_LEN);

	snprintf(forum_topic, TOPIC_MAX_LEN, "users/%s/forum/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID_ME));
	snprintf(personal_topic, TOPIC_MAX_LEN, "users/%s/personal/%s",
			 j_find_ref(ctl->me, JK_USER),
			 j_find_ref(ctl->me, JK_UID_ME));
	ctl_unlock(ctl);

	DD("Creating mosquitto client.. ");
	ctl_lock(ctl);
	ctl->mosq = mosquitto_new(j_find_ref(ctl->me, JK_UID_ME), true,
							  (void *)j_find_ref(ctl->me, JK_UID_ME));
	ctl_unlock(ctl);
	DD("Done\n");


	if (NULL == ctl->mosq) {
		DE("Can't create mosq object\n");
		return (NULL);
	}

	DD("Setting user / pass.. ");
	rc = mosquitto_username_pw_set(ctl->mosq, clientid, PASS);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't set user / pass\n");
		return (NULL);
	}
	DD("Done\n");

	DD("Setting TLS.. ");
	rc = mosquitto_tls_set(ctl->mosq, cert, NULL, NULL, NULL, NULL);

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't set certificate\n");
		ctl->status = ST_STOP;
		return (NULL);
	}
	DD("Done\n");

	DD("Connecting callbacks.. ");

	mosquitto_connect_callback_set(ctl->mosq, connect_callback_l);
	//TESTI_MES(rc, EBAD, "Can't register 'connect' callback");
	mosquitto_message_callback_set(ctl->mosq, mp_main_on_message_cl);
	//TESTI_MES(rc, EBAD, "Can't register 'message' callback");
	mosquitto_disconnect_callback_set(ctl->mosq, mp_main_on_disconnect_l_cl);
	//TESTI_MES(rc, EBAD, "Can't register 'disconnect' callback");
	mosquitto_publish_callback_set(ctl->mosq, mp_main_on_publish_cb);
	//TESTI_MES(rc, EBAD, "Can't register 'publish' callback");
	DD("Done\n");

	DD("Setting last will.. ");
	ctl_lock(ctl);
	buf = mp_requests_build_last_will();
	ctl_unlock(ctl);

	TESTP_MES(buf, NULL, "Can't build last will");

	rc = mosquitto_will_set(ctl->mosq, forum_topic, (int)buf->size, buf->data, 1, false);
	buf_free_force(buf);

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't register last will\n");
		DE("Error: %s\n", mosquitto_strerror(rc));
		return (NULL);
	}
	DD("Done\n");

	DD("Connecting.. ");
	rc = mosquitto_connect(ctl->mosq, SERVER, PORT, 60);

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Error: %s\n", mosquitto_strerror(rc));
		return (NULL);
	}
	DD("Done\n");

	memset(topic, 0, TOPIC_MAX_LEN);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/forum/#", clientid);

	DD("Subscribing to topic 1.. ");
	rc = mosquitto_subscribe(ctl->mosq, NULL, topic, 0);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can not subsribe\n");
		return (NULL);
	}
	DD("Done\n");

	ctl_lock(ctl);
	snprintf(topic, TOPIC_MAX_LEN, "users/%s/private/%s", clientid, j_find_ref(ctl->me, JK_UID_ME));
	ctl_unlock(ctl);

	DD("Subscribing to topic 2.. ");
	rc = mosquitto_subscribe(ctl->mosq, NULL, topic, 0);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can not subsribe\n");
		return (NULL);
	}
	DD("Done\n");

	rc = mosquitto_loop_start(ctl->mosq);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't start loop\n");
		return (NULL);
	}
	DDD("Starting main loop\n");

	//while (counter++ < 40) {
	while (ST_STOP != ctl->status) {
		DDD("Client status is: %d\n", ctl->status);

		if (ST_DISCONNECTED == ctl->status) {
			DD("Client is disconnected, trying reconnect\n");
			rc = mosquitto_reconnect(ctl->mosq);
			if (MOSQ_ERR_SUCCESS != rc) {
				DE("Connection error: ");

				if (MOSQ_ERR_SUCCESS == rc) {
					DE("invalid params\n");
				}

				if (MOSQ_ERR_NOMEM == rc) {
					DE("no memory\n");
				}
				if (MOSQ_ERR_SUCCESS == rc) {
					DE("unkown, error: %d\n", rc);
				}
			} else {
				DD("Finished reconnect\n");
				ctl->status = ST_CONNECTED;
			}
		}
		DDD("Finished disconnection check\n");

		usleep((__useconds_t)mp_os_random_in_range(100, 300));
		if (0 == (counter % 7) && (ST_DISCONNECTED != ctl->status)) {
			DD("Client connected, sending keepalive message\n");
			rc = send_keepalive_l(ctl->mosq);
			if (EOK != rc) {}
			counter = 1;
		}
		if (ST_DISCONNECTED == ctl->status) {
			DD("Client in DISCONNECTED status\n");
		}

		for (i = 0; i < 200; i++) {
			if (ST_STOP == ctl->status) break;
			usleep((__useconds_t)mp_os_random_in_range(10000, 40000));
		}
		counter++;
	}

	ctl_lock(ctl);
	rc = mosquitto_loop_stop(ctl->mosq, true);
	mosquitto_destroy(ctl->mosq);
	ctl->mosq = NULL;
	ctl_unlock(ctl);
	mosquitto_lib_cleanup();
	rc = j_rm(ctl->hosts);
	ctl->hosts = j_new();
	TESTP(ctl->hosts, NULL);
	if (EOK != rc) {
		DE("Error: couldn't remove old ctl->hosts");
	}
	D("Exit thread\n");
	return (NULL);

}

static void *mp_main_mosq_thread_manager(void *arg)
{
	control_t *ctl = NULL;
	void *status = NULL;
	pthread_t mosq_thread_id;
	pthread_detach(pthread_self());

	ctl = ctl_get();
	while (ST_STOP != ctl->status) {
		pthread_create(&mosq_thread_id, NULL, mp_main_mosq_thread, arg);
		pthread_join(mosq_thread_id, &status);
	}
	D("Exit\n");
	ctl->status = ST_STOPPED;
	return (NULL);
}

static int mp_main_print_info_banner()
{
	control_t *ctl = ctl_get();
	printf("=======================================\n");
	printf("Router IP:\t%s:%s\n", j_find_ref(ctl->me, JK_IP_EXT), j_find_ref(ctl->me, JK_PORT_EXT));
	printf("Local IP:\t%s:%s\n", j_find_ref(ctl->me, JK_IP_INT), j_find_ref(ctl->me, JK_PORT_INT));
	printf("Name of comp:\t%s\n", j_find_ref(ctl->me, JK_NAME));
	printf("Name of user:\t%s\n", j_find_ref(ctl->me, JK_USER));
	printf("UID of user:\t%s\n", j_find_ref(ctl->me, JK_UID_ME));
	printf("=======================================\n");
	return (EOK);
}

static void mp_main_signal_handler(int sig)
{
	control_t *ctl;
	if (SIGINT != sig) {
		DD("Got signal: %d, ignore\n", sig);
		return;
	}

	ctl = ctl_get();
	ctl->status = ST_STOP;
	while (ST_STOPPED != ctl->status) {
		usleep(200);
	}
	_exit(0);
}

/* This function complete ctl->me init.
   We call this function after config file loaded.
   If we run for the first time, the config file
   doesn't exist. In this case we create all fields we
   need for run, and later [see main() before threads started]
   we dump these values to config.*/
int mp_main_complete_me_init(void)
{
	int rc = EBAD;
	char *var = NULL;
	control_t *ctl = ctl_get();

	/* SEB: TODO: This should be defined by user from first time config */
	if (EOK != j_test_key(ctl->me, JK_USER)) {
		rc = j_add_str(ctl->me, JK_USER, "seb");
		TESTI_MES(rc, EBAD, "Can't add 'user'\n");
	}

	/* Try to read hostname of this machine. */

	if (EOK != j_test_key(ctl->me, JK_NAME)) {
		var = mp_os_get_hostname();
		if (NULL == var) {
			var = strdup("can-not-resolve-name");
		}

		rc = j_add_str(ctl->me, JK_NAME, var);
		TFREE(var);
		TESTI_MES(rc, EBAD, "Could not add string for 'name'\n");
	}

	if (EOK != j_test_key(ctl->me, JK_UID_ME)) {
		var = mp_os_generate_uid(j_find_ref(ctl->me, JK_USER));
		TESTP(var, EBAD);
		TESTI_MES(rc, EBAD, "Can't generate UID\n");

		rc = j_add_str(ctl->me, JK_UID_ME, var);
		TESTI_MES(rc, EBAD, "Can't add JK_UID into etcl->me");
	}

	if (EOK != j_test_key(ctl->me, JK_SOURCE)) {
		rc = j_add_str(ctl->me, JK_SOURCE, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_SOURCE");
	}

	if (EOK != j_test_key(ctl->me, JK_TARGET)) {
		rc = j_add_str(ctl->me, JK_TARGET, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_TARGET");
	}

	if (EOK != j_test_key(ctl->me, JK_BRIDGE)) {
		rc = j_add_str(ctl->me, JK_BRIDGE, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_BRIDGE");
	}

	printf("UID: %s\n", j_find_ref(ctl->me, JK_UID_ME));
	return (EOK);
}

int main(int argc __attribute__((unused)), char *argv[])
{
	char *cert = NULL;
	control_t *ctl = NULL;
	pthread_t cli_thread_id;
	pthread_t mosq_thread_id;
	json_t *ports;

	int rc = EOK;

	rc = ctl_allocate_init();
	TESTI_MES(rc, EBAD, "Can't allocate and init control struct\n");

	/* We don't need it locked - nothing is running yet */
	ctl = ctl_get();

	if (EOK != mp_config_load(ctl) || NULL == ctl->config) {
		DDD("Can't read config\n");
	}

	rc = mp_main_complete_me_init();
	TESTI_MES(rc, EBAD, "Can't finish 'me' init\n");

	if (0 != mp_network_init_network_l()) {
		DE("Can't init network\n");
		return (EBAD);
	}

	/* TODO: We should have this certifivate in the config file */
	cert = strdup(argv[1]);
	if (NULL == cert) {
		printf("arg1 should be path to certificate\n");
		return (EBAD);
	}

	ports = j_find_j(ctl->me, "ports");
	if (mp_ports_scan_mappings(ports, j_find_ref(ctl->me, JK_IP_INT))) {
		DE("Port scanning failed\n");
	}

	if (SIG_ERR == signal(SIGINT, mp_main_signal_handler)) {
		DE("Can't register signal handler\n");
		return (EBAD);
	}

	/* Here test the config. If it not loaded - we create it and save it */
	if (NULL == ctl->config) {
		rc = mp_config_from_ctl(ctl);
		if (EOK == rc) {
			rc = mp_config_save(ctl);
			if (EOK != rc) {
				DE("Cant' save config\n");;
			}
		}
	}

	mp_main_print_info_banner();
	pthread_create(&mosq_thread_id, NULL, mp_main_mosq_thread_manager, cert);
	pthread_create(&cli_thread_id, NULL, mp_cli_thread, NULL);

	while (ctl->status != ST_STOP) {
		usleep(300);
	}

	return (rc);
}
