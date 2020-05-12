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

extern int mp_ports_remap_port(/*@only@*/const int external_port, /*@only@*/const int internal_port, /*@only@*/const char *protocol /* "TCP", "UDP" */);
extern int mp_ports_unmap_port(/*@only@*/const json_t *root, /*@only@*/const char *internal_port, /*@only@*/const char *external_port, /*@only@*/const char *protocol);
extern int mp_ports_if_mapped(/*@only@*/const int external_port, /*@only@*/const int internal_port, /*@only@*/const char *local_host, /*@only@*/const char *protocol);
extern int test_if_port_mapped(int internal_port);
extern /*@null@*/ char *mp_ports_get_external_ip(void);

extern /*@null@*/ json_t *mp_ports_if_mapped_json(/*@only@*/const json_t *root, /*@only@*/const char *internal_port, /*@only@*/const char *local_host, /*@only@*/const char *protocol);
extern int mp_ports_scan_mappings(json_t *arr, /*@only@*/const char *local_host);
extern /*@null@*/ json_t *mp_ports_remap_any(/*@only@*/ const json_t *root, /*@only@*/ const char *internal_port, /*@only@*/ const char *protocol /* "TCP", "UDP" */);

extern /*@null@*/ json_t *mp_ports_ssh_port_for_uid(/*@only@*/const char *uid);

#endif /* _SEC_REMAP_PORT_H_ */
