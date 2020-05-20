#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/prctl.h>
#include <libssh2.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "mp-debug.h"
#include "mp-jansson.h"
#include "mp-dict.h"

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

//const char *pub_key_name = "/home/se/.ssh_orig/id_rsa.pub";
//const char *priv_key_name = "/home/se/.ssh_orig/id_rsa";
//const char *username = "adminroot";
//const char *password = "";

//const char *server_ip = "127.0.0.1";
//const char *server_ip = "185.177.92.146";

//const char *local_listenip = "127.0.0.1";
//unsigned int local_listenport = 2222;

//const char *remote_desthost = "localhost"; resolved by the server */
//unsigned int remote_destport = 2244;

enum {
	AUTH_NONE = 0,
	AUTH_PASSWORD,
	AUTH_PUBLICKEY
};

//int main(int argc, char *argv[])
/* 
   Connect to remote host and create forwarding port on this machine.
   Params:
   const char *server_ip - remote ip in form "185.177.92.146"
   unsigned int remote_destport - port where remote sshd listens; ususaly 22
   const char *local_listenip - local ip, most probably "127.0.0.1"
   unsigned int local_listenport  - local port, entry to forwarding channed;
   const char *pub_key_name - file name of public key
   const char *priv_key_name - file name of private key
   const char *username - name of user on remote port
   const char *password - password, in out case just "", we use public key auth
   */
int mp_ssh_direct_forward(const char *server_ip,
						  int remote_destport,
						  const char *local_listenip,
						  int local_listenport,
						  const char *pub_key_name,
						  const char *priv_key_name,
						  const char *username,
						  const char *password)
{
	const char *remote_desthost = "localhost";
	int rc, i;
	//int auth = AUTH_NONE;
	struct sockaddr_in sin;
	socklen_t sinlen = 0;
	const char *fingerprint = NULL;
	char *userauthlist = NULL;
	LIBSSH2_SESSION *session = NULL;
	LIBSSH2_CHANNEL *channel = NULL;
	const char *shost = NULL;
	int sport = 0;
	fd_set fds;
	struct timeval tv;
	ssize_t len = 0;
	ssize_t wr = 0;
	char buf[16384];

	int sockopt = -1;
	int sock = -1;
	int listensock = -1;
	int forwardsock = -1;

	password = "";

	DSVAR(server_ip);
	DSVAR(local_listenip);
	DSVAR(pub_key_name);
	DSVAR(priv_key_name);
	DSVAR(username);
	DSVAR(password);
	DIVAR(remote_destport);
	DIVAR(local_listenport);

	rc = libssh2_init(0);

	if (rc) {
		DE("libssh2 initialization failed (%d)\n", rc);
		return (1);
	}

	/* Connect to SSH server */
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == -1) {
		perror("socket");
		return (-1);
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(server_ip);
	if (INADDR_NONE == sin.sin_addr.s_addr) {
		perror("inet_addr");
		return (-1);
	}

	sin.sin_port = htons(remote_destport);
	if (connect(sock, (struct sockaddr *)(&sin),
				sizeof(struct sockaddr_in)) != 0) {
		DE("failed to connect!\n");
		return (-1);
	}

	/* Create a session instance */
	session = libssh2_session_init();

	if (!session) {
		DE("Could not initialize SSH session!\n");
		return (-1);
	}

	rc = libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1);
	if (rc) {
		DE("Can't enable compression\n");
	}

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */
	rc = libssh2_session_handshake(session, sock);

	if (rc) {
		DE("Error when starting up SSH session: %d\n", rc);
		return (-1);
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */
	fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

	DE("Fingerprint: ");
	for (i = 0; i < 20; i++) printf("%02X ", (unsigned char)fingerprint[i]);
	DE("\n");

	/* check what authentication methods are available */
	userauthlist = libssh2_userauth_list(session, username, strlen(username));

	DD("Authentication methods: %s\n", userauthlist);
	//if (strstr(userauthlist, "password")) auth |= AUTH_PASSWORD;
	//if (strstr(userauthlist, "publickey")) auth |= AUTH_PUBLICKEY;
	//auth |= AUTH_PUBLICKEY;

	rc = libssh2_userauth_publickey_fromfile(session, username, pub_key_name, priv_key_name, password);
	if (0 != rc) {
		DE("\tAuthentication by public key failed : rc = %d\n", rc);
		switch (rc) {
		case LIBSSH2_ERROR_EAGAIN:
			DE("Error is LIBSSH2_ERROR_EAGAIN:\n"
			   "It returns LIBSSH2_ERROR_EAGAIN when it would otherwise block.\n"
			   "While LIBSSH2_ERROR_EAGAIN is a negative number, it isn't really a failure per se\n");
			break;
		case LIBSSH2_ERROR_ALLOC:
			DE("Error is LIBSSH2_ERROR_ALLOC: An internal memory allocation call failed\n");
			break;
		case LIBSSH2_ERROR_SOCKET_SEND:
			DE("Error is LIBSSH2_ERROR_SOCKET_SEND: Unable to send data on socket.\n");
			break;
		case LIBSSH2_ERROR_SOCKET_TIMEOUT:
			DE("Error is LIBSSH2_ERROR_SOCKET_TIMEOUT\n");
			break;
		case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
			DE("Error is LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED: The username/public key combination was invalid\n");
			break;
		case LIBSSH2_ERROR_AUTHENTICATION_FAILED:
			DE("Error is LIBSSH2_ERROR_AUTHENTICATION_FAILED: Authentication using the supplied public key was not accepted\n");
			break;
		}
		goto shutdown;
	}
	DE("\tAuthentication by public key succeeded.\n");

	listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (listensock == -1) {
		perror("socket");
		return (-1);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(local_listenport);
	sin.sin_addr.s_addr = inet_addr(local_listenip);
	if (INADDR_NONE == sin.sin_addr.s_addr) {
		perror("inet_addr");
		goto shutdown;
	}
	sockopt = 1;
	setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &sockopt,
			   sizeof(sockopt));
	sinlen = sizeof(sin);
	if (-1 == bind(listensock, (struct sockaddr *)&sin, sinlen)) {
		perror("bind");
		goto shutdown;
	}
	if (-1 == listen(listensock, 2)) {
		perror("listen");
		goto shutdown;
	}

	DE("Waiting for TCP connection on %s:%d...\n",
	   inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

	forwardsock = accept(listensock, (struct sockaddr *)&sin, &sinlen);

	if (forwardsock == -1) {
		perror("accept");
		goto shutdown;
	}

	shost = inet_ntoa(sin.sin_addr);
	sport = ntohs(sin.sin_port);

	DE("Forwarding connection from %s:%d here to remote %s:%d\n",
	   shost, sport, remote_desthost, remote_destport);

	channel = libssh2_channel_direct_tcpip_ex(session,
											  remote_desthost,
											  remote_destport,
											  shost, sport);
	if (!channel) {
		DE("Could not open the direct-tcpip channel!\n"
		   "(Note that this can be a problem at the server!"
		   " Please review the server logs.)\n");
		goto shutdown;
	}

	/* Must use non-blocking IO hereafter due to the current libssh2 API */
	libssh2_session_set_blocking(session, 0);


	while (1) {
		FD_ZERO(&fds);
		FD_SET(forwardsock, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		rc = select(forwardsock + 1, &fds, NULL, NULL, &tv);
		if (-1 == rc) {
			perror("select");
			goto shutdown;
		}
		if (rc && FD_ISSET(forwardsock, &fds)) {
			len = recv(forwardsock, buf, sizeof(buf), 0);
			if (len < 0) {
				perror("read");
				goto shutdown;
			} else if (0 == len) {
				DE("The client at %s:%d disconnected!\n", shost, sport);
				goto shutdown;
			}
			wr = 0;
			while (wr < len) {
				i = libssh2_channel_write(channel, buf + wr, len - wr);

				if (LIBSSH2_ERROR_EAGAIN == i) {
					continue;
				}
				if (i < 0) {
					DE("libssh2_channel_write: %d\n", i);
					goto shutdown;
				}
				wr += i;
			}
		}
		while (1) {
			len = libssh2_channel_read(channel, buf, sizeof(buf));

			if (LIBSSH2_ERROR_EAGAIN == len) break;
			else if (len < 0) {
				DE("libssh2_channel_read: %d", (int)len);
				goto shutdown;
			}
			wr = 0;
			while (wr < len) {
				i = send(forwardsock, buf + wr, len - wr, 0);
				if (i <= 0) {
					perror("write");
					goto shutdown;
				}
				wr += i;
			}
			if (libssh2_channel_eof(channel)) {

				DE("The server at %s:%d disconnected!\n", remote_desthost, remote_destport);
				goto shutdown;
			}
		}
	}

shutdown:
	close(forwardsock);
	close(listensock);
	if (channel) libssh2_channel_free(channel);

	libssh2_session_disconnect(session, "Client disconnecting normally");

	libssh2_session_free(session);

	close(sock);
	libssh2_exit();

	return (0);
}

/*@null@*/ void *ssh_tunnel_pthread(void *arg)
{
	json_t *root = arg;
	int rc = EBAD;

	const char *local_listenip = "127.0.0.1";
	const char *server_ip = NULL;
	const char *remote_destport_src = NULL;
	int remote_destport = 0;
	const char *local_listenport_str = NULL;
	int local_listenport = 0;
	const char *pub_key_name = NULL;
	const char *priv_key_name = NULL;
	const char *username = NULL;
	const char *password = NULL;

	TESTP(root, NULL);
	DDD("root = %p\n", root);

	rc = pthread_detach(pthread_self());
	if (0 != rc) {
		DE("Thread: can't detach myself\n");
		perror("Thread: can't detach myself");
		abort();
	}

	//rc = pthread_setname_np(pthread_self(), "ssh_thread");
	rc = prctl(PR_SET_NAME, "ssh_thread");
	if (0 != rc) {
		DE("Can't set pthread name\n");
	}

	/* 
	   Connect to remote host and create forwarding port on this machine.
	   Params:
	   const char *server_ip - remote ip in form "185.177.92.146"
	   unsigned int remote_destport - port where remote sshd listens; ususaly 22
	   const char *local_listenip - local ip, most probably "127.0.0.1"
	   unsigned int local_listenport  - local port, entry to forwarding channed;
	   const char *pub_key_name - file name of public key
	   const char *priv_key_name - file name of private key
	   const char *username - name of user on remote port
	   const char *password - password, in out case just "", we use public key auth
	   */

	j_print(root, "In ssh_thread: arguments are: ");
	server_ip = j_find_ref(root, JK_SSH_SERVER);
	TESTP(server_ip, NULL);
	remote_destport_src = j_find_ref(root, JK_SSH_DESTPORT);
	TESTP(remote_destport_src, NULL);
	remote_destport = atoi(remote_destport_src);

	local_listenport_str = j_find_ref(root, JK_SSH_LOCALPORT);
	TESTP(local_listenport_str, NULL);
	local_listenport = atoi(local_listenport_str);

	pub_key_name = j_find_ref(root, JK_SSH_PUBKEY);
	TESTP(pub_key_name, NULL);
	priv_key_name = j_find_ref(root, JK_SSH_PRIVKEY);
	TESTP(priv_key_name, NULL);
	username = j_find_ref(root, JK_SSH_USERNAME);
	TESTP(username, NULL);
	password = "";


	rc = mp_ssh_direct_forward(server_ip, remote_destport, local_listenip,
							   local_listenport, pub_key_name, priv_key_name,
							   username, password);

	if (EOK != rc) {
		DE("Error in ssh forward thread\n");
	}

	return (NULL);
}

int ssh_thread_start(/*@temp@*/json_t *root)
{
	pthread_t ssh_thread_id;
	TESTP(root, EBAD);
	DDD("root = %p\n", root);
	j_print(root, "ssh_thread_start: params are: ");

	pthread_create(&ssh_thread_id, NULL, ssh_tunnel_pthread, root);
	return (EOK);
}
