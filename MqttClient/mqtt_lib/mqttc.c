#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "MQTTConnect.h"
#include "MQTTSubscribe.h"
#include "MQTTPublish.h"
#include "MQTTUnsubscribe.h"
#include "MQTTPacket.h"
#include "mqttc.h"
#include "pt.h"

int MQTT_decode(char *data, int *value)
{
	unsigned char c;	
	int multiplier = 1;	
	int index = 0;
	int len = 0;
#define MAX_NO_OF_REMAINING_LENGTH_BYTES 4

	do
	{
		if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
		{
			return len;
		}
		c = data[index++];
		*value += (c & 0x7F) * multiplier;
		multiplier *= 128;
	}while ((c & 128) != 0);
	return len;
}

int MQTT_encode(char *buf, int length)
{
	int rc = 0;

	do	
	{		
		char d = length % 128;		
		length /= 128;		/* if there are more digits to encode, set the top bit of this digit */		
		if (length > 0)			
			d |= 0x80;		
		buf[rc++] = d;	
	} while (length > 0);		
	return rc;
}

int parse_MQTTDataFixHead(char *data, int num)
{
	int rc = -1;
	MQTTHeader header = {0};
	int rem_len;
	
	MQTT_decode(data+1, &rem_len);
	//printf("rem_len is %d\n", rem_len);
	header.byte = data[0];
	rc = header.bits.type;
	return rc;
}

int MQTT_process_TCPRecvData(char *data, int num)
{
	char mqtt_buf[4096];
	int rc = 0;
	int ret = -1;
	
	memset(mqtt_buf, 0, sizeof(mqtt_buf));
	if (num == -1)
	{
		printf("tcp have closed\n");
		return -1;
	}
	else
	{
		int result;
		memcpy(mqtt_buf, data, num);
		result = parse_MQTTDataFixHead(mqtt_buf, num);
		switch(result)
		{
		case CONNACK:
			{
				unsigned char sessionPresent, connack_rc;		
				if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, mqtt_buf, sizeof(mqtt_buf)) != 1 || connack_rc != 0)		
				{			
					printf("Unable to connect, return code %d\n", connack_rc);			
					//connect fail
					MQTT_Core_sm(MQTT_CON_FAIL, 0, NULL);
				}
				else
				{
					printf("connect mqtt server success %d\n", connack_rc);
					//connect success
					MQTT_Core_sm(MQTT_CON_SUCC, 0, NULL);
					ret = MQTT_CON_SUCC;
				}
				
			}
			break;
		case PUBLISH:
			{			
				unsigned char dup;
				int qos;
				unsigned char retained;	
				unsigned short msgid;
				int payloadlen_in;
				unsigned char* payload_in;
				int rc;	
				MQTTString receivedTopic = MQTTString_initializer;
				rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,	&payload_in, &payloadlen_in, mqtt_buf, sizeof(mqtt_buf));			
				if (rc == 1)
				{
					mqtt_subscribe_msg_st msg;
					msg.topic = receivedTopic.lenstring.data;
					msg.len = receivedTopic.lenstring.len;
					printf("%d packetid %s:%d message arrived %d->%s\n", msgid, receivedTopic.lenstring.data, receivedTopic.lenstring.len, payloadlen_in, payload_in);
					MQTT_RecvData(&msg, payload_in, payloadlen_in);
					if (qos == 1)
					{
						MQTT_Core_sm(MQTT_PUB_ACK, 0, (void*)(&msgid));
					}
					else if (qos == 2)
					{
						MQTT_Core_sm(MQTT_PUB_RECV, 0, (void*)(&msgid));
					}
					else
					{
						
					}
					ret = 0;
				}
				else
				{
					printf("public msg fail %d\n", rc);
				}
			}
			break;
		case PUBACK:
		case PUBREC:
		case PUBREL:
		case PUBCOMP:
			{
				unsigned char packettype, dup;
				unsigned short packetid;
				if (MQTTDeserialize_ack(&packettype, &dup, &packetid, mqtt_buf, sizeof(mqtt_buf)) == 1)
				{
					if (result == PUBACK)
					{
						printf("public %d packedid 1 qos success\n", packetid);
					}
					else if (result == PUBREC)
					{
						unsigned int param = 0;
						printf("public %d packedid 2 qos recv\n", packetid);	
						param = dup << 16 | packetid;
						MQTT_Core_sm(MQTT_PUB_RELE, 0, (void*)(&param));
					}
					else if (result == PUBREL)
					{
						printf("recv public %d packedid 2 qos release\n", packetid);
						MQTT_Core_sm(MQTT_PUB_COMP, 0, (void*)(&packetid));
					}
					else
					{
						printf("public %d packedid 2 qos success\n", packetid);	
					}
					ret = 0;
				}
			}
			break;
		case SUBACK:
			{
				unsigned short submsgid;		
				int subcount;		
				int granted_qos[MAX_SUBCOUNT];		
				rc = MQTTDeserialize_suback(&submsgid, MAX_SUBCOUNT, &subcount, granted_qos, mqtt_buf, sizeof(mqtt_buf));		
				if (rc == 1)		
				{		
					int i;
					printf("packerid=%d, subcount = %d\n", submsgid, subcount);
					for (i = 0; i < subcount; i++)
					{
						printf("granted_qos[%d]=%d\n", i, granted_qos[i]);
					}
					MQTT_Core_sm(MQTT_SUB_SUCC, 0, NULL);
					ret = MQTT_SUB_SUCC;
				}
				else
				{
					printf("suback rc=%d\n", rc);
				}
			}
			break;
		case UNSUBACK:
			{
				unsigned short submsgid;		
				rc = MQTTDeserialize_unsuback(&submsgid, mqtt_buf, sizeof(mqtt_buf));
				if (rc == 1)
				{
					printf("unsub success\n");
					ret = MQTT_USUB_SUCC;
				}
				else
				{
					printf("unsub fail\n");
				}
			}
			break;
		case PINGRESP:
			{
				printf("recv ping response\n");
				ret = 0;
			}
			break;
		default:
			break;
		}
	}
	return ret;
}


int MQTT_connect(char* username, char* password)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	
	data.clientID.cstring = username;	
	data.keepAliveInterval = 60;	
	data.cleansession = 0;	
	data.username.cstring = username;	
	data.password.cstring = password;

	len = MQTTSerialize_connect(buf, buflen, &data);
	MQTT_Send_Data(buf,len);
	
	return 1;
}

int MQTT_subscribe(char* topic, int qos)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);	
	int msgid = 1;	
	MQTTString topicString = MQTTString_initializer;	
	int req_qos = qos;
	int len;

	topicString.cstring = topic;
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_unsubscribe(char* topic)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);	
	int msgid = 1;	
	MQTTString topicString = MQTTString_initializer;	
	int len;

	topicString.cstring = topic;
	len = MQTTSerialize_unsubscribe(buf, buflen, 0, msgid, 1, &topicString);
	MQTT_Send_Data(buf,len);
	return 0;
}

unsigned short MQTT_public(char* topic, int qos, char* payload, int payloadlen)
{
	unsigned char buf[1024];	
	int buflen = sizeof(buf);
	MQTTString topicString = MQTTString_initializer;
	int len;
	unsigned short packetId;

	srand((unsigned)time(NULL));
	packetId = rand() & 0xFFFF;
	topicString.cstring = topic;
	len = MQTTSerialize_publish(buf, buflen, 0, qos, 0, packetId, topicString, (unsigned char*)payload, payloadlen);
	MQTT_Send_Data(buf,len);
	return packetId;
}

int MQTT_puback(unsigned short packetid)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;

	len = MQTTSerialize_puback(buf, buflen, packetid);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_pubrecv(unsigned short packetid)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;

	len = MQTTSerialize_pubrec(buf, buflen, packetid);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_pubrelease(unsigned char dup, unsigned short packetid)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;

	len = MQTTSerialize_pubrel(buf, buflen, dup, packetid);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_pubcomplete(unsigned short packetid)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;

	len = MQTTSerialize_pubcomp(buf, buflen, packetid);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_ping(void)
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;
	
	len = MQTTSerialize_pingreq(buf, buflen);
	MQTT_Send_Data(buf,len);
	return 0;
}

int MQTT_disconnect()
{
	unsigned char buf[200];	
	int buflen = sizeof(buf);
	int len;
	
	len = MQTTSerialize_disconnect(buf, buflen);
	MQTT_Send_Data(buf,len);
	return 0;
}

static struct pt mqtt_pt;

char MQTT_Core_sm(int status, uint16 ms, void* param)
{
	static uint8 init = 0;
	static uint16 tmo;
	static mqtt_server_st mqttserver;
	static mqtt_login_st mqttlogin;
	static mqtt_global_info_st *mqttinfo = NULL;

	int qos;
	unsigned short packetid;
	mqtt_public_st *pub = NULL;
	mqtt_subscribe_st *sub = NULL;
	static int ret;

	if(tmo > ms)
	{
		tmo -= ms;        
	}
	else if(tmo> 0)
	{
		tmo = 0;
	}

	if(init == 0)
	{
		PT_INIT(&mqtt_pt);
		init = 1;
	}

	if (status == MQTT_RECONN_REQ)
	{
		MQTT_Disconnect_Server();
		PT_INIT(&mqtt_pt);
	}

	PT_BEGIN(&mqtt_pt);

	PT_WAIT_UNTIL(&mqtt_pt, (status == MQTT_START_REQ));

	mqttinfo = (mqtt_global_info_st*)param;
	strcpy(mqttserver.mqttAddr, mqttinfo->ser_info.mqttAddr);
	mqttserver.port = mqttinfo->ser_info.port;
	strcpy(mqttlogin.name, mqttinfo->login_info.name);
	strcpy(mqttlogin.password, mqttinfo->login_info.password);

	tmo = 5000;
	ret = MQTT_Connect_Server(mqttserver.mqttAddr, mqttserver.port);

	PT_WAIT_UNTIL(&mqtt_pt, (status == MQTT_TCP_CONNNECTED) || (ret <= 0) || (tmo == 0));

	if (ret < 0 || tmo == 0)
	{
		printf("mqtt connect fail %d,%d\n", ret, tmo);
		PT_EXIT(&mqtt_pt);
	}
	printf("conn server left time %d\n", tmo);
	MQTT_connect(mqttlogin.name, mqttlogin.password);

	tmo = 10000;
	PT_WAIT_UNTIL(&mqtt_pt, (tmo == 0)||(status == MQTT_CON_SUCC)||(status == MQTT_CON_FAIL));
	if (tmo == 0 || status == MQTT_CON_FAIL)
	{
		printf("connect mqtt server timeout or connect fail\n");
		PT_EXIT(&mqtt_pt);
	}
	printf("login server left time %d\n", tmo);
	while(1)
	{
		PT_WAIT_UNTIL(&mqtt_pt, (status != MQTT_STATUS_NONE));
		if (status == MQTT_PING_REQ)
		{
			printf("send ping request\n");
			MQTT_ping();
			status = MQTT_STATUS_NONE;
			continue;
		}
		if (status == MQTT_DIS_REQ)
		{
			break;
		}
		switch(status)
		{
		case MQTT_PUB_REQ:
			{
				pub = (mqtt_public_st*)param;
				qos = pub->qos;
				packetid = MQTT_public(pub->topic, qos, pub->body, pub->bodylen);
				tmo = 5000;
				PT_WAIT_UNTIL(&mqtt_pt, (qos == 0)||(status == MQTT_PUB_SUCC)||(tmo == 0));
				if (tmo != 0)
				{
					printf( "public success\n");
				}
			}
			break;
		case MQTT_SUB_REQ:
			{
				sub = (mqtt_subscribe_st*)param;
				//char *topic = (char*)param;
				MQTT_subscribe(sub->topic, sub->qos);
			}
			break;
		case MQTT_USUB_REQ:
			{
				char *topic = (char*)param;
				MQTT_unsubscribe(topic);
			}
			break;
		case MQTT_PUB_ACK:
			{
				packetid = *((unsigned short*)param);
				MQTT_puback(packetid);
			}
			break;
		case MQTT_PUB_RECV:
			{
				packetid = *((unsigned short*)param);
				MQTT_pubrecv(packetid);
			}
			break;
		case MQTT_PUB_COMP:
			{
				packetid = *((unsigned short*)param);
				MQTT_pubcomplete(packetid);
			}
			break;
		case MQTT_PUB_RELE:
			{
				int para = *((unsigned int*)param);
				unsigned char dup = (unsigned char)((para &0x00ff0000)>>16);
				packetid = (unsigned short)(para & 0x0000ffff);
				printf("pub release dup:%d,pid:%d", dup, packetid);
				MQTT_pubrelease(dup, packetid);
			}
			break;
		case MQTT_SUB_SUCC:
			break;
		default:
			break;
		}
		status = MQTT_STATUS_NONE;
	}
	MQTT_disconnect();
	tmo = 500;
	PT_WAIT_UNTIL(&mqtt_pt, (tmo == 0));
	MQTT_Disconnect_Server();
	PT_END(&mqtt_pt);
}


