#ifndef MP_DISPATCHER_H_
#define MP_DISPATCHER_H_

/* Ticket is unsigned 32 bit integer */
typedef uint32_t ticket_t;

/* Application types */
/* WARNING! If you modified this array,
   you should also modify function mp_disp_app_name() ! */
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
typedef int (*mp_disp_cb_t)(void *);
/* A ticket receiver function: the first argument it the JSON structure, the second is private data */
typedef int (*mp_dist_ticket_cb_t)(void *, void *);

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

typedef enum {
	MES_DEST_ERR = -1,
	MES_DEST_ME = 0,
	MES_DEST_REMOTE = 1,
	MES_SRC_ME = 2,
	MES_SRC_REMOTE = 3
} mes_dest_e;


/**
 * @author Sebastian Mountaniol (05/09/2020)
 * @brief Register new APP handlers in dispatcher
 * @param app_id Application ID, from app_type_e list
 * @param func_send "Send" function, can't be NULL
 * @param func_recv "Receive function" can be NULL
 * @return int EOK on success, EBAD on error
 * @details
 */
int mp_disp_register(size_t app_id, mp_disp_cb_t func_send, mp_disp_cb_t func_recv);

/**
 * @author se (05/09/2020)
 * @func int mp_disp_recv(void *json)
 * @brief Generic "send message" function
 * @param json Json object to send
 * @return int EOK on success, EBAD on failure
 * @details This function owns the JSON object. The sender
 *  		should not crelease it
 */

int mp_disp_send(void *json);

/**
 * @author Sebastian Mountaniol (05/09/2020)
 * @brief Generic receive function. User should not call it.
 * @param json Recevice JSON object
 * @return int EOK on success, EBAD on error
 * @details
 */
int mp_disp_recv(void *json);

/**
 * @author Sebastian Mountaniol (30/08/2020)
 * @func int mp_disp_prepare_request(void *json, char *target_host, app_type_e dest, app_type_e source, ticket_t ticket)
 * @brief Fill JSON object with the fields needed for the dispatcher work
 * @param void * "json": JSON object to fill
 * @param char * "target_host": The UID of the tagert machine
 * @param app_type_e "dest_app": Application ID this message
 *  				 dedicated to
 * @param app_type_e "src_app": Application ID of the app
 *  				 sending this message
 * @param ticket_t "ticket": A ticket; if the tocket == 0, a new ticket will be generated
 * @return int EOK on success, < 0 on error
 * @details
 */
extern int mp_disp_prepare_request(void *json, const char *target_host, app_type_e dest_app, app_type_e src_app, ticket_t ticket);

/**
 * @author se (04/09/2020)
 * @func j_t* mp_disp_create_request(const char *target_host, app_type_e dest, app_type_e source, ticket_t ticket)
 * @brief This function creates a request and fills it
 * @param target_host Target machine UID (siurce machine filled
 *  				  for this machin)
 * @param dest Target application ID
 * @param source Source application ID
 * @param ticket Ticket number, created if it 0
 * @return j_t* Pointer to JSON structure, NULL on error
 */
extern j_t *mp_disp_create_request(const char *target_host, app_type_e dest, app_type_e source, ticket_t ticket);

/**
 * @author se (04/09/2020)
 * @func int mp_disp_prepare_response(void *json_req, void *json_resp)
 * @brief Fill JSON response with dispatcher data extracted from
 *  	  requiest
 * @param json_req Request to use
 * @param json_resp Response to fill
 * @return int EOK on success, EBAD on error
 */
extern int mp_disp_prepare_response(const void *json_req, void *json_resp);

/**
 * @author se (04/09/2020)
 * @func j_t* mp_disp_create_response(void *json_req)
 * @brief Create and fill response message from request message
 * @param json_req Request message to used for response
 * @return j_t* JSON object on success, NULL on error
 */
extern j_t *mp_disp_create_response(const void *json_req);

/**
 * @author se (05/09/2020)
 * @func j_t* mp_disp_create_ticket_answer(void *json_req)
 * @brief Create and fill ticket notification (progress status)
 *  	  message from request message
 * @param json_req Request to use answer to
 * @return j_t* Pointer to the created JSON object; NULL on
 *  	   error
 */
extern j_t *mp_disp_create_ticket_answer(void *json_req);

#endif /* MP_DISPATCHER_H_ */
