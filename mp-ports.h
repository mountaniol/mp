#ifndef _SEC_REMAP_PORT_H_
#define _SEC_REMAP_PORT_H_

#include "mp-jansson.h"

typedef struct port_map_struct {
	char *port_external;
	char *port_internal;
	char *local_ip;
	char *router_ip;
	char *protocol;
} port_map_t;

extern err_t mp_ports_unmap_port(/*@temp@*/const json_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *external_port, /*@temp@*/const char *protocol);
extern err_t test_if_port_mapped(int internal_port);
extern /*@null@*/ char *mp_ports_get_external_ip(void);

extern /*@null@*/ /*@only@*/ json_t *mp_ports_if_mapped_json(/*@temp@*/const json_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *local_host, /*@temp@*/const char *protocol);
extern err_t mp_ports_scan_mappings(json_t *arr, /*@temp@*/const char *local_host);
extern /*@null@*/ json_t *mp_ports_remap_any(/*@temp@*/const json_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */);
extern /*@null@*/ json_t *mp_ports_ssh_port_for_uid(/*@temp@*/const char *uid);

#endif /* _SEC_REMAP_PORT_H_ */
