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

extern int mp_ports_remap_port(const int external_port, const int internal_port, const char *protocol /* "TCP", "UDP" */);
extern int mp_ports_unmap_port(const char *internal_port, const char *external_port,  const char *protocol);
extern int mp_ports_if_mapped(int external_port, int internal_port, char *local_host, char *protocol);
extern int test_if_port_mapped(int internal_port);
extern char *mp_ports_get_external_ip(void);

extern json_t *mp_ports_if_mapped_json(const char *internal_port, const char *local_host, const char *protocol);
extern int mp_ports_scan_mappings(json_t *arr, const char *local_host);
extern json_t *mp_ports_remap_any(const char *internal_port, const char *protocol /* "TCP", "UDP" */);

extern json_t *mp_ports_ssh_port_for_uid(const char *uid);

#endif /* _SEC_REMAP_PORT_H_ */
