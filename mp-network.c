/*@-skipposixheaders@*/
#define _XOPEN_SOURCE500
//#define _POSIX_C_SOURCE
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
/*@=skipposixheaders@*/

#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-memory.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-dict.h"
#include "mp-limits.h"
#include "mp-os.h"

#define BUF_INTERFACE (1024)
/*@null@*/ static buf_t *mp_network_find_wan_interface(void)
{
	FILE  *fd   = NULL;
	char  *rc   = NULL;
	char  *ptr  = NULL;
	buf_t *buft;

	fd = mp_os_fopen_regular("/proc/net/route", "r");
	TESTP_MES(fd, NULL, "Can't open /proc/net/route\n");

	buft = buf_string(BUF_INTERFACE);
	TESTP_GO(buft, err);

	BUF_DUMP(buft);

	do {
		char *interface = NULL;
		char *dest      = NULL;

		/* Read the next string from /proc/net/route*/
		rc = fgets(buft->data, buf_room(buft), fd);
		if (NULL == rc) {
			break;
		}

		buf_detect_used(buft);

		/* First string is a header, skip it */
		if (0 == strncmp("Iface", buft->data, 5)) {
			continue;
		}

		/* Fields are separated by tabulation. The first field is the interface name */
		interface = strtok_r(buft->data, "\t", &ptr);
		if (NULL == interface) {
			break;
		}

		/* Fields are separated by tabulation. The second field is the default route */
		dest = strtok_r(NULL, "\t", &ptr);
		if (NULL == dest) {
			break;
		}

		//D("Interface = %s, dest = %s\n", interface, dest);

		/* If the default route is all nulls this should be the WAN interface */
		if (0 == strncmp(dest, "00000000", 8)) {
			/* We don't know what is the length of interface name... suppose it  up to 32? */
			size_t len;
			char   *ret = NULL;
			
			len  = strnlen(interface, MP_LIMIT_INTERFACE_NAME_MAX);
			if (len < 1) {
				DE("Can't count interface name\n");
				goto err;
			}
			ret = strndup(interface, len);
			if (NULL == ret) {
				DE("Can't duplicate interface name\n");
				goto err;
			}
			
			buf_clean(buft);
			buf_set_data(buft, ret, len + 1, len);
			//buf_detect_used(buft);
			//buft->used = len -1;

			//D("Found default WAN interface: %s\n", interface);
			if (0 != fclose(fd)) {
				DE("Can't close file!");
				perror("Can't close file");
				abort();
			}

			BUF_DUMP(buft);
			DD("Found wan interface: %s\n", buft->data);
			return (buft);
		}

		/* Read lines until NULL */
	} while (NULL != rc);

	/* If we here it means nothing is found. Probably we don't have any inteface connected to WAN */
err:
	buf_free(buft);
	if (fd) {
		if (0 != fclose(fd)) {
			DE("Can't close file!");
			perror("Can't close file");
			abort();
		}
	}
	return (NULL);
}

/* Find interface connected to WAN and return its IP */
/*@null@*/ static buf_t *mp_network_get_internal_ip(void)
{
	struct ifaddrs *ifaddr        = NULL;
	struct ifaddrs *ifa           = NULL;
	int            s;
	char           host[NI_MAXHOST];

	buf_t          *wan_interface = mp_network_find_wan_interface();
	TESTP_MES(wan_interface, NULL, "No WAN connected inteface");

	if (getifaddrs(&ifaddr) == -1) {
		DE("Failed: getifaddrs\n");
		perror("getifaddrs");
		buf_free(wan_interface);
		return (NULL);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) continue;

		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

		if ((0 == strcmp(ifa->ifa_name, wan_interface->data)) && (ifa->ifa_addr->sa_family == AF_INET)) {
			if (s != 0) {
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				buf_free(wan_interface);
				return (NULL);
			}

			DDD("Interface: %s\n", ifa->ifa_name);
			DDD("Address: %s\n", host);
			freeifaddrs(ifaddr);
			return (wan_interface);
		}
	}

	buf_free(wan_interface);
	freeifaddrs(ifaddr);
	return (NULL);
}

/* Probe network and write all values to global ctl structure */
err_t mp_network_init_network_l()
{
	/*@shared@*/control_t *ctl = ctl_get();
	buf_t *bvar = NULL;

	/* Try to read external IP from Upnp */
	/* We don't even try if no router found */
	if (NULL != ctl->rootdescurl) {
		bvar = mp_ports_get_external_ip();
	}

	/* If can't read from Upnp assign it to 0.0.0.0 - means "can't use Upnp" */
	if (NULL == bvar) {
		DE("Can't get my IP\n");
		bvar = buf_string(8);
		TESTP(bvar, EBAD);
		BUF_DUMP(bvar);
		buf_add(bvar, "0.0.0.0", 7);
		/* If we can't find out external address,
		   it means we ca't communicate with the router
		   and open ports. So, we neither target nor bridge */
		j_add_str(ctl->me, JK_TARGET, JV_NO);
		j_add_str(ctl->me, JK_BRIDGE, JV_NO);
	}

	ctl_lock();
	if (EOK != j_add_str(ctl->me, JK_IP_EXT, bvar->data)) DE("Can't add 'JK_IP_EXT'\n");
	ctl_unlock();
	buf_free(bvar);
	D("My external ip: %s\n", j_find_ref(ctl->me, JK_IP_EXT));
	/* By default the port is "0". It will be changed when we open an port */
	if (EOK != j_add_str(ctl->me, JK_PORT_EXT, JV_BAD)) DE("Can't add 'JK_PORT_EXT'\n");

	bvar = mp_network_get_internal_ip();
	TESTP(bvar, EBAD);
	ctl_lock();
	if (EOK != j_add_str(ctl->me, JK_IP_INT, bvar->data)) DE("Can't add 'JK_IP_INT'\n");
	if (EOK != j_add_str(ctl->me, JK_PORT_INT, JV_BAD)) DE("Can't add 'JK_PORT_INT'\n");
	ctl_unlock();
	buf_free(bvar);
	return (EOK);
}

#ifdef STANDALONE
int main()
{
	char *ip = mp_network_get_internal_ip();
	D("Found IP of WAN interface: %s\n", ip);
	TFREE_STR(ip);
	//linux_find_wan_interface();
	return (0);
}
#endif
