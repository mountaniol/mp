#ifndef MP_DICT_H_
#define MP_DICT_H_

/* 
 * This dictionary used in JSON objects 
 * The dictionary defines standard Json Keys (JK_*)
 * and standard Json Values (JT_*) used 
 * over the code 
 */

/*** Keys ***/

/* Client UID */
//#define JK_UID "uid"
/* My UID - used in ctl->me */
#define JK_UID_ME "uid-me"
/* Source UID - who sent this message */
#define JK_UID_SRC "uid-src"
/* Dest UID - who should receive this message */
#define JK_UID_DST "uid-dst"
/* Target UID: to whom this message dedicated*/
#define JK_DEST "destination"
/* Client user name, used to form topic name */
#define JK_USER "user"
/* Client machine name (defined by user) */
#define JK_NAME "name"
/* Client external (router) IP */
#define JK_IP_EXT "ip_ext"
/* Client local (machine interface) IP */
#define JK_IP_INT "ip_int"
/* Client external (router) opened / mapped port */
#define JK_PORT_EXT "port_ext"
/* Client local (machine interface) opened / mapped port */
#define JK_PORT_INT "port_int"
/* Protocol: for TCP / UDP */
#define JK_PROTOCOL "protocol"
/* Status field, used mostly in responces */
#define JK_STATUS "status"
/* We may send readon of BAD status using JK_REASON key */
#define JK_REASON "reason"
/* JSON request / responce type. The key "type" is reserved for kansson internal usage */
#define JK_TYPE "tp"

/* Time value, used in tickets */
#define JK_TIME "time-value"

/*** Keys for SSH commands ***/
#define JK_SSH_SERVER "ssh-remove-server"
#define JK_SSH_DESTPORT "ssh-remote-port"
#define JK_SSH_LOCALPORT "ssh-local-port"
#define JK_SSH_PUBKEY "ssh-public-key"
#define JK_SSH_PRIVKEY "ssh-private-key"
#define JK_SSH_USERNAME "ssh-user-name"

/* Command: used in mp-shell, probably we should redefine it*/
#define JK_COMMAND "command"

/** This used in mp-shell only **/
#define JK_SHOW_PORTS "show-ports-local"
#define JK_SHOW_RPORTS "show-ports-remote"
#define JK_SHOW_INFO "show-info"
#define JK_SHOW_HOSTS "show-hosts"

/** Config file fields **/

/* Is this machine source? */
#define JK_SOURCE "source"
/* Is this machine target? */
#define JK_TARGET "target"
/* Is this machine a bridge? */
#define JK_BRIDGE "bridge"

/* If the JSON object includes a list, its name should be defined as well */
/** Array type **/

/* list of mapped ports */
#define JK_ARR_PORTS "list_ports"
/* list of remote hosts */
#define JK_ARR_HOSTS "list_remote_hosts"

/* Ticket: how we define session between mp-shell and remote machine */
#define JK_TICKET "ticket"

/*** Values ***/

#define JV_YES "1"
#define JV_NO "0"
#define JV_NA "NA" /* For undefined state */
#define JV_OK "1"
#define JV_BAD "0"
#define JV_NO_PORT "0"
#define JV_NO_IP "0.0.0.0"
#define JV_TCP "TCP"
#define JV_UDP "UDP"

/* These statuses indended for ticketing */
/* 
 *  
 * JK_TICKET = JV_STATUS_STARTED - ticket accepted and work started
 */
#define JV_STATUS_STARTED "started"
/* JK_TICKET = JV_STATUS_UPDATE  - ticket in processing */
#define JV_STATUS_UPDATE  "working"
/* JK_TICKET = JV_STATUS_UPDATE  - ticket is done successfully */
#define JV_STATUS_SUCCESS    "done-success"
/* JK_TICKET = JV_STATUS_UPDATE  - ticket is done with a failure */
#define JV_STATUS_FAIL    "done-fail"

/* Used in requests and in CLI; for CLI also used as valie for "command" key */

/* This is a regular keepalive */
#define JV_TYPE_ME "me"
#define JV_TYPE_MY_PORTS "myports"
#define JV_TYPE_CONNECT "connect"
#define JV_TYPE_DISCONNECT "disconnect"
#define JV_TYPE_REVEAL "reveal"
#define JV_TYPE_SSH "ssh"
#define JV_TYPE_SSH_DONE "ssh-done"
#define JV_TYPE_SSHR "sshr"
#define JV_TYPE_SSHR_DONE "sshr-done"
#define JV_TYPE_OPENPORT "openport"
#define JV_TYPE_CLOSEPORT "closeport"
#define JV_TYPE_KEEPALIVE "keepelive"
/* Ask for tickets. Ticket ID is in JK_TICKET */
#define JV_TYPE_TICKET_REQ  "ticket-type-req"
#define JV_TYPE_TICKET_RESP "ticket-type-resp"

/* These used between mp-shell and mp-cli */
#define JV_COMMAND_LIST "list"	/* list remote hosts */
#define JV_COMMAND_PORTS "ports"	/* show opened ports */
#define JV_COMMAND_RPORTS "rports" /* Shell asks for remote ports */

#endif /* MP_DICT_H_ */
