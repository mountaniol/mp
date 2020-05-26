#ifndef _CLI_THREAD_T_
#define _CLI_THREAD_T_

/* Socket _SRC - for commands from shell to CLI thread */
#define CLI_SOCKET_PATH_SRV "/tmp/mightydaddysrv"
/* Socket _CLI - for commands from CLI thread to shell */
#define CLI_SOCKET_PATH_CLI "/tmp/mightydaddycli"

extern /*@null@*/ void *mp_cli_pthread(/*@temp@*/void *arg);
extern err_t mp_cli_send_to_cli(/*@temp@*/ const j_t *root);
#endif /* _CLI_THREAD_T_ */
