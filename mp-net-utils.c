/*@-skipposixheaders@*/
#include <netdb.h>
/*@=skipposixheaders@*/
#include <jansson.h>

#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-jansson.h"
#include "mp-net-utils.h"

/*** Network helpers ***/

/* Receive buf_t from socket */
buf_t *mp_net_utils_receive_buf_t(int con)
{
	int rc = 0;
	size_t received = 0;
	buf_t *buf = NULL;

	/* Allocate new buffer */
	buf = buf_string(0);

	if (NULL == buf) {
		DE("Can't allocate buf_t\n");
		abort();
	}

	BUF_DUMP(buf);

	do {
		/* Receive the first buffer: the first buffer we expect is 'buf_t' struct without pointer */
		DD("Going to receive buffer: con = %d, (char *)buf = %p, NET_REST(BUF_T_STRUCT_NET_SIZE, received) = %ld\n", con, (char *)buf, NET_REST(BUF_T_STRUCT_NET_SIZE, received));
		rc = recv(con, (char *)buf + received, NET_REST(BUF_T_STRUCT_NET_SIZE, received), 0);

		DD("Received: %d\n", rc);

		if (rc < 0) {
			DE("Can't receive buf_t structure!\n");
			buf_free(buf);
			return (NULL);
		}
		received += rc;
	} while (received < BUF_T_STRUCT_NET_SIZE);

	buf->data = NULL;

	/* Now we have buf_t structure. We expect to receive its buf->used data */
	rc = buf_add_room(buf, buf_used(buf) + 1);
	if (EOK != rc) {
		DE("Can't add room to buffer\n");
		buf_free(buf);
		return (NULL);
	}

	received = 0;

	do {
		/* Receive the first buffer: the first buffer we expect is 'buf_t' struct without pointer */
		DD("Going to receive buffer: con = %d, (char *)buf = %p, NET_REST(buf->len, received) = %ld\n", con, (char *)buf, NET_REST(buf->used, received));
		rc = recv(con, buf->data + received, NET_REST(buf->used, received), 0);
		DD("Received: %d\n", rc);

		if (rc < 0) {
			DE("Can't receive buf_t structure!\n");
			buf_free(buf);
			return (NULL);
		}
		received += rc;
	} while (received < (size_t) buf_used(buf));

	rc = buf_pack(buf);
	if (EOK != rc) {
		DE("Can't pack buf_t\n");
	}
	return (buf);
}

j_t *mp_net_utils_receive_json(int con)
{
	j_t *root;
	buf_t *buf = mp_net_utils_receive_buf_t(con);
	TESTP(buf, NULL);
	root = j_buf2j(buf);
	buf_free(buf);
	if (NULL == root) {
		DE("Can't decode buf_t to JSON\n");
	}

	return root;
}

/* Send buf_t to socket */
err_t mp_net_utils_send_buf_t(int con, buf_t *buf)
{
	int rc = 0;

	TESTP(buf, EBAD);

	/* First, send buf_t struct itself, buf without buf->data pointer */

	DD("Going to send buffer: con = %d, (char *)buf = %p, BUF_T_STRUCT_NET_SIZE = %ld\n", con, (char *)buf, BUF_T_STRUCT_NET_SIZE);
	rc = send(con, (char *)buf, BUF_T_STRUCT_NET_SIZE, MSG_MORE);
	DD("Sent: %d\n", rc);
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (EBAD);
	}

	if (rc != BUF_T_STRUCT_NET_SIZE) {
		DE("Wrong number of bytes sent: expected %lu but sent %d\n", BUF_T_STRUCT_NET_SIZE, rc);
		perror("send() failed");
		return (EBAD);
	}

	/* Nown send the data */
	DD("Going to send buffer data: con = %d, buf->data = %p, buf->len = %zu\n", con, buf->data, buf_used(buf));
	rc = send(con, buf->data, buf_used(buf), 0);
	DD("Sent: %d\n", rc);
	if (rc < 0) {
		DE("Failed\n");
		perror("send() failed");
		return (EBAD);
	}

	if ((uint32_t) rc != buf_used(buf)) {
		DE("Wrong number of bytes sent: expected %zu but sent %d\n", buf_used(buf), rc);
		perror("send() failed");
		return (EBAD);
	}

	return (EOK);
}

err_t mp_net_utils_send_json(int con, j_t *root)
{
	err_t rc;
	buf_t *buf;
	TESTP(root, EBAD);
	buf = j_2buf(root);
	TESTP(buf, EBAD);

	rc = mp_net_utils_send_buf_t(con, buf);
	buf_free(buf);
	return rc;
}

