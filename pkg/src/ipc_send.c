/*
 * ipc_send.c
 *
 * A tool used for building script that sends a command to an arbitrary server
 * daemon (FSD) service.
 *
 * Copyright (c) 2015 Afero Inc. All rights reserved.
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


#define IPC_SEND_INSTR_MAX_LEN 256
#define IPC_SEND_MAX_PARAMS 16

#define IPC_SEND_ERR    (-1)
#define IPC_SEND_OK     0

uint32_t g_debugLevel = LOG_DEBUG4;

/* the server name */
static const char *name   = NULL;

/* the IPC client server control block */
static af_ipcc_server_t     *s_server;

/* Storage for input parameters, and output data */
static af_rpc_param_t in_params[IPC_SEND_MAX_PARAMS];
static int            num_in_params = 0;
uint8_t               dataReceived = 0;
static int            outBufferSize = 0;
static uint8_t        outBuffer[IPC_SEND_INSTR_MAX_LEN];
static uint8_t        txBuffer[IPC_SEND_INSTR_MAX_LEN];

static struct event_base *s_base = NULL;

/* Usage info for the scripting tool */
static
void usage()
{
    fprintf(stderr, "ipc_send <dest_server> [-u[8|16,32] <val>] [-i[8|16|32] <val>] [-s <string>] [-h <hex>] ... \n");
    fprintf(stderr, "Arguments:\n\n");
    fprintf(stderr, "  -u8 or -u16 or -u32: designate uint8_t or uint16_t, uint32_t\n");
    fprintf(stderr, "  -i8 or -i16 or -i32: designate int8_t or int16_t, int32_t\n");
    fprintf(stderr, "  -s:   designate a string input (max length is %d chars)\n", IPC_SEND_INSTR_MAX_LEN);
    fprintf(stderr, "  -h:   designate a hex value input\n");
}

/* check to see if the input string is all digits */
static int
is_digits(const char *s)
{
    if (s) {
        while (*s) {
            if (isdigit(*s++) == 0) {
                return 0;
            }
        }
        /* check all the string, and every one is a digit */
        return 1;
    }
    return 0;
}

static int
is_xdigits(const char *s)
{
    if (s) {
        while (*s) {
            if (isxdigit(*s++) == 0) {
                return 0;
            }
        }
        /* check all the string, and every one is a hex digit */
        return 1;
    }
    return 0;
}

static uint8_t hexnybble(char c)
{
    if (c >= '0' && c <= '9') {
        c = c - '0';
    } else if (c >= 'a' && c <= 'f') {
        c = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        c = c - 'A' + 10;
    } else {
        /* invalid hex digit maps to 0 */
        c = 0;
    }
    return (uint8_t)c;
}

/* format a af_param_t for int8_t, int16_t, int32_t */
static int
format_int_param(int size, const char *pval_str, af_rpc_param_t *param, int index)
{
//  fprintf(stderr, "AT format_int_param: size=%d pval_str=%s param_val=%d\n", size, pval_str, param_val);
    if (pval_str == NULL)
        return IPC_SEND_ERR;
    if (pval_str[0] == '-') {  // negative number
        if (is_digits(&pval_str[1]) == 0)
            return IPC_SEND_ERR;
    } else {
        if (is_digits(pval_str) == 0)
            return IPC_SEND_ERR;
    }

    int32_t param_val = strtol(pval_str, NULL, 10);

    switch (size) {
        case 8 :
            if ((param_val < SCHAR_MIN) || (param_val > SCHAR_MAX)) {
                fprintf(stderr, "int%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_INT8(param[index],param_val);
            break;

        case 16 :
            if ((param_val < SHRT_MIN) || (param_val > SHRT_MAX)) {
                fprintf(stderr, "int%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_INT16(param[index],param_val);
            break;

        case 32 :
            if ((param_val < INT_MIN) || (param_val > INT_MAX)) {
                fprintf(stderr, "int%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_INT32(param[index],param_val);
            break;

        default :
            return IPC_SEND_ERR;
            // break;
    }

    return IPC_SEND_OK;
}


/* format for uint8_t, uint16_t, uint32_t */
static int
format_uint_param(int size, const char *pval_str, af_rpc_param_t *param, int index)
{
//  fprintf(stderr, "AT format_uint_param: size=%d pval_str=%s param_val=%d\n", size, pval_str, param_val);
    if (pval_str == NULL)
        return IPC_SEND_ERR;
    if (is_digits(pval_str) == 0)
        return IPC_SEND_ERR;

    uint32_t param_val = strtoul(pval_str, NULL, 10);

    switch (size) {
        case 8 :
            if ((param_val < 0) || (param_val > UCHAR_MAX)) {
                fprintf(stderr, "uint%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_UINT8(param[index],param_val);
            break;

        case 16 :
            if ((param_val < 0) || (param_val > USHRT_MAX)) {
                fprintf(stderr, "uint%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_UINT16(param[index],param_val);
            break;

        case 32 :
            if ((param_val < 0) || (param_val > UINT_MAX)) {
                fprintf(stderr, "uint%d_t: Invalid value (%d)\n", size, param_val);
                return IPC_SEND_ERR;
            }
            AF_RPC_SET_PARAM_AS_UINT32(param[index],param_val);
            break;

        default :
            return IPC_SEND_ERR;
            // break;
    }

    return IPC_SEND_OK;
}


/* main routine to parser the command line input parameters */
static int
param_parser(int argc, const char * argv[], int *num, af_rpc_param_t  *cmd_params)
{
    int         i = 2;   // skip the program name and server name
    const char *s;
    int         num_params = 0;

    /* number of parameters should be in pairs not including executable and server */
    if (argc % 2 != 0) {
        fprintf(stderr, "number of parameters should be even\n");
        return IPC_SEND_ERR;
    }

    if (argc >= i) {
        name = argv[1];
    }

    while (i < argc) {
        s = argv[i];

        if (s[0] == '-')
        {
            switch(s[1]) {

                case 'd':   /* destination server name */
                    name = argv[++i];
                    break;

                case 'u':
                {
                    int size = atoi(&s[2]);
                    if (num_params < IPC_SEND_MAX_PARAMS) {
                        if (format_uint_param(size, argv[++i], cmd_params, num_params) == IPC_SEND_ERR) {
                            fprintf(stderr, "badly formated uint in param %d\n", num_params);
                            return IPC_SEND_ERR;
                        }
                        num_params++;
                    } else {
                        fprintf(stderr, "too many parameters %d\n", num_params);
                        return IPC_SEND_ERR;
                    }
                    break;
                }

                case 'i':
                {
                    int size = atoi(&s[2]);
                    if (num_params < IPC_SEND_MAX_PARAMS) {
                        if (format_int_param(size, argv[++i], cmd_params, num_params) == IPC_SEND_ERR) {
                            fprintf(stderr, "badly formated int in param %d\n", num_params);
                            return IPC_SEND_ERR;
                        }
                        num_params++;
                    } else {
                        fprintf(stderr, "too many parameters %d\n", num_params);
                        return IPC_SEND_ERR;
                    }
                    break;
                }

                case 's':  // STRING
                {
                    /* TODO check if we need to escape strings ... */
                    int len = strlen(argv[++i]) + 1; /* include null terminator */
                    void *base;
                    if (len <= 0 || len > IPC_SEND_INSTR_MAX_LEN) {
                        fprintf(stderr, "string length exceeds maximum length of %d\n", IPC_SEND_INSTR_MAX_LEN);
                        return IPC_SEND_ERR;
                    }

                    if (num_params < IPC_SEND_MAX_PARAMS) {
                        base = (uint8_t *)malloc(len);
                        if (base == NULL) {
                            fprintf(stderr, "Memory allocation error\n");
                            return IPC_SEND_ERR;
                        }
                        memcpy(base, argv[i], len);
                        AF_RPC_SET_PARAM_AS_STRING(cmd_params[num_params],base,len);
                        num_params++;
                    } else {
                        fprintf(stderr, "too many parameters %d\n", num_params);
                        return IPC_SEND_ERR;
                    }

                    break;
                }

                case 'h':
                {
                    const char *tmp = argv[++i];
                    int i, l = strlen(tmp);
                    int len;
                    uint8_t *base;

                    if (l == 0 || (l % 2) != 0) {
                        fprintf(stderr, "hex values must have an even number of digits in param %d\n", num_params);
                        return IPC_SEND_ERR;
                    }

                    if (is_xdigits(tmp) == 0) {
                        fprintf(stderr, "illegal hex digit in param %d\n", num_params);
                        return IPC_SEND_ERR;
                    }

                    len = l / 2;

                    if (num_params < IPC_SEND_MAX_PARAMS) {
                        base = (uint8_t *)malloc(len);
                        if (base == NULL) {
                            fprintf(stderr, "Memory allocation error\n");
                            return IPC_SEND_ERR;
                        }

                        for (i = 0; i < len; i++) {
                            base[i] = hexnybble(tmp[i*2]) * 16 + hexnybble(tmp[i*2+1]);
                        }
                        AF_RPC_SET_PARAM_AS_BLOB(cmd_params[num_params],base,len);
                        num_params++;
                    } else {
                        fprintf(stderr, "too many parameters %d\n", num_params);
                        return IPC_SEND_ERR;
                    }

                    break;
                }
                default:
                    return IPC_SEND_ERR;
                    // break;
            }

            // increment to next pamarater
            i++;

        } else {
            fprintf(stderr, "bad argument %s\n", argv[i]);
            return IPC_SEND_ERR;
        }
    }

    if (name == NULL)
        return IPC_SEND_ERR;

    *num = num_params;

    return IPC_SEND_OK;
}


/* ipc_send_rx_callback
 *
 * When data is reply, it is stored in the 'data' with its length specified
 * in dataLen.
 *
 */
static void
ipc_send_rx_callback(int err, uint32_t seqNum, uint8_t *data, int dataLen, void *context)
{
    struct timeval  timeout_ms = {0, 100};
    if (data != NULL) {
        memcpy(outBuffer, data, dataLen);
        dataReceived = 1;
        outBufferSize = dataLen;
    }

    event_base_loopexit(s_base, &timeout_ms);
    return;
}


/* internal event used to handle a rare no response timeout */
static void
ipc_send_tmout_callback(evutil_socket_t fd, short what, void *arg)
{
    struct timeval  tmout_ms = {0, 100};

    /* just exist the eventloop */
    event_base_loopexit(s_base, &tmout_ms);
}


/* Wrapper routine to invoke the call to fsd_send_cmd due to
 * the event base nature of the program
 */
void
ipc_send_wrapper(evutil_socket_t fd, short what, void *arg)
{
    int size = af_rpc_create_buffer_with_params(txBuffer, sizeof(txBuffer), &in_params[0], num_in_params);

    if (g_debugLevel >= LOG_DEBUG4) {
        int i;
        for (i = 0; i < num_in_params; i++) {
            AFLOG_DEBUG3("ipc_send:in_params[%d].type=%04x, basep_null=%d", i, in_params[i].type,
                         (in_params[i].base==NULL));
        }

        char buffer[sizeof(txBuffer)*3];
        int pos = 0;

        for (i = 0; i < size ; i++) {
            pos += sprintf(&buffer[pos], i == 0 ? "%02x" : " %02x", txBuffer[i]);
        }
        buffer[pos++] = '\n';
        buffer[pos] = '\0';
        AFLOG_DEBUG3("ipc_send:buffer=%s", buffer);
    }

    if (size > 0) {
        af_ipcc_send_request(s_server, txBuffer, size, ipc_send_rx_callback, NULL, 3000);
    } else {
        AFLOG_ERR("ipc_send_wrapper:send_fail:ret=%d", size);
    }
}


/* internal routine to clear the allocate memory */
static void
dealloc_params()
{
    int i;

    for (i=0; i<IPC_SEND_MAX_PARAMS; i++) {
        if (in_params[i].base && AF_RPC_TYPE_IS_BLOB(in_params[i].type)) {
            free(in_params[i].base);
            in_params[i].base = NULL;
            in_params[i].type = 0;
        }
    }
    return;
}

static void
on_close(int status, void *context)
{
    /* the IPC layer has freed the server. Set the pointer
     * to it to NULL so we don't double free
     */
    if (s_server) {
        s_server = NULL;
    }

    struct timeval  tmout_ms = {0, 100};
    event_base_loopexit(s_base, &tmout_ms);
}

/* main for ipc_send
 */
int main(int argc, const char * argv[])
{
    int                 i;
    struct timeval      send_cmd_in_ms_time = { 0, 200000 };
    struct event        *test_event = NULL;
    af_rpc_param_t      ret_params[IPC_SEND_MAX_PARAMS];
    char                ret_string[256];
    struct timeval      tmout_ms = { 5, 500000 };
    struct event        *tm_event = NULL;
    int                 retVal = 1;

    memset(in_params, 0, sizeof(in_params));
    memset(outBuffer, 0, sizeof(outBuffer));
    memset(ret_params, 0, sizeof(ret_params));

    openlog("ipc_send", 0, LOG_USER);

    /* parse the input parameters */
    if ((param_parser(argc, argv, &num_in_params, &in_params[0]) == IPC_SEND_ERR) || (num_in_params <= 0 )) {
        dealloc_params();

        fprintf(stderr, "Exit: Invalid input parameters\n");
        usage();
        return retVal;
    }

    s_base = event_base_new();
    if (s_base == NULL) {
        fprintf(stderr, "Exit.  Unable to create event_base\n");
        dealloc_params();

        return retVal;
    }

    s_server = af_ipcc_open_server(s_base, (char *)name,
                                   NULL, NULL, on_close);
    if (s_server == NULL) {
        event_base_free(s_base);
        dealloc_params();

        fprintf(stderr, "Unable to connect to server %s\n", name);
        return retVal;
    }

    test_event = evtimer_new(s_base, ipc_send_wrapper, NULL);
    event_add(test_event, &send_cmd_in_ms_time);

    /* Create an event in the rare case where server never talked to us */
    tm_event = evtimer_new(s_base, ipc_send_tmout_callback, NULL);
    event_add(tm_event, &tmout_ms);

    /* Activate the eventloop */
    event_base_dispatch(s_base);

    /* The event loop exits as the reply is received */
    if (dataReceived == 0) {
        fprintf(stderr, "Timed out before receiving data\n");
        goto exit;
    }

    int  pos = 0;

    memset(ret_string, 0, sizeof(ret_string));

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
                    bytes = sprintf(tmp, "%u", (uint32_t)(ptrdiff_t)ret_params[i].base);
                    break;
                case AF_RPC_TYPE_INT8 :
                case AF_RPC_TYPE_INT16 :
                case AF_RPC_TYPE_INT32 :
                    bytes = sprintf(tmp, "%d", (int32_t)(ptrdiff_t)ret_params[i].base);
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
    dealloc_params();

    if (s_server) {
        af_ipcc_close(s_server);
    }

    if (test_event) {
        event_free(test_event);
    }

    if (tm_event) {
        event_free(tm_event);
    }

    if (s_base) {
        event_base_free(s_base);
    }

    return retVal;
}
