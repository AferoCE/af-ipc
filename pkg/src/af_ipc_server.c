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
#include "build_info.h"

#include "af_log.h"
#include "af_ipc_server.h"
#include "af_ipc_prv.h"

/*
 * This structure is used by the server to represent each client
 * (i.e., the daemon process) the server communicates with.
 *
 * next            - next client this server is connected to
 * client_fd       - the socket of this client (e.g freed)
 * cid             - the client ID as assigned by the server
 * seqNum          - the sequence number of the next command sent from server to client
 * recv_event      - libevent2 event used to watch client socket
 * server          - pointer to the server connected to this client
 * req_control     - database of requests initiated by the client
 * clientContext   - the context info to pass into the client callback function
 * flags           - flags related to the client
 * pad             - pad to align the structure to a 32-bit boundary
 */
typedef struct af_ipcs_client_struct {
    struct af_ipcs_client_struct *next;
    int                          client_fd;
    uint16_t                     cid;
    uint16_t                     seqNum;
    struct event                 *recv_event;
    struct af_ipcs_server_struct *server;
    af_ipc_req_control_t         req_control;
    void                         *clientContext;
    uint16_t                     flags;
    uint16_t                     pad;
} af_ipcs_client_t;

#define CLIENT_FLAGS_MARKED_FOR_SHUTDOWN (1 << 0)
#define CLIENT_FLAGS_IN_RECEIVE          (1 << 1)

#define DEFAULT_NUM_CLIENTS    4

/*
 * This structure is used to represent the server.
 *
 * server_fd             - the file descripter of the listening socket
 * base                  - the event base structure of the event loop
 * server_listener_event - the event that fires when data is available on the listening
 *                         socket
 * client_mutex          - the mutex protecting the integrity of the client list
 * clientPool            - the pool of clients from which new clients can be allocated
 * lastCid               - the last client ID; used to generating new client IDs
 * acceptCallback        - called when a new client is accepted by the server
 * acceptContext         - the context provided to the accept callback
 * receiveCallback       - called when data is received by the client
 * closeCallback         - called when the client is closed down
 */
struct af_ipcs_server_struct {
    int                          server_fd;
    struct                       event_base *base;
    struct event                 *server_listener_event;
    pthread_mutex_t              clnt_mutex;
    struct af_ipcs_client_struct *clients;
    af_mempool_t                 *clientPool;
    uint16_t                     lastCid;
    uint16_t                     flags;
    af_ipcs_accept_callback_t    acceptCallback;
    void                         *acceptContext;
    af_ipc_receive_callback_t    receiveCallback;
    af_ipcs_close_callback_t     closeCallback;
};

#define SERVER_FLAGS_MUTEX_INITIALIZED (1 << 0)

#define IPC_SERVER_BACKLOG_QLEN  16

static void on_recv(evutil_socket_t fd, short events, void *arg);

/* generate a non-zero client ID; must be called in the client mutex */
static uint16_t
get_next_cid(af_ipcs_server_t *s)
{
    while(1) {
        s->lastCid++;

        if (s->lastCid == 0) {
            s->lastCid++;
        }
        af_ipcs_client_t *c;

        for (c = s->clients; c; c = c->next) {
            if (c->cid == s->lastCid) {
                break;
            }
        }
        if (c == NULL) {
            break;
        }
    }
    return s->lastCid;
}

static af_ipcs_client_t *
alloc_and_init_client(af_ipcs_server_t *s)
{
    af_ipcs_client_t *c = (af_ipcs_client_t *)af_mempool_alloc(s->clientPool);

    if (c == NULL) {
        AFLOG_ERR("ipc_server_cant_alloc::unable to allocate new client");
    } else {
        memset(c, 0, sizeof(af_ipcs_client_t));

        c->cid = get_next_cid(s);
        if (af_ipc_util_init_requests(&c->req_control) < 0) {
            AFLOG_ERR("af_ipcs_find_unused_client_init_req::");
            af_mempool_free(c);
            return NULL;
        }

        /* add client to head of list */
        c->next = s->clients;
        s->clients = c;
    }

    return c;
}


/* find the client based on the client ID or cid */
static af_ipcs_client_t *
find_client_by_cid(af_ipcs_server_t *s, int cid)
{
    af_ipcs_client_t *c;

    for (c = s->clients; c; c = c->next) {
        if (c->cid == cid) {
            return c;
        }
    }
    return NULL;
}

#define AF_IPC_STATUS_DONT_CALL_CALLBACK 1

static void
close_client(int status, af_ipcs_client_t *client)
{
    if (!client || !client->server) return;

    uint16_t cid = client->cid;
    void *context = client->clientContext;
    af_ipcs_server_t *server = client->server;

    // shutdown the client socket
    shutdown(client->client_fd, SHUT_RD);

    // turn off receive event
    if (client->recv_event) {
        event_free(client->recv_event);
        client->recv_event = NULL;
    }

    // remove client from list of active clients
    int client_fd = client->client_fd;

    pthread_mutex_lock(&server->clnt_mutex);
    af_ipcs_client_t *c, *last = NULL;
    for (c = server->clients; c; c = c->next) {
        if (client == c) {
            if (last) {
                last->next = c->next;
            } else {
                server->clients = c->next;
            }
            break;
        }
        last = c;
    }
    pthread_mutex_unlock(&server->clnt_mutex);

    // clean up the request control structure
    af_ipc_util_shutdown_requests(&client->req_control);

    // free the client memory
    af_mempool_free(client);

    // close the client fd
    EVUTIL_CLOSESOCKET(client_fd);

    if (status != AF_IPC_STATUS_DONT_CALL_CALLBACK && server->closeCallback) {
        (server->closeCallback)(status, cid, context);
    }
}


/*
 * Internal function to add a connected client
 */
static af_ipcs_client_t *
add_client (af_ipcs_server_t *s, int  client_fd)
{
    // **********************************
    // Control the access of the client DB
    pthread_mutex_lock(&s->clnt_mutex);

    af_ipcs_client_t *client = alloc_and_init_client(s);

    if (client != NULL) {

        client->server = s;
        client->client_fd = client_fd;
        client->recv_event = event_new(s->base, client_fd, (EV_READ|EV_PERSIST|EV_ET),
                                       on_recv, (void *)client);

        if (client->recv_event == NULL) {
            AFLOG_ERR("add_client_recv_event::can't allocate receive event; closing");
            close_client(AF_IPC_STATUS_DONT_CALL_CALLBACK, client);
            client == NULL;
        } else {
            AFLOG_DEBUG1("ipc_server_connect:client_fd=%d", client_fd);
        }
    }

    pthread_mutex_unlock(&s->clnt_mutex);
    // end mutex lock
    // **********************************

    if (client && s->acceptCallback) {
        (s->acceptCallback)(s->acceptContext, client->cid, &client->clientContext);
    }


    return (client);
}

int
af_ipcs_send_response(af_ipcs_server_t *s, uint32_t seqNum, uint8_t *txBuffer, int txBufferSize)
{
    uint16_t seqId = AF_IPC_GET_SEQ_ID(seqNum);

    if (s == NULL) {
        AFLOG_ERR("ipc_client_send_resp:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    int result = -1;

    pthread_mutex_lock(&s->clnt_mutex);

    if (seqId != 0) {
        uint16_t clientId = AF_IPC_GET_CLIENT_ID(seqNum);

        af_ipcs_client_t *this_client = find_client_by_cid(s, clientId);

        if (this_client) {

            result = af_ipc_send(this_client->client_fd, NULL, NULL,
                                 seqNum, txBuffer, txBufferSize,
                                 NULL, NULL, 0, "server");
            if (result < 0) {
                AFLOG_ERR("af_ipcs_send_response_send:errno=%d:failed to send, closing client", errno);
                pthread_mutex_unlock(&s->clnt_mutex);
                close_client(AF_IPC_STATUS_ERROR, this_client);
                return result;
            }
        }
        else {
            AFLOG_ERR("ipc_server_send_resp_client_id:clientId=%d,seqNum=%x:client id not found", clientId, seqNum);
            errno = EINVAL;
        }
    } else {
        AFLOG_ERR("ipc_server_send_resp_seq_id:seqId=%d,seqNum=%x:Invalid sequence id", seqId, seqNum);
        errno = EINVAL;
    }

    pthread_mutex_unlock(&s->clnt_mutex);

    return result;
}

int
af_ipcs_send_unsolicited (af_ipcs_server_t *s, uint16_t clientId, uint8_t *txBuffer, int txBufferSize)
{
    int result = 0;

    if (s == NULL) {
        AFLOG_ERR("ipc_client_send_unsol:server=NULL:bad server");
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&s->clnt_mutex);
    if (clientId == 0) {
        AFLOG_INFO("broadcast_message::");
        af_ipcs_client_t *c;
        for (c = s->clients; c; c = c->next) {
            /* send the same message to every client on the server's db */
            if (af_ipc_send(c->client_fd, NULL, NULL,
                            0, txBuffer, txBufferSize,
                            NULL, NULL, 0, "server") < 0) {
            }
        }
    } else {
        /* unsolicited message to specific client */
        af_ipcs_client_t *this_client = find_client_by_cid(s, clientId);

        if (this_client != NULL) {
            int fd = this_client->client_fd;
            if (fd >= 0) {
                result = af_ipc_send(fd, NULL, NULL,
                                     0, txBuffer, txBufferSize,
                                     NULL, NULL, 0, "server");
                if (result < 0) {
                    AFLOG_ERR("af_ipcs_send_unsol_send:errno=%d:failed to send, closing client", errno);
                    pthread_mutex_unlock(&s->clnt_mutex);
                    close_client(AF_IPC_STATUS_ERROR, this_client);
                    return result;
                }
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

    pthread_mutex_unlock(&s->clnt_mutex);
    return result;
}

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

    int result = -1;
    pthread_mutex_lock(&s->clnt_mutex);

    af_ipcs_client_t *this_client = find_client_by_cid(s, clientId);
    if (this_client != NULL) {
        int fd = this_client->client_fd;
        if (fd >= 0) {
            result = af_ipc_send(fd, &this_client->req_control, s->base,
                                 0, txBuffer, txBufferSize,
                                 callback, context, timeoutMs, "server");
            if (result < 0) {
                AFLOG_ERR("af_ipcs_send_req_send:errno=%d:failed to send, closing client", errno);
                pthread_mutex_unlock(&s->clnt_mutex);
                close_client(AF_IPC_STATUS_ERROR, this_client);
                return result;
            }
        } else {
            AFLOG_ERR("ipc_server_send_req_client_fd:fd=%d:invalid client fd", fd);
            errno = ENOENT;
        }
    } else {
        AFLOG_ERR("ipc_server_send_req_client_id:clientId=%d:client not found", clientId);
        errno = ENOENT;
    }

    pthread_mutex_unlock (&s->clnt_mutex);
    return result;
}

int
af_ipcs_close_client(af_ipcs_server_t *server, uint16_t clientId)
{
    af_ipcs_client_t *client = find_client_by_cid(server, clientId);
    if (client == NULL) {
        AFLOG_ERR("af_ipcs_shutdown_client_id:clientId=%d:client not found", clientId);
        return -1;
    }
    close_client(AF_IPC_STATUS_OK, client);
    return 0;
}

static void
on_recv(evutil_socket_t fd, short events, void *arg)
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

                close_client(AF_IPC_STATUS_ERROR, client);
                break;
            }
            else {  // we got a message from the client
                client->flags |= CLIENT_FLAGS_IN_RECEIVE;
                af_ipc_handle_receive_message(fd, (uint8_t *)buf, recvmsg_len,
                                              client->cid, &client->req_control,
                                              client->server->receiveCallback,
											  client->clientContext);
                client->flags &= ~CLIENT_FLAGS_IN_RECEIVE;
                if ((client->flags & CLIENT_FLAGS_MARKED_FOR_SHUTDOWN) != 0) {
                    close_client(AF_IPC_STATUS_OK, client);
                    break;
                }
            }
        }
    }
    else {
        /* Log an error.  Do we need to do more? */
        AFLOG_WARNING("ipc_server_event:event=%d:Unsupported event type received", events);
    }

} // on_recv


/*
 * Accept a new connection request and save the incoming fd to
 * the client db (ie.)
 *
 *  -- need to save incoming client fd
 *  -- when to free fd_state
 */
static void
on_accept(evutil_socket_t server_fd, short event, void *arg)
{
    int client_fd;
    af_ipcs_server_t *s = (af_ipcs_server_t *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);


    if (!(event & EV_READ)) {
        AFLOG_ERR("ipc_server_on_accept_event:event=0x%hx:unhandled accept event", event);
        return;
    }

    client_fd = accept(server_fd, (struct sockaddr*)&ss, &slen);
    if (client_fd < 0) {
        if(errno != EWOULDBLOCK && errno != EAGAIN) {
            AFLOG_ERR("ipc_server_accept_err:errno=%d,server_fd=%d:Error accepting an incoming connection", errno, server_fd);
        }
        else  {
            AFLOG_INFO("ipc_server_accept_again::");
        }
    } else if (client_fd > FD_SETSIZE) {
        EVUTIL_CLOSESOCKET(client_fd);
    } else {
        /* connect is OK, accept it -- client_fd needs to be saved */
        af_ipcs_client_t       *client = NULL;

        /* make the client_fd_nonblocking, and allocate fd_state */
        evutil_make_socket_nonblocking(client_fd);

        /* Initialize a read event on client_fd: associate the event with
         * the given base, and set up the on_recv callback to
         * be invoked whenever data is available to be read on the client_fd.
         */
        client = add_client(s, client_fd);

        if (client) {
            AFLOG_DEBUG3("ipc_server:schedule the event on_recv for client_fd=%d, recv_event_null=%d",
                         client->client_fd, (client->recv_event==NULL));
            event_add(client->recv_event, NULL);
        }
        else {
            AFLOG_ERR("ipc_server_add_client:client_fd=%d:add client failed", client_fd);
            EVUTIL_CLOSESOCKET(client_fd);
        }
    }

    return;
}

extern const char REVISION[];
extern const char BUILD_DATE[];

/*
 * Initialize the IPC Layer Infrastructure for the Server
 *
 */
af_ipcs_server_t *
af_ipcs_open(struct event_base *base,
             char              *name,
             af_ipcs_accept_callback_t acceptCallback, void *acceptContext,
             af_ipc_receive_callback_t receiveCallback,
             af_ipcs_close_callback_t closeCallback)
{
    struct sockaddr_un  server_addr;
    char   ss_path[128];
    int    tmp_reuse = 1;
    int    len;
    af_ipcs_server_t *s;

    AFLOG_INFO("start_ipc_server:revision=%s,build_date=%s", REVISION, BUILD_DATE);

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
        goto error;
    }

    s->server_fd = -1;

    s->clientPool = af_mempool_create(DEFAULT_NUM_CLIENTS, sizeof(af_ipcs_client_t), AF_MEMPOOL_FLAG_EXPAND);
    if (s->clientPool == NULL) {
        AFLOG_ERR("ipc_server_mempool_create::");
        errno = ENOSPC;
        goto error;
    }

    /* create the server socket for listening to the incoming clients */
    s->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s->server_fd < 0) {
        AFLOG_ERR("ipc_server_open_listen:errno=%d:socket failed", errno);
        goto error;
    }

    /* make the server socket nonblocking, set socket reuseable */
    evutil_make_socket_nonblocking(s->server_fd);
    if (setsockopt(s->server_fd, SOL_SOCKET, SO_REUSEADDR, &tmp_reuse, sizeof(tmp_reuse))) {
        AFLOG_ERR("ipc_server_reuseable:errno=%d:Error enabling socket address reuse on listening socket", errno);
        goto error;
    }

    memset(ss_path, 0, sizeof(ss_path));
    memset(&server_addr, 0, sizeof(server_addr));

    sprintf(ss_path, "%s%s", af_ipc_server_sock_path_prefix, name);
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, ss_path, sizeof(server_addr.sun_path)-1);

    unlink(server_addr.sun_path);  /* remove existing socket */

    len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family);
    if (bind(s->server_fd, (struct sockaddr *)&server_addr, len) < 0) {
        AFLOG_ERR("ipc_server_bind:errno=%d,server_fd=%d,ss_path=%s:bind failed", errno, s->server_fd, ss_path);
        goto error;
    }

    /* Mark the socket as the passive socket, for listening to incoming
     * connection requests.
     */
    if (listen(s->server_fd, IPC_SERVER_BACKLOG_QLEN) < 0) {
        AFLOG_ERR("ipc_server_listen:errno=%d,server_fd=%d:listen failed", errno, s->server_fd);
        goto error;
    }
    AFLOG_INFO("ipc_server_up:fd=%d,ss_path=%s", s->server_fd, ss_path);

    /* initialize the clients mutex */
    int err = pthread_mutex_init(&s->clnt_mutex, NULL);
    if (err != 0) {
        AFLOG_ERR("ipc_server_init_mutex:err=%d:Failed to init pthread_mutex_t", err);
        goto error;
    }

    /* Set up the callback function for the server to accept the incoming
     * connection request
     * - error check
     */
    s->server_listener_event = event_new(base, s->server_fd, (EV_READ | EV_PERSIST),
                                         on_accept, s);
    if (s->server_listener_event == NULL) {
        AFLOG_ERR("ipc_server_listener_event_new::Can't create listener event");
        goto error;
    }

    /* add the server listening event and schedule the event */
    if (event_add(s->server_listener_event, NULL)) {
        AFLOG_ERR("ipc_server_event_add::Error scheduling connect event on the event loop.");
        goto error;
    }

    /* Update the server control block */
    s->base = base;
    s->acceptContext = acceptContext;
    s->acceptCallback = acceptCallback;
    s->receiveCallback = receiveCallback;
    s->closeCallback = closeCallback;
    s->lastCid = 0;

    return s;

error:
    if (s) {
        af_ipcs_close(s);
    }

    return NULL;
}


/* closing down the server */
void
af_ipcs_close(af_ipcs_server_t *s)
{
    if (s == NULL) {
        AFLOG_ERR("ipc_server_shutdown:server=NULL:bad server");
        return;
    }

    AFLOG_INFO("af_ipcs_shutdown:fd=%d", s->server_fd);

    if (s->server_fd != -1) {
        /* shut down the listening socket */
        shutdown(s->server_fd, SHUT_RD);
    }

    /* delete the server listening event */
    if (s->server_listener_event) {
        event_free(s->server_listener_event);
    }

    /* close the server socket connection */
    if (s->server_fd != -1) {
        /* shut down the listening socket */
        EVUTIL_CLOSESOCKET(s->server_fd);
    }

    if (s->flags & SERVER_FLAGS_MUTEX_INITIALIZED) {
        af_ipcs_client_t *c;

        for (c = s->clients; c; c = c->next) {
            shutdown(c->client_fd, SHUT_RDWR);

            if (c->recv_event) {
                event_free(c->recv_event);
            }

            /* clean up outstanding requests */
            af_ipc_util_shutdown_requests(&c->req_control);

            EVUTIL_CLOSESOCKET(c->client_fd);

            /* call the close callback */
            if (s->closeCallback) {
                s->closeCallback(AF_IPC_STATUS_OK, c->cid, c->clientContext);
            }
        }

        pthread_mutex_destroy(&s->clnt_mutex);
    }

    if (s->clientPool) {
        af_mempool_destroy(s->clientPool);
    }

    free(s);
}
