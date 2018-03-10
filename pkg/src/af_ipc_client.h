/*
 * af_ipc_client.h -- IPC definitions for clients
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 * Clif Liu
 */

#ifndef __AF_IPC_CLIENT_H__
#define __AF_IPC_CLIENT_H__

#include "af_ipc_common.h"

typedef void (*af_ipcc_close_callback_t) (void *receiveContext);

/*
 * Opaque type representing a server
 */
typedef struct af_ipcc_server_struct af_ipcc_server_t;

/*
 * af_ipcc_get_server -- connects to a server
 *
 * base - event_base to use to handle IPC events
 * serverName - name of server
 * receiveCallback - function that will be called when a command or unsolicited message is received
 * receiveContext - context for the unsolicited message callback
 * closeCallback - callback when server exits prematurely
 *
 * Returns a pointer to the server if successful; otherwise returns NULL and
 * errno contains the error code
 *
 * The receiveContext is passed as a parameter to the closeCallback. When the closeCallback is called
 * do not call af_ipcc_shutdown. The server will be cleaned up and freed for you after the callback
 * completes.
 */

af_ipcc_server_t *af_ipcc_get_server(struct event_base *base, char *serverName,
                                     af_ipc_receive_callback_t receiveCallback, void *receiveContext,
                                     af_ipcc_close_callback_t closeCallback);

/*
 * af_ipcc_send_response -- send a response to the server
 *
 * s - server to send message to
 * seqNum - sequence number of request to which this is the response
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * The sequence number allows the server to match the response to a particular request.
 *
 * The caller is responsible for allocating the transmit buffer. The buffer can be
 * reused as soon as the function returns.
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcc_send_response (af_ipcc_server_t *s, uint32_t seqNum, uint8_t *txBuffer, int txBufferSize);

/*
 * af_ipcc_send_unsolicited -- send an unsolicited message to the server
 *
 * s - server to send message to
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * The caller is responsible for allocating the transmit buffer. The buffer can be
 * reused as soon as the function returns.
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcc_send_unsolicited (af_ipcc_server_t *s, uint8_t *txBuffer, int txBufferSize);

/*
 * af_ipcc_send_request -- sends request to server with an expected response
 *
 * s - server to send message to
 * txBuffer - buffer used to compose a message
 * txBufferSize - size of composition buffer
 * callback - callback that is called when the response is received or a timeout occurs
 *            Set to NULL for an unsolicited message.
 * context - context for the callback
 * timeoutMs - milliseconds before the request times out, or 0 if no timeout (see notes)
 *
 * The caller is responsible for allocating the transmit buffer. The buffer can be
 * reused as soon as the function returns.
 *
 * The timeoutMs parameter specifies the maximum time that can pass before the
 * receive callback gets called. If the server responds to the request after
 * this time, the response is dropped.
 *
 * Returns 0 on success or -1 on failure; errno contains the error.
 */
int af_ipcc_send_request(af_ipcc_server_t *s,
                         uint8_t *txBuffer, int txBufferSize,
                         af_ipc_receive_callback_t callback, void *context,
                         int timeoutMs);

/*
 * af_ipcc_shutdown -- shuts down connection to server
 *
 * s - server for which the connection will be shut down
 *
 * The close callback will not be called in response to this function
 *
 * This function can be called from inside the receive callback.
 * You should NOT use the server structure after calling this function.
 *
 */
void af_ipcc_shutdown(af_ipcc_server_t *s);

#endif // __AF_IPC_CLIENT_H__
