#ifndef S_SPLINT_S
	#define _POSIX_C_SOURCE 200809L
	#define __USE_POSIX
	#include <limits.h>
	#include <string.h>
	#define STATICLIB
#endif

#ifndef _POSIX_HOST_NAME_MAX
	#define _POSIX_HOST_NAME_MAX	255
#endif

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include <jansson.h>

#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-jansson.h"
#include "mp-dict.h"
#include "mp-main.h"
#include "mp-ctl.h"
#include "mp-os.h"
#include "mp-mqtt-module.h"
#include "mp-dispatcher.h"

/* The miniupnpc library API changed in version 14.
   After API version 14 it accepts additional param "ttl" */
#define MINIUPNPC_API_VERSION_ADDED_TTL 14

#define PORT_STR_LEN 8
#define IP_STR_LEN 46
#define REQ_STR_LEN 256

typedef struct {
	char *s_index;              /* 1 */
	char *map_wan_port;         /* 2 */
	char *map_lan_address;      /* 3 */
	char *map_lan_port;         /* 4 */
	char *map_protocol;         /* 5 */
	char *map_description;      /* 6 */
	char *map_mapping_enabled;  /* 7 */
	char *map_remote_host;      /* 8 */
	// original time, not remaining time :(
	char *map_lease_duration;   /* 9 */
} upnp_req_str_t;

static void upnp_req_str_t_free(/*@only@*/upnp_req_str_t *req)
{
	TFREE_STR(req->s_index);            /* 1 */
	TFREE_STR(req->map_wan_port);       /* 2 */
	TFREE_STR(req->map_lan_address);    /* 3 */
	TFREE_STR(req->map_lan_port);       /* 4 */
	TFREE_STR(req->map_protocol);       /* 5 */
	TFREE_STR(req->map_description);    /* 6 */
	TFREE_STR(req->map_mapping_enabled); /* 7 */
	TFREE_STR(req->map_remote_host);    /* 8 */
	TFREE_STR(req->map_lease_duration); /* 9 */
	TFREE_SIZE(req, sizeof(upnp_req_str_t));
}

static upnp_req_str_t *upnp_req_str_t_alloc(void)
{
	upnp_req_str_t *req = zmalloc(sizeof(upnp_req_str_t));
	TESTP_MES(req, NULL, "Can't allocate upnp_req_str_t structure");

	req->s_index = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->s_index, err);

	req->map_wan_port = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->s_index, err);
	if (NULL == req->map_wan_port) goto err;

	req->map_lan_address = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_lan_address, err);

	req->map_lan_port = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_lan_port, err);

	req->map_protocol = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_protocol, err);

	req->map_description = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_description, err);

	req->map_mapping_enabled = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_mapping_enabled, err);

	req->map_remote_host = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_remote_host, err);

	req->map_lease_duration = zmalloc(REQ_STR_LEN);
	TESTP_GO(req->map_lease_duration, err);

	return (req);
err:
	upnp_req_str_t_free(req);
	return (NULL);
}

/*@null@*/ static struct UPNPDev *mp_ports_upnp_discover(void)
{
	int error = 0;
	return (upnpDiscover(
			2000, // time to wait (milliseconds)
			NULL, // multicast interface (or null defaults to 239.255.255.250)
			NULL, // path to minissdpd socket (or null defaults to /var/run/minissdpd.sock)
			0, // source port to use (or zero defaults to port 1900)
			0, // 0==IPv4, 1==IPv6
#if MINIUPNPC_API_VERSION >= MINIUPNPC_API_VERSION_ADDED_TTL
			2, // TTL should default to 2
#endif
			&error)); // error condition
}

static int upnp_get_generic_port_mapping_entry(struct UPNPUrls *upnp_urls,
											   struct IGDdatas *upnp_data,
											   upnp_req_str_t *req)
{
	int error = UPNP_GetGenericPortMappingEntry(
				upnp_urls->controlURL,
				upnp_data->first.servicetype,
				req->s_index,
				req->map_wan_port,
				req->map_lan_address,
				req->map_lan_port,
				req->map_protocol,
				req->map_description,
				req->map_mapping_enabled,
				req->map_remote_host,
				req->map_lease_duration);

	/* All these errors are real errors */
	switch (error) {
	case UPNPCOMMAND_SUCCESS:
		return (EOK);
		break;
	case UPNPCOMMAND_UNKNOWN_ERROR:
		DE("Error on ports scanning: UPNPCOMMAND_SUCCESS\n");
		return (EBAD);
	case UPNPCOMMAND_INVALID_ARGS:
		DE("Error on ports scanning: UPNPCOMMAND_INVALID_ARGS\n");
		return (EBAD);
	case UPNPCOMMAND_HTTP_ERROR:
		DE("Error on ports scanning: UPNPCOMMAND_HTTP_ERROR\n");
		return (EBAD);
	case UPNPCOMMAND_INVALID_RESPONSE:
		DE("Error on ports scanning: UPNPCOMMAND_INVALID_RESPONSE\n");
		return (EBAD);
		/* TODO: Find exact version where this change became */
#if MINIUPNPC_API_VERSION >= MINIUPNPC_API_VERSION_ADDED_TTL
	case UPNPCOMMAND_MEM_ALLOC_ERROR:
		DE("Error on ports scanning: UPNPCOMMAND_MEM_ALLOC_ERROR\n");
		return (EBAD);
#endif
	default:
		break;
	}
	return (EBAD);
}

/* Discover router and get its root description, if not done yet */
err_t mp_ports_router_root_discover(void)
{
	struct UPNPDev  *upnp_dev = NULL;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	int             status    = -1;

	control_t       *ctl      = ctl_get();
	if (NULL != ctl->rootdescurl) {
		return (EOK);
	}

	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, -1, "UPNP discover failed\n");

	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, NULL, 0);
	freeUPNPDevlist(upnp_dev);
	if (1 != status) {
		return (EBAD);
	}

	ctl->rootdescurl = strndup(upnp_urls.rootdescURL, 4096);
	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* Send upnp request to router, ask to remap "external_port" of the router
   to "internal_port" on this machine */
static err_t mp_ports_remap_port(const int external_port, const int internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */)
{
	int             error               = 0;
	char            *lan_address        = NULL;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	char            s_ext[PORT_STR_LEN];
	char            s_int[PORT_STR_LEN];
	int             status              = -1;

	control_t       *ctl                = ctl_get();

	TESTP(protocol, EBAD);

	lan_address = zmalloc(IP_STR_LEN);
	TESTP(lan_address, EBAD);

	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, lan_address, IP_STR_LEN);

	if (1 != status) {
		DE("UPNP_GetValidIGD failed\n");
		return (EBAD);
	} else {
		DD("UPNP_GetIGDFromUrl returned %d\n", status);
		DD("ctl->rootdescurl is %s\n", ctl->rootdescurl);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	memset(s_int, 0, PORT_STR_LEN);
	status = snprintf(s_ext, PORT_STR_LEN, "%d", external_port);
	if (status < 0) {
		DE("Can't transfortm external port from integer to string\n");
		return (EBAD);
	}

	status = snprintf(s_int, PORT_STR_LEN, "%d", internal_port);
	if (status < 0) {
		DE("Can't transfortm internal port from integer to string\n");
		return (EBAD);
	}

	error = UPNP_AddPortMapping(
			upnp_urls.controlURL,
			upnp_data.first.servicetype,
			s_ext,  // external (WAN) port requested
			s_int,  // internal (LAN) port to which packets will be redirected
			lan_address, // internal (LAN) address to which packets will be redirected
			"Mighty Papa Connector", // text description to indicate why or who is responsible for the port mapping
			protocol, // protocol must be either TCP or UDP
			NULL, // remote (peer) host address or nullptr for no restriction
			"0"); // port map lease duration (in seconds) or zero for "as long as possible"

	TFREE_SIZE(lan_address, IP_STR_LEN);
	FreeUPNPUrls(&upnp_urls);

	if (0 != error) return (EBAD);

	return (EOK);
}

/* Send upnp request to router, ask to remap "internal_port"
   to any external port on the router */
/*@null@*/ j_t *mp_ports_remap_any(/*@temp@*/const j_t *req, /*@temp@*/const char *internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */)
{
	buf_t *reservedPort = NULL;
	int   i             = 0;
	j_t   *resp         = NULL;
	int   rc;

	for (i = 0; i < 3; i++) {
		int e_port = mp_os_random_in_range(1024, 65535);
		int i_port = atoi(internal_port);

		if (i_port < 0 || i_port > 65535) {
			DE("Wrong port number: %d, must be between 0 and 65535\n", i_port);
			return (NULL);
		}

		rc = mp_ports_remap_port(e_port, i_port, protocol);

		if (EOK == rc) {
			reservedPort = buf_sprintf("%d", e_port);
			TESTP(reservedPort, NULL);
			DD("Mapped port: %s -> %s\n", reservedPort->data, internal_port);
			rc = EOK;
			goto end;
		}
	}

	if (EOK != rc) {
		DE("Failed to map port\n");
		return (NULL);
	}

	if (NULL == reservedPort) {
		DE("Not found\n");
		return (NULL);
	}

end:
	/* If there was an error we try to generate some random port and map it */
	// resp = j_new();
	resp = mp_disp_create_response(req);
	TESTP(resp, NULL);
	rc = j_add_str(resp, JK_PORT_EXT, reservedPort->data);
	TESTI_MES(rc, NULL, "Can't add JK_PORT_EXT, reservedPort");
	rc = j_add_str(resp, JK_PORT_INT, internal_port);
	TESTI_MES(rc, NULL, "Can't add JK_PORT_INT, internal_port");
	rc = j_add_str(resp, JK_PROTOCOL, protocol);
	TESTI_MES(rc, NULL, "Can't add JK_PROTOCOL, protocol");
	//TFREE_SIZE(reservedPort, PORT_STR_LEN);
	buf_free(reservedPort);
	return (resp);
}

err_t mp_ports_unmap_port(/*@temp@*/const char *internal_port, /*@temp@*/const char *external_port, /*@temp@*/const char *protocol)
{
	int             error     = 0;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	int             status;
	control_t       *ctl      = ctl_get();

	TESTP(internal_port, EBAD);
	TESTP(external_port, EBAD);
	TESTP(protocol, EBAD);

	DD("internal_port = %s, external_port = %s,  protocol = %s\n", internal_port, external_port, protocol);

	if (EOK != mp_ports_router_root_discover()) {
		DE("Can't discover router\n");
		abort();
	}

	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, NULL, 0);

	if (1 != status) {
		DE("Can't get valid IGD\n");
		return (EBAD);
	}

	// remove port mapping from WAN port 12345 to local host port 24680
	error = UPNP_DeletePortMapping(
			upnp_urls.controlURL,
			upnp_data.first.servicetype,
			external_port,  // external (WAN) port requested
			protocol, // protocol must be either TCP or UDP
			NULL); // remote (peer) host address or nullptr for no restriction



	if (0 != error) {
		DE("Can't delete port %s\n", external_port);
		FreeUPNPUrls(&upnp_urls);
		return (EBAD);
	}

	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* 
 * Test if the port mapping exists. 
 * internal_port: to wich internal port the external port redirected 
 * local_host - IP address of expected machine; if NULL - not checked 
 * Return:
 * port_map_t structure on success (mapping found) 
 * The structure will contain nothing if no mapping found
 * NULL on an error 
 */
/*@null@*//*@only@*/ j_t *mp_ports_if_mapped_json(/*@temp@*/const char *internal_port, /*@temp@*/const char *local_host, /*@temp@*/const char *protocol)
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	int             status;
	/*@temp@*/j_t *mapping    = NULL;
	upnp_req_str_t  *req      = NULL;
	size_t          index     = 0;
	int             rc;
	control_t       *ctl      = ctl_get();

	if (EOK != mp_ports_router_root_discover()) {
		DE("Can't discover router\n");
		abort();
	}

	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, NULL, 0);

	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (NULL);
	}

	// list all port mappings
	req = upnp_req_str_t_alloc();
	if (NULL == req) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(req, NULL, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;

		/*@ignore@*/

#ifndef S_SPLINT_S
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
#endif
		/*@end@*/
		error = upnp_get_generic_port_mapping_entry(&upnp_urls, &upnp_data, req);

		if (error) {
			/* No more ports, and asked port not found in the list */
			upnp_req_str_t_free(req);
			/* Port not mapped at all */
			FreeUPNPUrls(&upnp_urls);
			return (NULL);
		}

		index++;

		/* Check case 1: port mapped but local port is different */
		if (0 == strncmp(req->map_lan_port, internal_port, PORT_STR_LEN) &&
			0 == strncmp(req->map_protocol, protocol, 4) &&
			0 == strncmp(req->map_lan_address, local_host, IP_STR_LEN)) {
			D("Asked mapping is already exists: ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			mapping = j_new();
			if (NULL == mapping) {
				DE("Can't allocate port_map_t\n");
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (NULL);
			}

			rc = j_add_str(mapping, JK_PORT_INT, req->map_lan_port);
			TESTI_MES(rc, NULL, "Can't add JK_PORT_INT, req->map_lan_port");
			rc = j_add_str(mapping, JK_PORT_EXT, req->map_wan_port);
			TESTI_MES(rc, NULL, "Can't add JK_PORT_EXT, req->map_wan_port");
			rc = j_add_str(mapping, JK_PROTOCOL, req->map_protocol);
			TESTI_MES(rc, NULL, "Can't add JK_PROTOCOL, req->map_protocol");

			upnp_req_str_t_free(req);
			FreeUPNPUrls(&upnp_urls);
			return (mapping);
		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (mapping);
}

/* Scan existing mappings from router to this machine and add them to the given array 'arr' */
err_t mp_ports_scan_mappings(j_t *arr, /*@temp@*/const char *local_host)
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	buf_t           *wan_address = NULL;
	int             status;
	j_t             *j_mapping   = NULL;
	upnp_req_str_t  *req         = NULL;
	size_t          index        = 0;
	int             rc;
	control_t       *ctl         = ctl_get();

	DD("ctl->rootdescurl: %s\n", ctl->rootdescurl);

	TESTP_ASSERT(arr, "Bad param NULL");
	TESTP_ASSERT(local_host, "Bad param NULL");

	if (EOK != mp_ports_router_root_discover()) {
		DE("Can't discover router\n");
		abort();
	}

	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, NULL, 0);

	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (EBAD);
	}

	//wan_address = zmalloc(IP_STR_LEN);
	wan_address = buf_string(IP_STR_LEN);
	TESTP_MES(wan_address, EBAD, "Can't allocate memory");

	//BUF_DUMP(wan_address);

	//rc = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, NULL);
	rc = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address->data);
	buf_detect_used(wan_address);
	buf_free(wan_address);

	if (0 != rc) {
		DE("UPNP_GetExternalIPAddress failed\n");
		return (EBAD);
	}

	// list all port mappings
	req = upnp_req_str_t_alloc();
	if (NULL == req) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(req, EBAD, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;

		/*@ignore@*/
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
		/*@end@*/

		error = upnp_get_generic_port_mapping_entry(&upnp_urls, &upnp_data, req);

		if (error) {
			/* This error is a legal situation and happens when no more entries */
			/* No more ports, and asked port not found in the list */
			upnp_req_str_t_free(req);
			/* Port not mapped at all */
			FreeUPNPUrls(&upnp_urls);
			return (EOK);
		}

		index++;

		/* A mapping found */
		if (0 == strncmp(req->map_lan_address, local_host, strnlen(local_host, _POSIX_HOST_NAME_MAX))) {
			D("Found mapping : ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			j_mapping = j_new();
			if (NULL == j_mapping) {
				DE("Can't allocate port_map_t\n");
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (EBAD);
			}

			rc = j_add_str(j_mapping, JK_PORT_INT, req->map_lan_port);
			TESTI_MES(rc, EBAD, "Can't add JK_PORT_INT, req->map_lan_port");
			rc = j_add_str(j_mapping, JK_PORT_EXT, req->map_wan_port);
			TESTI_MES(rc, EBAD, "Can't add JK_PORT_EXT, req->map_wan_port");
			rc = j_add_str(j_mapping, JK_PROTOCOL, req->map_protocol);
			TESTI_MES(rc, EBAD, "Can't add JK_PROTOCOL, req->map_protocol");
			rc = j_arr_add(arr, j_mapping);
			TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* Test if internal port already mapped.
   If it mapped, the external port returned */
/*@null@*/ buf_t *mp_ports_get_external_ip(void)
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	buf_t           *wan_address = NULL;
	int             status       = -1;
	control_t       *ctl         = ctl_get();

	if (EOK != mp_ports_router_root_discover()) {
		DE("Can't discover router\n");
		abort();
	}

	DD("Discovered router:\n%s\n", ctl->rootdescurl);
	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, NULL, 0);

	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (NULL);
	}

	wan_address = buf_string(IP_STR_LEN + 1);
	if (NULL == wan_address) {
		FreeUPNPUrls(&upnp_urls);
		DE("Can't allocate wan_address\n");
		return (NULL);
	}

	buf_detect_used(wan_address);
	//BUF_DUMP(wan_address);

	status = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address->data);
	if (0 != status) {
		DE("Error: can't get IP address\n");
		FreeUPNPUrls(&upnp_urls);
		buf_free(wan_address);
		return (NULL);
	}

	DDD("Got my IP: %s\n", wan_address->data);
	FreeUPNPUrls(&upnp_urls);
	return (wan_address);
}

/*** Local port manipulation */


/*** Dispatcher functionality */
/* This function is called when remote machine asks to open port for imcoming connection */
static err_t mp_ports_do_open_port_l(const j_t *root)
{
	/*@temp@*/ const control_t *ctl = ctl_get();
	/*@temp@*/j_t *mapping = NULL;
	/*@temp@*/const char *asked_port = NULL;
	/*@temp@*/const char *protocol = NULL;
	/*@temp@*/j_t *val = NULL;
	/*@temp@*/j_t *ports = NULL;
	size_t index = 0;
	/*@temp@*/const char *ip_internal = NULL;
	err_t  rc;

	TESTP(root, EBAD);

	/*** Get fields from JSON request */

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	/*** Check if the asked port + protocol is already mapped; if yes, return OK */

	ctl_lock();
	/*@ignore@*/
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_IP_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			ctl_unlock();
			DD("Already mapped port\n");
			return (EOK);
		}
	}
	/*@end@*/
	ctl_unlock();

	/*** If we here, this means that we don't have a record about this port.
	 * So we run UPNP request to our router to test this port */

	/* this function probes the internal port. Is it alreasy mapped, it returns the mapping */
	ip_internal = j_find_ref(ctl->me, JK_IP_INT);
	TESTP_ASSERT(ip_internal, "internal IP is NULL");
	mapping = mp_ports_if_mapped_json(asked_port, ip_internal, protocol);

	/*** UPNP request found an existing mapping of asked port + protocol */
	if (NULL != mapping) {

		/* Add this mapping to our internal table table */
		ctl_lock();
		rc = j_arr_add(ports, mapping);
		ctl_unlock();
		if (EOK != rc) {
			DE("Can't add mapping to responce array\n");
			j_rm(mapping);
			mapping = NULL;
			return (EBAD);
		}

		/* Return here. The new port added to internal table in ctl->me.
		   At the end of the process the updated ctl->me object will be sent to the client.
		   And this ctl->me object contains all port mappings.
		   The client will check this host opened ports and see that asked port mapped. */
		return (EOK);
	}

	/*** If we here it means no such mapping exists. Let's map it */
	mapping = mp_ports_remap_any(root, asked_port, protocol);
	TESTP_MES(mapping, EBAD, "Can't map port");

	/*** Ok, port mapped. Now we should update ctl->me->ports hash table */

	ctl_lock();
	rc = j_arr_add(ports, mapping);
	ctl_unlock();
	TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
	return (EOK);
}

/* This function is called when remote machine asks to close a port on this machine */
static err_t mp_ports_do_close_port_l(const j_t *root)
{
	/*@temp@*/const control_t *ctl = ctl_get();
	/*@temp@*/const char *asked_port = NULL;
	/*@temp@*/const char *protocol = NULL;
	/*@temp@*/j_t *val = NULL;
	/*@temp@*/j_t *ports = NULL;
	size_t index      = 0;
	int    index_save = 0;
	/*@temp@*/const char *external_port = NULL;
	err_t  rc         = EBAD;

	TESTP(root, EBAD);

	asked_port = j_find_ref(root, JK_PORT_INT);
	TESTP_MES(asked_port, EBAD, "Can't find 'port' field");

	protocol = j_find_ref(root, JK_PROTOCOL);
	TESTP_MES(protocol, EBAD, "Can't find 'protocol' field");

	ports = j_find_j(ctl->me, "ports");
	TESTP(ports, EBAD);

	ctl_lock();
	json_array_foreach(ports, index, val) {
		if (EOK == j_test(val, JK_PORT_INT, asked_port) &&
			EOK == j_test(val, JK_PROTOCOL, protocol)) {
			external_port = j_find_ref(val, JK_PORT_EXT);
			index_save = index;
			D("Found opened port: %s -> %sd %s\n", asked_port, external_port, protocol);
		}
	}

	ctl_unlock();

	if (NULL == external_port) {
		DE("No such an open port\n");
		return (EBAD);
	}

	/* this function probes the internal port. If it alreasy mapped, it returns the mapping */
	rc = mp_ports_unmap_port(asked_port, external_port, protocol);

	if (0 != rc) {
		DE("Can'r remove port \n");
		return (EBAD);
	}

	ctl_lock();
	rc = json_array_remove(ports, index_save);
	ctl_unlock();
	if (EOK != rc) {
		/*@ignore@*/
		DE("Can't remove port from ports: asked index %d, size of ports arrays is %zu", index_save, json_array_size(ports));
		/*@end@*/
	}
	return (EOK);
}

/* Dispatcher hook, called when a message for MODULE_PORTS received */
int mp_ports_recv(void *root)
{
	j_t *resp = NULL;
	int rv    = EBAD;

	/* Find out the command and execute it */
	if (EOK == j_test(root, JK_COMMAND, JV_PORTS_OPEN)) {
		rv = mp_ports_do_open_port_l(root);
	}

	if (EOK == j_test(root, JK_COMMAND, JV_PORTS_CLOSE)) {
		rv = mp_ports_do_close_port_l(root);
	}

	/* Construct response and send it */
	resp = mp_disp_create_response(root);

	/* Prepsre dispatcher related fields */
	if (NULL == resp) {
		DE("Failure in dispatcher: can't prepare response\n");
		rv = EBAD;
		goto err;
	}

	/* Add operation status */
	if (EOK == rv) {
		j_add_str(resp, JK_STATUS, JV_OK);
	} else {
		j_add_str(resp, JK_STATUS, JV_BAD);
	}

	/* Send response */
	rv = mp_disp_send(resp);

	if (EOK != rv) {
		DE("Can't send json\n");
	}

err:
	/* We don't need the received request */
	j_rm(root);
	/* We also don't need the response */
	if (NULL != resp) {
		j_rm(resp);
	}

	return (rv);
}

int mp_ports_init_module(void)
{
	int rc;

	/* Register this app in dispatcher: the MODULE_PORT does not implement the "send" function.
	   All messages it produces should be returned to sender */
	rc = mp_disp_register(MODULE_PORTS, NULL, mp_ports_recv);

	if (EOK != rc) {
		DE("Can't register in dispatcher\n");
		return (EBAD);
	}

	return (EOK);
}

#ifdef STANDALONE
int main(int argi, char **argc)
{
	int rc;
	int i_ext;
	int i_int;
	j_t *mapping;

	if (argi < 2) {
		D("Usage: this_util external_port internal_port\n");
		return (-1);
	}

	i_ext = atoi(argc[1]);
	i_int = atoi(argc[2]);

	rc = mp_ports_if_mapped(i_ext, i_int, NULL, "TCP");
	if (rc < 0) {
		D("Error on port mapping\n");
		//delete_port(i_ext);
		return (0);
	}

	if (0 == rc) {
		D("Port already mapped\n");
		//delete_port(i_ext);
		return (0);
	}

	if (1 == rc) {
		D("External port mapped, local host is different\n");
		return (0);
	}

	if (2 == rc) {
		D("External port mapped, internal port different\n");
		return (0);
	}

	mapping = mp_ports_remap_any("22", "TCP");
	if (NULL != mapping) {
		j_rm(mapping);
		D("Port was mapped\n");
	} else {
		DE("Can't map any port\n");
	}

	return (rc);
}
#endif /* STANDALONE */
