#ifndef _SEC_SSH_PORT_FORWARD_H_
#define _SEC_SSH_PORT_FORWARD_H_

typedef struct ssh_forward_args_struct {
	char *user;
	char *from_server;
	char *to_server;
	int from_port;
	int to_port;

}ssh_forward_args_t;

extern int ssh_thread_start(json_t *root);
#endif _SEC_SSH_PORT_FORWARD_H_
