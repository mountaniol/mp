#include <jansson.h>
#include "jansson.h"
#include "buf_t.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-network.h"
#include "mp-dict.h"

/* SEB:TODO: What exactly the params? */
/* Connect request: ask remote host to open port for ssh connection */
/*@unused@*/ buf_t *mp_requests_build_connect(const char *uid_remote, const char *user_remote)
{
	buf_t *buf = NULL;

	TESTP_MES(user_remote, NULL, "Got NULL");

	json_t *root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_CONNECT)) goto err;
	//if (EOK != j_add_str(root, JK_USER, user_remote)) goto err;
	//if (EOK != j_add_str(root, JK_UID, uid_remote)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid_remote)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		if (EOK != j_rm(root)) DE("Couldn't remove json object\n");
	}
	return (buf);
}

/* Last well sent by server on the client disconnect to all other listeners  */
buf_t *mp_requests_build_last_will()
{
	const char *name;
	const char *uid_me;
	buf_t *buf = NULL;
	control_t *ctl = ctl_get();

	name = j_find_ref(ctl->me, JK_NAME);
	uid_me = j_find_ref(ctl->me, JK_UID_ME);

	TESTP_MES(name, NULL, "Got NULL");

	json_t *root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_DISCONNECT)) goto err;
	/* SEB: TODO: Whay exactly do I send the machine name to remote? */
	if (EOK != j_add_str(root, JK_NAME, name)) goto err;
	if (EOK != j_add_str(root, JK_UID_SRC, uid_me)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object");
	}
	if (NULL == buf) DE("Returning NULL lastwill\n");
	return (buf);
}

/* Reveal request: ask all my clients to send information */
buf_t *mp_requests_build_reveal()
{
	buf_t *buf = NULL;
	json_t *root = NULL;
	const char *uid;
	const char *name;
	control_t *ctl = ctl_get();
	name = j_find_ref(ctl->me, JK_NAME);
	uid = j_find_ref(ctl->me, JK_UID_ME);

	TESTP_MES(name, NULL, "Got NULL");

	root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_REVEAL)) goto err;
	if (EOK != j_add_str(root, JK_UID_SRC, uid)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

/* ssh request this client want to connect to client "uid"
   The client "uid" should open a port and return it in "ssh-done" responce */
/*@unused@*/ buf_t *mp_requests_build_ssh(const char *uid)
{
	buf_t *buf = NULL;
	json_t *root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	/* Type of the message */
	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_SSH)) goto err;

	/* To whom this message */
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

/* "ssh-done" responce: this client opened a port and informes
   about it. This is responce to "ssh" requiest */
/*@unused@*/ buf_t *mp_requests_build_ssh_done(const char *uid, const char *ip, const char *port)
{
	buf_t *buf = NULL;
	json_t *root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	/* Type of the message */
	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_SSH_DONE)) goto err;
	/* To whom */
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;
	/* This is my external IP */
	if (EOK != j_add_str(root, JK_IP_EXT, ip)) goto err;
	/* This is my external port */
	if (EOK != j_add_str(root, JK_PORT_EXT, port)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

/* sshr: This client can't connect to the remote client "uid", but the
   remote client "uid" can connect here. So this client asks to
   open ssh reverse connection.
   How do we know that we can not connect to the remote "uid" client?
   Simple: its IP address is "0.0.0.0" because this "uid" client
   already tried to open a port and failed.
   Another scenario: the remote client "uid" succeeded to open
   port, but we cannot connect. In this case we move on to "sshr" requiest */
/*@unused@*/ buf_t *mp_requests_build_sshr(const char *uid, const char *ip, const char *port)
{
	buf_t *buf = NULL;
	json_t *root = NULL;

	root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	/* Type of the message */
	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_SSHR)) goto err;

	/* To whom */
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;
	/* This is my external IP */
	if (EOK != j_add_str(root, JK_IP_EXT, ip)) goto err;
	/* This is my external port */
	if (EOK != j_add_str(root, JK_PORT_EXT, port)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

/* sshr-done: we opened reversed channel to the client "uid".
   The remote client "uid" may use "localport" on its side
   to establish connection */
/*@unused@*/ buf_t *mp_requests_build_sshr_done(const char *uid, const char *localport, const char *status)
{
	buf_t *buf = NULL;
	json_t *root = NULL;

	root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	/* Type of the message */
	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_SSHR_DONE)) goto err;
	/* To whom */
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;
	/* This is my external IP */
	if (EOK != j_add_str(root, JK_PORT_INT, localport)) goto err;
	/* Operation status */
	if (EOK != j_add_str(root, JK_STATUS, status)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

/* SEB:TODO: I should just send ctl->me structure as keepalive */
buf_t *mp_requests_build_keepalive()
{
	control_t *ctl = ctl_get();
	return (j_2buf(ctl->me));
}

/* SEB:TODO: We should form this request in mp-shell */
buf_t *mp_requests_open_port(const char *uid, const char *port, const char *protocol)
{
	buf_t *buf = NULL;
	json_t *root = NULL;

	TESTP_MES(uid, NULL, "Got NULL");
	TESTP_MES(port, NULL, "Got NULL");

	root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_OPENPORT)) goto err;
	if (EOK != j_add_str(root, JK_PORT_INT, port)) goto err;
	if (EOK != j_add_str(root, JK_PROTOCOL, protocol)) goto err;
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}

buf_t *mp_requests_close_port(const char *uid, const char *port, const char *protocol)
{
	buf_t *buf = NULL;
	json_t *root = NULL;

	TESTP_MES(uid, NULL, "Got NULL");
	TESTP_MES(port, NULL, "Got NULL");

	root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_CLOSEPORT)) goto err;
	if (EOK != j_add_str(root, JK_PORT_INT, port)) goto err;
	if (EOK != j_add_str(root, JK_PROTOCOL, protocol)) goto err;
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	if (EOK != j_add_str(root, JK_DEST, uid)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		int rc = j_rm(root);
		TESTI_MES(rc, NULL, "Can't remove json object 'root'\n");
	}
	return (buf);
}




