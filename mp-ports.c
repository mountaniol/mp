#include <string.h>
#include <stdlib.h>
#define STATICLIB
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include "mp-common.h"
#include "mp-debug.h"
#include "mp-memory.h"
#include "mp-ports.h"
#include "mp-jansson.h"
#include "mp-dict.h"
#include "mp-main.h"
#include "mp-ctl.h"

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

static int upnp_req_str_t_free(upnp_req_str_t *req)
{
	TESTP_MES(req, -1, "Got NULL");

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
	return (0);
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

static struct UPNPDev *mp_ports_upnp_discover(void)
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


/* Send upnp request to router, ask to remap "external_port" of the router
   to "internal_port" on this machine */
int mp_ports_remap_port(const int external_port, const int internal_port, const char *protocol)
{
	size_t index = 0;
	int error = 0;
	struct UPNPDev *upnp_dev = NULL;
	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	char wan_address[IP_STR_LEN];
	char s_ext[PORT_STR_LEN];
	char s_int[PORT_STR_LEN];
	int status = -1;
	upnp_req_str_t *req = NULL;

	TESTP(protocol, EBAD);

	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, -1, "UPNP discover failed\n");

	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	freeUPNPDevlist(upnp_dev);

	if (1 != status) {
		DE("UPNP_GetValidIGD failed\n");
		return (-1);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	memset(s_int, 0, PORT_STR_LEN);
	snprintf(s_ext, PORT_STR_LEN, "%d", external_port);
	snprintf(s_int, PORT_STR_LEN, "%d", internal_port);
	UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);

	// add a new TCP port mapping from WAN port 12345 to local host port 24680
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

	if (0 != error) {
		DE("Can't map port %d -> %d\n", external_port, internal_port);
		FreeUPNPUrls(&upnp_urls);
		return (EBAD);
	}
	// list all port mappings
	index = 0;
	req = upnp_req_str_t_alloc();
	TESTP_MES(req, -1, "Can't alloc upnp_req_str_t");

	while (1) {
		int l_ext_port;
		int l_int_port;

#ifndef S_SPLINT_S /* For splint parser: it doesn't recognize %zu */
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
#endif
		error = UPNP_GetGenericPortMappingEntry(
				upnp_urls.controlURL,
				upnp_data.first.servicetype,
				req->s_index,
				req->map_wan_port,
				req->map_lan_address,
				req->map_lan_port,
				req->map_protocol,
				req->map_description,
				req->map_mapping_enabled,
				req->map_remote_host,
				req->map_lease_duration);

		if (UPNPCOMMAND_SUCCESS != error) {
			upnp_req_str_t_free(req);
			FreeUPNPUrls(&upnp_urls);
			return (0);
		}

		index++;

		l_ext_port = atoi(req->map_wan_port);
		l_int_port = atoi(req->map_lan_port);

		if (l_ext_port == external_port && l_int_port == internal_port) {
			DDD("Asked mapping done\n");
		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (-1);
}

/* Send upnp request to router, ask to remap "internal_port"
   to any external port on the router */
json_t *mp_ports_remap_any(json_t *req, const char *internal_port, const char *protocol)
{
	//size_t index = 0;
	struct UPNPDev *upnp_dev = NULL;
	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	char wan_address[IP_STR_LEN];
	char s_ext[PORT_STR_LEN];
	//char s_int[PORT_STR_LEN];
	char reservedPort[PORT_STR_LEN];
	int status = -1;
	//upnp_req_str_t *req = NULL;
	int i_port = 0;
	int i = 0;
	json_t *root = NULL;
	int rc;

	TESTP_MES(internal_port, NULL, "Got NULL\n");
	TESTP_MES(protocol, NULL, "Got NULL\n");

	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, NULL, "UPNP discover failed\n");

	rc = mp_main_ticket_responce(req, JV_STATUS_UPDATE, "Found UPNP device");

	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	freeUPNPDevlist(upnp_dev);
	rc = mp_main_ticket_responce(req, JV_STATUS_UPDATE, "Contacted UPNP device");

	if (1 != status) {
		DE("UPNP_GetValidIGD failed\n");
		return (NULL);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	memset(reservedPort, 0, PORT_STR_LEN);
	//memset(s_int, 0, PORT_STR_LEN);
	//snprintf(s_int, PORT_STR_LEN, "%d", internal_port);
	status = UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);
	if (0 != status) {
		DE("UPNP_GetExternalIPAddress returned error\n");
		FreeUPNPUrls(&upnp_urls);
		return (NULL);
	}

	rc = mp_main_ticket_responce(req, JV_STATUS_UPDATE, "Got IP of UPNP device");

	for (i = 0; i < 3; i++) {
		int error;

		while (i_port <= 1024 || i_port >= 65535) {
			i_port = rand();
		}

		rc = mp_main_ticket_responce(req, JV_STATUS_UPDATE, "Trying to map a port");
		error = mp_ports_remap_port(i_port, atoi(internal_port), protocol);

		if (0 == error) {
			snprintf(reservedPort, PORT_STR_LEN, "%d", i_port);
			DD("Mapped port: %s -> %s\n", reservedPort, internal_port);
			break;
		}
	}

	/* If there was an error we try to generate some random port and map it */
	root = j_new();
	j_add_str(root, JK_PORT_EXT, reservedPort);
	j_add_str(root, JK_PORT_INT, internal_port);
	j_add_str(root, JK_PROTOCOL, protocol);
	FreeUPNPUrls(&upnp_urls);
	return (root);
#if 0 /* SEB DEADCODE 04/05/2020 10:10  */
	/* The port is remapped */
	mapping = port_map_t_alloc();
	if (NULL == mapping) {
		DE("Can't allocate port_map_t\n");
		//upnp_req_str_t_free(req);
		return (NULL);
	}

	mapping->port_internal = internal_port;
	mapping->port_external = strndup(reservedPort, PORT_STR_LEN);
	//upnp_req_str_t_free(req);
#endif /* SEB DEADCODE 04/05/2020 10:10 */

	return (root);
}

#if 0 /* SEB 28/04/2020 16:46  */
r = UPNP_AddAnyPortMapping(urls->controlURL, data->first.servicetype,
						   eport, iport, iaddr, description,
						   proto, remoteHost, leaseDuration, reservedPort);
if(r==UPNPCOMMAND_SUCCESS)
eport = reservedPort;
else
printf("AddAnyPortMapping(%s, %s, %s) failed with code %d (%s)\n",
	   eport, iport, iaddr, r, strupnperror(r));

#endif /* SEB 28/04/2020 16:46 */

int mp_ports_unmap_port(json_t *root, const char *internal_port, const char *external_port, const char *protocol)
{
	int error = 0;
	struct UPNPDev *upnp_dev;

	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;
	int status;
	int rc;

	TESTP(internal_port, EBAD);
	TESTP(external_port, EBAD);
	TESTP(protocol, EBAD);

	DD("internal_port = %s, external_port = %s,  protocol = %s\n", internal_port, external_port, protocol);
	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, -1, "UPNP discover failed\n");

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Found UPNP device");
	if (EOK != rc) DD("Can't add ticket\n");
	
	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	freeUPNPDevlist(upnp_dev);
	if (1 != status) {
		DE("Can't get valid IGD\n");
		return (-1);
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
	return (0);
}

/* 
 * Test if the port mapping exists. 
 * external_port: port opened on router 
 * internal_port: to wich internal port the external port redirected 
 * local_host - IP address of expected machine; if NULL - not checked 
 * Return: 
 * 0 it all right 
 * 1 if port mapped but local host is different 
 * 2 if port mapped but internal port is different 
 * 3 if port not mapped at all 
 * -1 on an error 
 */
int mp_ports_if_mapped(int external_port, int internal_port, char *local_host, char *protocol)
{
	struct UPNPDev *upnp_dev;
	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	// get the external (WAN) IP address
	char wan_address[IP_STR_LEN];
	char s_ext[PORT_STR_LEN];
	char s_int[PORT_STR_LEN];
	int status;
	upnp_req_str_t *req = NULL;
	size_t index = 0;

	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, -1, "UPNP discover failed\n");
	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));

	freeUPNPDevlist(upnp_dev);
	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (-1);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	memset(s_int, 0, PORT_STR_LEN);

	snprintf(s_ext, PORT_STR_LEN, "%d", external_port);
	snprintf(s_int, PORT_STR_LEN, "%d", internal_port);
	UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);

	// list all port mappings
	req = upnp_req_str_t_alloc();
	TESTP_MES(req, -1, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;
		int l_ext_port;
		int l_int_port;

#ifndef S_SPLINT_S /* For splint: it has a problem with %zu */
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
#endif
		error = UPNP_GetGenericPortMappingEntry(
				upnp_urls.controlURL,
				upnp_data.first.servicetype,
				//std::to_string(index).c_str()   ,
				req->s_index,
				req->map_wan_port,
				req->map_lan_address,
				req->map_lan_port,
				req->map_protocol,
				req->map_description,
				req->map_mapping_enabled,
				req->map_remote_host,
				req->map_lease_duration);

		if (error) {
			/* No more ports, and asked port not found in the list */
			upnp_req_str_t_free(req);
			/* Port not mapped at all */
			FreeUPNPUrls(&upnp_urls);
			return (3);
		}

		if (0 != strcmp(req->map_protocol, protocol)) {
			continue;
		}

		index++;

		l_ext_port = atoi(req->map_wan_port);
		l_int_port = atoi(req->map_lan_port);

		/* port mapped but internal port is different */
		if (l_ext_port == external_port && l_int_port != internal_port) {
			upnp_req_str_t_free(req);
			FreeUPNPUrls(&upnp_urls);
			return (2);
		}

		/* Check case 1: port mapped but local port is different */
		if (l_ext_port == external_port && l_int_port == internal_port) {
			D("Asked mapping done: ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			if ((0 != local_host) && (0 != strncmp(req->map_lan_port, local_host, REQ_STR_LEN))) {
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (1);
			}

			/* If we here it means that local_host is the same as asked, or it is NULL and should not be testes*/
			upnp_req_str_t_free(req);
			FreeUPNPUrls(&upnp_urls);
			return (0);
		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (-1);
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
json_t *mp_ports_if_mapped_json(json_t *root, const char *internal_port, const char *local_host, const char *protocol)
{
	struct UPNPDev *upnp_dev;
	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char wan_address[IP_STR_LEN];
	char s_ext[PORT_STR_LEN];
	int status;
	json_t *mapping = NULL;
	upnp_req_str_t *req = NULL;
	size_t index = 0;
	int rc;


	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Beginning check of opened ports");
	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, NULL, "UPNP discover failed\n");
	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Found UPNP device");

	freeUPNPDevlist(upnp_dev);
	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (NULL);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);

	rc = mp_main_ticket_responce(root, JV_STATUS_UPDATE, "Contacted UPNP device");

	// list all port mappings
	req = upnp_req_str_t_alloc();
	if (NULL == req) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(req, NULL, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;

#ifndef S_SPLINT_S /* FOr splint: it has a problem with %zu */
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
#endif
		error = UPNP_GetGenericPortMappingEntry(
				upnp_urls.controlURL,
				upnp_data.first.servicetype,
				//std::to_string(index).c_str()   ,
				req->s_index,
				req->map_wan_port,
				req->map_lan_address,
				req->map_lan_port,
				req->map_protocol,
				req->map_description,
				req->map_mapping_enabled,
				req->map_remote_host,
				req->map_lease_duration);

		if (error) {
			/* No more ports, and asked port not found in the list */
			upnp_req_str_t_free(req);
			/* Port not mapped at all */
			FreeUPNPUrls(&upnp_urls);
			return (NULL);
		}

		index++;

		/* Check case 1: port mapped but local port is different */
		if (0 == strncmp(req->map_lan_port, internal_port, strlen(internal_port)) &&
			0 == strncmp(req->map_protocol, protocol, strlen(protocol)) &&
			0 == strncmp(req->map_lan_address, local_host, strlen(protocol))) {
			D("Asked mapping is already exists: ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			mapping = j_new();
			if (NULL == mapping) {
				DE("Can't allocate port_map_t\n");
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (NULL);
			}

			j_add_str(mapping, JK_PORT_INT, req->map_lan_port);
			j_add_str(mapping, JK_PORT_EXT, req->map_wan_port);
			j_add_str(mapping, JK_PROTOCOL, req->map_protocol);

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
int mp_ports_scan_mappings(json_t *arr, const char *local_host)
{
	struct UPNPDev *upnp_dev;
	char lan_address[IP_STR_LEN];
	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char wan_address[IP_STR_LEN];
	char s_ext[PORT_STR_LEN];
	int status;
	json_t *mapping = NULL;
	upnp_req_str_t *req = NULL;
	size_t index = 0;


	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, EBAD, "UPNP discover failed\n");
	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	freeUPNPDevlist(upnp_dev);

	if (1 != status) {
		DE("Error on UPNP_GetValidIGD: status = %d\n", status);
		return (EBAD);
	}

	memset(s_ext, 0, PORT_STR_LEN);
	UPNP_GetExternalIPAddress(upnp_urls.controlURL, upnp_data.first.servicetype, wan_address);

	// list all port mappings
	req = upnp_req_str_t_alloc();
	if (NULL == req) FreeUPNPUrls(&upnp_urls);
	TESTP_MES(req, EBAD, "Can't allocate upnp_req_str_t");

	while (1) {
		int error;

#ifndef S_SPLINT_S /* FOr splint: it has a problem with %zu */
		snprintf(req->s_index, PORT_STR_LEN, "%zu", index);
#endif
		error = UPNP_GetGenericPortMappingEntry(
				upnp_urls.controlURL,
				upnp_data.first.servicetype,
				//std::to_string(index).c_str()   ,
				req->s_index,
				req->map_wan_port,
				req->map_lan_address,
				req->map_lan_port,
				req->map_protocol,
				req->map_description,
				req->map_mapping_enabled,
				req->map_remote_host,
				req->map_lease_duration);

		if (error) {
			/* No more ports, and asked port not found in the list */
			upnp_req_str_t_free(req);
			/* Port not mapped at all */
			FreeUPNPUrls(&upnp_urls);
			return (EBAD);
		}

		index++;

		/* A mapping found */
		if (0 == strncmp(req->map_lan_address, local_host, strlen(local_host))) {
			D("Asked mapping is already exists: ext port %s -> %s:%s\n", req->map_wan_port, req->map_lan_address, req->map_lan_port);

			mapping = j_new();
			if (NULL == mapping) {
				DE("Can't allocate port_map_t\n");
				upnp_req_str_t_free(req);
				FreeUPNPUrls(&upnp_urls);
				return (EBAD);
			}

			j_add_str(mapping, JK_PORT_INT, req->map_lan_port);
			j_add_str(mapping, JK_PORT_EXT, req->map_wan_port);
			j_add_str(mapping, JK_PROTOCOL, req->map_protocol);
			j_arr_add(arr, mapping);

		}
	}

	upnp_req_str_t_free(req);
	FreeUPNPUrls(&upnp_urls);
	return (EOK);
}

/* Test if internal port already mapped.
   If it mapped, the external port returned */
char *mp_ports_get_external_ip()
{
	struct UPNPDev *upnp_dev = NULL;

	struct UPNPUrls upnp_urls;
	struct IGDdatas upnp_data;

	char lan_address[IP_STR_LEN];
	char *wan_address = NULL;
	int status = -1;

	upnp_dev = mp_ports_upnp_discover();
	TESTP_MES(upnp_dev, NULL, "UPNP discover failed\n");

	status = UPNP_GetValidIGD(upnp_dev, &upnp_urls, &upnp_data, lan_address, (int)sizeof(lan_address));
	freeUPNPDevlist(upnp_dev);

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
json_t *mp_ports_ssh_port_for_uid(const char *uid)
{
	json_t *root = NULL;
	//json_t *val = NULL;
	json_t *host = NULL;
	control_t *ctl = ctl_get();
	const char *key;

	json_object_foreach(ctl->hosts, key, host) {
		if (EOK == strcmp(key, uid)) {
			json_t *ports = NULL;
			json_t *port;
			int index;
			/* Found host */
			ports = j_find_j(host, "ports");

			json_array_foreach(ports, index, port) {

				/* For now we search for intenal port 22 and protocol TCP */
				if (EOK == j_test(port, JK_PORT_INT, "22") && EOK == j_test(port, JK_PROTOCOL, "TCP")) {
					root = j_new();
					/* We need external port */
					j_cp(port, root, JK_PORT_EXT);
					/* And IP */
					j_cp(host, root, JK_IP_EXT);
				} /* if */
			} /* End of json_array_foreach */
		}
	}
#if 0

	json_t *ports = j_find_j(ctl->me, "ports");
	TESTP(ports, NULL);

	json_object_foreach(ports, key, val) {
		json_t *port;
		int index;
		json_t *host_ports = NULL;

		j_print(val, "Testin ports");

		/* Here we are inside of a host */
		if (EOK != j_test(val, JK_UID, uid)) {
			DDD("Looking for UID %s\n", uid);
			j_print(val, "Looking for UID - not found - continue");
			continue;
		}

		DDD("Found UID %s\n", uid);
		host_ports = j_find_j(val, "ports");
		TESTP(host_ports, NULL);

		json_array_foreach(host_ports, index, port) {
			/* For now we search for port 22 */
			if (EOK == j_test(port, JK_PORT_INT, "22") && EOK == j_test(port, JK_PROTOCOL, "TCP")) {
				root = j_new();
				j_cp(port, root, JK_PORT_EXT);
				j_cp(port, root, JK_PORT_EXT);
				j_cp(port, root, JK_PROTOCOL);
			} /* if */
		} /* End of json_array_foreach */
	} /* json_object_foreach*/

	/* We founf open ssh port. Now we need IP address */
	if (NULL != root) {
		json_object_foreach(ctl->hosts, key, val) {
			if (EOK == j_test(val, JK_UID, uid)) {
				const char *ip = j_find_ref(val, JK_IP_EXT);

				j_add_str(root, JK_IP_EXT, ip);
			} /* json_object_foreach */
		} /* NULL != root */
	}
#endif

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
		D("Port mapped\n");
	} else {
		DE("Can't map any port\n");
	}

	return (rc);
}
#endif /* STANDALONE */
