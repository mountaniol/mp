#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-dict.h"
#include "mp-dispatcher.h"

/* Last well sent by server on the client disconnect to all other listeners  */
/*@null@*/ buf_t *mp_requests_build_last_will(void)
{
	const char *name;
	buf_t      *buf  = NULL;
	/*@shared@*/control_t *ctl = ctl_get();

	name = j_find_ref(ctl->me, JK_NAME);

	TESTP_MES(name, NULL, "Got NULL");

	j_t *root = j_new();
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_DISCONNECTED)) goto err;
	/* SEB: TODO: Whay exactly do I send the machine name to remote? */
	if (EOK != j_add_str(root, JK_NAME, name)) goto err;
	if (EOK != j_add_str(root, JK_DISP_SRC_UID, ctl_uid_get())) goto err;

	buf = j_2buf(root);

err:
	j_rm(root);
	if (NULL == buf) DE("Returning NULL lastwill\n");
	return (buf);
}

/* Reveal request: ask all my clients to send information */
/*@null@*/ buf_t *mp_requests_build_reveal(void)
{
	buf_t      *buf  = NULL;
	j_t        *root = NULL;
	const char *name;
	/*@shared@*/control_t *ctl = ctl_get();
	name = j_find_ref(ctl->me, JK_NAME);

	TESTP_MES(name, NULL, "Got NULL");

	root= mp_disp_create_request("ALL", APP_CONNECTION, APP_CONNECTION, 0);
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_REVEAL)) goto err;
	//if (EOK != j_add_str(root, JK_UID_SRC, ctl_uid_get())) goto err;

	buf = j_2buf(root);

err:
	j_rm(root);
	return (buf);
}

/* SEB:TODO: I should just send ctl->me structure as keepalive */
/*@null@*/ buf_t *mp_requests_build_keepalive(void)
{
	/*@shared@*/control_t *ctl = ctl_get();
	buf_t *buf;
	j_t   *root = mp_disp_create_request("ALL", APP_CONNECTION, APP_CONNECTION, 0);
	TESTP_MES(root, NULL, "Can't create request");
	j_print(root, "Created request");

	j_merge(root, ctl->me);
	//return (j_2buf(ctl->me));
	//buf = j_2buf(ctl->me);
	j_print(root, "Added ctl->me");
	buf = j_2buf(root);
	j_rm(root);
	DD("Created keepalive:\n%s\n", buf->data);
	return (buf);
}

/* SEB:TODO: We should form this request in mp-shell */
/*@null@*/ buf_t *mp_requests_open_port(const char *uid_dest, const char *port, const char *protocol)
{
	buf_t *buf  = NULL;
	j_t   *root = NULL;

	TESTP_MES(uid_dest, NULL, "Got NULL");
	TESTP_MES(port, NULL, "Got NULL");

	root = mp_disp_create_request(uid_dest, APP_CONNECTION, APP_CONNECTION, 0);
	TESTP_MES(root, NULL, "Can't create json\n");

	if (EOK != j_add_str(root, JK_TYPE, JV_TYPE_OPENPORT)) goto err;
	if (EOK != j_add_str(root, JK_PORT_INT, port)) goto err;
	if (EOK != j_add_str(root, JK_PROTOCOL, protocol)) goto err;
	//if (EOK != j_add_str(root, JK_UID, uid)) goto err;
	//if (EOK != j_add_str(root, JK_DEST, uid_dest)) goto err;

	buf = j_2buf(root);

err:
	if (NULL != root) {
		j_rm(root);
	}
	return (buf);
}