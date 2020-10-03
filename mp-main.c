#ifndef S_SPLINT_S
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <unistd.h>
#include <signal.h>

#include "openssl/ssl.h"
#include "openssl/err.h"

#endif

#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-ctl.h"
#include "mp-jansson.h"
#include "mp-ports.h"
#include "mp-cli.h"
#include "mp-config.h"
#include "mp-network.h"
#include "mp-security.h"
#include "mp-os.h"
#include "mp-dict.h"
#include "mp-mqtt-app.h"

static void mp_main_print_info_banner()
{
	/*@temp@*/const control_t *ctl = ctl_get();
	printf("=======================================\n");
	printf("Router IP:\t%s:%s\n", j_find_ref(ctl->me, JK_IP_EXT), j_find_ref(ctl->me, JK_PORT_EXT));
	printf("Local IP:\t%s:%s\n", j_find_ref(ctl->me, JK_IP_INT), j_find_ref(ctl->me, JK_PORT_INT));
	printf("Name of comp:\t%s\n", j_find_ref(ctl->me, JK_NAME));
	printf("Name of user:\t%s\n", ctl_user_get());
	printf("UID of user:\t%s\n", ctl_uid_get());
	printf("=======================================\n");
}

static err_t mp_main_init_security()
{
	err_t     rc;
	control_t *ctl = ctl_get();

	rc = mp_config_probe_rsa_priv();
	if (EOK == rc) {
		ctl->rsa_priv = mp_config_load_rsa_priv();
	}

	if (NULL == ctl->rsa_priv) {
		void *rsa;
		rsa = mp_security_generate_rsa_pem_RSA(2048);
		if (NULL == rsa) {
			DE("Can't generate RSA key\n");
			return (EBAD);
		}

		rc = mp_config_save_rsa_keys(rsa);
		if (EOK != rc) {
			DE("Can't save RSA private and public key\n");
			return (EBAD);
		}

		ctl->rsa_priv = mp_config_load_rsa_priv();
		if (NULL == ctl->rsa_priv) {
			DE("Error: generated RSA key but can't load private key");
			return (EBAD);
		}
	}

	ctl->rsa_pub = mp_config_load_rsa_pub();
	if (NULL == ctl->rsa_pub) {
		DE("Can't load RSA public key");
		return (EBAD);
	}

	rc = mp_config_probe_x509();
	if (EOK == rc) {
		ctl->x509 = mp_config_load_X509();
	}

	if (NULL == ctl->x509) {
		void *x509 = mp_security_generate_x509(ctl->rsa_priv);
		if (NULL == x509) {
			DE("Can't generate X509 cetrificate\n");
			return (EBAD);
		}

		rc = mp_config_save_rsa_x509(x509);
		if (EOK != rc) {
			DE("Can't save X509 certificate\n");
			return (EBAD);
		}

		ctl->x509 = mp_config_load_X509();
		if (NULL == ctl->x509) {
			DE("Error: generated and saved X509 certificate but can't load it\n");
			return (EBAD);
		}
	}

	ctl->ctx = mp_security_init_server_tls_ctx();
	if (NULL == ctl->ctx) {
		DE("Can't create OpelSSL CTX object\n");
		return (EBAD);
	}

	return (EOK);
}

static void mp_main_signal_handler(int sig)
{
	/*@temp@*/control_t *ctl;
	if (SIGINT != sig) {
		DD("Got signal: %d, ignore\n", sig);
		return;
	}

	ctl = ctl_get();
	DD("Found signal: %d, setting stop\n", sig);
	ctl->status = ST_STOP;
	while (ST_STOPPED != ctl->status) {
		DD("Waiting all threads to finish\n");
		int rc = mp_os_usleep(200);
		if (0 != rc) {
			DE("usleep returned error\n");
			perror("usleep returned error");
		}
	}
	_exit(0);
}

/* This function complete ctl->me init.
   We call this function after config file loaded.
   If we run for the first time, the config file
   doesn't exist. In this case we create all fields we
   need for run, and later [see main() before threads started]
   we dump these values to config.*/
static err_t mp_main_complete_me_init(void)
{
	err_t rc;
	/*@temp@*/char *var = NULL;
	/*@temp@*/const control_t *ctl = ctl_get();

	/* SEB: TODO: This should be defined by user from first time config */
	if (EOK != j_test_key(ctl->me, JK_USER)) {
		/* TODO: Check user name len, must be no more than JK_USER_MAX_LEN */
		ctl_user_set("seb");
	}

	/* Try to read hostname of this machine. */

	if (EOK != j_test_key(ctl->me, JK_NAME)) {
		var = mp_os_get_hostname();
		if (NULL == var) {
			var = strdup("can-not-resolve-name");
		}

		rc = j_add_str(ctl->me, JK_NAME, var);
		TFREE_STR(var);
		TESTI_MES(rc, EBAD, "Could not add string for 'name'\n");
	}

	if (EOK != j_test_key(ctl->me, JK_UID_ME)) {
		var = mp_os_generate_uid(ctl_user_get());
		TESTP_MES(var, EBAD, "Can't generate UID\n");

		ctl_uid_set(var);
		TFREE_STR(var);
		var = NULL;
	}

	if (EOK != j_test_key(ctl->me, JK_SOURCE)) {
		rc = j_add_str(ctl->me, JK_SOURCE, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_SOURCE");
	}

	if (EOK != j_test_key(ctl->me, JK_TARGET)) {
		rc = j_add_str(ctl->me, JK_TARGET, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_TARGET");
	}

	if (EOK != j_test_key(ctl->me, JK_BRIDGE)) {
		rc = j_add_str(ctl->me, JK_BRIDGE, JV_YES);
		TESTI_MES(rc, EBAD, "Can't add JK_BRIDGE");
	}

	printf("UID: %s\n", ctl_uid_get());
	return (EOK);
}

int main(/*@unused@*/int argc __attribute__((unused)), char *argv[])
{
	/*@only@*/char *cert_path = NULL;
	/*@temp@*/control_t *ctl = NULL;
	pthread_t cli_thread_id;
	// pthread_t mosq_thread_id;
	/*@temp@*/j_t *ports;

	int       rc             = EOK;

	/* Set ABORT state: abort on error */
	buf_set_abort();

	/* Add CANARY to every buffer */
	buf_default_flags(BUF_T_CANARY);

	rc = ctl_allocate_init();
	TESTI_MES(rc, EBAD, "Can't allocate and init control struct\n");

	/* We don't need it locked - nothing is running yet */
	ctl = ctl_get();

	if (EOK != mp_config_load() || NULL == ctl->config) {
		DDD("Can't read config\n");
	}

	rc = mp_main_complete_me_init();
	TESTI_MES(rc, EBAD, "Can't finish 'me' init\n");


	buf_t *hash = mp_security_system_footprint();
	buf_free(hash);

	/* Start CLI thread as fast as we can */
	rc = pthread_create(&cli_thread_id, NULL, mp_cli_pthread, NULL);
	if (0 != rc) {
		DE("Can't create thread mp_cli_thread\n");
		perror("Can't create thread mp_cli_thread");
		cli_destoy();
		abort();
	}

	/* Find our router */
	mp_ports_router_root_discover();

	if (0 != mp_network_init_network_l()) {
		DE("Can't init network\n");
		cli_destoy();
		return (EBAD);
	}

	/* TODO: We should have this certifivate in the config file */
	cert_path = strdup(argv[1] );
	if (NULL == cert_path) {
		printf("arg1 should be path to certificate\n");
		cli_destoy();
		return (EBAD);
	}

	ports = j_find_j(ctl->me, "ports");
	if (NULL != ctl->rootdescurl &&
		EOK != mp_ports_scan_mappings(ports, j_find_ref(ctl->me, JK_IP_INT))) {
		DE("Port scanning failed\n");
	}

	if (SIG_ERR == signal(SIGINT, mp_main_signal_handler)) {
		DE("Can't register signal handler\n");
		cli_destoy();
		return (EBAD);
	}

	/* Here test the config. If it not loaded - we create it and save it */
	if (NULL == ctl->config) {
		rc = mp_config_from_ctl_l();
		if (EOK == rc) {
			rc = mp_config_save();
			if (EOK != rc) {
				DE("Cant' save config\n");;
			}
		}
	}

	rc = mp_main_init_security();
	if (EOK != rc) {
		DE("Can't init security\n");
		return (EBAD);
	}
	mp_main_print_info_banner();

	rc = mp_mqtt_start_module(cert_path);

	if (EOK != rc) {
		DE("Can't create thread mqtt-app\n");
		cli_destoy();
		abort();
	}

	while (ctl->status != ST_STOPPED) {
		//DD("Before sleep 300 millisec\n");
		rc = mp_os_usleep(300);
		//DD("After sleep 300 millisec\n");
		if (0 != rc) {
			DE("usleep returned error\n");
			perror("usleep returned error");
		}
	}

	TFREE_STR(cert_path);
	cli_destoy();
	return (rc);
}