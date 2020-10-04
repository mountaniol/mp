#ifndef S_SPLINT_S
	#define _GNU_SOURCE             /* See feature_test_macros(7) */
	#include <sys/prctl.h>
	#include <unistd.h>
	#include <string.h>
	#include <pthread.h>

	#include "openssl/ssl.h"
	#include "openssl/err.h"
#endif

#include "mosquitto.h"
#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-cli.h"
#include "mp-config.h"
#include "mp-requests.h"
#include "mp-communicate.h"
#include "mp-os.h"
#include "mp-dict.h"
#include "mp-mqtt-module.h"
#include "mp-dispatcher.h"


#define SERVER "185.177.92.146"
#define PORT 8883
#define PASS "asasqwqw"

pthread_t mosq_thread_id;
/*@null@*/static void *mp_mqtt_message_processor_pthread(/*@only@*/void *v);

/* Dispatcher hook, called when a message from a remote host received */
int mp_module_recv(void *root)
{
	const char *command;
	DD("Recevived a message\n");

	/* Process it here */
	j_print_v(root, "Reveived message is", __FILE__, __LINE__);

	/* Process the message */
	command = j_find_ref(root, JK_TYPE);
	if (NULL == command) {
		DE("No JK_TYPE found in the message\n");
		j_rm(root);
		return EBAD;
	}

	if (0 == strcmp(command, JV_TYPE_ME)) {
		DD("Found JV_TYPE_ME type\n");
		goto finish;
	}

	if (0 == strcmp(command, JV_TYPE_CLOSEPORT)) {
		DD("Found JV_TYPE_CLOSEPORT type: bad, it dedicated to MODULE_PORTS\n");
		goto finish;
	}

	if (0 == strcmp(command, JV_TYPE_CONNECT)) {
		DD("Found JV_TYPE_CONNECT type: a remote host connected\n");
		goto finish;
	}

	if (0 == strcmp(command, JV_TYPE_DISCONNECTED)) {
		DD("Found JV_TYPE_DISCONNECTED type: a remote host disconnected\n");
		goto finish;
	}

	if (0 == strcmp(command, JV_TYPE_OPENPORT)) {
		DD("Found JV_TYPE_OPENPORT type: bad, it dedicated to MODULE_PORTS\n");
		goto finish;
	}

	if (0 == strcmp(command, JV_TYPE_REVEAL)) {
		DD("Found JV_TYPE_REVEAL type: we should send our configuration\n");
		goto finish;
	}

	DE("Found unknown command: %s\n", command);

finish:
	j_rm(root);
	return (0);
}

/* Dispatcher hook, called when a message to a remote machine asked to be sent */
int mp_module_send(void *root)
{
	int   rc           = -1;
	/* TODO: by default we publush message on own topic */
	/* TODO: Probably it's better to push it to the personal topic of the receiver */
	buf_t *forum_topic = mp_communicate_forum_topic();
	buf_t *buf;

	TESTP(forum_topic, EBAD);
	TESTP(root, EBAD);

	buf = j_2buf(root);
	/* We must save this buffer; we will free it later, in mp_main_on_publish_cb() */
	TESTP(buf, EBAD);

	j_print_v(root, "Going to send the JSON", __FILE__, __LINE__);
	rc = mp_communicate_mosquitto_publish(forum_topic->data, buf);
	buf_free(forum_topic);
	return (rc);
}

/* If we got request with a ticket, it means that the scond part (remote client)
   waits for a responce.
   We test the client's request, and if find a ticket - we create the
   ticket reponce with 'status'
   It is up to the client to ask again for the ticket status */
/* TODO: add time to ticket such a way we may scan all tickets and remove old ones */

/* Parameters:
   req - request which must contain JK_TICKET with ticket id
   status - operation status: must be JV_STATUS_STARTED, JV_STATUS_UPDATE, JV_STATUS_DONE
   comment (optional) - free form test explaining what happens. This text will be displeyed to user */
err_t mp_mqtt_ticket_responce(const j_t *req, const char *status, const char *comment)
{
	/*@only@*/j_t *root = NULL;
	/*@temp@*/const char *ticket = NULL;
	/*@temp@*/const char *uid = NULL;
	err_t rc;
	/*@only@*/ //char *forum = NULL;
	/*@only@*/buf_t *forum = NULL;

	TESTP(req, EBAD);
	//j_print(req, "Got req:");
	TESTP(status, EBAD);

	uid = j_find_ref(req, JK_DISP_SRC_UID);
	TESTP(uid, EBAD);

	ticket = j_find_ref(req, JK_TICKET);
	if (NULL == ticket) {
		DE("No ticket\n");
		j_print_v(req, "req is:", __FILE__, __LINE__);
		return (EOK);
	}

	/* Create JSON object for the response */
	root = j_new();
	TESTP(root, EBAD);

	rc = j_add_str(root, JK_TYPE, JV_TYPE_TICKET_RESP);
	TESTI_GO(rc, end);
	rc = j_add_str(root, JK_TICKET, ticket);
	TESTI_GO(rc, end);
	rc = j_add_str(root, JK_STATUS, status);
	TESTI_GO(rc, end);
	rc = j_add_str(root, JK_DISP_TGT_UID, uid);
	TESTI_GO(rc, end);

	if (NULL != comment) {
		rc = j_add_str(root, JK_REASON, comment);
		TESTI_GO(rc, end);
	}

	/* Send it */

	forum = mp_communicate_forum_topic();
	TESTP_ASSERT(forum, "Can't allocate forum!");
	rc = mp_communicate_send_json(forum->data, root);
	buf_free(forum);

end:
	j_rm(root);
	return (rc);
}

/* This function called when a remote machine disconnected from the server.
   When it happens, we remove all information about this machine */
/* TODO: MODULE_CONFIG should receive it */
static err_t mp_mqtt_remove_host_l(const j_t *root)
{
	/*@temp@*/const control_t *ctl = NULL;
	/*@temp@*/const char *uid_src = NULL;
	err_t rc;

	TESTP(root, EBAD);

	DD("Starting\n");

	uid_src = j_find_ref(root, JK_DISP_SRC_UID);
	TESTP_MES(uid_src, EBAD, "Can't extract uid from json\n");
	DD("uid_src = %s\n", uid_src);

	ctl = ctl_get_locked();

	if (EOK != j_test_key(ctl->hosts, uid_src)) {
		DE("Not found client %s\n", uid_src);
		ctl_unlock();
		return (EBAD);
	}

	DD("Found UID in hosts\n");

	rc = j_rm_key(ctl->hosts, uid_src);
	ctl_unlock();

	if (EOK != rc) {
		DE("Cant remove key from ctl->hosts:\n");
		DE("UID_SRC = |%s|\n", uid_src);
		j_print_v(ctl->hosts, "Hosts in ctl->hosts:", __FILE__, __LINE__);
	}
	DD("Removed UID\n");
	return (EOK);
}


/* This function is called when remote machine asks to open port for imcoming connection */
static err_t mp_mqtt_do_open_port_l(const j_t *root)
{
	/*@temp@*/ const control_t *ctl = ctl_get();
	/*@temp@*/j_t *mapping = NULL;
	/*@temp@*/const char *asked_port = NULL;
	/*@temp@*/const char *protocol = NULL;
	/*@temp@*/j_t *val = NULL;
	/*@temp@*/j_t *ports = NULL;
	size_t index = 0;
	/*@temp@*/const char *ip_internal = NULL;
	err_t  rc;

	TESTP(root, EBAD);

	/*** Get fields from JSON request */

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	/*** Check if the asked port + protocol is already mapped; if yes, return OK */

	ctl_lock();
	/*@ignore@*/
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_IP_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			ctl_unlock();
			DD("Already mapped port\n");
			return (EOK);
		}
	}
	/*@end@*/
	ctl_unlock();

	/*** If we here, this means that we don't have a record about this port.
	 * So we run UPNP request to our router to test this port */

	/* this function probes the internal port. Is it alreasy mapped, it returns the mapping */
	ip_internal = j_find_ref(ctl->me, JK_IP_INT);
	TESTP_ASSERT(ip_internal, "internal IP is NULL");
	mapping = mp_ports_if_mapped_json(root, asked_port, ip_internal, protocol);

	/*** UPNP request found an existing mapping of asked port + protocol */
	if (NULL != mapping) {

		/* Add this mapping to our internal table table */
		ctl_lock();
		rc = j_arr_add(ports, mapping);
		ctl_unlock();
		if (EOK != rc) {
			DE("Can't add mapping to responce array\n");
			j_rm(mapping);
			mapping = NULL;
			return (EBAD);
		}

		/* Return here. The new port added to internal table in ctl->me.
		   At the end of the process the updated ctl->me object will be sent to the client.
		   And this ctl->me object contains all port mappings.
		   The client will check this host opened ports and see that asked port mapped. */
		return (EOK);
	}

	/*** If we here it means no such mapping exists. Let's map it */
	mapping = mp_ports_remap_any(root, asked_port, protocol);
	TESTP_MES(mapping, EBAD, "Can't map port");

	/*** Ok, port mapped. Now we should update ctl->me->ports hash table */

	ctl_lock();
	rc = j_arr_add(ports, mapping);
	ctl_unlock();
	TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
	return (EOK);
}

/* This function is called when remote machine asks to close a port on this machine */
static err_t mp_mqtt_do_close_port_l(const j_t *root)
{
	/*@temp@*/const control_t *ctl = ctl_get();
	/*@temp@*/const char *asked_port = NULL;
	/*@temp@*/const char *protocol = NULL;
	/*@temp@*/j_t *val = NULL;
	/*@temp@*/j_t *ports = NULL;
	size_t index      = 0;
	int    index_save = 0;
	/*@temp@*/const char *external_port = NULL;
	err_t  rc         = EBAD;

	TESTP(root, EBAD);

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	ctl_lock();
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_PORT_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			external_port = j_find_ref(val, JK_PORT_EXT);
			index_save = index;
			D("Found opened port: %s -> %sd %s\n", asked_port, external_port, protocol);
		}
	}

	ctl_unlock();

	if (NULL == external_port) {
		DE("No such an open port\n");
		return (EBAD);
	}

	rc = mp_mqtt_ticket_responce(root, JV_STATUS_UPDATE, "Starting port removing");
	if (EOK != rc) {
		DE("Can't send ticket");
	}
	/* this function probes the internal port. If it alreasy mapped, it returns the mapping */
	rc = mp_ports_unmap_port(root, asked_port, external_port, protocol);

	if (0 != rc) {
		DE("Can'r remove port \n");
		return (EBAD);
	}

	ctl_lock();
	rc = json_array_remove(ports, index_save);
	ctl_unlock();
	if (EOK != rc) {
		/*@ignore@*/
		DE("Can't remove port from ports: asked index %d, size of ports arrays is %zu", index_save, json_array_size(ports));
		/*@end@*/
	}
	return (EOK);
}

/* 
 * Here we may receive several types of the request: 
 * type: "keepalive" - a source sends its status 
 * type: "reveal" - a new host asks all clients to send information
 */

static err_t mp_mqtt_parse_message_l(const char *uid, j_t *root)
{
	err_t rc = EBAD;
	/*@temp@*/control_t *ctl = ctl_get();

	TESTP(root, EBAD);
	TESTP_GO(uid, end);

	if (EOK != j_test_key(root, JK_TYPE)) {
		DE("No type in the message\n");
		j_print_v(root, "root", __FILE__, __LINE__);
		TESTI_MES(rc, EBAD, "Can't remove json object");
		return (EBAD);
	}

	DDD("Got message type: %s\n", j_find_ref(root, JK_TYPE));

	/**
	 *   1. Remote host sent 'keepalive'.
	 *    'keepalive' it is client's ctl->me structure.
	 */

	/* This is "me" object sent from remote host */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_ME)) {
		j_t        *root_dup;

		/* Find uid of this remote host */
		const char *uid_src  = j_find_ref(root, JK_UID_ME);
		TESTP_MES_GO(uid_src, end, "Can't find 'JK_UID_ME'");

		/* We do not own this object, it will be cleaned in caller */
		root_dup = j_dup(root);
		TESTP(root_dup, EBAD);
		ctl_lock();
		rc = j_replace(ctl->hosts, uid_src, root_dup);
		ctl_unlock();
		if (EOK != rc) {
			DE("Can't replace 'me' message for remote host\n");
		}
		return (rc);
	}

	/**
	 *  2. Remote host sent 'reveal' request.
	 *    We reply with our ctl->me structure.
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_REVEAL)) {
		rc = send_keepalive_l();
		if (EOK != rc) {
			DE("Failed to send keepalive message\n");
		}
		DD("Found reveal\n");
		rc = EOK;
		goto end;
	}

	/**
	 *    3. Remote host sent 'disconnect' request.
	 *    It means the remote client disconnected.
	 *    We should remove its record from ctl->hosts
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_DISCONNECTED)) {
		DD("Found disconnected client\n");
		rc = mp_mqtt_remove_host_l(root);
		goto end;
	}


	/**
	 *  All requests except above must be be dedicated to us.
	 *  From this point we accept only messages for us.
	 */


	if (EOK != j_test(root, JK_DISP_TGT_UID, ctl_uid_get())) {
		rc = 0;
		DDD("This request not for us: JK_UID_DST = %s, us = %s\n",
			j_find_ref(root, JK_DISP_TGT_UID), ctl_uid_get());
		goto end;
	}

	/**
	 * 4. Request "openport" from remote client. The remote client
	 *    wants us to close UPNP port.
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_OPENPORT)) {

		DD("Got 'openport' request\n");

		if (NULL == ctl->rootdescurl) {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_FAIL, "This machine doesn't have UPNP ability");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}
			return (EBAD);
		}

		rc = mp_mqtt_do_open_port_l(root);
		/* 
		 * When the port opened, it added ctl global control_t structure 
		 * We don't need to know what port exactly opened, 
		 * we just send update to all listeners
		 */

		/*** TODO: SEB: Send update to all */
		if (EOK == rc) {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_SUCCESS, "Port opening finished OK");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}

			rrc = send_keepalive_l();
			if (EOK != rrc) {
				DE("Can't send keepalive\n");
			}
		} else {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_FAIL, "Port opening failed");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}
		}

		/*** TODO: SEB: After keepalive send report of "openport" is finished */
		goto end;
	}

	/**
	 * 5. Request "closeport" from remote client. The remote client
	 *    wants us to close UPNP port
	 */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_CLOSEPORT)) {
		DD("Got 'closeport' request\n");

		if (NULL == ctl->rootdescurl) {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_FAIL, "This machine doesn't have UPNP ability");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}
			return (EBAD);
		}

		rc = mp_mqtt_do_close_port_l(root);
		/* 
		 * When the port opened, it added ctl global control_t structure 
		 * We don't need to know what port exactly opened, 
		 * we just send update to all listeners
		 */

		if (EOK == rc) {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_SUCCESS, "Port closing finished OK");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}

			rrc = send_keepalive_l();
			if (EOK != rrc) {
				DE("Can't send keepalive\n");
			}

		} else {
			int rrc = mp_mqtt_ticket_responce(root, JV_STATUS_FAIL, "Port closing failed");
			if (EOK != rrc) {
				DE("Can't send ticket\n");
			}
		}

		/*** TODO: SEB: After keepalive send report of "openport" is finished */
		goto end;
	}


	/*** Message "ticket" */

	/* The user asks for his tickets */
	/* This command we receive from outside */

	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_REQ)) {
		DD("Got 'JV_TYPE_TICKET_REQ' request\n");
		rc = send_request_return_tickets_l(root);
		goto end;
	}

	/* We received a ticket responce. We should keep it localy until shell client grab it */
	if (EOK == j_test(root, JK_TYPE, JV_TYPE_TICKET_RESP)) {
		DD("Got 'JV_TYPE_TICKET_RESP' request\n");
		rc = mp_cli_send_to_cli(root);
		goto end;
	}

	/*** Message "ssh-done" */
	/*
	 * A client responds that "ssh" command executed, ready for ssh connection 
	 */

	/*** Message "sshr" */
	/*
	 * A client wants to connect to us using reversed ssh channel. 
	 * We should open a port and establish reversed SSH connection with the 
	 * senser; then we send notification "sshr-done" about it 
	 */

	/*** Message "sshr-done */
	/*
	 * Responce to "sshr" message, see above
	 */

	if (0 != rc) DE("Unknown type\n");

end:
	DD("Finished, returning\n");
	return (rc);
}

/* Message processor, runs as a thread */
/*@null@*/static void *mp_mqtt_message_processor_pthread(/*@only@*/void *v)
{
	/*@only@*/char **topics = NULL;
	/*@temp@*/ const char *topic = NULL;
	int topics_count = 0;
	int rc           = EBAD;
	/*@temp@*/ const char *uid = NULL;
	/*@only@*/j_t *root = v;

	TESTP(root, NULL);

	//rc = pthread_setname_np(pthread_self(), "mp_main_on_message_processor");

	rc = prctl(PR_SET_NAME, "mp_main_on_message_processor");
	if (0 != rc) {
		DE("Can't set pthread name:\n");
		errno = rc;
		perror("pthread error");
	}

	rc = pthread_detach(pthread_self());
	if (0 != rc) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	/* XXX */
	/* TODO: Here we should call int mp_disp_recv(void *json) */

	/* We pass the received message to dispatcher, and we are finished.
	 * The dispatcher tests the message and calls appropriate function to process it.
	 */

	rc = mp_disp_recv(root);
	TESTI_MES(rc, NULL, "mp_disp_recv returned error");

	return NULL;

	/******************/

	topic = j_find_ref(root, JK_TOPIC);
	if (NULL == topic) {
		DE("No topic in input JSON object\n");
		j_rm(root);
		root = NULL;
		return (NULL);
	}

	rc = mosquitto_sub_topic_tokenise(topic, &topics, &topics_count);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't tokenize topic\n");
		j_rm(root);
		root = NULL;
		return (NULL);
	}

	/* TODO: SEB: Move numner of levels intopic to a define */
	if (topics_count < 4) {
		DE("Expected at least 4 levels of topic, got %d\n", topics_count);
		rc = mosquitto_sub_topic_tokens_free(&topics, topics_count);
		if (EOK != rc) {
			DE("Can't free tokenized topic\n");
		}

		j_rm(root);
		root = NULL;
		return (NULL);
	}

	uid = ctl_uid_get();

	if (0 == strcmp(uid, topics[3])) {
		j_rm(root);
		root = NULL;
		rc = mosquitto_sub_topic_tokens_free(&topics, topics_count);
		if (EOK != rc) {
			DE("Can't free tokenized topic\n");
		}
		return (NULL);
	}

	rc = mp_mqtt_parse_message_l(topics[3], root);
	if (EOK != rc) {
		DE("Can't parse message\n");
		j_print_v(root, "Message is", __FILE__, __LINE__);
	}

	rc = mosquitto_sub_topic_tokens_free(&topics, topics_count);
	if (EOK != rc) {
		DE("Can't free tokenized topic\n");
	}

	j_rm(root);
	root = NULL;
	return (NULL);
}

/* This callback works when a message received */
static void mp_mqtt_on_message_cl(/*@unused@*/struct mosquitto *mosq __attribute__((unused)),
								  /*@unused@*/void *userdata __attribute__((unused)),
								  const struct mosquitto_message *msg)
{
	int       rc;
	pthread_t message_thread;
	/*@temp@*/ j_t *root = NULL;
	if (NULL == msg) {
		DE("msg is NULL!\n");
		return;
	}

	root = j_strn2j(msg->payload, msg->payloadlen);
	if (NULL == root) {
		DE("Can't convert payload to JSON\n");
		return;
	}

	rc = j_add_str(root, JK_TOPIC, msg->topic);
	if (EOK != rc) {
		DE("Can't add topic to JSON\n");
		perror("Can't add topic to JSON");
		abort();
	}

	rc = pthread_create(&message_thread, NULL, mp_mqtt_message_processor_pthread, root);
	if (0 != rc) {
		DE("Can't start mp_main_on_message_processor\n");
		perror("Can't start mp_main_on_message_processor");
		abort();
	}
}

/* This callback works when we are connected to the server */
static void mqtt_connect_callback_l(/*@unused@*/struct mosquitto *mosq __attribute__((unused)),
									/*@unused@*/void *obj __attribute__((unused)),
									/*@unused@*/int result __attribute__((unused)))
{
	int rc;
	/*@temp@*/control_t *ctl = ctl_get();
	printf("connected!\n");
	rc = send_reveal_l();
	if (EOK != rc) {
		DE("Can't send reveal request\n");
	}

	ctl_lock();
	ctl->status = ST_CONNECTED;
	ctl_unlock();
}

/* This callback works when we loose connection to the sever */
static void mp_mqtt_on_disconnect_l_cl(/*@unused@*/struct mosquitto *mosq __attribute__((unused)),
									   /*@unused@*/void *data __attribute__((unused)),
									   int reason)
{
	/*@temp@*/control_t *ctl = NULL;
	if (0 != reason) {
		switch (reason) {
		case MOSQ_ERR_NOMEM:
			DE("Memeory error: no memory\n");
			break;
		case MOSQ_ERR_PROTOCOL:
			DE("Protocol error\n");
			/* SEB: TODO: May we switch protocol?*/
			_exit(EBAD);
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
	j_rm(ctl->hosts);
	ctl->hosts = j_new();
	ctl_unlock();
	DDD("Exit from function\n");
}

/* This hook called when a message published on the server; we should clean the
   buffer related to this message */
static void mp_mqtt_on_publish_cb(/*@unused@*/struct mosquitto *mosq __attribute__((unused)),
								  /*@unused@*/void *data __attribute__((unused)),
								  int buf_id)
{
	int rc;
	/*@only@*/buf_t *buf = NULL;

	/* Sleep a couple of time to let the sending thread to add buffer */
	/* When we sleep, the Kernel scheduler switches to another task and,
	   most probably, the sending thread will manage to add the buffer.
	   I saw a lot of stuck buffers; this several short sleeps seems to fix it*/
	rc = mp_os_usleep(5);
	rc |= mp_os_usleep(10);
	rc |= mp_os_usleep(20);

	if (0 != rc) {
		DE("usleep returned error\n");
		perror("usleep returned error");
	}

	/* We saved buffer when asked to send the message. Now we extract it and free */
	buf = mp_communicate_get_buf_t_from_hash(buf_id);
	if (NULL == buf) {
		DE("Can't find buffer\n");
		return;
	}

	DDD("Found saved buffer %p, going to free one\n", buf);

	rc = mp_communicate_clean_missed_counters_hash();
	if (EOK != rc) {
		DE("Something went wrong when tried to remove stucj counters\n");
	}

	if (EOK != buf_free(buf)) {
		DE("Can't remove buf_t: probably passed NULL pointer?\n");
	}
}

/* This thread is responsible for connection to the broker */
/*@null@*/ static void *mp_mqtt_mosq_pthread(void *arg)
{
	/*@temp@*/control_t *ctl = NULL;
	int   rc               = EBAD;
	int   i;
	/*@temp@*/const char *cert_path                = (char *)arg;
	buf_t *forum_topic_all;
	/*@only@*/ //char *forum_topic_me = NULL;
	/*@only@*/buf_t *forum_topic_me = NULL;
	/*@only@*/buf_t *personal_topic = NULL;
	/*@only@*/buf_t *buf = NULL;

	/* TODO: Client ID, should be assigned on registartion and gotten from config file */
	char  clientid[24]     = "seb";
	/* Instance ID, we dont care, it always random */
	int   counter          = 0;

	rc = pthread_detach(pthread_self());
	if (0 != rc) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}
	/* Set name */
	//rc = pthread_setname_np(pthread_self(), "main_mosq_thread");
	rc = prctl(PR_SET_NAME, "main_mosq_thread");
	if (0 != rc) {
		DE("Can't set pthread name\n");
	}


	TESTP(cert_path, NULL);

	/* Return MOSQ_ERR_SUCCESS - always */
	rc = mosquitto_lib_init();

	ctl = ctl_get();

	forum_topic_all = mp_communicate_forum_topic_all();
	TESTP(forum_topic_all, NULL);

	forum_topic_me = mp_communicate_forum_topic();
	TESTP(forum_topic_me, NULL);

	personal_topic = mp_communicate_private_topic();
	TESTP_GO(personal_topic, end);

	DD("Creating mosquitto client\n");
	ctl_lock();
	if (ctl->mosq) mosquitto_destroy(ctl->mosq);

	ctl->mosq = mosquitto_new(ctl_uid_get(), true, (void *)ctl_uid_get());
	ctl_unlock();
	TESTP_GO(ctl->mosq, end);

	/* TODO: Registration must be another function */
	DD("Setting user / pass\n");
	rc = mosquitto_username_pw_set(ctl->mosq, clientid, PASS);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't set user / pass\n");
		return (NULL);
	}

	DD("Setting TLS\n");
	rc = mosquitto_tls_set(ctl->mosq, cert_path, NULL, NULL, NULL, NULL);

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't set certificate\n");
		ctl->status = ST_STOP;
		goto end;
	}

	/* This is "we connected" callback */
	mosquitto_connect_callback_set((struct mosquitto *)ctl->mosq, mqtt_connect_callback_l);
	/* This is "message received" callback */
	mosquitto_message_callback_set((struct mosquitto *)ctl->mosq, mp_mqtt_on_message_cl);
	/* This is "we disconnected" callback */
	mosquitto_disconnect_callback_set((struct mosquitto *)ctl->mosq, mp_mqtt_on_disconnect_l_cl);
	/* This is "message is published" callback */
	mosquitto_publish_callback_set((struct mosquitto *)ctl->mosq, mp_mqtt_on_publish_cb);

	buf = mp_requests_build_last_will();
	TESTP_MES_GO(buf, end, "Can't build last will");

	/* This message will be sent to all other hosts when out connection is dropped */
	rc = mosquitto_will_set(ctl->mosq, forum_topic_me->data, (int)buf_used(buf), buf->data, 1, false);
	if (EOK != buf_free(buf)) {
		DE("Can't remove buf_t: probably passed NULL pointer?\n");
	}

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't register last will\n");
		DE("Error: %s\n", mosquitto_strerror(rc));
		goto end;
	}

	/* Reconnect timeout */
	rc = mosquitto_reconnect_delay_set(ctl->mosq, 1, 30, false);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't set reconnection delay params\n");
		DE("Error: %s\n", mosquitto_strerror(rc));
		goto end;
	}

	/* And now we ready - let's connect */
	rc = mosquitto_connect(ctl->mosq, SERVER, PORT, 60);

	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Error: %s\n", mosquitto_strerror(rc));
		goto end;
	}

	rc = mosquitto_subscribe(ctl->mosq, NULL, forum_topic_all->data, 0);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can not subsribe\n");
		goto end;
	}

	rc = mosquitto_subscribe(ctl->mosq, NULL, personal_topic->data, 0);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can not subsribe\n");
		goto end;
	}

	rc = mosquitto_loop_start(ctl->mosq);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't start loop\n");
		goto end;
	}

	/* The ST_STOP flag can be set:
	   1. From sugnal trap, when user presses Ctrc-C
	   2. From shell / gui client by sending request to disconnect
	   3. TODO: by signal trap by USR1 / USR2 signal */
	while (ST_STOP != ctl->status) {
		//DDD("Client status is: %d\n", ctl->status);

		if (ST_DISCONNECTED == ctl->status) {
			/* We are disconnected */
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
			} else {
				DD("Finished reconnect\n");
				ctl->status = ST_CONNECTED;
			}
		}
		//DDD("Finished disconnection check\n");

		//DD("Before sleep random millisec\n");
		rc = mp_os_usleep(mp_os_random_in_range(100, 300));
		//DD("After sleep random millisec\n");
		if (0 != rc) {
			DE("usleep returned error\n");
			perror("usleep returned error");
		}
		if (ST_STOP == ctl->status) {
			break;
		}

		if (0 == (counter % 7) && (ST_DISCONNECTED != ctl->status)) {
			// DD("Client connected, sending keepalive message\n");
			rc = send_keepalive_l();
			if (EOK != rc) {}
			counter = 1;
		}

		if (ST_DISCONNECTED == ctl->status) {
			DD("Client in DISCONNECTED status\n");
		}

		for (i = 0; i < 200; i++) {
			if (ST_STOP == ctl->status) {
				DD("I am in stopping mode\n");
				break;
			}
			/* Random time sleep to randomize message sending and spread the server loading */
			rc = mp_os_usleep(mp_os_random_in_range(100, 500));
			if (0 != rc) {
				DE("usleep returned error\n");
				perror("usleep returned error");
			}
		}
		counter++;
	}

	rc = mosquitto_loop_stop(ctl->mosq, true);
	if (MOSQ_ERR_SUCCESS != rc) {
		DE("Can't stop mosquitto loop\n");
		abort();
	}
	ctl_lock();
	mosquitto_destroy(ctl->mosq);
	ctl->mosq = NULL;

	j_rm(ctl->hosts);
	ctl->hosts = j_new();
	ctl_unlock();

	/* We don't check status, it always returns MOSQ_ERR_SUCCESS */
	rc = mosquitto_lib_cleanup();

end:
	buf_free(forum_topic_all);
	buf_free(forum_topic_me);
	buf_free(personal_topic);
	D("Exit thread\n");
	return (NULL);
}

/*@null@*/ void *mp_mqtt_mosq_threads_manager_pthread(void *arg)
{
	/*@temp@*/control_t *ctl = NULL;
	pthread_t mosq_thread_id;
	int       rc;

	rc = pthread_detach(pthread_self());
	if (0 != rc) {
		DE("Can't detach thread\n");
		perror("Can't detach thread");
		abort();
	}

	/* Set name */
	rc = prctl(PR_SET_NAME, "mqpp-app");
	if (0 != rc) {
		DE("Can't set pthread name\n");
	}

	ctl = ctl_get();
	rc = pthread_create(&mosq_thread_id, NULL, mp_mqtt_mosq_pthread, arg);
	if (0 != rc) {
		DE("Can't create thread\n");
		perror("Can't create thread");
		abort();
	}

	while (ST_STOP != ctl->status) {
		/* TODO: WE should restart mosq thread if it terminated */
		mp_os_usleep(500);
	}

	D("Exit\n");
	ctl->status = ST_STOPPED;
	return (NULL);
}

int mp_mqtt_start_module(void *cert_path)
{
	int rc;

	/* Register this module in dispatcher */
	rc = mp_disp_register(MODULE_CONNECTION, mp_module_send, mp_module_recv);

	if (EOK != rc) {
		DE("Can't register in dispatcher\n");
		return (EBAD);
	}

	/* Create connection thread */
	rc = pthread_create(&mosq_thread_id, NULL, mp_mqtt_mosq_threads_manager_pthread, cert_path);
	if (0 != rc) {
		DE("Can't create thread mp_main_mosq_thread_manager\n");
		perror("Can't create thread mp_main_mosq_thread_manager");
		return (EBAD);
	}
	return (EOK);
}
