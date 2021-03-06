
#include "c_types.h"
#include "tcp_client.h"
#include "esp_common.h"
#include "espconn.h"
#include "user_config.h"
//cJSON library
#include "json/cJSON.h"
//JSON RPC
#include "my_jsonrpc.h"



/*************** TEST **************/

//peripherals and drivers
#include "gpio.h"


//GPIO OUTPUT SETTINGS
#define  LED_IO_MUX  PERIPHS_IO_MUX_MTDO_U
#define  LED_IO_NUM  5
#define  LED_IO_FUNC FUNC_GPIO15
#define  LED_IO_PIN  GPIO_Pin_5

/******************TEST *************/



#define DEMO_AP_SSID ""
#define DEMO_AP_PASSWORD ""

#define SOFT_AP_SSID "DEMO_AP"
#define SOFT_AP_PASSWORD "12345678"

#define	MAXSIZE 4096

static char buffer [MAXSIZE];


/*---------------------------------------
									cJSON
------------------------------------------*/
/*

void doit(char *text)
{
	char *out;cJSON *json;

	json=cJSON_Parse(text);
	if (!json) {printf("Error before: [%s]\n",cJSON_GetErrorPtr());}
	else
	{
		out=cJSON_Print(json);
		cJSON_Delete(json);
		printf("%s\n",out);
		free(out);
	}
}


*/


/*--------------------------------------------------------------
                             Tcp Client
-------------------------------------------------------------*/


void TcpClientConnect(void*arg)
{
    struct espconn* tcp_server_local=arg;
#if TCP_CLIENT_KEEP_ALIVE_ENABLE
	espconn_set_opt(tcp_server_local,BIT(3));//enable keep alive ,this api must call in connect callback

	uint32 keep_alvie=0;
	keep_alvie=TCP_CLIENT_KEEP_ALIVE_IDLE_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPIDLE,&keep_alvie);
	keep_alvie=TCP_CLIENT_RETRY_INTVL_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPINTVL,&keep_alvie);
	keep_alvie=keep_alvie=TCP_CLIENT_RETRY_INTVL_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPCNT,&keep_alvie);
	DBG_PRINT("keep alive enable\n");
#endif
	DBG_LINES("TCP CLIENT CONNECT");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
	espconn_send(tcp_server_local,TCP_CLIENT_GREETING,strlen(TCP_CLIENT_GREETING));
}

void TcpClientDisConnect(void* arg)
{
    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP CLIENT DISCONNECT");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
}
void TcpClienSendCb(void* arg)
{
    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP CLIENT SendCb");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
	//pdata = "{\"jsonrpc\": \"2.0\", \"method\": \"POST\", \"params\": [42, 23], \"id\": 1}";
	//espconn_send(tcp_server_local, pdata, len);
}

void TcpRecvCb(void *arg, char *pdata, unsigned short len)
{
   struct espconn* tcp_server_local=arg;
   DBG_PRINT("Recv tcp client ip:%d.%d.%d.%d port:%d len:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port,
		                                          len);

  int sz = sizeof(pdata)/sizeof(pdata[0]);
	buffer[sz] = '\0';
	strcpy(buffer, pdata);
	printf("TCP Buffer is : %s\n", buffer);
	//Trying to parse as JSONRPC
	//creating Structure with Type Jsonrpc_request_t
	Jsonrpc_request_t request;
	jsonrpc_parse(buffer, &request);
	//printf("Method is : %s\n", request.jsonrpc_req_meth);


  //espconn_send(tcp_server_local,pdata,len);

}
void TcpReconnectCb(void *arg, sint8 err)
{
    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP CLIENT RECONNECT");
	DBG_PRINT("status:%d\n",err);
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port\
		                                          );
}











/****************************************** TCP CLIENT *******************************************************************************************/


void TcpLocalClient(void* arg)
{

/***** change print function name****/
	    printf("%s\n", __func__);
/********/

	uint8 con_status=wifi_station_get_connect_status();

	while(con_status!=STATION_GOT_IP)
	{
		con_status=wifi_station_get_connect_status();
	  DBG_PRINT("Connect ap %s\n",con_status==STATION_IDLE?"IDLE":con_status==STATION_CONNECTING?\
	                        "Connecting...":con_status==STATION_WRONG_PASSWORD?\
	                        "Password":con_status==STATION_NO_AP_FOUND?\
	                        "ap_not_find":con_status==STATION_CONNECT_FAIL?"Connect fail":"Get ip"\
	                        );
			vTaskDelay(100);
	}

  static struct espconn tcp_client;
	static esp_tcp tcp;

	// host: "10.10.1.61", Andreas data collector
  // port: 42006

	tcp.remote_port= 42006;
	tcp.remote_ip[0] = 10;
	tcp.remote_ip[1] = 10;
	tcp.remote_ip[2] = 1;
	tcp.remote_ip[3] = 61;


	tcp.local_port = espconn_port();
	struct ip_info ipconfig;
	wifi_get_ip_info(STATION_IF, &ipconfig);
	//memcpy(tcp.local_ip, &ipconfig.ip, 4);

	tcp_client.type=ESPCONN_TCP;
	tcp_client.state = ESPCONN_NONE;

	tcp_client.proto.tcp=&tcp;

	espconn_regist_connectcb(&tcp_client,TcpClientConnect);
	espconn_regist_recvcb(&tcp_client,TcpRecvCb);
	espconn_regist_reconcb(&tcp_client,TcpReconnectCb);
	espconn_regist_disconcb(&tcp_client,TcpClientDisConnect);
	espconn_regist_sentcb(&tcp_client,TcpClienSendCb);

	DBG_PRINT("tcp client connect server,server ip:%d.%d.%d.%d port:%d\n",tcp_client.proto.tcp->remote_ip[0],
		                                          tcp_client.proto.tcp->remote_ip[1],
		                                          tcp_client.proto.tcp->remote_ip[2],
		                                          tcp_client.proto.tcp->remote_ip[3],
		                                          tcp_client.proto.tcp->remote_port\
		                                          );
	espconn_connect(&tcp_client);
	vTaskDelete(NULL);
}







/**************************************************************************************************************************************/





void StaConectApConfig(char*ssid,char*password)
{
    if(wifi_get_opmode()!=0x01&&wifi_get_opmode()!=0x03){
        ERR_PRINT("Mode not include sta\n");
		wifi_set_opmode(0x03);
		//return;
	}
	struct station_config* sta=(struct station_config*)malloc(sizeof(struct station_config));
	if(sta==NULL){
        ERR_PRINT("memory not enough,heap_size=%ukByte\n",system_get_free_heap_size()/1023);
		return;
	}
	bzero(sta,sizeof(struct station_config));
	wifi_station_get_config(sta);
	sprintf(sta->ssid,"%s",ssid);
	sprintf(sta->password,"%s",password);
	wifi_station_set_config(sta);
	wifi_station_connect();

	bzero(sta,sizeof(struct station_config));
	wifi_station_get_config(sta);
	DBG_LINES("STA_CONNECT_AP");
	DBG_PRINT("ssid:%s\n",sta->ssid);
	DBG_PRINT("password:%d\n",sta->password);
	free(sta);
	sta=NULL;

}

void TcpServerClientConnect(void*arg)
{

    struct espconn* tcp_server_local=arg;
#if TCP_SERVER_KEEP_ALIVE_ENABLE
	espconn_set_opt(tcp_server_local,BIT(3));//enable keep alive ,this api must call in connect callback

	uint32 keep_alvie=0;
	keep_alvie=TCP_SERVER_KEEP_ALIVE_IDLE_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPIDLE,&keep_alvie);
	keep_alvie=TCP_SERVER_RETRY_INTVL_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPINTVL,&keep_alvie);
	keep_alvie=keep_alvie=TCP_SERVER_RETRY_INTVL_S;
	espconn_set_keepalive(tcp_server_local,ESPCONN_KEEPCNT,&keep_alvie);
	DBG_PRINT("keep alive enable\n");
#endif
	DBG_LINES("TCP server CONNECT");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
	espconn_send(tcp_server_local,TCP_SERVER_GREETING,strlen(TCP_SERVER_GREETING));
}
void TcpServerClientDisConnect(void* arg)
{

    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP server DISCONNECT");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
}

void TcpServerClienSendCb(void* arg)
{

    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP server SendCb");
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port
		                                          );
}



void TcpServerRecvCb(void *arg, char *pdata, unsigned short len)
{

   struct espconn* tcp_server_local=arg;
   DBG_PRINT("Recv tcp client ip:%d.%d.%d.%d port:%d len:%d\n data: %s\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port,
                                              len, pdata);
	 //Echo back at sender
   //espconn_send(tcp_server_local,pdata,len);
	 int sz = sizeof(pdata)/sizeof(pdata[0]);
	 buffer[sz] = '\0';
	 strcpy(buffer, pdata);



	 printf("TCP Buffer is : %s\n", buffer);
	  //free(buffer);

	 //Trying to parse as JSONRPC
	 Jsonrpc_request_t request;
	 jsonrpc_parse(pdata, &request);
//GPIO Access example UDP (GPIO 5, Relay)
	 if (request.parse_status != 0) {

		 printf("Method is : %s\n", request.jsonrpc_req_meth);

		 static uint8 val = 0;
		 //post put get
		 if((strstr(request.jsonrpc_req_meth, "PUT"))){
			 if(val == 0){
				 gpio16_output_set(0);
				 GPIO_OUTPUT_SET(LED_IO_NUM,1);
				 val = 1;
				 printf("Aight, I put on my robe and wizard hat...\n");
			 }else{
				 gpio16_output_set(1);
				 GPIO_OUTPUT_SET(LED_IO_NUM,0);
				 val = 0;
			 }
		 }
		 //Freeeedom!!!11
		 free(request.jsonrpc_req_meth);
		 free(request.jsonrpc_req_params_data_id);
		 free(request.jsonrpc_req_params_data_type);
	 }






}

void TcpServerReconnectCb(void *arg, sint8 err)
{
    struct espconn* tcp_server_local=arg;
	DBG_LINES("TCP server RECONNECT");
	DBG_PRINT("status:%d\n",err);
	DBG_PRINT("tcp client ip:%d.%d.%d.%d port:%d\n",tcp_server_local->proto.tcp->remote_ip[0],
		                                          tcp_server_local->proto.tcp->remote_ip[1],
		                                          tcp_server_local->proto.tcp->remote_ip[2],
		                                          tcp_server_local->proto.tcp->remote_ip[3],
		                                          tcp_server_local->proto.tcp->remote_port\
		                                          );
}








/*****************************************************************************************************************************************************/






void TcpLocalServer(void* arg)
{
	// print function name
	  printf("%s\n", __func__);


	static struct espconn tcp_server_local;
	static esp_tcp tcp;
	tcp_server_local.type=ESPCONN_TCP;
	tcp_server_local.proto.tcp=&tcp;
	tcp.local_port=42006;

	espconn_regist_connectcb(&tcp_server_local,TcpServerClientConnect);
	espconn_regist_recvcb(&tcp_server_local,TcpServerRecvCb);
	espconn_regist_reconcb(&tcp_server_local,TcpServerReconnectCb);
	espconn_regist_disconcb(&tcp_server_local,TcpServerClientDisConnect);
	espconn_regist_sentcb(&tcp_server_local,TcpServerClienSendCb);

	espconn_accept(&tcp_server_local);
	vTaskDelete(NULL);
}









/********************************************************************************************************************************************************************************/










void UdpRecvCb(void *arg, char *pdata, unsigned short len)
{
    struct espconn* udp_server_local=arg;
	DBG_LINES("UDP_RECV_CB");
	DBG_PRINT("UDP_RECV_CB len:%d ip:%d.%d.%d.%d port:%d\n data:%s\n", len,udp_server_local->proto.tcp->remote_ip[0],
		                                          udp_server_local->proto.tcp->remote_ip[1],
		                                          udp_server_local->proto.tcp->remote_ip[2],
		                                          udp_server_local->proto.tcp->remote_ip[3],
		                                          udp_server_local->proto.tcp->remote_port,
																							pdata
		                                          );

		//Echo back at sender
    //espconn_send(udp_server_local,pdata,len);

//Calculate the number of elements(characters) in array pdata
//Set the NUL terminator on the max number for message
		int sz = sizeof(pdata)/sizeof(pdata[0]);
		buffer[sz] = '\0';
		strcpy(buffer, pdata);
		printf("UDP Buffer is : %s\n", buffer);
		//Trying to parse as JSONRPC
		Jsonrpc_request_t request;
		jsonrpc_parse(buffer, &request);
	//GPIO Access example UDP (GPIO 5, Relay)
		if (request.parse_status != 0) {

 		 printf("Method is : %s\n", request.jsonrpc_req_meth);

 		 static uint8 val = 0;
 		 //post put get
 		 if((strstr(request.jsonrpc_req_meth, "PUT"))){
 			 if(val == 0){
 				 gpio16_output_set(0);
 				 GPIO_OUTPUT_SET(LED_IO_NUM,1);
 				 val = 1;
 				 printf("Aight, I put on my robe and wizard hat...\n");
 			 }else{
 				 gpio16_output_set(1);
 				 GPIO_OUTPUT_SET(LED_IO_NUM,0);
 				 val = 0;
 			 }
 		 }
 		 //Freeeedom!!!11
 		 free(request.jsonrpc_req_meth);
 		 free(request.jsonrpc_req_params_data_id);
 		 free(request.jsonrpc_req_params_data_type);
 	 }


		//printf("%s\n", buffer);


}
void UdpSendCb(void* arg)
{
    struct espconn* udp_server_local=arg;
	DBG_LINES("UDP_SEND_CB");
	DBG_PRINT("UDP_SEND_CB ip:%d.%d.%d.%d port:%d\n",udp_server_local->proto.tcp->remote_ip[0],
		                                          udp_server_local->proto.tcp->remote_ip[1],
		                                          udp_server_local->proto.tcp->remote_ip[2],
		                                          udp_server_local->proto.tcp->remote_ip[3],
		                                          udp_server_local->proto.tcp->remote_port\
		                                          );
}

/***************************************************************************************************************************************/


void udpServer(void*arg)
{

	/***** change print function name****/
				printf("%s\n", __func__);
	/********/

    static struct espconn udp_client;
	static esp_udp udp;
	udp_client.type=ESPCONN_UDP;
	udp_client.proto.udp=&udp;
	udp.local_port=UDP_SERVER_LOCAL_PORT;



	espconn_regist_recvcb(&udp_client,UdpRecvCb);
	espconn_regist_sentcb(&udp_client,UdpSendCb);
    int8 res=0;
	res=espconn_create(&udp_client);
	if(res!=0){
        DBG_PRINT("UDP SERVER CREAT ERR ret:%d\n",res);
	}
	vTaskDelete(NULL);
}



os_timer_t time1;
static struct espconn udp_client;

void t1Callback(void* arg)
{
    os_printf("t1 callback\n");
	espconn_send(&udp_client,TCP_CLIENT_GREETING,strlen(TCP_CLIENT_GREETING));

}
void udpClient(void*arg)
{

	static esp_udp udp;
	udp_client.type=ESPCONN_UDP;
	udp_client.proto.udp=&udp;
	udp.remote_port=UDP_SERVER_LOCAL_PORT;
    memcpy(udp.remote_ip,udp_server_ip,sizeof(udp_server_ip));

	espconn_regist_recvcb(&udp_client,UdpRecvCb);
	espconn_regist_sentcb(&udp_client,UdpSendCb);
    int8 res=0;
	res=espconn_create(&udp_client);

	if(res!=0){
        DBG_PRINT("UDP SERVER CREAT ERR ret:%d\n",res);
	}

	os_timer_disarm(&time1);
	os_timer_setfn(&time1,t1Callback,NULL);
	os_timer_arm(&time1,1000,1);


	vTaskDelete(NULL);

}





void WifiConfig(void* arg)
{
	StaConectApConfig(DEMO_AP_SSID,DEMO_AP_PASSWORD);
    vTaskDelete(NULL);
}
