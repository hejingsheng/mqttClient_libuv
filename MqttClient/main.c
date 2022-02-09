#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h> 
#include <pthread.h>

#include "mqttc.h"
#include "uv.h"

typedef struct 
{
	int cmd;
	void *data;
}async_cmd_st;

uv_tcp_t mqttTcpClient;
uv_connect_t conn;
char sendbuf[200];
int timeout = 100;

static void onAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	static char slab[65536];
	buf->base = slab;
	buf->len = sizeof(slab);
}

static void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	if (nread > 0)
	{
		if (MQTT_process_TCPRecvData(buf->base, nread) == MQTT_CON_SUCC)
		{
			timeout = 60 * 1000;
		}
	}
	else if (nread < 0)
	{
		MQTT_Core_sm(MQTT_RECONN_REQ, 0, NULL);
	}
	else
	{

	}
}

static void onClose(uv_handle_t* handle)
{

}

static void onConnect(uv_connect_t* req, int status)
{
	if (0 != status)
	{
		printf("connect error\n");
		return;
	}
	else
	{
		uv_read_start(&mqttTcpClient, onAlloc, onRead);
		MQTT_Core_sm(MQTT_TCP_CONNNECTED, 0, NULL);
	}
}

static void onWrite(uv_write_t* req, int status)
{
	if (req)
	{
		free(req);
	}
}

int MQTT_Send_Data(char * buf, int len)
{
	//need send data by tcp socket
	//memset(sendbuf, 0, sizeof(sendbuf));
	//memcpy(sendbuf, buf, len);
	uv_write_t *req = (uv_write_t*)malloc(sizeof(uv_write_t));
	uv_buf_t sendBuf = uv_buf_init(buf, len);
	uv_write(req, &mqttTcpClient, &sendBuf, 1, onWrite);
	return 0;
}

int MQTT_Connect_Server(char* ip, unsigned short port)
{
	//need connect server
	struct sockaddr_in addr;

	uv_ip4_addr(ip, port, &addr);
	uv_tcp_init(uv_default_loop(), &mqttTcpClient);

	uv_tcp_connect(&conn, &mqttTcpClient, (struct sockaddr*)&addr, onConnect);
	return 1;
}

int MQTT_Disconnect_Server(void)
{
	//need close socket
	uv_close(&mqttTcpClient, onClose);
	return 0;
}

int MQTT_RecvData(mqtt_subscribe_msg_st *msg, char *data, int len)
{
	printf("recv topic %s data %s len %d", msg->topic, data, len);
	return 0;
}

static void core_timer_cb(uv_timer_t* handle)
{
	//printf("timer coming...\n");
	if (timeout != 60 * 1000)
	{
		MQTT_Core_sm(MQTT_STATUS_NONE, timeout, NULL);
	}
	else
	{
		MQTT_Core_sm(MQTT_PING_REQ, timeout, NULL);
	}
	uv_timer_start(handle, core_timer_cb, timeout, 0);
}

uv_thread_t thread;

static void cmd_thread(void *arg)
{
	int cmd;
	async_cmd_st *async_cmd;
	uv_async_t *async = (uv_async_t*)arg;
	
	for (;;)
	{
		scanf("%d", &cmd);
		if (cmd == 1)
		{
			mqtt_global_info_st *mqttInfo;
			async_cmd = (async_cmd_st*)malloc(sizeof(async_cmd_st));
			mqttInfo = (mqtt_global_info_st*)malloc(sizeof(mqtt_global_info_st));
			strcpy(mqttInfo->ser_info.mqttAddr, "");
			mqttInfo->ser_info.port = 1883;
			strcpy(mqttInfo->login_info.name, "");
			strcpy(mqttInfo->login_info.password, "");
			async_cmd->cmd = cmd;
			async_cmd->data = (void*)mqttInfo;
			async->data = (void*)async_cmd;
			uv_async_send(async);
		}
		else if (cmd == 2)
		{
			mqtt_public_st *publicInfo;
			async_cmd = (async_cmd_st*)malloc(sizeof(async_cmd_st));
			publicInfo = (mqtt_public_st*)malloc(sizeof(mqtt_public_st));
			publicInfo->body = "test";
			publicInfo->bodylen = 4;
			publicInfo->topic = "testTopic";
			publicInfo->qos = 0;
			async_cmd->cmd = cmd;
			async_cmd->data = (void*)publicInfo;
			async->data = (void*)async_cmd;
			uv_async_send(async);
		}
		else if (cmd == 3)
		{
			mqtt_subscribe_st *subInfo;
			async_cmd = (async_cmd_st*)malloc(sizeof(async_cmd_st));
			subInfo = (mqtt_subscribe_st*)malloc(sizeof(mqtt_subscribe_st));
			subInfo->topic = "test";
			subInfo->qos = 0;
			async_cmd->cmd = cmd;
			async_cmd->data = (void*)subInfo;
			async->data = (void*)async_cmd;
			uv_async_send(async);
		}
	}
}

static void async_callback(uv_async_t* handle)
{
	async_cmd_st *async_cmd = (async_cmd_st*)(handle->data);
	if (async_cmd == NULL)
	{
		return;
	}

	if (async_cmd->cmd == 1)
	{
		mqtt_global_info_st *mqttInfo = (mqtt_global_info_st*)(async_cmd->data);
		MQTT_Core_sm(MQTT_START_REQ, 0, mqttInfo);
		if (mqttInfo)
		{
			free(mqttInfo);
		}
	}
	else if (async_cmd->cmd == 2)
	{
		mqtt_public_st *publicInfo = (mqtt_public_st*)(async_cmd->data);
		MQTT_Core_sm(MQTT_PUB_REQ, 0, publicInfo);
		if (publicInfo)
		{
			free(publicInfo);
		}
	}
	else if (async_cmd->cmd == 3)
	{
		mqtt_subscribe_st *subInfo = (mqtt_subscribe_st*)(async_cmd->data);
		MQTT_Core_sm(MQTT_SUB_REQ, 0, subInfo);
		if (subInfo)
		{
			free(subInfo);
		}
	}
	if (async_cmd)
	{
		free(async_cmd);
	}
}

int main(int argc, char* argv[])
{
	// need create thread call MQTT_Core_sm() by timer
	// need call MQTT_process_TCPRecvData() when tcp socket recv data
	
	
	uv_timer_t core_timer;
	uv_async_t async;
	

	uv_timer_init(uv_default_loop(), &core_timer);
	uv_timer_start(&core_timer, core_timer_cb, 100, 0);

	uv_async_init(uv_default_loop(), &async, async_callback);
	uv_thread_create(&thread, cmd_thread, &async);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	return 0;
}