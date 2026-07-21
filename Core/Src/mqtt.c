#include "mqtt.h"
#include "main.h"
#include "wizchip_port.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "utils.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "MQTT/MQTTClient.h"
#include "MQTT/mqtt_interface.h"

#define MQTT_SOCKET 5

extern wiz_NetInfo netInfo;

Network mqtt_network;
MQTTClient mqtt_client;
unsigned char mqtt_send_buf[1280];
unsigned char mqtt_read_buf[1024];

uint8_t mqtt_broker_ip[4] = {DEFAULT_MQTT_BROKER_IP_1, DEFAULT_MQTT_BROKER_IP_2, DEFAULT_MQTT_BROKER_IP_3, DEFAULT_MQTT_BROKER_IP_4};
uint16_t mqtt_broker_port = DEFAULT_MQTT_BROKER_PORT;
char master_uid[25];
bool mqtt_is_connected = false;

void mqtt_blink_led_indicator(void) {
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(100);
    }
}

// Generic MQTT publish function
int MQTT_Publish(const char *topic, const char *payload, int qos) {
    if (!mqtt_is_connected) {
        printf("MQTT: Cannot publish, client not connected\r\n");
        return -1;
    }
    
    MQTTMessage pub_msg;
    pub_msg.qos = (enum QoS)qos;
    pub_msg.retained = 0;
    pub_msg.dup = 0;
    pub_msg.payload = (void*)payload;
    pub_msg.payloadlen = strlen(payload);
    
    int rc = MQTTPublish(&mqtt_client, topic, &pub_msg);
    printf("MQTT: Published to %s (rc=%d)\r\n", topic, rc);
    
    return rc;
}

extern volatile uint32_t display_value;
extern volatile uint16_t display_intensity;
extern volatile uint16_t display_padding;
extern volatile uint16_t display_state;
extern volatile bool display_updated;
extern volatile uint8_t display_target_id;
extern slave_display_t slave_registry[131];
extern uint8_t online_slave_ids[130];
extern volatile int online_slave_count;

// Example callback function for subscribed topics
void on_message_received(MessageData* md) {
    mqtt_blink_led_indicator();

    char payload[256];
    int len = md->message->payloadlen;
    if (len >= (int)sizeof(payload)) len = (int)sizeof(payload) - 1;
    memcpy(payload, md->message->payload, len);
    payload[len] = '\0';
    
    printf("MQTT: Message received on topic '%.*s': %s\r\n", md->topicName->lenstring.len, md->topicName->lenstring.data, payload);

    // Parse JSON values
    int id = get_json_int_value(payload, "id", -1);
    char val_str[16] = {0};
    get_json_val_as_string(payload, "val", val_str, sizeof(val_str));
    int intensity = get_json_int_value(payload, "intensity", -1);
    int state = get_json_int_value(payload, "state", -1);
    int pad = get_json_int_value(payload, "pad", -1);

    // If ID is missing or invalid, reject the message
    if (id == -1) {
        printf("MQTT: Rejected message, 'id' field is missing or invalid.\r\n");
        return;
    }

    bool updated = false;

    display_target_id = (uint8_t)id;

    if (val_str[0] != '\0') {
        display_value = (uint32_t)strtol(val_str, NULL, 10);
        updated = true;
    }
    if (intensity >= 0 && intensity <= 15) {
        display_intensity = (uint16_t)intensity;
        updated = true;
    }
    if (state == 0 || state == 1) {
        display_state = (uint16_t)state;
        updated = true;
    }
    if (pad == 0 || pad == 1) {
        display_padding = (uint16_t)pad;
        updated = true;
    }

    if (updated) {
        // Update registry
        if (display_target_id == 0) {
            for (int i = 0; i < online_slave_count; i++) {
                uint8_t sid = online_slave_ids[i];
                if (val_str[0] != '\0') {
                    strncpy(slave_registry[sid].text, val_str, 5);
                    slave_registry[sid].text[5] = '\0';
                    slave_registry[sid].is_ascii = is_string_numeric(val_str) ? 0 : 1;
                }
                if (intensity >= 0 && intensity <= 15) slave_registry[sid].intensity = (uint8_t)intensity;
                if (state == 0 || state == 1) slave_registry[sid].state = (uint8_t)state;
                if (pad == 0 || pad == 1) slave_registry[sid].padding = (uint8_t)pad;
                slave_registry[sid].is_configured = 1;
                slave_registry[sid].pending_write = 1;
            }
        } else {
            uint8_t sid = display_target_id;
            if (val_str[0] != '\0') {
                strncpy(slave_registry[sid].text, val_str, 5);
                slave_registry[sid].text[5] = '\0';
                slave_registry[sid].is_ascii = is_string_numeric(val_str) ? 0 : 1;
            }
            if (intensity >= 0 && intensity <= 15) slave_registry[sid].intensity = (uint8_t)intensity;
            if (state == 0 || state == 1) slave_registry[sid].state = (uint8_t)state;
            if (pad == 0 || pad == 1) slave_registry[sid].padding = (uint8_t)pad;
            slave_registry[sid].is_configured = 1;
            slave_registry[sid].pending_write = 1;
        }

        display_updated = true;
        printf("MQTT: Updated display state -> target_id: %d, val: %s, intensity: %d, state: %d, pad: %d\r\n", 
               display_target_id, val_str, display_intensity, display_state, display_padding);
        extern osSemaphoreId_t display_update_sem;
        if (display_update_sem != NULL) {
            osSemaphoreRelease(display_update_sem);
        }
    }
}

// Subscribes
void mqtt_subscribe_topics(void) {
    static char display_set_topic[128];
    int rc;
    
    // Display control subscription
    sprintf(display_set_topic, "master/%s/display/set", master_uid);
    rc = MQTTSubscribe(&mqtt_client, display_set_topic, QOS1, on_message_received);
    printf("MQTT: Subscribed to %s (rc=%d)\r\n", display_set_topic, rc);
}

// Connection management
int mqtt_connect_to_broker(void) {
    int rc;
    printf("MQTT: Connecting network to broker %d.%d.%d.%d:%d...\r\n", mqtt_broker_ip[0], mqtt_broker_ip[1], mqtt_broker_ip[2], mqtt_broker_ip[3], mqtt_broker_port);
           
    rc = ConnectNetwork(&mqtt_network, mqtt_broker_ip, mqtt_broker_port);
    if (rc != SOCK_OK) {
        printf("MQTT: ConnectNetwork failed, err = %d\r\n", rc);
        return rc;
    }
    
    printf("MQTT: TCP connected. Sending MQTT CONNECT...\r\n");
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = master_uid;
    data.keepAliveInterval = 60;
    data.cleansession = 1;
    
    rc = MQTTConnect(&mqtt_client, &data);
    if (rc != 0) {
        printf("MQTT: MQTTConnect failed, err = %d\r\n", rc);
        mqtt_client.isconnected = 0;
        mqtt_network.disconnect(&mqtt_network);
        close(MQTT_SOCKET);
        return rc;
    }
    
    printf("MQTT: Connected successfully!\r\n");
    mqtt_is_connected = true;
    
    mqtt_subscribe_topics();
    
    publish_initial_slave_states();
    
    return 0;
}

void MQTT_App_Init(void) {
    // Format STM32 Unique ID as UID
    sprintf(master_uid, "%08X%08X%08X", (unsigned int)HAL_GetUIDw0(), (unsigned int)HAL_GetUIDw1(), (unsigned int)HAL_GetUIDw2());
    printf("MQTT: Master UID is %s\r\n", master_uid);
    
    NewNetwork(&mqtt_network, MQTT_SOCKET);
    MQTTClientInit(&mqtt_client, &mqtt_network, 1000, mqtt_send_buf, sizeof(mqtt_send_buf), mqtt_read_buf, sizeof(mqtt_read_buf));
                   
    mqtt_is_connected = false;
}

void MQTT_App_Process(void) {
    // Maintain Connection
    if (!mqtt_is_connected) {
        static uint32_t last_reconnect_time = 0;
        if (HAL_GetTick() - last_reconnect_time > 5000) {
            last_reconnect_time = HAL_GetTick();
            uint8_t link = PHY_LINK_OFF;
            ctlwizchip(CW_GET_PHYLINK, &link);
            if (link == PHY_LINK_ON) {
                mqtt_connect_to_broker();
            } else {
                printf("MQTT: Link is down. Reconnect deferred.\r\n");
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            }
        }
        return;
    }
    
    // Check TCP socket status
    uint8_t state = getSn_SR(MQTT_SOCKET);
    if (state != SOCK_ESTABLISHED && state != SOCK_CLOSE_WAIT) {
        printf("MQTT: Connection lost, state = 0x%02x\r\n", state);
        mqtt_is_connected = false;
        mqtt_client.isconnected = 0;
        mqtt_network.disconnect(&mqtt_network);
        close(MQTT_SOCKET);
        return;
    } else if (state == SOCK_CLOSE_WAIT) {
        printf("MQTT: Peer requested disconnect\r\n");
        mqtt_is_connected = false;
        mqtt_client.isconnected = 0;
        mqtt_network.disconnect(&mqtt_network);
        close(MQTT_SOCKET);
        return;
    }
    
    // Process outgoing messages from the queue
    extern osMessageQueueId_t mqtt_tx_queueHandle;
    if (mqtt_tx_queueHandle != NULL) {
        mqtt_msg_t msg;
        while (osMessageQueueGet(mqtt_tx_queueHandle, &msg, NULL, 0) == osOK) {
            printf("MQTT: Dequeued message for topic -> %s\r\n", msg.topic);
            MQTT_Publish(msg.topic, msg.payload, 0);
            vPortFree(msg.payload);
        }
    }

    // Yield to Paho MQTT to process incoming packets and send keepalive pingreqs
    int rc = MQTTYield(&mqtt_client, 200); // 200ms timeout
    if (rc != 0) {
        printf("MQTT: Yield failed, error = %d\r\n", rc);
        mqtt_is_connected = false;
        mqtt_client.isconnected = 0;
        mqtt_network.disconnect(&mqtt_network);
        close(MQTT_SOCKET);
    }
}

// Publish initial display status of all currently online slaves safely
void publish_initial_slave_states(void) {
    static bool initial_published = false;
    if (initial_published) return;
    if (!mqtt_is_connected || online_slave_count == 0) return;
    
    printf("MQTT: Publishing initial display status of online slaves...\r\n");
    for (int i = 0; i < online_slave_count; i++) {
        uint8_t sid = online_slave_ids[i];
        char ack_payload[128];
        if (slave_registry[sid].is_ascii) {
            sprintf(ack_payload, "{\"status\":\"success\",\"id\":%d,\"val\":\"%s\",\"intensity\":%d,\"state\":%d,\"pad\":%d}", 
                    sid, slave_registry[sid].text, slave_registry[sid].intensity, slave_registry[sid].state, slave_registry[sid].padding);
        } else {
            sprintf(ack_payload, "{\"status\":\"success\",\"id\":%d,\"val\":%s,\"intensity\":%d,\"state\":%d,\"pad\":%d}", 
                    sid, slave_registry[sid].text, slave_registry[sid].intensity, slave_registry[sid].state, slave_registry[sid].padding);
        }
        char ack_topic[128];
        sprintf(ack_topic, "master/%s/display/status", master_uid);
        MQTT_Queue_Publish(ack_topic, ack_payload);
        printf("MQTT: Queued initial display status of Slave %d to %s -> %s\r\n", sid, ack_topic, ack_payload);
        osDelay(50);
    }
    initial_published = true;
}

void MQTT_Queue_Publish(const char *topic, const char *payload) {
    extern osMessageQueueId_t mqtt_tx_queueHandle;
    if (mqtt_tx_queueHandle == NULL) return;
    
    mqtt_msg_t msg;
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    
    msg.payload = pvPortMalloc(strlen(payload) + 1);
    if (msg.payload != NULL) {
        strcpy(msg.payload, payload);
        if (osMessageQueuePut(mqtt_tx_queueHandle, &msg, 0, 0) != osOK) {
            printf("MQTT Queue: Queue full, dropping message!\r\n");
            vPortFree(msg.payload); // Free to prevent memory leak
        }
    } else {
        printf("MQTT Queue: Memory allocation failed!\r\n");
    }
}
