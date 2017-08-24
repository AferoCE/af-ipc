/*
 * af_ipc_server.h -- IPC definitions for servers
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 * Clif Liu
 */

#ifndef __AF_IPC_SERVER_H__
#define __AF_IPC_SERVER_H__

#include "af_ipc_common.h"


#define AF_IPCS_MAX_CLIENTS    (10)
#define AF_IPCS_NOT_INUSE      (-1)
#define AF_IPCS_CID_NONE       (0)


/* prototypes for the callback functions */
typedef void (*af_ipcs_close_callback_t) (void *clientContext);
typedef int (*af_ipcs_accept_callback_t) (void *acceptContext, uint16_t clientId, void **clientContextP);

/*
 * This structure is used by the server to identify the number
 * clients (i.e the daemon processes) that server wants to
 * communicate with.
 *
 * client_fd - the socket of this client (e.g freed)
 * cid             - the client ID as assigned by the server
 * seqNum          - the sequence number of the next command sent from server to client
 * recv_event      - libevent2 event used to watch client socket
 * req_control     - database of server requests
 * clientContext   - the context info to pass into the client callback
 *                   function.
 */
typedef struct af_ipcs_client_struct {
    int                     client_fd;
    uint16_t                cid;    // client_id
    uint16_t                seqNum; // server to client sequence number
    struct event            *recv_event;
    struct af_ipcs_server_struct *server;
    af_ipc_req_control_t    req_control;
    void                    *clientContext;
} af_ipcs_client_t;

/*
 * This structure constains all of the server member data
 *
 */
typedef struct af_ipcs_server_struct {

    int          server_fd;
    struct       event_base *base;
    struct event *server_listener_event;

    pthread_mutex_t clnt_mutex; /* serialize access to clients */
    int          numClients;
    struct af_ipcs_client_struct clients[AF_IPCS_MAX_CLIENTS];
    int          lastCid;

    /* callback func when the server accepts a socket
     */
    af_ipcs_accept_callback_t acceptCallback;
    void         *acceptContext;
    af_ipc_receive_callback_t receiveCallback;
    af_ipcs_close_callback_t closeCallback;
} af_ipcs_server_t;



/*
 * af_ipcs_init -- Set up server
 *
 * base - event_base to use for socket events
 * name - name of server
 * acceptCallback - callback when server accepts a socket
 * acceptContext - context for accept callback
 * receiveCallback - callback when server receives data from the socket
 * closeCallback - callback when client closes or exits prematurely
 *
 * returns pointer to server if successful; otherwise returns NULL and
 * errno contains the error code
 */
af_ipcs_server_t *af_ipcs_init(struct event_base *base,
                               char              *name,
                               af_ipcs_accept_callback_t acceptCallback, void *acceptContext,
                               af_ipc_receive_callback_t receiveCallback,
                               af_ipcs_close_callback_t closeCallback);


/*
 * af_ipcs_send_response -- send a response to a client
 *
 * server - pointer to server object
 * seqNum - sequence number of request to which this is the response
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcs_send_response (af_ipcs_server_t *server, uint32_t seqNum, uint8_t *txBuffer, int txBufferSize);

/*
 * af_ipcs_send_unsolicited -- send an unsolicited message to a client
 *
 * server - pointer to server object
 * clientId - client ID of client to which message should be sent, 0 for broadcast
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcs_send_unsolicited (af_ipcs_server_t *server, uint16_t clientId, uint8_t *txBuffer, int txBufferSize);

/*
 * af_ipcs_send_request -- sends request to a client with an expected response
 *
 * server - pointer to server object
 * clientId - ID of client to which message should be sent. Must be nonzero.
 * txBuffer - buffer used to compose a message
 * txBufferSize - size of composition buffer
 * callback - callback that is called when the response is received or a timeout occurs
 *            Set to NULL if unsolicited
 * context - context for the callback
 * timeoutMs - milliseconds before the request times out, or 0 if no timeout (see notes)
 *
 * The caller is responsible for allocating the transmit buffer. The buffer can be
 * reused as soon as the function returns.
 *
 * The timeoutMs parameter specifies the maximum time that can pass before the
 * receive callback gets called. If the client responds to the request after
 * this time, the response is dropped.
 *
 * Returns 0 on success or -1 on failure; errno contains the error.
 */
int af_ipcs_send_request(af_ipcs_server_t *server, uint16_t clientId,
                         uint8_t *txBuffer, int txBufferSize,
                         af_ipc_receive_callback_t callback, void *context,
                         int timeoutMs);

/*
 * Disconnect a client, if they're still currently connected.
 *
 * server - pointer to server object
 * clientId - ID of client to disconnect
 */
int af_ipcs_disconnect_client(af_ipcs_server_t *server, uint16_t clientId);

/*
 * af_ipcs_shutdown -- shutdown the server
 *
 * server - server to shutdown
 *
 * Any clients will be disconnected first and their callbacks will be called before this function
 * returns.
 *
 */
void
af_ipcs_shutdown(af_ipcs_server_t *server);

#endif // __AF_IPC_SERVER_H__
