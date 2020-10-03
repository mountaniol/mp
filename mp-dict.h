#ifndef MP_DICT_H_
#define MP_DICT_H_

/* 
 * This dictionary used in JSON objects 
 * The dictionary defines standard Json Keys (JK_*)
 * and standard Json Values (JT_*) used 
 * over the code 
 */

/*** Keys ***/

/* My UID - used in ctl->me */
#define JK_UID_ME "uid-me"
/* Client user name, used to form topic name */
#define JK_USER "user"
/* Client machine name (defined by user) */
#define JK_NAME "name"
/* Client external (router) IP */
#define JK_IP_EXT "ip-ext"
/* Client local (machine interface) IP */
#define JK_IP_INT "ip-int"
/* Client external (router) opened / mapped port */
#define JK_PORT_EXT "port-ext"
/* Client local (machine interface) opened / mapped port */
#define JK_PORT_INT "port-int"
/* Protocol: for TCP / UDP */
#define JK_PROTOCOL "protocol"
/* We may send reason of BAD status using JK_REASON key */
#define JK_REASON "reason"
/* JSON request / responce type. The key "type" is reserved for jansson internal usage */
#define JK_TYPE "tp"

/** This used in mp-shell only **/
#define JK_CMDLINE_SHOW_PORTS "show-ports-local"
#define JK_CMDLINE_SHOW_RPORTS "show-ports-remote"
#define JK_CMDLINE_SHOW_INFO "show-info"
#define JK_CMDLINE_SHOW_HOSTS "show-hosts"

/** Config file fields **/

/* Is this machine source? */
#define JK_SOURCE "source"
/* Is this machine target? */
#define JK_TARGET "target"
/* Is this machine a bridge? */
#define JK_BRIDGE "bridge"

/* If the JSON object includes a list, its name should be defined as well */
/** Array type **/

/* Ticket: how we define session between mp-shell and remote machine */
#define JK_TICKET "ticket"
#define JK_TOPIC "topic"

/*** Values ***/

#define JV_YES "1"
#define JV_NO "0"

#define JV_OK "1"
#define JV_BAD "0"

/* These statuses indended for ticketing */
/* JK_TICKET = JV_STATUS_UPDATE  - ticket in processing */
#define JV_STATUS_UPDATE  "working"
/* JK_TICKET = JV_STATUS_UPDATE  - ticket is done successfully */
#define JV_STATUS_SUCCESS    "done-success"
/* JK_TICKET = JV_STATUS_UPDATE  - ticket is done with a failure */
#define JV_STATUS_FAIL    "done-fail"

/* Used in requests and in CLI; for CLI also used as valie for "command" key */

/* This is a regular keepalive */
#define JV_TYPE_ME "me"
#define JV_TYPE_CONNECT "connect"
#define JV_TYPE_DISCONNECTED "disconnected"
#define JV_TYPE_REVEAL "reveal"
#define JV_TYPE_SSH "ssh"
#define JV_TYPE_OPENPORT "openport"
#define JV_TYPE_CLOSEPORT "closeport"
/* Ask for tickets. Ticket ID is in JK_TICKET */
#define JV_TYPE_TICKET_REQ  "ticket-type-req"
#define JV_TYPE_TICKET_RESP "ticket-type-resp"

/* These used between mp-shell and mp-cli */
#define JV_COMMAND_LIST "list"	/* list remote hosts */
#define JV_COMMAND_PORTS "ports"	/* show opened ports */

/*******************************************************************/

/* Redesign: adding dispatcher and new commands set */

/* Command: The same module receives multiple commands.
   For example, MODULE_PORTS can receive "close-port", "open-port",
   "list-ports" and more commands.
   The receiver function choose what to run by JK_COMMAND field
   */

/* Dispatcher related fields */

/* Source machine */
#define JK_DISP_SRC_UID "disp-src-machine"
/* Source Module */
#define JK_DISP_SRC_MODULE "disp-src-app"
/* Target machine */
#define JK_DISP_TGT_UID "disp-tgt-machine"
/* Target Module */
#define JK_DISP_TGT_MODULE "disp-tgt-app"

/* Status field, used mostly responce, can be OK, BAD or WORKING */
#define JK_STATUS "status"

/* Returned status: operation completed OK */
#define JV_STATUS_OK "ok"
/* Returned status: operation failed */
#define JV_STATUS_BAD "bad"
/* Returned status: operation still in progress, this is a notification */
#define JV_STATUS_WORK "work"

/* What exactly do we want from the module? */
#define JK_COMMAND "command"

/* Values of the JK_COMMAND */

/* MODULE_PORTS related */
#define JV_PORTS_OPEN "port-open"
#define JV_PORTS_CLOSE "port-close"

#endif /* MP_DICT_H_ */
