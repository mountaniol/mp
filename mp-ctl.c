/*@-skipposixheaders@*/
#include <stdlib.h>
/*@=skipposixheaders@*/
#include "mp-ctl.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-jansson.h"
#include "mp-dict.h"

/*@only@*/control_t *g_ctl = NULL;
err_t ctl_allocate_init(void)
{
	j_t *ports = NULL;
	err_t rc;

	if (NULL != g_ctl) return (EBAD);
	g_ctl = zmalloc(sizeof(control_t));
	TESTP_MES(g_ctl, -1, "Can't allocate control_t struct");
	g_ctl->me = j_new();
	TESTP(g_ctl->me, EBAD);
	ports = j_arr();
	TESTP(ports, EBAD);
	if (EOK != j_add_j(g_ctl->me, "ports", ports)) {
		DE("Can't add array 'ports' to 'me'\n");
		return (EBAD);
	}

	g_ctl->hosts = j_new();
	TESTP_MES(g_ctl->me, -1, "Can't allocate json object");

	g_ctl->buffers = j_new();
	TESTP_MES(g_ctl->buffers, -1, "Can't allocate json object");

	g_ctl->buf_missed = j_new();
	TESTP_MES(g_ctl->buf_missed, -1, "Can't allocate json object");

	g_ctl->tickets_out = j_arr();
	TESTP(g_ctl->tickets_out, -1);

	g_ctl->tickets_in = j_arr();
	TESTP(g_ctl->tickets_out, -1);

	rc = j_add_str(g_ctl->me, JK_TYPE, JV_TYPE_ME);
	TESTI_MES(rc, EBAD, "Can't JK_TYPE = JV_TYPE_ME");

	g_ctl->status = ST_START;
	return (sem_init(&g_ctl->lock, 0, 1));
}

err_t cli_destoy()
{
	ctl_lock();
	j_rm(g_ctl->hosts);
	j_rm(g_ctl->config);
	j_rm(g_ctl->tickets_out);
	j_rm(g_ctl->buffers);
	j_rm(g_ctl->buf_missed);
	TFREE(g_ctl->rootdescurl);
	free(g_ctl);
	g_ctl = NULL;
	return (EOK);
}

void ctl_lock()
{
	int rc;
	sem_getvalue(&g_ctl->lock, &rc);
	if (rc > 1) {
		DE("Semaphor count is too high: %d > 1\n", rc);
		abort();
	}

	rc = sem_wait(&g_ctl->lock);
	if (0 != rc) {
		DE("Can't wait on semaphore; abort\n");
		perror("Can't wait on semaphore; abort");
		abort();
	}
}

void ctl_unlock()
{
	int rc;
	sem_getvalue(&g_ctl->lock, &rc);
	if (rc > 0) {
		DE("Tried to unlock not locked semaphor\n");
		abort();
	}
	rc = sem_post(&g_ctl->lock);
	if (0 != rc) {
		DE("Can't unlock ctl->lock");
		perror("Can't unlock ctl->lock: abort");
		abort();
	}
}

/*@temp@*//*@notnull@*/control_t *ctl_get(void)
{
	TESTP_ASSERT(g_ctl, "NULL!");
	return (g_ctl);
}

/*@temp@*//*@notnull@*/control_t *ctl_get_locked(void)
{
	ctl_lock();
	TESTP_ASSERT(g_ctl, "NULL!");
	return (g_ctl);
}


/*** Interface function for most important control_t fields ***/
/*@temp@*//*@notnull@*/ const char *ctl_uid_get(void)
{
	const char *uid;
	TESTP_ASSERT(g_ctl->me, "NULL!");
	uid = j_find_ref(g_ctl->me, JK_UID_ME);
	return (uid);
}

void ctl_uid_set(const char *uid)
{
	int rc;
	TESTP_ASSERT(g_ctl->me, "NULL!");
	TESTP_ASSERT(uid, "NULL!");
	rc = j_add_str(g_ctl->me, JK_UID_ME, uid);
	if (EOK != rc) {
		DE("Can't set uid\n");
		abort();
	}
}

/*@temp@*//*@notnull@*/const char *ctl_user_get()
{
	const char *user;
	TESTP_ASSERT(g_ctl->me, "NULL!");
	user = j_find_ref(g_ctl->me, JK_USER);
	TESTP_ASSERT(user, "NULL!");
	return (user);
}

void ctl_user_set(const char *user)
{
	int rc;
	TESTP_ASSERT(g_ctl->me, "NULL!");
	TESTP_ASSERT(user, "NULL!");
	rc = j_add_str(g_ctl->me, JK_USER, user);
	if (EOK != rc) {
		DE("Can't set uid\n");
		abort();
	}
}

