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
#include "af_ipc_client.h"
#include "libaf_ipc_test_common.h"

#define   SERVER_NAME "TEST_W"

uint32_t g_debugLevel=1;

af_ipcc_server_t *my_server = NULL;
static struct event_base *s_client_evbase = NULL;

static int s_failures = 0;
static int s_replies = 0;

#define MAX_REQUESTS 4

/* test_receive
 *
 * - This function is passed in when application sends a request. 
 * - To verify the IPC layer invokes this callback func when 
 *   a reply is received.
 *
 */
void 
test_receive (int status, uint32_t seqNum,
              uint8_t *rxBuf, int rxSize, 
              void *context)
{
    if (status != AF_IPC_STATUS_OK) {
        AFLOG_ERR("receive_failed:status=%d", status);
        return;
    }

    AFLOG_DEBUG1("receive_cb:seqNum=%d,rxBuffer[0]=%d", seqNum, rxBuf[0]);
    uint8_t message[2];

    switch (rxBuf[0]) {

        case SERVER_BROADCAST_TO_CLIENTS :
        {
            /* send a bunch of messages */
            int i;
            s_replies = 0;
            for (i = 0; i < MAX_REQUESTS; i++) {
                message[0] = CLIENT_COMMANDS;
                message[1] = i;
                int status = af_ipcc_send_request(my_server, message, 2, test_receive, (void *)i, 2000);
                if (status != 0) {
                    AFLOG_ERR("broadcast_to_clients:status=%d:send failed", status);
                    s_failures++;
                }
            }
            break;
        }

        case SERVER_COMMANDS_TO_CLIENT :
        {
            message[0] = CLIENT_REPLIES;
            message[1] = rxBuf[1];
            int status = af_ipcc_send_response(my_server, seqNum, message, 2);
            if (status != 0) {
                AFLOG_ERR("broadcast_to_clients:status=%d:send failed", status);
                s_failures++;
            }
            s_replies |= (1 << rxBuf[1]);
            AFLOG_DEBUG1("s_replies=%d,target=%d", s_replies, (1 << MAX_REQUESTS) - 1);
            if (s_replies == (1 << MAX_REQUESTS) - 1) {
                event_base_loopbreak(s_client_evbase);
                s_replies = 0;
            }
            break;
        }

        case SERVER_UNSOL_TO_CLIENT :
        {
            AFLOG_INFO("successfully received unsolicited message from server");
            event_base_loopbreak(s_client_evbase);
            break;
        }

        case SERVER_REPLIES :
        {
            if (rxBuf[1] != (uint8_t)(uint32_t)context) {
                AFLOG_ERR("server_replies expected %d got %d", (uint8_t)(uint32_t)context, rxBuf[1]);
                s_failures++;
            } else {
                AFLOG_INFO("successfully received %d", rxBuf[1]);
                s_replies |= (1 << rxBuf[1]);
                if (s_replies == (1 << MAX_REQUESTS) - 1) {
                    event_base_loopbreak(s_client_evbase);
                    s_replies = 0;
                }
            }
            break;
        }

        default:
            break;
    }
}



/* af_ipc_event_fatal_error_cb ()
 * af_ipc_event_log_cb() 
 *
 * Callback functions used to setup libevent logging 
 */
static void
af_ipcc_event_fatal_error_cb(int err) {
    AFLOG_ERR("IPCC_TEST::%s:err=%d\n", __FUNCTION__, err);
    return;
}

static void
af_ipcc_event_log_cb(int severity, const char *msg)
{
    AFLOG_INFO("IPCC_TEST::%s:severity=%d, msg=%s\n", __FUNCTION__, severity, msg);
    return;
}


/* test_client:  Main 
 *
 * to test the client IPC APIs.
 * - af_ipcc_get_server
 * - af_ipcc_shutdown
 */
int main(int argc, char *argv[])
{
    openlog ("test_client", 0, LOG_USER);

    /* turn on event debug - needs to be done before event_base is created */
	if (AFLOG_DEBUG_ENABLED()) {
    	event_set_fatal_callback(af_ipcc_event_fatal_error_cb);
    	event_set_log_callback(af_ipcc_event_log_cb);
    	event_enable_debug_mode();
	} 

    /* Initialize libevent. */
    event_init();
    AFLOG_INFO("libevent_version:version=%s:", event_get_version());
    s_client_evbase = event_base_new();
    if(s_client_evbase == NULL) {
        AFLOG_ERR("IPCC_TEST::Unable to create client event base");
        closelog();
        return 1;
    }
    AFLOG_INFO("libevent is using %s for events", event_base_get_method(s_client_evbase));


    my_server = af_ipcc_get_server(s_client_evbase,
                                   SERVER_NAME,
                                   test_receive, NULL, NULL);

	if (my_server == NULL) {
        AFLOG_ERR("failed to get server %s", SERVER_NAME);
        closelog();
        return 1;
    }

    if (argc > 1 && argv[1][0] == '-') {
        uint8_t unsolicited[1];
        switch (argv[1][1]) {
            case 'b' :
                unsolicited[0] = CLIENT_TEST_BROADCAST_CLIENTCMDS;
                af_ipcc_send_unsolicited(my_server, unsolicited, sizeof(unsolicited));
                break;
            case 's' :
                unsolicited[0] = CLIENT_TEST_SERVERCMDS;
                af_ipcc_send_unsolicited(my_server, unsolicited, sizeof(unsolicited));
                break;
            case 'u' :
                unsolicited[0] = CLIENT_TEST_SERVERUNSOL;
                af_ipcc_send_unsolicited(my_server, unsolicited, sizeof(unsolicited));
                break;
            case 'q' :
                unsolicited[0] = CLIENT_SERVEREXIT;
                af_ipcc_send_unsolicited(my_server, unsolicited, sizeof(unsolicited));
                break;
            default :
                break;
        }
    }

	// Start the event loop
    if(event_base_dispatch(s_client_evbase)) {
        AFLOG_ERR("loop_err::Error running event loop");
	}
    AFLOG_INFO("shut_down::Test Client is shutting down");

    // Clean up libevent and  the ipc layer 
	af_ipcc_shutdown(my_server);

    event_base_free(s_client_evbase);

    closelog();
    return s_failures != 0;
}
