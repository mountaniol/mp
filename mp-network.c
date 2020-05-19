#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include "mp-common.h"
#include "buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-memory.h"
#include "mp-network.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-dict.h"

/*@null@*/ static char *mp_network_find_wan_interface(void)
{
	FILE *fd = NULL;
	char *buf = NULL;
	char *rc = NULL;
	char *ptr = NULL;

	fd = fopen("/proc/net/route", "r");
	TESTP_MES(fd, NULL, "Can't open /proc/net/route\n");

	buf = zmalloc(1024);
	TESTP_GO(buf, err);

	do {
		char *interface = NULL;
		char *dest = NULL;

		/* Read the next string from /proc/net/route*/
		rc = fgets(buf, 1024, fd);
		if (NULL == rc) {
			break;
		}

		/* First string is a header, skip it */
		if (0 == strncmp("Iface", buf, 5)) {
			continue;
		}

		/* Fields are separated by tabulation. The first field is the interface name */
		interface = strtok_r(buf, "\t", &ptr);
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
			char *ret = strdup(interface);
			//D("Found default WAN interface: %s\n", interface);
			TFREE(buf);
			if (0 != fclose(fd)) {
				DE("Can't close file!");
				perror("Can't close file");
				abort();
			}

			return (ret);
		}

		/* Read lines until NULL */
	} while (NULL != rc);

	/* If we here it means nothing is found. Probably we don't have any inteface connected to WAN */
err:
	TFREE(buf);
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
/*@null@*/ static char *mp_network_get_internal_ip(void)
{
	struct ifaddrs *ifaddr = NULL;
	struct ifaddrs *ifa = NULL;
	int s;
	char host[NI_MAXHOST];

	char *wan_interface = mp_network_find_wan_interface();

	TESTP_MES(wan_interface, NULL, "No WAN connected inteface");

	if (getifaddrs(&ifaddr) == -1) {
		DE("Failed: getifaddrs\n");
		perror("getifaddrs");
		TFREE(wan_interface);
		return (NULL);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL) continue;

		s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

		if ((0 == strcmp(ifa->ifa_name, wan_interface)) && (ifa->ifa_addr->sa_family == AF_INET)) {
			char *ret;
			if (s != 0) {
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				TFREE(wan_interface);
				return (NULL);
			}

			DDD("Interface: %s\n", ifa->ifa_name);
			DDD("Address: %s\n", host);
			ret = strdup(host);
			TFREE(wan_interface);
			freeifaddrs(ifaddr);
			return (ret);
		}
	}

	TFREE(wan_interface);
	freeifaddrs(ifaddr);
	return (NULL);
}

/* Probe network and write all values to global ctl structure */
err_t mp_network_init_network_l()
{
	/*@shared@*/control_t *ctl = ctl_get();
	char *var = NULL;

	/* Try to read external IP from Upnp */
	/* We don't even try if no router found */
	if (NULL != ctl->rootdescurl) {
		var = mp_ports_get_external_ip();
	}

	/* If can't read from Upnp assign it to 0.0.0.0 - means "can't use Upnp" */
	if (NULL == var) {
		DE("Can't get my IP\n");
		var = strdup("0.0.0.0");
	}

	ctl_lock();
	if (EOK != j_add_str(ctl->me, JK_IP_EXT, var)) DE("Can't add 'JK_IP_EXT'\n");
	ctl_unlock();
	TFREE(var);
	D("My external ip: %s\n", j_find_ref(ctl->me, JK_IP_EXT));
	/* By default the port is "0". It will be changed when we open an port */
	if (EOK != j_add_str(ctl->me, JK_PORT_EXT, JV_NO_PORT)) DE("Can't add 'JK_PORT_EXT'\n");

	var = mp_network_get_internal_ip();
	TESTP(var, EBAD);
	ctl_lock();
	if (EOK != j_add_str(ctl->me, JK_IP_INT, var)) DE("Can't add 'JK_IP_INT'\n");
	if (EOK != j_add_str(ctl->me, JK_PORT_INT, JV_NO_PORT)) DE("Can't add 'JK_PORT_INT'\n");
	ctl_unlock();
	TFREE(var);
	return (EOK);
}

#ifdef STANDALONE
int main()
{
	char *ip = mp_network_get_internal_ip();
	D("Found IP of WAN interface: %s\n", ip);
	TFREE(ip);
	//linux_find_wan_interface();
	return (0);
}
#endif
