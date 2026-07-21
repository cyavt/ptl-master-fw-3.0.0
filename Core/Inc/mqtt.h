#ifndef INC_MQTT_APP_H_
#define INC_MQTT_APP_H_

#define DEFAULT_MQTT_BROKER_IP_1      192
#define DEFAULT_MQTT_BROKER_IP_2      168
#define DEFAULT_MQTT_BROKER_IP_3      123
#define DEFAULT_MQTT_BROKER_IP_4      232
#define DEFAULT_MQTT_BROKER_PORT      1883

void MQTT_App_Init(void);
void MQTT_App_Process(void);
int MQTT_Publish(const char *topic, const char *payload, int qos);
void MQTT_Queue_Publish(const char *topic, const char *payload);
void publish_initial_slave_states(void);

#endif /* INC_MQTT_APP_H_ */
