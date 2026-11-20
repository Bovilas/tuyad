#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "tuyalink_core.h"
#include "tuya_cacert.h"
#include "tuya_error_code.h"
#include "tuya_helper.h"


int tuya_connect(tuya_mqtt_context_t *client_instance, char deviceId[], char deviceSecret[])
{
    int ret;
    ret = tuya_mqtt_init(client_instance, &(const tuya_mqtt_config_t){
        .host = "m1.tuyacn.com",
        .port = 8883,
        .cacert = (const uint8_t *)tuya_cacert_pem,
        .cacert_len = sizeof(tuya_cacert_pem),
        .device_id = deviceId,
        .device_secret = deviceSecret,
        .keepalive = 100,
        .timeout_ms = 2000,
        .on_connected = on_connected,
        .on_disconnect = on_disconnect,
        .on_messages = on_messages});

    ret = tuya_mqtt_connect(client_instance);

    if (ret != OPRT_OK) {
        syslog(LOG_ERR, "Tuya MQTT initialization failed: %d", ret);
    }

    return ret;
}

int tuya_client_send_system_status(cJSON *json, tuya_mqtt_context_t *client_instance)
{
    if (!json){
        syslog(LOG_ERR, "JSON not found.\n");
        return -1;
    }

    char *out_str = cJSON_Print(json);
    if (out_str == NULL)
    {
        syslog(LOG_ERR, "Failed to make json out string\n");
        return -1;
    }

    int res = tuyalink_thing_property_report(client_instance, NULL, out_str);
    if (res != OPRT_OK)
    {
        syslog(LOG_ERR, "Failed sending system status JSON data : %d\n", res);
    }

    free(out_str);
    return res;
}

void on_connected(tuya_mqtt_context_t *context, void *user_data)
{
    syslog(LOG_DEBUG, "on connected");
    (void)context;
    (void)user_data;
}

void on_disconnect(tuya_mqtt_context_t *context, void *user_data)
{
    syslog(LOG_DEBUG, "on disconnect");
    (void)context;
    (void)user_data;
}

int handle_log_action(const tuyalink_message_t* msg) {

    char message[128];
    char filename[80];
    char path[80];

    strcpy(filename, getenv("HOME"));
    strcpy(path, getenv("HOME"));

    strcat(filename, "/tmp/tuya_action.log");
    strcat(path, "/tmp");

    cJSON *json = cJSON_Parse(msg->data_string);

    if (json == NULL) {
        syslog(LOG_ERR, "Failed to parse JSON");
        return -1;
    }

    cJSON *inputParams = cJSON_GetObjectItemCaseSensitive(json, "inputParams");

    cJSON *arg = cJSON_GetObjectItemCaseSensitive(inputParams, "arg");

    if (!cJSON_IsString(arg) || !arg->valuestring) {
        cJSON_Delete(json);
        return -1;
    }

    strncpy(message, arg->valuestring, 128);

    syslog(LOG_DEBUG, "Message got: %s\n", message);

    struct stat st = {0};

    if (stat(path, &st) == -1) {
        syslog(LOG_DEBUG, "Dir not found, creating. \n");
        
        if(mkdir(path, 0700) == -1) {
            syslog(LOG_ERR, "Error making dir. \n");
            cJSON_Delete(json);
            return -1;
        }
    }

    FILE *fp = fopen(filename, "a");
    if(!fp) {
        syslog(LOG_ERR, "Error opening log file for writing. \n");
        cJSON_Delete(json);
        return -1;
    }

    fprintf(fp, "%s\n", message);

    if(fp) fclose(fp);

    cJSON_Delete(json);

    return 0;
}

void get_action_code(const tuyalink_message_t* msg, char actionCode[]) {
    cJSON *json = cJSON_Parse(msg->data_string);
    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "actionCode");
    strncpy(actionCode, code->valuestring, 30);
    cJSON_Delete(json);
}

void action_handler(const tuyalink_message_t* msg) {
    int ret;
    char actionCode[30];
    int cmp;
    get_action_code(msg, actionCode);
    cmp = strcmp("testAction", actionCode);
    if(cmp == 0) {
        ret = handle_log_action(msg);
        if (ret == -1) {
            syslog(LOG_ERR, "Error handling action. \n");
        }
    }
}

void on_messages(tuya_mqtt_context_t* context, void* user_data, const tuyalink_message_t* msg)
{    
    switch (msg->type) {
        case THING_TYPE_ACTION_EXECUTE:
            action_handler(msg);
            break;

        default:
            syslog(LOG_NOTICE, "Unknown message type got. \n");
            break;
    }

    (void)context;
    (void)user_data;
}


int read_stat(const char *interface, const char *stat, unsigned long long *value)
{
    char path[256];

    snprintf(path, sizeof(path),
             "/sys/class/net/%s/statistics/%s",
             interface, stat);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file.\n");
        return -1;
    }
    int result = fscanf(file, "%llu", value);
    fclose(file);

    return result == 1 ? 0 : -1;
}


cJSON *data_packet_to_json(const struct dataPacket *data, int count)
{
    if (!data) {
        syslog(LOG_ERR, "Data not found.\n");
        return NULL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        syslog(LOG_ERR, "JSON not found.\n");
        return NULL;
    }

    if (!cJSON_AddNumberToObject(json, "totalRAMGB", data->totalRAM) ||
        !cJSON_AddNumberToObject(json, "freeRAMGB", data->freeRAM) ||
        !cJSON_AddNumberToObject(json, "systemuptime", data->systemUptime) ||
        !cJSON_AddNumberToObject(json, "cpuload", data->cpuload)) {
        syslog(LOG_ERR, "Error adding system data.\n");
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *interfaces = cJSON_AddArrayToObject(json, "interfaces");
    if (!interfaces) {
        syslog(LOG_ERR, "Interfaces not found.\n");
        cJSON_Delete(json);
        return NULL;
    }

    for (int k = 0; k < count; ++k) {
        cJSON *interface = cJSON_CreateObject();
        if (!interface) {
            cJSON_Delete(json);
            return NULL;
        }

        if (!cJSON_AddStringToObject(interface, "interfaceName",
                                     data->interfaceNames[k]) ||
            !cJSON_AddStringToObject(interface, "ipAddress",
                                     data->ipAddresses[k]) ||
            !cJSON_AddStringToObject(interface, "netmask",
                                     data->netmasks[k]) ||
            !cJSON_AddNumberToObject(interface, "recievedMB",
                                     data->recievedMB[k]) ||
            !cJSON_AddNumberToObject(interface, "sentMB",
                                     data->sentMB[k])) {
            cJSON_Delete(interface);
            cJSON_Delete(json);
            return NULL;
        }

        char *interface_str = cJSON_PrintUnformatted(interface);
        cJSON_Delete(interface);

        if (!interface_str) {
            cJSON_Delete(json);
            return NULL;
        }

        cJSON *item = cJSON_CreateString(interface_str);
        cJSON_free(interface_str);

        if (!item) {
            cJSON_Delete(json);
            return NULL;
        }

        cJSON_AddItemToArray(interfaces, item);
    }

    char *output = cJSON_Print(json);
    if (output) {
        fprintf(stderr, "%s\n", output);
        cJSON_free(output);
    }

    return json;
}

int get_ips(struct interface_info **ips)
{
    if (ips == NULL) {
        syslog(LOG_ERR, "IPs not found.\n");
        return -1;
    }

    *ips = NULL;

    struct ifaddrs *ifap;

    if (getifaddrs(&ifap) == -1)
    {
        syslog(LOG_ERR, "getifaddrs\n");
        return -1;
    }

    struct interface_info *interfaces = NULL;
    size_t count = 0;

    for (struct ifaddrs *temp = ifap;
         temp != NULL;
         temp = temp->ifa_next)
    {

        // Skip loopback interface
        if (temp->ifa_flags & IFF_LOOPBACK)
            continue;

        if (temp->ifa_addr == NULL)
            continue;

        int family = temp->ifa_addr->sa_family;

        if (family != AF_INET && family != AF_INET6)
            continue;

        struct interface_info entry = {0};

        snprintf(entry.name, sizeof(entry.name), "%s",
                 temp->ifa_name);

        // Get IP address
        int result = getnameinfo(
            temp->ifa_addr,
            family == AF_INET
                ? sizeof(struct sockaddr_in)
                : sizeof(struct sockaddr_in6),
            entry.ip,
            sizeof(entry.ip),
            NULL,
            0,
            NI_NUMERICHOST);

        if (result != 0)
            continue;

        // Get netmask
        if (temp->ifa_netmask != NULL)
        {
            result = getnameinfo(
                temp->ifa_netmask,
                family == AF_INET
                    ? sizeof(struct sockaddr_in)
                    : sizeof(struct sockaddr_in6),
                entry.netmask,
                sizeof(entry.netmask),
                NULL,
                0,
                NI_NUMERICHOST);

            if (result != 0)
                entry.netmask[0] = '\0';
        }

        // Get interface traffic counters
        // These are cumulative byte counts.
        read_stat(temp->ifa_name, "rx_bytes",
                  &entry.rx_bytes);

        read_stat(temp->ifa_name, "tx_bytes",
                  &entry.tx_bytes);

        // Grow array
        struct interface_info *new_interfaces =
            realloc(interfaces,
                    (count + 1) * sizeof(*interfaces));

        if (new_interfaces == NULL)
        {
            syslog(LOG_ERR, "Error allocating memory.\n");
            free(interfaces);
            freeifaddrs(ifap);
            return -1;
        }

        interfaces = new_interfaces;
        interfaces[count++] = entry;
    }

    freeifaddrs(ifap);

    *ips = interfaces;
    return (int)count;
}