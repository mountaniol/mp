#ifndef _SEC_SERVER_NETWORK_H_
#define _SEC_SERVER_NETWORK_H_

#define TICKET_SIZE 8

typedef struct port_struct {
	char *port_external;
	char *port_internal;
	char *proto;
} port_t;

extern err_t mp_network_init_network_l(void);

#endif /* _SEC_SERVER_NETWORK_H_ */
