#ifndef _CLI_THREAD_T_
#define _CLI_THREAD_T_

/* Todo: move it to common header */
#define CLI_BUF_LEN 4096
#define CLI_SOCKET_PATH_SRV "/tmp/mightydaddysrv"
#define CLI_SOCKET_PATH_CLI "/tmp/mightydaddycli"

extern void *mp_cli_thread(void *arg);
int mp_cli_send_to_cli(json_t *root);
#endif /* _CLI_THREAD_T_ */
