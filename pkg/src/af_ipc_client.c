/*
 * af_ipc_client.c
 *
 * This is the APIs for the client side of the Afero IPC layer
 * infrastructure.
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <event.h>
#include <signal.h>
#include <limits.h>

#include "af_log.h"
#include "af_ipc_client.h"
#include "af_ipc_prv.h"

#define FLAGS_MARKED_FOR_SHUTDOWN (1 << 0)
#define FLAGS_IN_RECEIVE          (1 << 1)

/*
 * This structure represents the server.
 *
 * fd              - file descriptor used to communicate with server
 * event_base      - the event base structure for the event loop
 * lastSeqNum      - the last used sequence number; used to generate new sequence nums
 * flags           - flags related to the server
 * receiveCallback - called when data is received from the server
 * closeCallback   - called when connection to the server is closed
 * receiveContext  - context for the receiveCallback and closeCallback
 * event           - event that fires when data is received from the server
 * req_control     - database of requests to the server
 */
struct af_ipcc_server_struct {
    int                       fd;
    struct event_base         *event_base;
    uint16_t                  lastSeqNum;
    uint16_t                  flags;
    af_ipc_receive_callback_t receiveCallback;
    af_ipcc_close_callback_t  closeCallback;
    void                      *receiveContext;
    struct                    event *event;
    af_ipc_req_control_t      req_control;
};

static void
af_ipcc_client_on_recv(int listenfd, short evtype, void *arg);

af_ipcc_server_t
*af_ipcc_open_server(struct event_base *base, char *name,
                     af_ipc_receive_callback_t receiveCallback,
                     void *receiveContext,
                     af_ipcc_close_callback_t closeCallback)
{
    int             remote_server_fd;
    int             addrlen;
    struct          sockaddr_un   remote;
    char            server_path[128];

    if (base == NULL) {
        AFLOG_ERR("ipc_client_get_server:base=NULL:bad event base");
        return NULL;
    }

    /* allocate the server structure */
    af_ipcc_server_t *server = calloc(1, sizeof(af_ipcc_server_t));
    if (server == NULL) {
        AFLOG_ERR("ipc_client_get_server_malloc::failed to allocate memory");
        return NULL;
    }

    server->fd = -1;  /* not used */
    server->receiveContext  = receiveContext ;
    server->receiveCallback = receiveCallback;
    server->closeCallback = closeCallback;
    server->flags = 0;

    /* make the connection to the server */
    remote_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (remote_server_fd < 0) {
        AFLOG_ERR("ipc_client_socket:errno=%d,fd=%d:socket failed", errno, remote_server_fd);
        return NULL;
    }

    memset(server_path, 0, sizeof(server_path));
    sprintf(server_path, "%s%s", af_ipc_server_sock_path_prefix, name);

    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, server_path, sizeof(remote.sun_path)-1);
    addrlen = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(remote_server_fd, (struct sockaddr *)&remote, addrlen) < 0) {
        AFLOG_ERR("ipc_client_connect:fd=%d,errno=%d,server_path=%s:connect failed",
                  remote_server_fd, errno, remote.sun_path);
        EVUTIL_CLOSESOCKET(remote_server_fd);
        return NULL;
    }
    AFLOG_DEBUG1("ipc_client_connect:fd=%d,path=%s", remote_server_fd, server_path);


    /* Update the server_fd in the server cb.  These are one-time update */
    server->fd = remote_server_fd;
    server->event_base = base;
    server->event = NULL;

    /* initialize the req_control structure */
    int err = af_ipc_util_init_requests(&server->req_control);
    if (err != 0) {
        AFLOG_ERR("ipc_client_pthread_init:err=%d:Failed to init pthread_mutex_t", err);

        EVUTIL_CLOSESOCKET(remote_server_fd);
        server->fd = -1;
        return NULL;
    }

    /* This event is for notification */
    server->event = event_new(base, remote_server_fd, (EV_READ|EV_PERSIST|EV_ET),
                              af_ipcc_client_on_recv, (void *)server);
    event_add(server->event, NULL);

    return server;
}

static void
close_and_free_server(int status, af_ipcc_server_t *s)
{
    if (!s) return;

    af_ipcc_close_callback_t closeCb = s->closeCallback;
    void *closeContext = s->receiveContext;

    /* free the persist event for the read */
    if (s->event) {
        event_del(s->event);
        event_free(s->event);
        s->event = NULL;
    }

    /* close the server socket and free request resources */
    if (s->fd != -1) {
        af_ipc_util_shutdown_requests(&s->req_control);
        shutdown(s->fd, SHUT_RDWR);
        EVUTIL_CLOSESOCKET(s->fd);
        s->fd = -1;
    }

    free(s);

    /* execute the callback */
    if (closeCb) {
        (closeCb) (status, closeContext);
    }
}

/*
 * af_ipcc_client_on_recv
 *
 * Main routine used to handle incoming messages based on
 * the EV_READ event:
 *       - has an incoming notification
 *       - has an incoming command
 *       - received a reply
 */
static void
af_ipcc_client_on_recv(int listenfd, short evtype, void *arg)
{
    uint8_t          recv_buffer[AF_IPC_MAX_MSGLEN];
    af_ipcc_server_t *server = (af_ipcc_server_t *)arg;
	int              data_len;

    if (server == NULL) {
        AFLOG_ERR("af_ipcc_client_on_recv_server_NULL");
        return;
    }

    if (evtype & EV_READ) {
        while(1) {
            memset(recv_buffer, 0, sizeof(recv_buffer));
            data_len = recv(listenfd, recv_buffer, sizeof(recv_buffer), MSG_DONTWAIT);

            if (data_len <= 0) {
                if (data_len < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    AFLOG_ERR("ipc_client_receive_error:fd=%d,errno=%d", listenfd, errno);
                } else {
                    AFLOG_INFO("ipc_client_closed:fd=%d,errno=%d:closing client", listenfd, errno);
                }
                close_and_free_server(AF_IPC_STATUS_ERROR, server);
                break;
            } else {
                server->flags |= FLAGS_IN_RECEIVE;
                af_ipc_handle_receive_message(0, recv_buffer, data_len,
                                              0, &server->req_control,
                                              server->receiveCallback, server->receiveContext);
                server->flags &= ~FLAGS_IN_RECEIVE;
                if ((server->flags & FLAGS_MARKED_FOR_SHUTDOWN) != 0) {
                    close_and_free_server(AF_IPC_STATUS_OK, server);
                    break;
                }
            }
        }
    } else {
        AFLOG_WARNING("ipc_client_event:event=0x%hx:Unsupported event type received", evtype);
    }
}

int
af_ipcc_send_request(af_ipcc_server_t *server, uint8_t *txBuffer, int txBufferSize,
                     af_ipc_receive_callback_t callback, void *context,
                     int timeoutMs)
{
    if (server == NULL) {
        AFLOG_ERR("ipcc_send_req_server:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (server->fd < 0) {
        AFLOG_ERR("ipcc_send_req_fd:fd=%d:Server fd is invalid", server->fd);
        errno = EINVAL;
        return -1;
    }

    int retVal = af_ipc_send(server->fd, &server->req_control, server->event_base,
                             0, txBuffer, txBufferSize,
                             callback, context, timeoutMs, "client");
    if (retVal < 0) {
        AFLOG_ERR("ipcc_send_req_failed:errno=%d:closing client", errno);
        close_and_free_server(AF_IPC_STATUS_ERROR, server);
    }
    return retVal;
}

int
af_ipcc_send_response (af_ipcc_server_t *server, uint32_t seqNum,
                       uint8_t *txBuffer, int txBufferSize)
{
    if (server == NULL) {
        AFLOG_ERR("ipcc_send_resp_server:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (server->fd < 0) {
        AFLOG_ERR("ipcc_send_resp_fd:fd=%d:server fd is invalid", server->fd);
        errno = EINVAL;
        return -1;
    }

    int retVal = af_ipc_send(server->fd, NULL, NULL,
                             seqNum, txBuffer, txBufferSize,
                             NULL, NULL, 0, "client");
    if (retVal < 0) {
        AFLOG_ERR("ipcc_send_resp_failed:errno=%d:closing client", errno);
        close_and_free_server(AF_IPC_STATUS_ERROR, server);
    }
    return retVal;
}

int
af_ipcc_send_unsolicited (af_ipcc_server_t *server,
                          uint8_t *txBuffer, int txBufferSize)
{
    if (server == NULL) {
        AFLOG_ERR("ipcc_send_unsol_server:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (server->fd < 0) {
        AFLOG_ERR("ipcc_send_unsol_fd:fd=%d:server fd is invalid", server->fd);
        errno = EINVAL;
        return -1;
    }

    int retVal = af_ipc_send(server->fd, NULL, NULL,
                             0, txBuffer, txBufferSize,
                             NULL, NULL, 0, "client");
    if (retVal < 0) {
        AFLOG_ERR("ipcc_send_unsol_send:errno=%d:closing client", errno);
        close_and_free_server(AF_IPC_STATUS_ERROR, server);
    }
    return retVal;
}

void
af_ipcc_close(af_ipcc_server_t *s)
{
    if (s == NULL) {
        AFLOG_ERR("af_ipcc_close_NULL");
        return;
    }

    if ((s->flags & FLAGS_IN_RECEIVE) != 0) {
        AFLOG_DEBUG1("ipc_marked_for_shutdown");
        s->flags |= FLAGS_MARKED_FOR_SHUTDOWN;
    } else {
        close_and_free_server(AF_IPC_STATUS_OK, s);
    }
}

