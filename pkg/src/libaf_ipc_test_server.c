/*
 * Test daemon used to test the APIs
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
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

#include "af_log.h"
#include "af_ipc_server.h"
#include "af_ipc_prv.h"
#include "libaf_ipc_test_common.h"

#define   SERVER_NAME "TEST_W"

uint32_t g_debugLevel=1;

static struct event_base  *server_evbase = NULL;
static af_ipcs_server_t *my_server = NULL;
static int s_failures = 0;

#define MAX_REQUESTS 4

uint16_t originatingClient = 0; /* if zero, no client has originated test sequence */

void close_callback(void *clientContext)
{
    AFLOG_DEBUG1("closing_context:clientContext_null=%d", (clientContext==NULL));
}

int accept_callback (void *acceptContext, uint16_t clientId, void **clientContextP)
{
    if (clientId > 0) {
        AFLOG_DEBUG1("accepted_client:clientId=%d", clientId);
        *clientContextP = NULL;
    }
    return 0;
}

/* Subsys: TEST1's receive callback function */
void
subsys_test1_receive(int        status,
                     uint32_t   seqNum,
                     uint8_t    *rxBuffer,
                     int        rxBufferSize,
                     void       *clientContext)
{
    AFLOG_DEBUG3("Entering %s", __FUNCTION__);

    if (status != AF_IPC_STATUS_OK) {
        AFLOG_ERR("receive_failed:status=%d", status);
        return;
    }

    AFLOG_DEBUG1("receive_cb:seqNum=%d,rxBuffer[0]=%d", seqNum, rxBuffer[0]);

    uint16_t clientId = AF_IPC_GET_CLIENT_ID(seqNum);
    uint8_t message[2];

    switch (rxBuffer[0]) {
        case CLIENT_TEST_BROADCAST_CLIENTCMDS :
        {
            message[0] = SERVER_BROADCAST_TO_CLIENTS;
            int status = af_ipcs_send_unsolicited(my_server, 0, message, 1);
            if (status != 0) {
                s_failures++;
                AFLOG_ERR("client_test_broadcast_clientcmds:status=%d", status);
            }
            break;
        }

        case CLIENT_TEST_SERVERCMDS :
        {
            int i;
            for (i = 0; i < MAX_REQUESTS; i++) {
                message[0] = SERVER_COMMANDS_TO_CLIENT;
                message[1] = (uint8_t)i;
                int status = af_ipcs_send_request(my_server, clientId, message, 2, subsys_test1_receive, (void *)(ptrdiff_t)i, 2000);
                if (status != 0) {
                    s_failures++;
                    AFLOG_ERR("client_test_servercmds:status=%d", status);
                }
            }
            break;
        }

        case CLIENT_TEST_SERVERUNSOL :
        {
            message[0] = SERVER_UNSOL_TO_CLIENT;
            int status = af_ipcs_send_unsolicited(my_server, clientId, message, 1);
            if (status != 0) {
                s_failures++;
                AFLOG_ERR("client_test_serverunsol:status=%d", status);
            }
            break;
        }

        case CLIENT_SERVEREXIT :
        {
            event_base_loopbreak(server_evbase);
            break;
        }

        case CLIENT_REPLIES :
        {
            if (rxBufferSize > 1) {
                if (rxBuffer[1] == (int)(ptrdiff_t)clientContext) {
                    AFLOG_INFO("client reply for message %d is okay", (int)(ptrdiff_t)clientContext);
                } else {
                    AFLOG_ERR("client reply %d does not match expected %d", rxBuffer[1], (int)(ptrdiff_t)clientContext);
                    s_failures++;
                }
            } else {
                AFLOG_ERR("client reply misformed rxBufferSize=%d", rxBufferSize);
                s_failures++;
            }
            break;
        }

        case CLIENT_COMMANDS :
        {
            if (rxBufferSize > 1) {
                message[0] = SERVER_REPLIES;
                message[1] = rxBuffer[1];
                int status = af_ipcs_send_response(my_server, seqNum, message, 2);
                if (status != 0) {
                    AFLOG_ERR("bad status on client reply %d", status);
                    s_failures++;
                }
            } else {
                AFLOG_ERR("client command misformed rxBufferSize=%d", rxBufferSize);
                s_failures++;
            }
            break;
        }

        default :
            break;
    }
}


/* af_ipc_event_fatal_error_cb ()
 * af_ipc_event_log_cb()
 *
 * Callback functions used to setup libevent logging
 */
static void
af_ipcs_event_fatal_error_cb(int err) {
    AFLOG_ERR("IPCS_TEST::%s:err=%d\n", __FUNCTION__, err);
    return;
}

static void
af_ipcs_event_log_cb(int severity, const char *msg)
{
    AFLOG_INFO("IPCS_TEST::%s:severity=%d, msg=%s\n", __FUNCTION__, severity, msg);
    return;
}


/* test_server:
 * - test program to verify the IPC server APs
 * - test af_ipcs_init()
 * - test af_ipcs_register_recv_callback()
 * - test af_ipcs_shutdown()
 */
int main(int argc, char *argv[])
{
    openlog("test_server", 0, LOG_USER);

    /* turn on event debug - needs to be done before event_base is created */
    if (AFLOG_DEBUG_ENABLED()) {
        event_set_fatal_callback(af_ipcs_event_fatal_error_cb);
        event_set_log_callback(af_ipcs_event_log_cb);
        event_enable_debug_mode();
    }

    /* Initialize libevent. */
    event_init();
    AFLOG_INFO("libevent_version:version=%s:", event_get_version());
    server_evbase = event_base_new();
    if(server_evbase == NULL) {
        AFLOG_ERR("event_base:Unable to create server event base");
        return (-1);
    }
    AFLOG_INFO("method:base_method=%s,base=%d:",
               event_base_get_method(server_evbase), (server_evbase==NULL));

    my_server = af_ipcs_init(server_evbase, SERVER_NAME,
                             accept_callback, NULL, // accept cb, accept ctx
                             subsys_test1_receive, close_callback); // rx cb, close cb
	if (my_server == NULL) {
		AFLOG_ERR("init_failed:errno=%d", errno);
        closelog();
		return (-1);
	}

    // Start the event loop
    if(event_base_dispatch(server_evbase)) {
        AFLOG_ERR("loop_err::Error running event loop");
    }
    AFLOG_INFO("shut_down::Server is shutting down");

    af_ipcs_shutdown(my_server);

    /* cleanup the event loop & the ipc layer */
    event_base_free(server_evbase);

    closelog();

    return s_failures != 0;
}
