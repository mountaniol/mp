#ifndef _SEC_CTL_H_
#define _SEC_CTL_H_

#include <semaphore.h>
#include "mp-htable.h"

/* What is the current client status now? */
enum e_status{
	ST_START=1,			/* We are starting, init phase*/
	ST_CONNECTED,		/* Connected, all works */
	ST_DISCONNECTED,	/* Somehow disconnected, need reconnect */
	ST_STOP,			/* Received "stop" signal from cli */
	ST_STOPPED				/* Received "stop" signal from cli */
};

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
	enum e_status status;	/* Connection status */
	/* These must be protected with lock */

	/* hosts is a JSON object holding remote machines.
	   The keys are UID of the remote machine.
	   The structure of this JSON is identical to "me" object.
	   Actually, all remotes send the "me" JSON object
	   to describe themselves. */
	void *hosts;

	//htable_t *holder_sources; /* Here we keep remote computers */
	//void *ports; /* JSON array - open ports */
	htable_t *htab_ports; /* Here we keep mapped ports (port_t, see above), sorted by internal port */
	void *config; /* The config file in form of JSON object */
	void *tickets;
} control_t;


/* Allocate and init sct scruct, must be called once */
extern int ctl_allocate_init(void);
/* Get pointer to global  control_structure */
extern control_t *ctl_get(void);
/* Lock ctl and get pointer to global  control_structure */
extern control_t *ctl_get_locked(void);
/* Lock global control_structure */
extern int ctl_lock(control_t *ctl);
/* Unock global control_structure */
extern int ctl_unlock(control_t *ctl);

#endif /* _SEC_CTL_H_ */
