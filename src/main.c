#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <stdio.h>

#include "tuya_error_code.h"
#include "cJSON.h"
#include "tuyalink_core.h"
#include "tuya_helper.h"
#include "utils.h"

const char *argp_program_version = "Tuya Daemon 1.0";
const char *argp_program_bug_address = "povilas.daugela@teltonika.lt"; // actually on outlook.com now


static volatile sig_atomic_t keep_running = 1;

static void stop_handler(int signo)
{
    (void)signo;
    keep_running = 0;
}

static int set_signal_action(void)
{
    struct sigaction act = {0};

    act.sa_handler = stop_handler;

    if (sigemptyset(&act.sa_mask) == -1)
        return -1;

    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1 ||
        sigaction(SIGQUIT, &act, NULL) == -1 ||
        sigaction(SIGTERM, &act, NULL) == -1) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    set_signal_action();

    struct arguments arguments = parse_arguments(argc, argv);

    if(arguments.deviceID == NULL) {
        syslog(LOG_ERR, "Error args. \n");
        return 1;
    }

    char _productId[40]; // not needed?
    char _deviceId[40];
    char _deviceSecret[40];

    strncpy(_productId, arguments.productID, 40);
    strncpy(_deviceId, arguments.deviceID, 40);
    strncpy(_deviceSecret, arguments.deviceSecret, 40);


    if (arguments.daemon){
        if(!daemon(0, 0) ) {
            syslog(LOG_ERR, "Error daemonizing. \n");
        }
    }
        
    struct dataPacket *data = NULL;

    int count = 0;

    tuya_mqtt_context_t client_instance;
    int ret = tuya_connect(&client_instance, _deviceId, _deviceSecret);

    if (ret != OPRT_OK) {
        syslog(LOG_ERR, "Error connecting. \n");
        keep_running = 0;
    }

    cJSON *json = NULL;

    while (keep_running)
    {
        /* Loop to receive packets, and handles client keepalive */
        tuya_mqtt_loop(&client_instance);

        data = collect_sys_data(&count);

        json = data_packet_to_json(data, count);

        if (json == NULL) {
            syslog(LOG_ERR, "JSON not found. \n");
            continue;
        }

        tuya_client_send_system_status(json, &client_instance);
        cJSON_Delete(json);
        free(data);
        data = NULL;
        sleep(10);
    }

    tuya_mqtt_disconnect(&client_instance);
    tuya_mqtt_deinit(&client_instance);

    if (data != NULL)
        free(data);

    return ret; // ret
}
