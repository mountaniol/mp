/* This code based on example copied from here:
https://www.libssh2.org/examples/tcpip-forward.html 
The original example also contains Windows parts */

/*@-skipposixheaders@*/
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
/*@=skipposixheaders@*/

#include "mp-debug.h"
#include "mp-common.h"

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

const char *pub_key_name = "/home/se/.ssh_orig/id_rsa.pub";
const char *priv_key_name = "/home/se/.ssh_orig/id_rsa";
//const char *username = "se";
const char *username = "adminroot";
const char *password = "";

/* This is IP of remote server */
//const char *server_ip = "127.0.0.1";
const char *server_ip = "185.177.92.146";
const char *remote_listenhost = "localhost"; /* resolved by the server */
int remote_wantport = 2244;
int remote_listenport = 0;

const char *local_destip = "127.0.0.1";
int local_destport = 2222;

enum {
	AUTH_NONE = 0,
	AUTH_PASSWORD,
	AUTH_PUBLICKEY
};


/*@null@*/ void *start_ssh_forward_thread(void *arg)
//int main()
{
	int rc, i, auth = AUTH_NONE;
	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	const char *fingerprint = NULL;
	char *userauthlist = NULL;
	LIBSSH2_SESSION *session;
	LIBSSH2_LISTENER *listener = NULL;
	LIBSSH2_CHANNEL *channel = NULL;
	fd_set fds;
	struct timeval tv;
	ssize_t len = 0;
	ssize_t wr = 0;
	char buf[16384];

	int sock = -1, forwardsock = -1;

	/* SEB: TODO: This should be called from main() */
	rc = libssh2_init(0);

	if (rc != 0) {
		DE("libssh2 initialization failed (%d)\n", rc);
		return (NULL);
	}

	/* Connect to SSH server */
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		DE("Can't create socket\n");
		perror("socket");
		return (NULL);
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(server_ip);
	if (INADDR_NONE == sin.sin_addr.s_addr) {
		DE("Can't create inet addr\n");
		perror("inet_addr");
		return (NULL);
	}

	sin.sin_port = htons(22);
	if (connect(sock, (struct sockaddr *)(&sin), sizeof(struct sockaddr_in)) != 0) {
		DE("failed to connect!\n");
		return (NULL);
	}

	/* Create a session instance */
	session = libssh2_session_init();

	if (!session) {
		DE("Could not initialize SSH session!\n");
		return (NULL);
	}

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */
	rc = libssh2_session_handshake(session, sock);

	if (rc) {
		DE("Error when starting up SSH session: %d\n", rc);
		return (NULL);
	}

	/* At this point we havn't yet authenticated.  The first thing to do
	 * is check the hostkey's fingerprint against our known hosts Your app
	 * may have it hard coded, may go to a file, may present it to the
	 * user, that's your call
	 */
	fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);

#if 0
	DE( "Fingerprint: ");
	for (i = 0; i < 20; i++) DE( "%02X ", (unsigned char)fingerprint[i]);
	DE( "\n");
#endif

	/* check what authentication methods are available */
	userauthlist = libssh2_userauth_list(session, username, strlen(username));

	DE("Authentication methods: %s\n", userauthlist);
	//if (strstr(userauthlist, "password")) auth |= AUTH_PASSWORD;
	//if (strstr(userauthlist, "publickey")) auth |= AUTH_PUBLICKEY;

	/* SEB: We only ise publickey to connect */
	auth |= AUTH_PUBLICKEY;

	/*
	 * int libssh2_userauth_publickey_fromfile(LIBSSH2_SESSION *session, const char *username, const char *publickey, const char *privatekey, const char *passphrase); 
	 */
	if (libssh2_userauth_publickey_fromfile(session, username, pub_key_name, priv_key_name, password)) {
		DE("\tAuthentication by public key failed!\n");
		goto shutdown;
	}

	DD("\tAuthentication by public key succeeded.\n");
	DD("Asking server to listen on remote %s:%d\n", remote_listenhost, remote_wantport);

	/* 
	 
	LIBSSH2_LISTENER * libssh2_channel_forward_listen_ex(LIBSSH2_SESSION *session, char *host, int port, int *bound_port, int queue_maxsize); 
	 
	Instruct the remote SSH server to begin listening for inbound TCP/IP connections. New connections will be queued by the library until accepted by libssh2_channel_forward_accept.
	 
	session - instance as returned by libssh2_session_init().
	host - specific address to bind to on the remote host. Binding to 0.0.0.0 (default when NULL is passed) will bind to all available addresses.
	port - port to bind to on the remote host. When 0 is passed, the remote host will select the first available dynamic port.
	bound_port - Populated with the actual port bound on the remote host. Useful when requesting dynamic port numbers.
	queue_maxsize - Maximum number of pending connections to queue before rejecting further attempts.  
	 
	*/

	listener = libssh2_channel_forward_listen_ex(session, remote_listenhost, remote_wantport, &remote_listenport, 1);
	if (!listener) {
		DE("Could not start the tcpip-forward listener!\n"
		   "(Note that this can be a problem at the server!"
		   " Please review the server logs.)\n");
		goto shutdown;
	}

	DD("Server is listening on %s:%d\n", remote_listenhost, remote_listenport);

	DD("Waiting for remote connection\n");
	channel = libssh2_channel_forward_accept(listener);

	if (!channel) {
		DE("Could not accept connection!\n"
		   "(Note that this can be a problem at the server!"
		   " Please review the server logs.)\n");
		goto shutdown;
	}

	DD("Accepted remote connection. Connecting to local server %s:%d\n",
	   local_destip, local_destport);

	/*** Opening forward socket ***/

	forwardsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (forwardsock == -1) {
		perror("socket");
		goto shutdown;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(local_destport);
	sin.sin_addr.s_addr = inet_addr(local_destip);
	if (INADDR_NONE == sin.sin_addr.s_addr) {
		perror("inet_addr");
		goto shutdown;
	}
	if (-1 == connect(forwardsock, (struct sockaddr *)&sin, sinlen)) {
		perror("connect");
		goto shutdown;
	}

	DD("Forwarding connection from remote %s:%d to local %s:%d\n",
	   remote_listenhost, remote_listenport, local_destip, local_destport);

	/* Must use non-blocking IO hereafter due to the current libssh2 API */
	libssh2_session_set_blocking(session, 0);


	while (1) {
		FD_ZERO(&fds);
		FD_SET(forwardsock, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		rc = select(forwardsock + 1, &fds, NULL, NULL, &tv);

		if (-1 == rc) {
			DE("Error on select\n");
			perror("select");
			goto shutdown;
		}

		/*** Read local socket and forward buffer ***/

		if (rc && FD_ISSET(forwardsock, &fds)) {
			len = recv(forwardsock, buf, sizeof(buf), 0);
			if (len < 0) {
				DE("Error on read\n");
				perror("read");
				goto shutdown;
			} else if (0 == len) {
				DE("The local server at %s:%d disconnected!\n", local_destip, local_destport);
				goto shutdown;
			}
			wr = 0;
			do {
				i = libssh2_channel_write(channel, buf, len);

				if (i < 0) {
					DE("libssh2_channel_write: %d\n", i);
					goto shutdown;
				}

				wr += i;
			} while (i > 0 && wr < len);
		}


		/*** Recevie buffer from remote by ssh and write to local buffer  ***/

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
					DE("Error on write\n");
					perror("write");
					goto shutdown;
				}
				wr += i;
			}

			/*
			Returns 1 if the remote host has sent EOF, otherwise 0. Negative on failure
			*/
			if (libssh2_channel_eof(channel)) {

				DE("The remote client at %s:%d disconnected!\n", remote_listenhost, remote_listenport);
				goto shutdown;
			}
		}
	}

shutdown:
	close(forwardsock);
	if (channel) libssh2_channel_free(channel);

	if (listener) libssh2_channel_forward_cancel(listener);

	libssh2_session_disconnect(session, "Client disconnecting normally");

	libssh2_session_free(session);


	close(sock);
	libssh2_exit();

	return (NULL);
}


int main()
{

	start_ssh_forward_thread(NULL);
	return (0);
}
