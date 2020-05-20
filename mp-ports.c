#include <string.h>
#include <limits.h>
#define STATICLIB
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-jansson.h"
#include "mp-dict.h"
#include "mp-main.h"
#include "mp-ctl.h"
#include "mp-os.h"

/* The miniupnpc library API changed in version 14.
   After API version 14 it accepts additional param "ttl" */
#define MINIUPNPC_API_VERSION_ADDED_TTL 14

#define PORT_STR_LEN 8
#define IP_STR_LEN 46
#define REQ_STR_LEN 256

typedef struct upnp_request_strings {
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
	TFREE(req->s_index);            /* 1 */
	TFREE(req->map_wan_port);       /* 2 */
	TFREE(req->map_lan_address);    /* 3 */
	TFREE(req->map_lan_port);       /* 4 */
	TFREE(req->map_protocol);       /* 5 */
	TFREE(req->map_description);    /* 6 */
	TFREE(req->map_mapping_enabled); /* 7 */
	TFREE(req->map_remote_host);    /* 8 */
	TFREE(req->map_lease_duration); /* 9 */
	TFREE(req);
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
	struct UPNPDev *upnp_dev = NULL;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	int status = -1;

	control_t *ctl = ctl_get();
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

	ctl->rootdescurl = strdup(upnp_urls.rootdescURL);
	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* Send upnp request to router, ask to remap "external_port" of the router
   to "internal_port" on this machine */
static err_t mp_ports_remap_port(const int external_port, const int internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */)
{
	int error = 0;
	char *lan_address = NULL;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	char s_ext[PORT_STR_LEN];
	char s_int[PORT_STR_LEN];
	int status = -1;

	control_t *ctl = ctl_get();

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

	TFREE(lan_address);
	FreeUPNPUrls(&upnp_urls);

	if (0 != error) return (EBAD);

	return (EOK);
}

/* Send upnp request to router, ask to remap "internal_port"
   to any external port on the router */
/*@null@*/ json_t *mp_ports_remap_any(/*@temp@*/const json_t *req, /*@temp@*/const char *internal_port, /*@temp@*/const char *protocol /* "TCP", "UDP" */)
{
	char *reservedPort = NULL;
	//char reservedPort[PORT_STR_LEN];
	int i = 0;
	json_t *resp = NULL;
	int rc;

	for (i = 0; i < 3; i++) {
		int e_port = mp_os_random_in_range(1024, 65535);
		int i_port = atoi(internal_port);

		if (i_port < 0 || i_port > 65535) {
			DE("Wrong port number: %d, must be between 0 and 65535\n", i_port);
			return (NULL);
		}

		rc = mp_main_ticket_responce(req, JV_STATUS_UPDATE, "Trying to map a port");
		if (EOK != rc) {
			DE("Can't send ticket\n");
		}

		rc = mp_ports_remap_port(e_port, i_port, protocol);

		if (EOK == rc) {
			reservedPort = zmalloc(PORT_STR_LEN);
			TESTP(reservedPort, NULL);
			rc = snprintf(reservedPort, PORT_STR_LEN, "%d", e_port);
			if (rc < 0) {
				DE("Can't transform port from integer to string\n");
				TFREE(reservedPort);
				return (NULL);
			}
			DD("Mapped port: %s -> %s\n", reservedPort, internal_port);
			rc = EOK;
			break;
		}
	}

	if (EOK != rc) {
		DE("Failed to map port\n");
		return (NULL);
	}
	/* If there was an error we try to generate some random port and map it */
	resp = j_new();
	TESTP(resp, NULL);
	rc = j_add_str(resp, JK_PORT_EXT, reservedPort);
	TESTI_MES(rc, NULL, "Can't add JK_PORT_EXT, reservedPort");
	rc = j_add_str(resp, JK_PORT_INT, internal_port);
	TESTI_MES(rc, NULL, "Can't add JK_PORT_INT, internal_port");
	rc = j_add_str(resp, JK_PROTOCOL, protocol);
	TESTI_MES(rc, NULL, "Can't add JK_PROTOCOL, protocol");
	TFREE(reservedPort);
	return (resp);
}

err_t mp_ports_unmap_port(/*@temp@*/const json_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *external_port, /*@temp@*/const char *protocol)
{
	int error = 0;
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	int status;
	int rc;
	control_t *ctl = ctl_get();

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

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Found IGD device, asking port remove");
	if (EOK != rc) DD("Can't add ticket\n");

	// remove port mapping from WAN port 12345 to local host port 24680
	error = UPNP_DeletePortMapping(
			upnp_urls.controlURL,
			upnp_data.first.servicetype,
			external_port,  // external (WAN) port requested
			protocol, // protocol must be either TCP or UDP
			NULL); // remote (peer) host address or nullptr for no restriction

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Finished port remove");
	if (EOK != rc) DD("Can't add ticket\n");


	if (0 != error) {
		DE("Can't delete port %s\n", external_port);
		rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Port remove: failed");
		if (EOK != rc) DD("Can't add ticket\n");
		FreeUPNPUrls(&upnp_urls);
		return (EBAD);
	} else {
		rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Port remove: success");
		if (EOK != rc) DD("Can't add ticket\n");
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
/*@null@*//*@only@*/ json_t *mp_ports_if_mapped_json(/*@temp@*/const json_t *root, /*@temp@*/const char *internal_port, /*@temp@*/const char *local_host, /*@temp@*/const char *protocol)
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char s_ext[PORT_STR_LEN];
	int status;
	/*@temp@*/json_t *mapping = NULL;
	upnp_req_str_t *req = NULL;
	size_t index = 0;
	int rc;
	control_t *ctl = ctl_get();

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Beginning check of opened ports");
	if (EOK != rc) {
		DE("Can't send ticket\n");
	}

	if (EOK != mp_ports_router_root_discover()) {
		DE("Can't discover router\n");
		abort();
	}

	status = UPNP_GetIGDFromUrl(ctl->rootdescurl, &upnp_urls, &upnp_data, NULL, 0);

	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (NULL);
	}
	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Found UPNP device");
	if (EOK != rc) {
		DE("Can't send ticket\n");
	}

	memset(s_ext, 0, PORT_STR_LEN);

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Contacted UPNP device");
	if (EOK != rc) {
		DE("Can't send ticket\n");
	}
	

	// list all port mappings
	req = upnp_req_str_t_alloc();
	if (NULL == req) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(req, NULL, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;

		/*@unused@*/
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
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

/* Scan existing mappings to this machine and add them to the given array 'arr' */
err_t mp_ports_scan_mappings(json_t *arr, /*@temp@*/const char *local_host)
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char *wan_address = NULL;
	int status;
	json_t *mapping = NULL;
	upnp_req_str_t *req = NULL;
	size_t index = 0;
	int rc;
	control_t *ctl = ctl_get();

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

	wan_address = zmalloc(IP_STR_LEN);
	TESTP_MES(wan_address, EBAD, "Can't allocate memory");

	rc = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);
	TFREE(wan_address);

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
			D("Asked mapping is already exists: ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			mapping = j_new();
			if (NULL == mapping) {
				DE("Can't allocate port_map_t\n");
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (EBAD);
			}

			rc = j_add_str(mapping, JK_PORT_INT, req->map_lan_port);
			TESTI_MES(rc, EBAD, "Can't add JK_PORT_INT, req->map_lan_port");
			rc = j_add_str(mapping, JK_PORT_EXT, req->map_wan_port);
			TESTI_MES(rc, EBAD, "Can't add JK_PORT_EXT, req->map_wan_port");
			rc = j_add_str(mapping, JK_PROTOCOL, req->map_protocol);
			TESTI_MES(rc, EBAD, "Can't add JK_PROTOCOL, req->map_protocol");
			rc = j_arr_add(arr, mapping);
			TESTI_MES(rc, EBAD, "Can't add mapping to responce array");
		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* Test if internal port already mapped.
   If it mapped, the external port returned */
/*@null@*/ char *mp_ports_get_external_ip()
{
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char *wan_address = NULL;
	int status = -1;
	control_t *ctl = ctl_get();

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

	wan_address = zmalloc(IP_STR_LEN);
	if (NULL == wan_address) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(wan_address, NULL, "Can't allocate wan_address\n");

	status = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);
	if (0 != status) {
		DE("Error: can't get IP address\n");
		FreeUPNPUrls(&upnp_urls);
		return (NULL);
	}
	DDD("Got my IP: %s\n", wan_address);
	FreeUPNPUrls(&upnp_urls);
	return (wan_address);
}

/*** Local port manipulation ****/

/* Find ip and port for ssh connection to UID */
/*@null@*/ json_t *mp_ports_ssh_port_for_uid(/*@temp@*/const char *uid)
{
	json_t *root = NULL;
	json_t *host = NULL;
	const char *key;
	/*@shared@*/control_t *ctl = ctl_get_locked();

	json_object_foreach(ctl->hosts, key, host) {
		if (EOK == strcmp(key, uid)) {
			json_t *ports = NULL;
			json_t *port;
			size_t index;
			/* Found host */
			ports = j_find_j(host, "ports");

			json_array_foreach(ports, index, port) {
				/* For now we search for intenal port 22 and protocol TCP */
				if (EOK == j_test(port, JK_PORT_INT, "22") && EOK == j_test(port, JK_PROTOCOL, "TCP")) {
					int rc;
					root = j_new();
					if (NULL == root) {
						DE("Can't get root\n");
						ctl_unlock();
						return (NULL);
					}
					/* We need external port */
					rc = j_cp(port, root, JK_PORT_EXT);
					if (EOK != rc) {
						DE("Can't add root, JK_PORT_EXT\n");
						ctl_unlock();
						return (NULL);
					}
					/* And IP */
					rc = j_cp(host, root, JK_IP_EXT);
					if (EOK != rc) {
						DE("Can't add root, JK_IP_EXT\n");
						ctl_unlock();
						return (NULL);
					}
				} /* if */
			} /* End of json_array_foreach */
		}
	}
	ctl_unlock();
	DDD("returning root = %p\n", root);
	return (root);
}

#ifdef STANDALONE
int main(int argi, char **argc)
{
	int rc;
	int i_ext;
	int i_int;
	json_t *mapping;

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
