#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <syslog.h>
#include "utils.h"

static struct argp_option options[] = {
    {"productID", 'p', "VALUE", 0, "Tuya product ID", 0},
    {"deviceID", 'i', "VALUE", 0, "Tuya device ID", 0},
    {"deviceSecret", 's', "VALUE", 0, "Tuya device secret", 0},
    {"daemon", 'D', 0, 0, "Run as a daemon", 0},
    {0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
    case 'p':
        arguments->productID = arg;
        break;
    case 'i':
        arguments->deviceID = arg;
        break;
    case 's':
        arguments->deviceSecret = arg;
        break;
    case 'D':
        arguments->daemon = 1;
        break;
    case ARGP_KEY_END:
        if (arguments->productID == NULL ||
            arguments->deviceID == NULL ||
            arguments->deviceSecret == NULL)
        {
            argp_usage(state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static char args_doc[] = "";
static char doc[] = "Tuya Daemon";

static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    NULL,
    NULL,
    NULL};

struct arguments parse_arguments(int argc, char **argv)
{
    struct arguments arguments = {NULL, NULL, NULL, 0};

    argp_parse(&argp, argc, argv, 0, NULL, &arguments);

    arguments.productID = trimwhitespace(arguments.productID);
    arguments.deviceID = trimwhitespace(arguments.deviceID);
    arguments.deviceSecret = trimwhitespace(arguments.deviceSecret);
    
    if (
    strcmp(arguments.productID, "") == 0 ||
    strcmp(arguments.deviceID, "") == 0 ||
    strcmp(arguments.deviceSecret, "") == 0 ) {
        arguments.deviceID = NULL;
        arguments.deviceSecret = NULL;
        arguments.productID = NULL;
    }

    return arguments;
}

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

struct dataPacket *collect_sys_data(int *returnCount)
{
    if (returnCount == NULL) {
        syslog(LOG_ERR, "return count ptr NULL. \n");
        return NULL;
    }

    struct sysinfo info;
    sysinfo(&info);

    if (sysinfo(&info) != 0) {
        syslog(LOG_ERR, "sysinfo failed");
        return NULL;
    }

    struct interface_info *interfaces = NULL;

    int count = get_ips(&interfaces);

    if (count < 0)
    {
        syslog(LOG_ERR, "Failed to get IP addresses\n");
        return NULL;
    }
    if (count > MAX_ADDRESS_COUNT) {
        syslog(LOG_ERR, "Too many IP addresses\n");
        return NULL;
    }

    struct dataPacket *data = malloc(sizeof(struct dataPacket));

    if (data == NULL)
    {
        syslog(LOG_ERR, "Error allocating memory.\n");
        free(interfaces);
        return NULL;
    }

    long double totalram = ((long double)info.totalram / 1000000000);
    long double freeram = (long double)info.freeram / 1000000000;

    double load1 = (double)info.loads[0] / (1 << SI_LOAD_SHIFT);

    data->totalRAM = totalram;
    data->freeRAM = freeram;
    data->systemUptime = info.uptime;
    data->cpuload = load1;

    for (int i = 0; i < count; i++)
    {
        strncpy(data->interfaceNames[i], interfaces[i].name, 32);
        strncpy(data->ipAddresses[i], interfaces[i].ip, 32);
        strncpy(data->netmasks[i], interfaces[i].netmask, 32);
        data->recievedMB[i] = (long double)interfaces[i].rx_bytes / 1000000;
        data->sentMB[i] = (long double)interfaces[i].tx_bytes / 1000000;
    }

    if(*returnCount == 0)
        *returnCount = -1;

    *returnCount = count;

    free(interfaces);

    return data;
}
