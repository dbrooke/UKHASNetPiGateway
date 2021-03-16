int mqtt_init(char *host, int port, char *user, char *pass, char *topic);
int mqtt_loop(void);
int mqtt_publish(char *topic_root, char *value, int rssi);
