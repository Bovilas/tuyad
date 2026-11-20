#ifndef TUYA_HELPER_H
#define TUYA_HELPER_H

#include <netdb.h>

#include "cJSON.h"
#include "tuyalink_core.h"

#define MAX_ADDRESS_COUNT 20

struct dataPacket
{
    double freeRAM;
    double totalRAM;
    long int systemUptime;
    char interfaceNames[MAX_ADDRESS_COUNT][32];
    char ipAddresses[MAX_ADDRESS_COUNT][32];
    char netmasks[MAX_ADDRESS_COUNT][32];
    double recievedMB[MAX_ADDRESS_COUNT];
    double sentMB[MAX_ADDRESS_COUNT];
    double cpuload;
};

struct interface_info
{
    char name[NI_MAXHOST];
    char ip[NI_MAXHOST];
    char netmask[NI_MAXHOST];

    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
};

int tuya_connect(tuya_mqtt_context_t *client_instance, char deviceId[], char deviceSecret[]);

int tuya_client_send_system_status(cJSON *json, tuya_mqtt_context_t *client_instance);

void on_connected(tuya_mqtt_context_t *context, void *user_data);

void on_disconnect(tuya_mqtt_context_t *context, void *user_data);

void on_messages(tuya_mqtt_context_t *context, void *user_data, const tuyalink_message_t *msg);

int read_stat(const char *interface, const char *stat, unsigned long long *value);

cJSON *data_packet_to_json(const struct dataPacket *data, int count);

int get_ips(struct interface_info **ips);

int handle_log_action(const tuyalink_message_t* msg);

#endif