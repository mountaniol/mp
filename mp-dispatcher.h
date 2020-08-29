#ifndef MP_DISPATCHER_H_
#define MP_DISPATCHER_H_

/* Ticket is unsigned 32 bit integer */
typedef uint32_t ticket_t;

/* Application types */
typedef enum app_type_enum {
	APP_CONNECTION = 1,
	APP_REMOTE,
	APP_CONFIG,
	APP_PORTS,
	APP_SHELL,
	APP_TUNNEL,
	APP_GUI,
	APP_SECURITY,
	APP_MPFS,
	APP_MAX
} app_type_e;

/* Sender / receiver function: the only argument it gets is the JSON structure */
typedef int (*mp_disp_cb_t) (void *);
/* A ticket receiver function: the first argument it the JSON structure, the second is private data */
typedef int (*mp_dist_ticket_cb_t) (void *, void *);

typedef struct dispatcher_struct {
	size_t disp_id;
	mp_disp_cb_t send;
	mp_disp_cb_t recv;
} disp_t;

typedef struct disp_ticket_struct {
	ticket_t ticket;    /* Ticket ID */
	mp_disp_cb_t recv;  /* Function of this ticket receiver */
	void *priv;         /* Private data (optional) */
} disp_ticket_t;

int mp_disp_register(size_t src_id, mp_disp_cb_t *func_send, mp_disp_cb_t *func_recv);
int mp_disp_is_mes_for_me(void *json);
int mp_disp_send(void *json);
int mp_disp_recv(void *json);
int mp_disp_prepare_request(void *json, char *target, app_type_e dest, app_type_e source, int ticket);
int mp_disp_prepare_response(void *json_req, void *json_resp);

#endif /* MP_DISPATCHER_H_ */
