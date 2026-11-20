#ifndef UTILS_H
#define UTILS_H

#include "tuya_helper.h"

struct arguments
{
    char *productID;
    char *deviceID;
    char *deviceSecret;
    int daemon;
};

struct arguments parse_arguments(int argc, char **argv);
struct dataPacket *collect_sys_data(int *returnCount);
char *trimwhitespace(char *str);


#endif