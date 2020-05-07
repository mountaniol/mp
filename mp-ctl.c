#include "mp-ctl.h"
#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-jansson.h"
#include "mp-dict.h"

control_t *g_ctl = NULL;
int ctl_allocate_init(void)
{
	json_t *ports = NULL;

	if (NULL != g_ctl) return (-1);
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
	
	g_ctl->tickets = j_arr();
	TESTP(g_ctl->tickets, -1);

	j_add_str(g_ctl->me, JK_TYPE, JV_TYPE_ME);
	g_ctl->status = ST_START;
	return (sem_init(&g_ctl->lock, 0, 1));
}

int ctl_lock(control_t *ctl)
{
	TESTP_ASSERT(ctl, "NULL!");
	sem_wait(&ctl->lock);
	return (0);
}

int ctl_unlock(control_t *ctl)
{
	TESTP_ASSERT(ctl, "NULL!");
	sem_post(&ctl->lock);
	return (EOK);
}

control_t *ctl_get(void)
{
	return (g_ctl);
}

control_t *ctl_get_locked(void)
{
	return (g_ctl);
}

