/*
 * af_ipc_common.h
 *
 * Contains the common ipc infrastructure defintion
 *
 * Copyright (c) 2015-2018 Afero, Inc. All rights reserved.
 *
 */

#ifndef __AF_IPC_COMMON_H__
#define __AF_IPC_COMMON_H__

#include <event2/event.h>
#include <stdint.h>

/* status values */
#define AF_IPC_STATUS_OK       0
#define AF_IPC_STATUS_ERROR    -1
#define AF_IPC_STATUS_TIMEOUT  -10

/* Maximum message length; you can send up to (AF_IPC_MAX_MSGLEN - 6) bytes */
#define AF_IPC_MAX_MSGLEN      4096

/* common receive callback */
typedef void (*af_ipc_receive_callback_t)(int status, uint32_t seqNum, uint8_t *rxBuf, int rxSize, void *context);

/* unpack sequence numbers */
uint16_t af_ipc_get_client_id_from_seq_num(uint32_t seqNum);
int af_ipc_seq_num_is_request(uint32_t seqNum);

/* for testing: use different path prefix for server sockets */
void af_ipc_set_server_sock_path_prefix(const char *prefix);

#endif // __AF_IPC_COMMON_H__
