/*
 * aflib_ipc_common.c
 *
 * Contains the code common to servers and clients
 *
 * Copyright (c) 2015-2016 Afero, Inc. All rights reserved.
 *
 */
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "af_ipc_common.h"
#include "af_ipc_prv.h"
#include "af_log.h"

uint16_t af_ipc_get_client_id_from_seq_num(uint32_t seqNum)
{
    return AF_IPC_GET_CLIENT_ID(seqNum);
}

int af_ipc_seq_num_is_request(uint32_t seqNum)
{
    return AF_IPC_GET_SEQ_ID(seqNum) != 0;
}

const char *af_ipc_server_sock_path_prefix = IPC_SERVER_DEFAULT_SOCK_PATH_PREFIX;

void
af_ipc_set_server_sock_path_prefix(const char *prefix) {
    af_ipc_server_sock_path_prefix = prefix;
}

/* af_ipc_util_find_unused_request
 *
 * Find an unused slot in the request db (to store an incoming
 * request).
 *
 * This function must be called while the req_control mutex is locked
 */
static af_ipc_request_t *
af_ipc_util_find_unused_request(af_ipc_req_control_t  *req_control)
{
    af_ipc_request_t *retVal = NULL;

    if (req_control) {
        if (req_control->freeRequests) {
            retVal = req_control->freeRequests;
            req_control->freeRequests = retVal->next;
            retVal->next = req_control->pendingRequests;
            req_control->pendingRequests = retVal;
            AFLOG_DEBUG3("ipc_req_alloc_alloc_from_free_list");
        } else {
            retVal = (af_ipc_request_t *)calloc(1, sizeof(af_ipc_request_t));
            if (retVal) {
                retVal->next = req_control->pendingRequests;
                req_control->pendingRequests = retVal;
                AFLOG_DEBUG3("ipc_req_alloc_malloc");
            } else {
                AFLOG_ERR("af_ipc_util_find_unused_calloc::");
            }
        }
    } else {
        AFLOG_ERR("af_ipc_util_find_unused_null::");
    }
    return retVal;
}

/* af_ipc_remove_request
 *
 * Remove a pending request
 *
 * This function must be called while the req_control mutex is locked
 */
static void
af_ipc_remove_request (af_ipc_req_control_t *req_control, uint32_t seqNum)
{
    af_ipc_request_t  *request, *prev = NULL;

    if (req_control == NULL) {
        AFLOG_ERR("remove_request:req_control=NULL:bad request");
        return;
    }

    for (request = req_control->pendingRequests; request; request = request->next) {
        if (request->seqNum == seqNum) {
            if (prev) {
                prev->next = request->next;
            } else {
                req_control->pendingRequests = request->next;
            }
            break;
        }
        prev = request;
    }

    if (request) {
        request->next = req_control->freeRequests;
        req_control->freeRequests = request;
        AFLOG_DEBUG3("af_ipc_remove_request:Removed pending req(seqNum=%08x)", seqNum);
    } else {
        AFLOG_WARNING("remove_req:seqNum=%08x:can't find request", seqNum);
    }

    return;
}

/* af_ipc_util_init_requests
 *
 * Internal routine to initialize request database
 */
int
af_ipc_util_init_requests(af_ipc_req_control_t *req_control)
{
    int ret;

    if (req_control == NULL) {
        AFLOG_ERR("init_req:req_control=NULL:");
        errno = EINVAL;
        return -1;
    }
    /* clear the structure */
    memset(req_control, 0, sizeof(af_ipc_req_control_t));

    req_control->lastSeqId = 0;   /* first generated sequence number is 1 */
    ret = pthread_mutex_init(&req_control->mutex, NULL);
    if (ret == 0) {
        req_control->mutexCreated = 1;
    }
    return ret;
}

/* af_ipc_util_shutdown_requests
 *
 * Internal routine to cleanly shut down request database
 */
void
af_ipc_util_shutdown_requests(af_ipc_req_control_t *req_control)
{
    if (req_control == NULL) {
        AFLOG_ERR("shutdown_req:req_control=NULL:");
        return;
    }

    // *** mutex lock ***
    pthread_mutex_lock(&req_control->mutex);

    /* free pending requests and any remaining timeout events */
    af_ipc_request_t *req = req_control->pendingRequests;
    while (req) {
        af_ipc_request_t *next = req->next;
        if (req->event) {
            event_del(req->event);
            event_free(req->event);
            req->event = NULL;
        }
        free(req);
        req = next;
    }

    req = req_control->freeRequests;
    while (req) {
        af_ipc_request_t *next = req->next;
        free(req);
        req = next;
    }

    pthread_mutex_unlock(&req_control->mutex);
    // *** end mutex lock ***

    if (req_control->mutexCreated) {
        pthread_mutex_destroy(&req_control->mutex);
        req_control->mutexCreated = 0;
    }
}

/* af_ipc_util_gen_seq_id
 *
 * Generate a new sequence number
 */
static uint32_t
af_ipc_util_gen_seq_id(af_ipc_req_control_t *req_control)
{
    req_control->lastSeqId = (req_control->lastSeqId + 1) & 0x7fff;
    if (req_control->lastSeqId == 0) {
        req_control->lastSeqId++; // start with 1
    }

    AFLOG_DEBUG3("gen_seq_id:seqId=%d", req_control->lastSeqId);
    return req_control->lastSeqId;
}

/* af_ipc_on_timeout
 *
 * Handle the EV_TIMEOUT event when a request times out
 */
static void
af_ipc_on_timeout(int listenfd, short evtype, void *arg)
{
    af_ipc_request_t *pendingReq = (af_ipc_request_t *)arg;
    if (pendingReq == NULL) {
        AFLOG_ERR("af_ipc_on_timeout_arg_null");
        return;
    }

    AFLOG_WARNING("ipc_timeout:seqNum=%08x", pendingReq->seqNum);

    af_ipc_req_control_t *rc = pendingReq->req_control;

    if (rc == NULL) {
        AFLOG_ERR("af_ipc_on_timeout_req_control_null");
        return;
    }

    // *** mutex lock ***
    pthread_mutex_lock(&rc->mutex);

    af_ipc_receive_callback_t callback = pendingReq->responseCallback;
    void *context = pendingReq->responseContext;

    /* clean up the event if it exists although we know it exists */
    if (pendingReq->event) {
        event_free(pendingReq->event);
        pendingReq->event = NULL;
    }

    af_ipc_remove_request(pendingReq->req_control, pendingReq->seqNum);

    pthread_mutex_unlock(&rc->mutex);
    // *** end mutex lock ***

    if (callback) {
        (callback)(AF_IPC_STATUS_TIMEOUT, 0, NULL, 0, context);
    }
}

/*
 * af_ipc_send -- base function for sending an IPC message
 *
 * if timeout <= 0 then
 *   if seqNum == 0 then unsolicited message with no reply
 *   if seqNum != 0 then this is a reply to a command from the server
 * if timeout > 0 then this is a command to the server and a reply is expected
 */
int
af_ipc_send(int fd, af_ipc_req_control_t *req_control, struct event_base *base,
            uint32_t seqNum, uint8_t *txBuffer, int txBufferSize,
            af_ipc_receive_callback_t callback, void *context,
            int timeoutMs, char *name)
{
    int err;
    af_ipc_request_t *req = NULL;

    /* using scatter/gather capability */
    struct msghdr msg;
    struct iovec iv[2];
    struct af_ipc_msghdr req_hdr;

    if (g_debugLevel >= 2 && timeoutMs <= 0) {
        char hexBuf[80];
        af_util_convert_data_to_hex_with_name("buf", txBuffer, txBufferSize, hexBuf, sizeof(hexBuf));
        if (seqNum) {
            AFLOG_DEBUG2("ipc_%s_tx_response:fd=%d,seqNum=%08x,%s", name, fd, seqNum, hexBuf);
        } else {
            AFLOG_DEBUG2("ipc_%s_tx_unsolicited:fd=%d,%s", name, fd, hexBuf);
        }
    }

    /* Check total size against receive buffer size */
    if (txBufferSize + sizeof(req_hdr) > AF_IPC_MAX_MSGLEN) {
        AFLOG_ERR("ipc_send_size:txBufferSize=%d,maxSize=%zd", txBufferSize, AF_IPC_MAX_MSGLEN - sizeof(req_hdr));
        errno = EINVAL;
        return -1;
    }

    /* format the message to send & update the request cb */
    memset(&msg, 0, sizeof(msg));
    memset(&req_hdr, 0, sizeof(req_hdr));

    iv[0].iov_base = &req_hdr;
    iv[0].iov_len  = sizeof(req_hdr);
    iv[1].iov_base = txBuffer;
    iv[1].iov_len  = txBufferSize;
    msg.msg_iov    = iv;
    msg.msg_iovlen = 2;
    req_hdr.len = txBufferSize + sizeof(req_hdr);

    /* check if this is a send with reply */
    if (timeoutMs > 0) {

        /* make sure the request control was provided */
        if (req_control == NULL) {
            AFLOG_ERR("ipc_send_req_control:req_control=NULL");
            errno = EINVAL;
            return -1;
        }

        // *** mutex lock ***
        pthread_mutex_lock(&req_control->mutex);

        req = af_ipc_util_find_unused_request(req_control);
        if (req) {
            /* set up sequence number, which is necessary for af_ipc_remove_request to succeed */
            seqNum  = af_ipc_util_gen_seq_id(req_control);
            req_hdr.seqNum = seqNum | AF_IPC_SEQNUM_NEW_REQUEST_MASK; /* this is a new request */
            req->seqNum = seqNum;

            if (g_debugLevel >= 2) {
                char hexBuf[80];
                af_util_convert_data_to_hex_with_name("buf", txBuffer, txBufferSize, hexBuf, sizeof(hexBuf));
                AFLOG_DEBUG2("ipc_%s_tx_request:fd=%d,seqNum=%08x,%s", name, fd, seqNum, hexBuf);
            }
            req->responseCallback = callback;
            req->responseContext = context;
            req->req_control = req_control;
            req->timeout_val = timeoutMs;
            req->event = NULL;

            /* set up the timeout event */
            struct event *e = evtimer_new(base, af_ipc_on_timeout, req);
            if (e) {
                req->event = e;

                struct timeval timeout;
                timeout.tv_sec = (timeoutMs / 1000);
                timeout.tv_usec = (timeoutMs % 1000) * 1000;

                evtimer_add(e, &timeout);
                AFLOG_DEBUG3("ipc_send:timeout_sec=%ld,timeout_usec=%ld", (long)timeout.tv_sec, (long)timeout.tv_usec);
            } else {
                err = errno;
                AFLOG_ERR("af_ipc_send_evtimer_new:base_null=%d,errno=%d", base==NULL, errno);
                af_ipc_remove_request(req_control, seqNum);
                req = NULL;
            }
        } else {
            err = ENOMEM;
            AFLOG_ERR("af_ipc_send_alloc_req:errno=%d", err);
        }

        pthread_mutex_unlock(&req_control->mutex);
        // *** end mutex lock ***

        if (!req) {
            errno = err;
            return -1;
        }

        AFLOG_DEBUG3("af_ipc_send_added_pending:seqNum=%08x", seqNum);


    } else {
        req_hdr.seqNum = seqNum;
    }

    /* Attempt to send a message to the server */
    if (sendmsg(fd, &msg, /*MSG_DONTWAIT*/ 0) < 0) {
        AFLOG_ERR("af_ipc_send_sendmsg:fd=%d,errno=%d", fd, errno);
        err = errno;

        /* clean up request if it's pending */
        if (req) {
            // *** mutex lock ***
            pthread_mutex_lock(&req_control->mutex);

            if (req->event) {
                event_del(req->event);
                event_free(req->event);
                req->event = NULL;
            }
            af_ipc_remove_request(req_control, seqNum);

            pthread_mutex_unlock(&req_control->mutex);
            // *** end mutex lock ***
        }
        req = NULL;

        errno = err;
        return -1;
    }

    return 0;
}


void af_ipc_handle_receive_message(int fd, uint8_t *buf, int len,
                                   uint16_t clientId,
                                   af_ipc_req_control_t *req_control,
                                   af_ipc_receive_callback_t receiveCallback,
                                   void *context)
{
    int pos = 0;

    while (pos < len) {
        if (pos + AF_IPC_MSGHDR_LEN >= len) {
            AFLOG_ERR("handle_receive_message_bad_packet:pos=%d,len=%d:bad receive packet; ignoring rest of packet", pos, len);
            break;
        }
        af_ipc_msghdr_t *msghdr = (af_ipc_msghdr_t *)&buf[pos];
        uint32_t seqNum = AF_IPC_GET_SEQ_ID(msghdr->seqNum);
        AFLOG_DEBUG3("handle_receive_message:len=%d,seqNum=%08x:received client message", len, seqNum);
        if (pos + msghdr->len > len || msghdr->len < AF_IPC_MSGHDR_LEN) {
            AFLOG_ERR("handle_receive_message_bad_packet2:pos=%d,msghdr->len=%d,len=%d:bad receive packet; ignoring rest of packet",
                      pos, msghdr->len, len);
            break;
        }

        if (seqNum == 0 || AF_IPC_SEQNUM_IS_NEW_REQUEST(seqNum)) {
            /* this is an incoming command or an unsolicited command */

            if (g_debugLevel >= 2) {
                char hexBuf[80];
                af_util_convert_data_to_hex_with_name("buf",
                                                      buf + pos + AF_IPC_MSGHDR_LEN, msghdr->len - AF_IPC_MSGHDR_LEN,
                                                      hexBuf, sizeof(hexBuf));
                if (seqNum == 0) {
                    if (fd) {
                        AFLOG_DEBUG2("ipc_server_rx_unsolicited:fd=%d,%s", fd, hexBuf);
                    } else {
                        AFLOG_DEBUG2("ipc_client_rx_unsolicited:%s", hexBuf);
                    }
                } else {
                    if (fd) {
                        AFLOG_DEBUG2("ipc_server_rx_request:fd=%d,seqNum=%08x,%s", fd,
                                     seqNum & ~AF_IPC_SEQNUM_NEW_REQUEST_MASK, hexBuf);
                    } else {
                        AFLOG_DEBUG2("ipc_client_rx_request:seqNum=%08x,%s",
                                     seqNum & ~AF_IPC_SEQNUM_NEW_REQUEST_MASK, hexBuf);
                    }
                }
            }

            /* encode the client ID so we know which client to send the reply to */
            seqNum |= (clientId << 16);

            /* mask off the new request bit */
            seqNum &= ~AF_IPC_SEQNUM_NEW_REQUEST_MASK;

            /* call the client's receive callback */
            if (receiveCallback) {
                AFLOG_DEBUG3("receiveCallback:pos=%d, buflen=%zd",
                             pos, msghdr->len - AF_IPC_MSGHDR_LEN);

                receiveCallback(0, seqNum,
                                (uint8_t *)&buf[pos + AF_IPC_MSGHDR_LEN], msghdr->len - AF_IPC_MSGHDR_LEN,
                                context);
            }
            else {
                AFLOG_WARNING("ipc_handle_receive_no_rx_cb::no receive callback provided, dropped request");
            }
        } else {
            if (g_debugLevel >= 2) {
                char hexBuf[80];
                af_util_convert_data_to_hex_with_name("buf",
                                                buf + pos + AF_IPC_MSGHDR_LEN,
                                                msghdr->len - AF_IPC_MSGHDR_LEN,
                                                hexBuf, sizeof(hexBuf));

                /* this is a reply to a command */
                if (fd) {
                    AFLOG_DEBUG2("ipc_server_rx_response:fd=%d,seqNum=%08x,%s", fd, seqNum, hexBuf);
                } else {
                    AFLOG_DEBUG2("ipc_client_rx_response:seqNum=%08x,%s", seqNum, hexBuf);
                }
            }

            af_ipc_receive_callback_t callback = NULL;
            void *context = NULL;
            af_ipc_request_t *pendingReq = NULL;

            // *** mutex lock ***
            pthread_mutex_lock(&req_control->mutex);

            /* find the pending request with matching sequence number */
            if (req_control) {
                for (pendingReq = req_control->pendingRequests; pendingReq; pendingReq = pendingReq->next) {
                    if (pendingReq->seqNum == seqNum) {
                        break;
                    }
                }
            }
            AFLOG_DEBUG3("handle_receive_message:seqNum=%08x,client_id=%d,pendingReq_null=%d:got reply",
                         seqNum, clientId, (pendingReq==NULL));

            if (pendingReq) {
                /* let's cancel the time first if we have timeout event so
                * timeout doesn't happen while we handle the callback
                */
                if (pendingReq->timeout_val > 0) {
                    AFLOG_DEBUG3("handle_receive_message:timeout=%d,event_NULL=%d:cancel request timer",
                                 pendingReq->timeout_val, pendingReq->event==NULL);
                    if (pendingReq->event) {
                        event_del(pendingReq->event);
                        event_free(pendingReq->event);
                        pendingReq->event = NULL;
                    }
                }

                callback = pendingReq->responseCallback;
                context = pendingReq->responseContext;

                AFLOG_DEBUG3("handle_receive_message:seqNum=%08x:done with request; remove from pending", seqNum);
                af_ipc_remove_request(req_control, seqNum);
            } else {
                AFLOG_WARNING("handle_receive_message_no_req:seqNum=%08x:no request matching sequence number; dropping response", seqNum);
            }
            pthread_mutex_unlock(&req_control->mutex);
            // *** end mutex lock ***

            /* call the application reply_callback function to handle the reply */
            if (callback) {
                AFLOG_DEBUG3("responseCallback:pos=%d,buflen=%zd",
                             pos, msghdr->len - AF_IPC_MSGHDR_LEN);
                callback(0, seqNum,
                         (uint8_t *)&buf[pos + AF_IPC_MSGHDR_LEN], msghdr->len - AF_IPC_MSGHDR_LEN,
                         context);
            }
        }
        pos += msghdr->len;
    }
}
