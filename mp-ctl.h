#ifndef _SEC_CTL_H_
#define _SEC_CTL_H_
/*@-skipposixheaders@*/
#include <semaphore.h>
/*@=skipposixheaders@*/

/* For err_t */
#include "mp-common.h"

/* What is the current client status now? */
typedef enum e_status {
	ST_START = 1,         /* We are starting, init phase*/
	ST_CONNECTED,       /* Connected, all works */
	ST_DISCONNECTED,    /* Somehow disconnected, need reconnect */
	ST_STOP,            /* Received "stop" signal from cli */
	ST_STOPPED              /* Received "stop" signal from cli */
} mp_status_t;

/* This is a global structure.
   Pointer to the structure shouldbe get with ctl_get()
   The structure should bw lock with ctl_lock()
   and unlocked with ctl_unlock()
   Allocated and inited in main() */
typedef struct control_struct {
	sem_t lock; /* get semaphore before manipulate this struct */
	struct mosquitto *mosq; /* Instance of mosquitto connection */
	void *me; /* JSON object describing this machine */
	/* We send this JSON to remote hosts. It contains full descruption
	   of everything the remote host should know */
	/*  The 'me' is a json object consists of:
	 
		field 'tp' = 'me'
		string 'user'
		string 'ip_external'
		string 'ip_internal'
		string 'port_external'
		string 'port_internal'
		string 'name'
		string 'uid'
	 
		Arrays:
		ports - mapped ports
		Every record in "ports_*" consists of:
		external port, internal port
	 
	 */
	mp_status_t status;   /* Connection status */
	/* These must be protected with lock */

	/* hosts is a JSON object holding remote machines.
	   The keys are UID of the remote machine.
	   The structure of this JSON is identical to "me" object.
	   Actually, all remotes send the "me" JSON object
	   to describe themselves. */
	void *hosts;

	/*@null@*/ void *config; /* The config file in form of JSON object */
	void *tickets_out;
	void *tickets_in;
	void *buffers; /* Here we keep allocate buffers until they sent */
	void *buf_missed; /* Here we keep buffer counters that we couldn't find on the first run */
	char *rootdescurl; /* The router UPNP description, use it to speed up UPNP requests */
	void *rsa_priv; /* RSA Private key */
	void *rsa_pub; /* RSA public key */
	void *x509; /* X509 certificate */
	void *ctx;	/* SSL context object */
} control_t;


/* Allocate and init sct scruct, must be called once */
extern err_t ctl_allocate_init(void);
/* Destoy control structure and all objects it holds */
err_t cli_destoy();
/* Get pointer to global  control_structure */
extern /*@temp@*//*@notnull@*/ control_t *ctl_get(void);
/* Lock ctl and get pointer to global  control_structure */
extern /*@temp@*//*@notnull@*/ control_t *ctl_get_locked(void);
/* Lock global control_structure */
extern void ctl_lock(void);
/* Unock global control_structure */
extern void ctl_unlock(void);

/* Get JK_UID_ME. This function always returns not NULL; if it can't extract JK_UID_ME it aborts execution */
extern /*@temp@*//*@notnull@*/ const char *ctl_uid_get(void);
/* Set JK_UID_ME */
extern void ctl_uid_set(const char *uid);

/* Get JK_USER. This function always returns not NULL; if it can't extract JK_USER it aborts execution */
extern /*@temp@*//*@notnull@*/ const char *ctl_user_get(void);
/* Set JK_USER */
extern void ctl_user_set(const char *uid);

#endif /* _SEC_CTL_H_ */
