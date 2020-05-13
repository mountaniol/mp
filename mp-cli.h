#ifndef _CLI_THREAD_T_
#define _CLI_THREAD_T_

/* Todo: move it to common header */
#define CLI_BUF_LEN 4096

/* Socket _SRC - for commands from shell to CLI thread */
#define CLI_SOCKET_PATH_SRV "/tmp/mightydaddysrv"
/* Socket _CLI - for commands from CLI thread to shell */
#define CLI_SOCKET_PATH_CLI "/tmp/mightydaddycli"

extern /*@null@*/ void *mp_cli_thread(/*@temp@*/void *arg);
int mp_cli_send_to_cli(/*@temp@*/ const json_t *root);
#endif /* _CLI_THREAD_T_ */
