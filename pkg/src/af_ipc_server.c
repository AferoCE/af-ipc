/*
 * af_ipc_server.c
 *
 * The IPC Server layer infrastructure.
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <assert.h>

#include "af_log.h"
#include "af_ipc_server.h"


#define IPC_SERVER_BACKLOG_QLEN  16


/* this might have to be in the server's db
 * - we probably want a read buffer, and send buffer?
 * */
void af_ipcs_server_on_recv(evutil_socket_t fd, short events, void *arg);
static af_ipcs_client_t *af_ipcs_add_client (af_ipcs_server_t *s, int  client_fd);

/* generate a non-zero client ID; must be called in the client mutex */
static uint16_t
af_ipcs_get_next_cid(af_ipcs_server_t *s)
{
    while(1) {
        int i;
        s->lastCid++;
        if (s->lastCid >= (1 << 15)) {  // check for rollover
            s->lastCid = 1;
        }
        for (i=0; i<AF_IPCS_MAX_CLIENTS; i++) {
            if (s->clients[i].client_fd != AF_IPCS_NOT_INUSE && s->clients[i].cid == s->lastCid) {
                break;
            }
        }
        if (i >= AF_IPCS_MAX_CLIENTS) {
            break;
        }
    }
    return s->lastCid;
}

static af_ipcs_client_t *
af_ipcs_find_unused_client(af_ipcs_server_t *s)
{
    int i;

    if ((s->numClients >= AF_IPCS_MAX_CLIENTS))  {
        AFLOG_ERR("ipc_server_max_clients:maxClients=%d:Exceeded max number of supported clients", AF_IPCS_MAX_CLIENTS);
        return NULL;
    }

    for (i=0; i<AF_IPCS_MAX_CLIENTS; i++) {
        if (s->clients[i].client_fd == AF_IPCS_NOT_INUSE) {
            return &s->clients[i];
        }
    }
    return NULL;
}


/* find the client based on the client ID or cid */
static af_ipcs_client_t *
af_ipcs_find_client_by_cid(af_ipcs_server_t *s, int  cid)
{
    int i;

    for (i=0; i<AF_IPCS_MAX_CLIENTS; i++) {
        if (s->clients[i].cid == cid) {
            return &(s->clients[i]);
        }
    }
    return NULL;
}


/* Internal function to 'remove' an connected client
 */
static int
af_ipcs_reset_client(af_ipcs_server_t *s, af_ipcs_client_t  *client)
{
    if (client) {
        client->client_fd = AF_IPCS_NOT_INUSE;
        client->cid       = AF_IPCS_CID_NONE;

        if (client->recv_event) {
            event_free(client->recv_event);
        }
        client->recv_event = NULL;
        client->clientContext = NULL;
        client->server = s;
        af_ipc_util_shutdown_requests(&client->req_control);
        return (0);
    }
    return (-1);
}

/*
 * Internal function to 'add' an connected client
 */
static af_ipcs_client_t *
af_ipcs_add_client (af_ipcs_server_t *s, int  client_fd)
{
    // **********************************
    // Control the access of the client DB
    pthread_mutex_lock(&s->clnt_mutex);

    af_ipcs_client_t *client = af_ipcs_find_unused_client(s);

    if (client != NULL) {
        client->cid = af_ipcs_get_next_cid(s);
        af_ipc_util_init_requests(&client->req_control);

        client->client_fd = client_fd;
        client->recv_event = event_new(s->base, client_fd, (EV_READ|EV_PERSIST|EV_ET),
                                       af_ipcs_server_on_recv, (void *)client);

        /* call the accept callback to inform the application of the new client */
        if (s->acceptCallback) {
            (s->acceptCallback)(s->acceptContext, client->cid, &client->clientContext);
        }

        AFLOG_DEBUG1("ipc_server_connect:client_fd=%d", client_fd);

        /* Update the server cb */
        s->numClients++;
    }

    pthread_mutex_unlock(&s->clnt_mutex);
    // end mutex lock
    // **********************************

    return (client);
}


/*
 * af_ipcs_send_response -- send a response to a client
 *
 * seqNum - sequence number of request to which this is the response
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcs_send_response(af_ipcs_server_t *s, uint32_t seqNum, uint8_t *txBuffer, int txBufferSize)
{
    uint16_t seqId = AF_IPC_GET_SEQ_ID(seqNum);

    if (s == NULL) {
        AFLOG_ERR("ipc_client_send_resp:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (seqId != 0) {
        uint16_t clientId = AF_IPC_GET_CLIENT_ID(seqNum);
        af_ipcs_client_t *this_client = af_ipcs_find_client_by_cid(s, clientId);

        if ((this_client) && (this_client->client_fd != AF_IPCS_NOT_INUSE)) {

            return af_ipc_send(this_client->client_fd, NULL, NULL,
                               seqNum, txBuffer, txBufferSize,
                               NULL, NULL, 0, "server");
        }
        else {
            AFLOG_ERR("ipc_server_send_resp_client_id:clientId=%d,seqNum=%x:client id not found", clientId, seqNum);
            errno = EINVAL;
            return (-1);
        }
    } else {
        AFLOG_ERR("ipc_server_send_resp_seq_id:seqId=%d,seqNum=%x:Invalid sequence id", seqId, seqNum);
        errno = EINVAL;
        return (-1);
    }
}

/*
 * af_ipcs_send_unsolicited -- send an unsolicited message to a client
 *
 * clientId - client ID of client to which message should be sent, 0 for broadcast
 * txBuffer - Buffer of data to send
 * txBufferSize - Size of data in buffer
 *
 * returns 0 if successful or -1 if not; errno contains error
 */
int
af_ipcs_send_unsolicited (af_ipcs_server_t *s, uint16_t clientId, uint8_t *txBuffer, int txBufferSize)
{
    int result = 0;

    if (s == NULL) {
        AFLOG_ERR("ipc_client_send_unsol:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (clientId == 0) {
        AFLOG_INFO("broadcast_message::");
        int count;
        for (count = 0; count < s->numClients; count++) {
            /* send the same message to every client on the server's db */
            if (s->clients[count].client_fd != AF_IPCS_NOT_INUSE) {
                if (af_ipc_send(s->clients[count].client_fd, NULL, NULL,
                                0, txBuffer, txBufferSize,
                                NULL, NULL, 0, "server") < 0) {
                }
            }
        }
    } else {
        /* unsolicited message to specific client */
        af_ipcs_client_t *this_client = af_ipcs_find_client_by_cid(s, clientId);

        if (this_client != NULL) {
            int fd = this_client->client_fd;
            if (fd >= 0) {
                result = af_ipc_send(fd, NULL, NULL,
                                     0, txBuffer, txBufferSize,
                                     NULL, NULL, 0, "server");
            } else {
                AFLOG_ERR("ipc_server_send_unsol_client_fd:fd=%d:client fd is invalid", fd);
                errno = EINVAL;
                result = -1;
            }
        } else {
            AFLOG_ERR("ipc_server_send_unsol_client_id:clientId=%d:client id not found", clientId);
            errno = ENOENT;
            result = -1;
        }
    }

    return result;
}

/*
 * af_ipcs_send_request -- sends request to a client with an expected response
 *
 * clientId - client ID of client to which message should be sent
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
int
af_ipcs_send_request(af_ipcs_server_t *s, uint16_t clientId, uint8_t *txBuffer, int txBufferSize,
                     af_ipc_receive_callback_t callback, void *context,
                     int timeoutMs)
{
    if (s == NULL) {
        AFLOG_ERR("ipc_client_send_req:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    if (clientId == 0) {
        AFLOG_ERR("ipc_server_send_req_bad_client_id:clientId=%d:invalid client id", clientId);
        errno = EINVAL;
        return (-1);
    }
    af_ipcs_client_t *client = af_ipcs_find_client_by_cid(s, clientId);
    if (client == NULL) {
        AFLOG_ERR("ipc_server_send_req_client_id:clientId=%d:client not found", clientId);
        errno = ENOENT;
        return (-1);
    }
    int fd = client->client_fd;
    if (fd < 0) {
        AFLOG_ERR("ipc_server_send_req_client_fd:fd=%d:invalid client fd", fd);
        errno = ENOENT;
        return (-1);
    } else {
        return af_ipc_send(fd, &client->req_control, s->base,
                           0, txBuffer, txBufferSize,
                           callback, context, timeoutMs, "server");
    }
}

/*
 * Callback function to receive data from client socket
 */
void
af_ipcs_server_on_recv(evutil_socket_t fd, short events, void *arg)
{
    af_ipcs_client_t *client = (af_ipcs_client_t *)arg;
    char    buf[AF_IPC_MAX_MSGLEN];
    ssize_t recvmsg_len;

    AFLOG_DEBUG3("on_recv:fd=%d,ev=0x%hx", fd, events);

    if (events & EV_READ) {
        while (1) {
            memset(buf, 0, sizeof(buf));
            recvmsg_len = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            AFLOG_DEBUG3("on_recv:recvmsg_len=%zd", recvmsg_len);

            if (recvmsg_len <= 0) { // EOF or error
                if (recvmsg_len == 0) {
                    AFLOG_INFO("ipc_server_client_closed:fd=%d,errno=%d:client closed", fd, errno);
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    AFLOG_ERR("receive_error:fd=%d,errno=%d", fd, errno);
                }

                // Call the close callback
                if (client->server->closeCallback) {
                    client->server->closeCallback(client->clientContext);
                }
                // Reset the client settings to mark it as free
                af_ipcs_reset_client(client->server, client);

                //semaphore/mutex
                pthread_mutex_lock(&client->server->clnt_mutex);
                client->server->numClients--;
                pthread_mutex_unlock(&client->server->clnt_mutex);
                // end mutex

                close (fd);
                break;
            }
            else {  // we got a message from the client
                af_ipc_handle_receive_message(fd, (uint8_t *)buf, recvmsg_len,
                                              client->cid, &client->req_control,
                                              client->server->receiveCallback,
											  client->clientContext);
            }
        }
    }
    else {
        /* Log an error.  Do we need to do more? */
        AFLOG_WARNING("ipc_server_event:event=%d:Unsupported event type received", events);
    }

    return;
} // af_ipcs_server_on_recv


/*
 * Accept a new connection request and save the incoming fd to
 * the client db (ie.)
 *
 *  -- need to save incoming client fd
 *  -- when to free fd_state
 */
void
af_ipcs_on_accept(evutil_socket_t server_fd, short event, void *arg)
{
    int client_fd;
    af_ipcs_server_t *s = (af_ipcs_server_t *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);


    if(!(event & EV_READ)) {
        AFLOG_ERR("ipc_server_on_accept_event:event=0x%hx:unhandled accept event", event);
        return;
    }

    client_fd = accept(server_fd, (struct sockaddr*)&ss, &slen);
    if (client_fd < 0) { // XXXX eagain - how best to handle that??
        if(errno != EWOULDBLOCK && errno != EAGAIN) {
            AFLOG_ERR("ipc_server_accept_err:errno=%d,server_fd=%d:Error accepting an incoming connection", errno, server_fd);
        }
        else  {
            AFLOG_INFO("ipc_server_accept_again::");
        }
    } else if (client_fd > FD_SETSIZE) {
        close(client_fd); // XXX replace all closes with EVUTIL_CLOSESOCKET */
    } else {
        /* connect is OK, accept it -- client_fd needs to be saved */
        af_ipcs_client_t       *client = NULL;

        /* make the client_fd_nonblocking, and allocate fd_state */
        evutil_make_socket_nonblocking(client_fd);

        /* Initialize a read event on client_fd: associate the event with
         * the given base, and set up the af_ipcs_server_on_recv callback to
         * be invoked whenever data is available to be read on the client_fd.
         */
        client = af_ipcs_add_client(s, client_fd);
        if (client) {
            AFLOG_DEBUG3("ipc_server:schedule the event on_recv for client_fd=%d, recv_event_null=%d",
                         client->client_fd, (client->recv_event==NULL));
            event_add(client->recv_event, NULL);
        }
        else {
            AFLOG_ERR("ipc_server_add_client:client_fd=%d:add client failed", client_fd);
            close(client_fd);
        }
    }

    return;
}

/* Internl routine to initialize various control data structures */
static void
af_ipcs_init_server_DBs(af_ipcs_server_t *s)
{
    int i;

    memset(s, 0, sizeof(af_ipcs_server_t));
    for (i=0; i<AF_IPCS_MAX_CLIENTS; i++) {
        af_ipcs_reset_client(s, &s->clients[i]);
    }

    return;
}


/*
 * Initialize the IPC Layer Infrastructure for the Server
 *
 */
af_ipcs_server_t *
af_ipcs_init(struct event_base *base,
             char              *name,
             af_ipcs_accept_callback_t acceptCallback, void *acceptContext,
             af_ipc_receive_callback_t receiveCallback,
             af_ipcs_close_callback_t closeCallback)
{
    int    server_fd;
    struct sockaddr_un  server_addr;
    struct event        *server_listener_event;
    char   ss_path[128];
    int    tmp_reuse = 1;
    int    len;
    af_ipcs_server_t *s;

    if (base == NULL) {
        AFLOG_ERR("ipc_server_init_event_base:base=NULL:event_base is invalid");
        errno = EINVAL;
        return NULL;
    }
    AFLOG_DEBUG3("ipc_server_libevent:version=%s,method=%s", event_get_version(), event_base_get_method(base));

    s = (af_ipcs_server_t *)calloc(1, sizeof (af_ipcs_server_t));
    if (s == NULL) {
        AFLOG_ERR("ipc_server_alloc::can't allocate server");
        errno = ENOMEM;
        return NULL;
    }

    /* initialize the DBs used by the server */
    af_ipcs_init_server_DBs(s);

    /* create the server socket for listening to the incoming clients */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        AFLOG_ERR("ipc_server_open_listen:errno=%d:socket failed", errno);
        free(s);
        return NULL;
    }

    /* make the server socket nonblocking, set socket reuseable */
    evutil_make_socket_nonblocking(server_fd);
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &tmp_reuse, sizeof(tmp_reuse))) {
        AFLOG_ERR("ipc_server_reuseable:errno=%d:Error enabling socket address reuse on listening socket", errno);
        free(s);
        return NULL;
    }

    memset(ss_path, 0, sizeof(ss_path));
    memset(&server_addr, 0, sizeof(server_addr));

    sprintf(ss_path, "%s%s", af_ipc_server_sock_path_prefix, name);
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, ss_path, sizeof(server_addr.sun_path)-1);

    unlink(server_addr.sun_path);  /* remove existing socket */

    len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family);
    if (bind(server_fd, (struct sockaddr *)&server_addr, len) < 0) {
        AFLOG_ERR("ipc_server_bind:errno=%d,server_fd=%d,ss_path=%s:bind failed", errno, server_fd, ss_path);
        close(server_fd);
        free(s);
        return NULL;
    }

    /* Mark the socket as the passive socket, for listening to incoming
     * connection requests.
     */
    if (listen(server_fd, IPC_SERVER_BACKLOG_QLEN)<0) {
        AFLOG_ERR("ipc_server_listen:errno=%d,server_fd=%d:listen failed", errno, server_fd);
        close(server_fd);
        free(s);
        return NULL;
    }
    AFLOG_INFO("ipc_server_up:fd=%d,ss_path=%s", server_fd, ss_path);

    /* Set up the callback function for the server to accept the incoming
     * connection request
     * - error check
     */
    server_listener_event = event_new(base, server_fd, (EV_READ|EV_PERSIST),
                                      af_ipcs_on_accept, s);

    /* add the server listening event and schedule the event */
    if (event_add(server_listener_event, NULL)) {
        AFLOG_ERR("ipc_server_event_add::Error scheduling connect event on the event loop.");
        close(server_fd);
        event_free(server_listener_event);
        free(s);
        return NULL;
    }

    /* initialize the clients mutex */
    int err = pthread_mutex_init(&s->clnt_mutex, NULL);
    if (err != 0) {
        AFLOG_ERR("ipc_server_init_mutex:err=%d:Failed to init pthread_mutex_t", err);
        close(server_fd);
        event_free(server_listener_event);
        free(s);
        return NULL;
    }

    /* Update the server control block of the following: */
    s->base = base;
    s->server_fd = server_fd;
    s->server_listener_event = server_listener_event;
    s->acceptContext = acceptContext;
    s->acceptCallback = acceptCallback;
    s->receiveCallback = receiveCallback;
    s->closeCallback = closeCallback;
    s->lastCid = 0;

    return s;
}


/* closing down the server */
void
af_ipcs_shutdown(af_ipcs_server_t *s)
{
    int i;

    if (s == NULL) {
        AFLOG_ERR("ipc_server_shutdown:server=NULL:bad server");
        return;
    }

    /* close the server socket connection */
    close(s->server_fd);

    /* delete the server listening event */
    if (s->server_listener_event)
    event_free(s->server_listener_event);

    for (i=0; i<AF_IPCS_MAX_CLIENTS; i++) {
        if (s->clients[i].client_fd != AF_IPCS_NOT_INUSE) {
            if (s->clients[i].recv_event) {
                event_free(s->clients[i].recv_event);
            }

            if (s->clients[i].clientContext) {
                free(s->clients[i].clientContext);
            }
        }
    } // for

    pthread_mutex_destroy(&s->clnt_mutex);

    free(s);
}
