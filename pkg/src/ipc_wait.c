/*
 * ipc_wait.c
 *
 * A tool used for building scripts that waits for an asynchronous event from a service
 *
 * Copyright (c) 2016 Afero, Inc. All rights reserved.
 */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <event2/event.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>     /* atoi */
#include <ctype.h>
#include <inttypes.h> // for the macros

#include "af_rpc.h"
#include "af_ipc_client.h"
#include "af_log.h"


#define IPC_WAIT_INSTR_MAX_LEN 256
#define IPC_WAIT_MAX_PARAMS 16

#define IPC_WAIT_ERR    (-1)
#define IPC_WAIT_OK     0

uint32_t g_debugLevel = LOG_DEBUG4;

/* the server name */
static const char *sName   = NULL;

/* the IPC client server control block */
static af_ipcc_server_t     *server;

/* Storage for input parameters, and output data */
uint8_t        dataReceived = 0;
static int     outBufferSize = 0;
static uint8_t outBuffer[IPC_WAIT_INSTR_MAX_LEN];


/* Usage info for the scripting tool */
static void
usage()
{
    fprintf(stderr, "ipc_wait <dest_server> [-t <timeout>]\n");
    fprintf(stderr, "  timeout defaults to 10 seconds\n");
}

/* handle_receive
 *
 * when an asynchonous message is received, the data parameter contains the raw
 * data message and the dataLen parameter contains the length of the message.
 *
 */
static void
handle_receive(int err, uint32_t seqNum, uint8_t *data, int dataLen, void *context)
{
    struct timeval  timeout_ms = {0, 100};
    if (data != NULL) {
        memcpy(outBuffer, data, dataLen);
        dataReceived = 1;
        outBufferSize = dataLen;
    }

    event_base_loopexit(server->event_base, &timeout_ms);
    return;
}


/* internal event used to handle a rare no response timeout */
static void
handle_timeout(evutil_socket_t fd, short what, void *arg)
{
    struct timeval  tmout_ms = {0, 100};

    /* just exist the eventloop */
    event_base_loopexit(server->event_base, &tmout_ms);
}

/* main for ipc_wait
 */
int
main(int argc, const char * argv[])
{
    int                 i;
    struct event_base   *client_evbase;
    af_rpc_param_t      ret_params[IPC_WAIT_MAX_PARAMS];
    char                ret_string[256];
    struct timeval      timeout_val;
    struct event        *tm_event = NULL;
    int                 timeout = 10;  /* default ten second timeout */
    int                 retVal = 1;

    memset(outBuffer, 0, sizeof(outBuffer));
    memset(ret_params, 0, sizeof(ret_params));

    openlog("ipc_wait", 0, LOG_USER);

    if (argc >= 2) {
        sName = argv[1];
        if (argc >= 3) {
            if (strcmp (argv[2], "-t") == 0) {
                if (argc == 4) {
                    timeout = atoi(argv[3]);
                } else {
                    usage();
                    return retVal;
                }
            } else {
                usage();
                return retVal;
            }
        }
    } else {
        usage();
        return retVal;
    }

    timeout_val.tv_sec = timeout;
    timeout_val.tv_usec = 0;

    client_evbase = event_base_new();
    if (client_evbase == NULL) {
        fprintf(stderr, "Exit.  Unable to create event_base\n");

        return retVal;
    }

    server = af_ipcc_get_server (client_evbase, (char *)sName,
                                 handle_receive, NULL, NULL);
    if (server == NULL) {
        event_base_free(client_evbase);

        fprintf(stderr, "Unable to connect to server %s\n", sName);
        return retVal;
    }

    /* Create an event in the rare case where server never talked to us */
    tm_event = event_new(client_evbase, server->fd,
                         EV_TIMEOUT, handle_timeout, NULL);
    event_add(tm_event, &timeout_val);

    /* Activate the eventloop */
    event_base_dispatch(client_evbase);

    /* The event loop exits as the reply is received */
    if (dataReceived == 0) {
        fprintf(stderr, "Timed out before receiving data\n");
        goto exit;
    }

    int  pos = 0;

    memset(ret_string, 0, sizeof(ret_string));

    af_log_buffer(LOG_DEBUG1, "outBuffer", outBuffer, outBufferSize);

    int num_ret_params = af_rpc_get_params_from_buffer(ret_params, ARRAY_SIZE(ret_params), outBuffer, outBufferSize, 1);
    if (num_ret_params < 0) {
        fprintf(stderr, "af_rpc_get_params_from_buffer failed: ret=%d\n", num_ret_params);
        goto exit;
    }

    for (i = 0; i < num_ret_params; i++) {
        int type = ret_params[i].type;

        /* add a space between parameters */
        if (i != 0) {
            if (pos + 1 < sizeof(ret_string) - 1) {
                ret_string[pos++] = ' ';
            } else {
                fprintf(stderr, "return string overflowed on parameter %d\n", i);
                goto exit;
            }
        }

        if (AF_RPC_TYPE_IS_INTEGER(type)) {
            int bytes;
            char tmp[11]; /* maximum size for an unsigned int + null terminator */

            switch (type) {
                case AF_RPC_TYPE_UINT8 :
                case AF_RPC_TYPE_UINT16 :
                case AF_RPC_TYPE_UINT32 :
                    bytes = sprintf(tmp, "%u", (uint32_t)ret_params[i].base);
                    break;
                case AF_RPC_TYPE_INT8 :
                case AF_RPC_TYPE_INT16 :
                case AF_RPC_TYPE_INT32 :
                    bytes = sprintf(tmp, "%d", (int32_t)ret_params[i].base);
                    break;
                default :
                    fprintf(stderr, "illegal type %04x for parameter %d\n", type, i);
                    goto exit;
                    // break;
            }
            if (pos + bytes < sizeof(ret_string) - 1) {
                strcpy(ret_string, tmp);
                pos += bytes;
            } else {
                fprintf(stderr, "return string overflowed on parameter %d\n", i);
                goto exit;
            }
        } else {
            int len = AF_RPC_BLOB_SIZE(type);

            if (AF_RPC_BLOB_IS_STRING(type)) {
                if (pos + len + 2 < sizeof(ret_string) - 1) {
                    ret_string[pos++] = '"';
                    memcpy(&ret_string[pos], ret_params[i].base, AF_RPC_BLOB_SIZE(type));
                    ret_string[pos + len - 1] = '"'; /* length actually includes null terminator */
                    ret_string[pos + len] = '\0';
                } else {
                    fprintf(stderr, "string on return parameter %d too long; len=%d\n", i, len);
                    goto exit;
                }
                break;
            } else {
                if (pos + len * 2 < sizeof(ret_string) - 1) {
                    int j;
                    for (j = 0; j < len; j++) {
                        pos += sprintf (&ret_string[pos], "%02x", ((uint8_t *)ret_params[i].base)[j]);
                    }
                } else {
                    fprintf(stderr, "hex blob on return parameter %d too long; len=%d\n", i, len);
                    goto exit;
                };
            }

        }
    }

    /* succeeded */
    printf("%s\n", ret_string);
    retVal = 0;

exit:
    /* Shutdown the fd to the server */
    shutdown(server->fd, SHUT_RDWR);

    af_ipcc_shutdown(server);
    event_free(tm_event);
    event_base_free(client_evbase);
    return retVal;
}
