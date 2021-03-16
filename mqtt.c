#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>

#include "mqtt.h"

static struct mosquitto *mosq;

int parse_ukhasnet_packet(char *packet, char *node_id, char *data)
{
	int d = 0;
	char *p = packet + 2; /* skip TTL and sequence */
	char *n = node_id;

	while (*p != '\0') {
		if(*p == '[') {
			*p++;

			while (*p != ']' && *p != ',' && *p != '\0')
			{
				*n++ = *p++;
			}
			*n='\0';
			break;
		} else if(*p == ':') {
			d += snprintf(data + d, 80 - d, "\"comment\": \"");
			*p++;
			while (*p != '[' && *p != '\0')
			{
				data[d++] = *p++;
			}
			data[d++] = '"';
		} else if(*p >= 'A' && *p <= 'Z') {
			d += snprintf(data + d, 80 - d, "\"%c\": [", *p);
			*p++;
			while (*p < ':') {
				data[d++] = *p++;
			}
			data[d++] = ']';
			if(*p != '[') {
				d += snprintf(data + d, 80 - d, ", ");
			}
		} else {
			*p++;
		}
	}
	data[d] = '\0';
}

void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	printf("connect callback, rc=%d\n", result);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);
}

int mqtt_init(char *host, int port, char *user, char *pass, char *topic)
{
	char clientid[24];
	int rc = true;

	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "ukhasnet_gw_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if(mosq) {
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		mosquitto_username_pw_set(mosq, user, pass);
		rc = (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS);
	}
	return rc;
}

int mqtt_loop(void)
{
	int rc;

	rc = mosquitto_loop(mosq, 250, 1);
	if(rc){
		printf("connection error!\n");
		sleep(1);
		mosquitto_reconnect(mosq);
	}
}

int mqtt_publish(char *topic_root, char *msg, int rssi)
{
	char topic[64], value[80], node_id[17], data[80], json[128];

	snprintf(topic, sizeof(topic), "%s/ukhasnet/raw", topic_root);
	snprintf(value, sizeof(value), "{ \"packet\": \"%s\", \"rssi\": %d }", msg, rssi);
	mosquitto_publish(mosq, NULL, topic, strlen(value), value, 0, false);

	parse_ukhasnet_packet(msg, node_id, data);

	snprintf(topic, sizeof(topic), "%s/ukhasnet/node/%s", topic_root, node_id);
	snprintf(json, sizeof(json), "{ %s }", data);
	mosquitto_publish(mosq, NULL, topic, strlen(json), json, 0, false);

}

