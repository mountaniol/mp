#ifndef _SEC_REMAP_PORT_H_
#define _SEC_REMAP_PORT_H_

//#include <jansson.h>
#include "mp-jansson.h"

typedef struct port_map_struct {
	char *port_external;
	char *port_internal;
	char *local_ip;
	char *router_ip;
	char *protocol;
} port_map_t;

extern err_t mp_ports_router_root_discover(void); 
extern err_t mp_ports_unmap_port(/*@temp@*/const char *internal_port, /*@temp@*/const char *external_port, /*@temp@*/const char *protocol);
extern err_t test_if_port_mapped(int internal_port);
extern /*@null@*/ buf_t *mp_ports_get_external_ip(void);

extern /*@null@*/ /*@only@*/ j_t *mp_ports_if_mapped_json(/*@temp@*/const char *internal_port, /*@temp@*/const char *local_host, /*@temp@*/const char *protocol);
extern err_t mp_ports_scan_mappings(j_t *arr, /*@temp@*/const char *local_host);
extern /*@null@*/ j_t *mp_ports_remap_any(/*@temp@*/const j_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */);
//extern /*@null@*/ j_t *mp_ports_ssh_port_for_uid(/*@temp@*/const char *uid);
extern int mp_ports_init_module(void);

#endif /* _SEC_REMAP_PORT_H_ */
