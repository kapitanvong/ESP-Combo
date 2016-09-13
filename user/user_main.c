/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"

#include "gpio.h"
#include "uart.h"
/**********************************Interrupt handler and button defines*************************************************/
#define ETS_GPIO_INTR_ENABLE()  _xt_isr_unmask(1 << ETS_GPIO_INUM)  //ENABLE INTERRUPTS
#define ETS_GPIO_INTR_DISABLE() _xt_isr_mask(1 << ETS_GPIO_INUM)    //DISABLE INTERRUPTS

//GPIO OUTPUT SETTINGS
#define  LED_IO_MUX  PERIPHS_IO_MUX_MTDO_U
#define  LED_IO_NUM  5
#define  LED_IO_FUNC FUNC_GPIO15
#define  LED_IO_PIN  GPIO_Pin_5

//GPIO INPUT SETTINGS
#define  BUTTON_IO_MUC  PERIPHS_IO_MUX_MTCK_U
#define  BUTTON_IO_NUM  0
#define  BUTTON_IO_FUNC FUNC_GPIO13
#define  BUTTON_IO_PIN  GPIO_Pin_0


/**** SMART CONFIG defines **********************************************************************************************/
#define server_ip "192.168.101.142"
#define server_port 9669


#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID

#define DEFAULT_LAN_PORT 	12476

/**** SMART CONFIG start**********************************************************************************************************************************/
LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8  lan_buf[200];
uint16 lan_buf_len;
uint8  udp_sent_cnt = 0;

const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;

	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}

	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;

	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;

	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);

        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;

		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);

		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");

		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}

		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;

	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
			}
            smartconfig_stop();
            break;
    }

}



/**** SMART CONFIG end********************************************************************************************************************************/






/*************TASKS***********************************************************************************************************************************/

void ICACHE_FLASH_ATTR smartconfig_task(void *pvParameters)
{
    smartconfig_start(smartconfig_done);
		vTaskDelay(3000);
		TcpLocalServer();


		// For some reason only first TCP related function is called
		//

    vTaskDelete(NULL);
}
void ICACHE_FLASH_ATTR udp_task(void *pvParameters)
{
		vTaskDelay(3000);
		udpServer();
}
/************END TASKS********************************************************************************************************************/



/*******************************************************************************************
 * FunctionName : io_intr_handler
 * Description  : Interrupt handler for button press
 * Parameters   : none
 * Returns      : none
*******************************************************************************************/
//portTickType since = 0;
void io_intr_handler(void)

{
	uint32 status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);          //READ STATUS OF INTERRUPT
    static uint8 val = 0;
////Daniels's changes
    //portTickType since = xTaskGetTickCountFromISR();
	//if((status & BUTTON_IO_PIN) && ((xTaskGetTickCountFromISR() - since) > 10)){
    //    since = xTaskGetTickCountFromISR();
    if(status & BUTTON_IO_PIN ){
		if(val == 0){
            gpio16_output_set(0);
			GPIO_OUTPUT_SET(LED_IO_NUM,1);
			val = 1;
		}else{
			gpio16_output_set(1);
            GPIO_OUTPUT_SET(LED_IO_NUM,0);
			val = 0;
		}
	}
	//should not add print in interruption, except that we want to debug something
    //printf("in io intr: 0X%08x\r\n",status);                    //WRITE ON SERIAL UART0
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS,status);       //CLEAR THE STATUS IN THE W1 INTERRUPT REGISTER

}
/*************************************************************************************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
***************************************************************************************************************************************************/
void ICACHE_FLASH_ATTR
user_init(void)
{
    printf("SDK version:%s\n", system_get_sdk_version());
/**********************Button and Interrupt*********************************************/
		printf("TEST TOGGLE ON GPIO15,YOU WILL SEE THE LED BLINKING ON IO15\n");
		GPIO_ConfigTypeDef io_out_conf;
		io_out_conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
		io_out_conf.GPIO_Mode = GPIO_Mode_Output;
		io_out_conf.GPIO_Pin = LED_IO_PIN ;
		io_out_conf.GPIO_Pullup = GPIO_PullUp_DIS;
		gpio_config(&io_out_conf);

	  GPIO_OUTPUT_SET(LED_IO_NUM,0);
		gpio16_output_conf();

	  printf("SETUP GPIO0 BUTTON INTERRUPT CONFIGURE..\r\n");
		GPIO_ConfigTypeDef io_in_conf;
		io_in_conf.GPIO_IntrType = GPIO_PIN_INTR_NEGEDGE;
		io_in_conf.GPIO_Mode = GPIO_Mode_Input;
		io_in_conf.GPIO_Pin = BUTTON_IO_PIN ;
		io_in_conf.GPIO_Pullup = GPIO_PullUp_EN;
		gpio_config(&io_in_conf);

	  gpio_intr_handler_register(io_intr_handler, NULL);
	  gpio16_output_set(1);
		ETS_GPIO_INTR_ENABLE();

/**************************************************************************************/

/* need to set opmode before you set config */
    wifi_set_opmode(STATION_MODE);

    xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);
		xTaskCreate(udp_task, "udp_task", 256, NULL, 2, NULL);

		uart_init_new();
		while(1)
		{
				uint8 alpha= 'A';
				char* test_str = "Your String Here\r\n";
				uart0_tx_buffer(&alpha, sizeof(alpha));
				uart0_tx_buffer(test_str, strlen(test_str));
				break;
		}
}
