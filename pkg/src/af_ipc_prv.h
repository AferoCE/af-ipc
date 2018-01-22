/*
 * af_ipc_prv.h
 *
 * Contains the common ipc infrastructure defintion
 *
 * Copyright (c) 2018 Afero, Inc. All rights reserved.
 *
 */

#ifndef __AF_IPC_PRV_H__
#define __AF_IPC_PRV_H__

#include <event2/event.h>
#include <sys/socket.h>
#include "af_mempool.h"

#define STRINGTIFY(str) #str

/*
 * How sequence numbers work
 *
 * There are four types of transactions
 *   server to client command broadcast (with no reply)
 *   server to client command with no reply
 *   server to client command with reply
 *   client to server command with no reply
 *   client to server command with reply
 *
 * The sequence number has two parts: client id and sequence id
 *
 * Server to Client Command Broadcast (with no reply)
 *   Application on server calls af_ipcs_send_message with client number set to -1
 *   af_ipcs_send_message sends IPC to all clients with seq_id set to 0 (no reply) and callback set to NULL
 *   message is passed to client application using the unsolicited receive callback
 *
 * Server to Client Command with no reply
 *   Application on server calls af_ipcs_send_message with specified client number
 *   af_ipcs_send_message sends IPC to specified client with seq_id set to 0 (no reply) and callback set to NULL
 *   message is passed to client application using the unsolicited receive callback
 *
 * Server to Client Command with reply
 *   Application on server calls af_ipcs_send_message with specified client number
 *   af_ipcs_send_message sends IPC to the specified client with seq_id having bit 15 set to 1
 *   message is passed to client application using the receive callback. Bit 15 of seq ID is set to 0
 *   client application calls af_ipcc_send_reply with sequence number it received
 *   af_ipcc_send_reply sends IPC back to the server with sequence number it received
 *   server sends the reply back to receive callback for the request
 *
 * Client to Server Command with no reply
 *   Application on client calls af_ipcc_send_message with the specified receive callback
 *   af_ipcc_send_message sends IPC to server with seq_id set to 0 (no reply)
 *   message is passed to server using the unsolicited receive callback
 *
 * Client to Server Command with reply
 *   Application on client calls af_ipcc_send_message with the receive callback set to NULL
 *   af_ipcc_send_message sends IPC to server with seq_id having bit 15 set to 1
 *   message is passed to server application using the receive callback. Bit 15 of seq ID is set to 0.
 *   server application calls af_ipcs_send_reply with sequence number it received
 *   af_ipcs_send_reply sends IPC back to the client with the sequence number it received
 *   client sends the reply back to the receive callback for the request
 *
 * Bit 15 of the sequence ID is used in the IPC message itself. It indicates to the receiver
 * whether the incoming message is a new request (bit 15 is set) or a reply to a request the
 * receiver made previously (bit 15 is clear)
 */

/*
 * Common constants
 */
#define IPC_SERVER_DEFAULT_SOCK_PATH_PREFIX     "/var/run/"

extern const char *af_ipc_server_sock_path_prefix;

/**************
 * IPC MSG
 **************/

/* the IPC message header
 */
typedef struct af_ipc_msghdr {
    uint32_t seqNum;   /* 2 bytes for client number, 2 bytes for sequence number */
    uint16_t len;      /* length of message including header */
} af_ipc_msghdr_t;

//sizeof(uint32_t) = 4
#define AF_IPC_MSGHDR_LEN   (sizeof(af_ipc_msghdr_t))


/* Sequence number : 32 bit unsigned integer consisting of two 16 bit unsigned integers
 *  [ 33222222222211111111110000000000 ]
 *  [ 10987654321098765432109876543210 ]
 *  [ |<- clientID ->|N|<---seqID--->| ]
 * A valid client ID is nonzero so sometimes a client ID of zero indicates a broadcast
 * A valid sequence ID is nonzero so sometimes a sequence ID of zero indicates an unsolicited message
 *
 * If the N bit is set, the sequence number is a new request and not a reply to an
 * existing request. This convention is used internally and is not visible to
 * servers or client applications, which will always see the N bit as cleared.
 */
#define AF_IPC_GET_CLIENT_ID(seqNum) ((seqNum) >> 16)
#define AF_IPC_GET_SEQ_ID(seqNum) ((seqNum) & 0xFFFF)
#define AF_IPC_SEQNUM_NEW_REQUEST_MASK (1<<15)
#define AF_IPC_SEQNUM_IS_NEW_REQUEST(seqNum) (((seqNum) & AF_IPC_SEQNUM_NEW_REQUEST_MASK) != 0)
#define AF_IPC_BUILD_SEQNUM(clientId,seqId) (((clientId)<<16)|(seqId))

#define AF_IPC_DEFAULT_NUM_REQS 4

/*
 * Data structure used to track a pending request
 */
typedef struct af_ipc_request_struct {
    struct af_ipc_request_struct *next;
    uint32_t           seqNum;
    struct event       *event;
    int                timeout_val;
    struct af_ipc_req_control_struct *req_control;
    af_ipc_receive_callback_t responseCallback;
    void               *responseContext;
} af_ipc_request_t;

/*
 * Data structure used as pending request DB
 */
typedef struct af_ipc_req_control_struct {
    uint16_t lastSeqId;
    uint8_t mutexCreated;
    uint8_t pad;
    struct af_ipc_request_struct *pendingRequests;
    af_mempool_t *reqPool;
    pthread_mutex_t mutex;
} af_ipc_req_control_t;

int af_ipc_util_init_requests(af_ipc_req_control_t *req_control);
void af_ipc_util_shutdown_requests(af_ipc_req_control_t *req_control);
int af_ipc_send(int fd, af_ipc_req_control_t *req_control, struct event_base *event_base,
                uint32_t seqNum, uint8_t *txBuffer, int txBufferSize,
                af_ipc_receive_callback_t callback, void *context, int timeoutMs, char *name);
void af_ipc_handle_receive_message(int fd, uint8_t *buf, int len,
                                   uint16_t clientId, af_ipc_req_control_t *req_control,
                                   af_ipc_receive_callback_t receiveCallback,
								   void *context);
#endif // __AF_IPC_PRV_H__

