#ifndef MP_NET_UTILS_H
#define MP_NET_UTILS_H

#include "buf_t/buf_t.h"
#include "mp-jansson.h"

#define NET_REST(expected, received) (expected - received)

extern buf_t *mp_net_utils_receive_buf_t(int con);
extern j_t *mp_net_utils_receive_json(int con);
extern err_t mp_net_utils_send_buf_t(int con, buf_t *buf);
extern err_t mp_net_utils_send_json(int con, j_t *root);

#endif /* MP_NET_UTILS_H */
