#ifndef _MQTTC_H_
#define _MQTTC_H_

#define MAX_SUBCOUNT           10


#define MQTT_CON_FAIL          -100

#define MQTT_STATUS_NONE       -1

#define MQTT_TCP_CONNNECTED    0
#define MQTT_CON_SUCC          1
#define MQTT_SUB_SUCC          2
#define MQTT_PUB_SUCC          3
#define MQTT_PUB_REQ           4
#define MQTT_SUB_REQ           5
#define MQTT_USUB_REQ          6
#define MQTT_PUB_ACK           7
#define MQTT_PUB_RECV          8
#define MQTT_PUB_COMP          9
#define MQTT_DIS_REQ           10
#define MQTT_RECONN_REQ        11
#define MQTT_START_REQ         12
#define MQTT_USUB_SUCC         13
#define MQTT_PUB_RELE          14
#define MQTT_PING_REQ          15

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

typedef signed int int32;
typedef signed short int16;
typedef signed char int8;

typedef struct
{
	char *topic;
	int qos;
	char *body;
	int bodylen;
}mqtt_public_st;

typedef struct
{
	char *topic;
	int qos;
}mqtt_subscribe_st;

typedef struct
{
	char *topic;
	int len;
}mqtt_subscribe_msg_st;

typedef struct
{
	char mqttAddr[64];
	unsigned short port;
}mqtt_server_st;

typedef struct
{
	char name[32];
	char password[32];
}mqtt_login_st;

typedef struct
{
	mqtt_server_st ser_info;
	mqtt_login_st login_info;
}mqtt_global_info_st;

/*
 * send data interface
 */
extern int MQTT_Send_Data(char *buf, int len);

/*
 * connect server interface
 */
extern int MQTT_Connect_Server(char* ip, unsigned short port);

/*
 * disconnect server server
 */
extern int MQTT_Disconnect_Server(void);

/*
 * recv data interface
 */
extern int MQTT_RecvData(mqtt_subscribe_msg_st *msg, char *data, int len);

/*
 * tcp socket recv data call this func process data
 */
int MQTT_process_TCPRecvData(char *data, int num);

/*
 * mqtt core status machine
 */
char MQTT_Core_sm(int status, uint16 ms, void* param);

#endif
