#ifndef _SEC_CLIENT_MOSQ_H_
#define _SEC_CLIENT_MOSQ_H_

#define TOPIC_MAX_LEN 1024
#define UID_LEN 16

/* We keep record of all remote machines
   using this structures.
   The structures kept in hash table
   where */
typedef struct host_struct
{
	char *uid;				/* Remote machine UID */
	char *name;				/* Remote machine name */
	char *ip;				/* Remote machine IP */
	char *port;				/* Remote machine port */
	unsigned long time;		/* time this record added / updated */
	int source;				/* If this remote machine allowed establish ssh connection? */
	int target;				/* If this remote machine accept ssh connection? */
	int bridge;				/* If this remote machine allowed to be jump server ? */
} host_t;

#endif /* _SEC_CLIENT_MOSQ_H_ */
