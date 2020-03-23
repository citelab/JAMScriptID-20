
#ifndef __MQTT_H__
#define __MQTT_H__

#include <MQTTAsync.h>
#include "command.h"


MQTTAsync mqtt_create(char *mhost, int i, char *devid);
void mqtt_subscribe(MQTTAsync mcl, char *topic);
void mqtt_publish(MQTTAsync mcl, char *topic, command_t *cmd);


#endif
