#ifdef __MACH__
#elif __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

#include <loragw_hal.h>
#include <loragw_aux.h>

#include <stdint.h>  /* C99 types */
#include <stdbool.h> /* bool type */
#include <stdio.h>   /* printf fprintf sprintf fopen fputs */

#include <string.h> /* memset */
#include <unistd.h> /* getopt access */
#include <stdlib.h> /* exit codes */
#include <signal.h> /* sigaction */
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "parson_config.h" /* parson json */



/* -------------------------------------------------------------------------- */
struct net_info_s net_info = {0};
int socket_fd = 0;
/* -------------------------------------------------------------------------- */
pthread_mutex_t work_mutex;
/* -------------------------------------------------------------------------- */
struct lgw_conf_board_s SX1301_configBoard = {0};  //configure struct of sx1301
struct lgw_tx_gain_lut_s SX1301_configTXlut = {0}; //configure struct of sx1301 tx
struct lgw_conf_rxrf_s SX1301_configRXrf[RF_Chain_SIZE] = {0};
struct lgw_conf_rxif_s SX1301_configRXif[Chan_MultiSF_SIZE] = {0};
struct lgw_conf_rxif_s SX1301_configLoraStd = {0};
struct lgw_conf_rxif_s SX1301_configFSK = {0};
struct lgw_pkt_rx_s rxpkt[TX_GAIN_LUT_SIZE] = {0}; /* array containing up to 16 inbound packets metadata */
/* -------------------------------------------------------------------------- */
static void sig_handler(int signo)
{
	lgw_stop();
	close(socket_fd);
	pthread_mutex_destroy(&work_mutex);

	MSG("INFO: Lora_bridge exited.\n");

	exit(0);
}
/* -------------------------------------------------------------------------- */

static void parseAllConfig(void)
{
	char global_conf_Path[] = "global_conf.json";
	char conf_obj_sx1301[] = "SX1301_conf";
	char conf_obj_net[] = "net_conf";

	JSON_Object *rootObj_sx1301 = parseJSON_getRootObject(global_conf_Path, conf_obj_sx1301);
	JSON_Object *rootObj_net = parseJSON_getRootObject(global_conf_Path, conf_obj_net);

	parseJSON_configBoard(rootObj_sx1301, &SX1301_configBoard);
	parseJSON_configLoraStd(rootObj_sx1301, &SX1301_configLoraStd);
	parseJSON_configFSK(rootObj_sx1301, &SX1301_configFSK);
	parseJSON_configTXlut(rootObj_sx1301, &SX1301_configTXlut);
	parseJSON_configRFchain(rootObj_sx1301, SX1301_configRXrf);
	paresJSON_configMultiSF(rootObj_sx1301, SX1301_configRXif);
	parseJSON_configNET(rootObj_net, &net_info);

	MSG("INFO: Parse all settings success\n");
}
/***********************************************************************************************/
/* config of gateway */

//rxrf
static void loraGW_RxrfSetconf(void)
{
	for (int i = 0; i < RF_Chain_SIZE; i++)
	{
		lgw_rxrf_setconf(i, SX1301_configRXrf[i]);
		// lgw_rxrf_setconf(RADIOB, SX1301_configRXrf[i]);
	}
}

//rxif
static void loraGW_RxifSetconf(void)
{
	for (int i = 0; i < Chan_MultiSF_SIZE; i++)
	{
		if (lgw_rxif_setconf(i, SX1301_configRXif[i]) != LGW_HAL_SUCCESS)
		{
			MSG("WARNING: invalid configuration for Lora multi-SF channel %d\n", i);
		}
	}
}

//lora std
static void loraGW_LoraStdSetconf(void)
{
	if (lgw_rxif_setconf(8, SX1301_configLoraStd) != LGW_HAL_SUCCESS)
	{
		MSG("WARNING: invalid configuration for Lora std channel \n");
	}
}

//fsk
static void loraGW_FSKSetconf(void)
{
	if (lgw_rxif_setconf(9, SX1301_configFSK) != LGW_HAL_SUCCESS)
	{
		MSG("WARNING: invalid configuration for FSK channel \n");
	}
}

//tx lut
static void loraGWTxLutSetconf(void)
{
	if (lgw_txgain_setconf(&SX1301_configTXlut) != LGW_HAL_SUCCESS)
	{
		MSG("WARNING: Failed to configure concentrator TX Gain LUT\n");
	}
}

//设置网关
static void loraGW_Init(void)
{
	/* board config */
	MSG("INFO: set all valuse...\n");
	lgw_board_setconf(SX1301_configBoard);
	//lgw_lbt ???
	loraGWTxLutSetconf();
	loraGW_RxrfSetconf();
	loraGW_RxifSetconf();
	loraGW_LoraStdSetconf();
	loraGW_FSKSetconf();
	
	MSG("INFO: All val are setted\n");
}

//启动网关
static STATUS loraGW_Start(void)
{
	int i = 5;
	char cmd[] = "./reset_lgw.sh start";

	do
	{
		--i;
		wait_ms(50);
		system(cmd);
		wait_ms(450);
		loraGW_Init();
	} while ((lgw_start() != LGW_HAL_SUCCESS) && (i));
	
	if (!i)
	{
		MSG("ERROR: Fail to init gateway\n");
		exit(1);
	}

	MSG("INFO: Success to start the gateway\n");
	return SUCCESS;
}

/*******************************************************************************/
//发送
STATUS loraGW_Send(char *msg_TX, uint16_t msg_Len)
{
	//the payload
	struct lgw_pkt_tx_s txpkt;
	uint8_t status_var;

	/* fill-up payload and parameters */
	memset(&txpkt, 0, sizeof(txpkt));
	txpkt.freq_hz = (uint32_t)((5000 * 1e5)); //472.3MHz
	txpkt.tx_mode = IMMEDIATE;
	txpkt.rf_chain = RADIOA;
	txpkt.rf_power = 20;
	txpkt.modulation = MOD_LORA;
	txpkt.bandwidth = BW_250KHZ;
	txpkt.datarate = DR_LORA_SF12;
	txpkt.coderate = CR_LORA_4_5;
	txpkt.invert_pol = false;
	txpkt.preamble = 8;
	txpkt.size = msg_Len;

	strcpy((char *)txpkt.payload, msg_TX); /* abc.. is for padding */

	if (lgw_send(txpkt) != LGW_HAL_SUCCESS)
	{
		MSG("ERR: Send data: \"%s\" fail\n", txpkt.payload);
		return FAIL;
	}
	MSG("INFO: Sending\"%s\"...\n", txpkt.payload);
	do
	{
		wait_ms(5);
		lgw_status(TX_STATUS, &status_var); /* get TX status */
	} while (status_var != TX_FREE);

	MSG("INFO: Send success\n");
	SHOW_LINE;

	return SUCCESS;
}

static int loraGW_Receive(void)
{

	int nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);

	if (nb_pkt)
	{
		MSG("INFO : Receive OK , pkg_num = %d\n", nb_pkt);
		for (int i = 0; i < nb_pkt; i++)
		{
			MSG("INFO : PKG[%d] : ", i);
			for (int j = 0; j < rxpkt[i].size; ++j)
			{
				MSG("%c", rxpkt[i].payload[j]);
			}
			MSG("\n");
		}
		SHOW_LINE;
	}

	if (nb_pkt == LGW_HAL_ERROR)
	{
		MSG("ERROR: failed packet fetch, exiting\n");
		return LGW_HAL_ERROR;
	}

	return nb_pkt;
}
/**********************************************************************************************/
void *thread_gw_send(void *socket_fd)
{
	struct sockaddr_in peerAddr;
	socklen_t peerLen;
	int n;
	char recvbuf[1024] = {0};

	while (1)
	{
		peerLen = sizeof(peerAddr);
		memset(recvbuf, 0, sizeof(recvbuf));
		n = recvfrom(*(int *)socket_fd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr_in *)&peerAddr, &peerLen);
		if (n <= 0)
		{
			continue;
		}
		else
		{
			//lock
			pthread_mutex_lock(&work_mutex);
			MSG("INFO : Start to send\n");
			int counter = 0;
			while (loraGW_Send(recvbuf, n) != SUCCESS)
			{
				counter++;
				if (counter == 2)
				{
					counter = 0;
					break;
				}
			}
			MSG("INFO : Send OK\n");
			//unlock
			pthread_mutex_unlock(&work_mutex);
		}
	}
}

void *thread_gw_recv(void *socket_fd)
{
	struct sockaddr_in addr_sevr;
	memset(&addr_sevr, 0, sizeof(addr_sevr));
	addr_sevr.sin_family = AF_INET;
	addr_sevr.sin_port = htons(net_info.port_up);
	addr_sevr.sin_addr.s_addr = inet_addr(net_info.ip_addr);

	while (1)
	{
		//check lock
		pthread_mutex_lock(&work_mutex);
		int len = loraGW_Receive();
		pthread_mutex_unlock(&work_mutex);

		if (!len)
		{
			wait_ms(10);
		}
		else
		{
			//sendto
			MSG("INFO : Send to Bridge\n");
			for (int i = 0; i < len; ++i)
			{
				sendto(*(int *)socket_fd, rxpkt[i].payload, rxpkt[i].size, 0, (struct sockaddr_in *)&addr_sevr, sizeof(addr_sevr));
			}
		}
	}
}

/**********************************************************************************************/
int main(void)
{
	/* configure signal handling */
	signal(SIGINT, sig_handler);
	/****************************/
	parseAllConfig(); //parse

	loraGW_Start(); //satrt
	/****************************/

	pthread_mutex_init(&work_mutex, NULL);

	pthread_t gw_send_t, gw_recv_t;

	/****************************/
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		MSG("socket build fail");
		exit(1);
	}

	struct sockaddr_in addr_sevr;
	memset(&addr_sevr, 0, sizeof(addr_sevr));
	addr_sevr.sin_family = AF_INET;
	addr_sevr.sin_port = htons(net_info.port_down);
	addr_sevr.sin_addr.s_addr = inet_addr(net_info.ip_addr);

	if (bind(socket_fd, (struct sockaddr *)&addr_sevr, sizeof(addr_sevr)) < 0)
	{
		MSG("ERROR: bind fail\n");
		exit(1);
	}

	if (pthread_create(&gw_recv_t, NULL, &thread_gw_recv, &socket_fd))
	{
		MSG("ERROR: thread_gw_recv fail to creat\n");
		exit(1);
	}

	if (pthread_create(&gw_send_t, NULL, &thread_gw_send, &socket_fd))
	{
		MSG("ERROR: thread_gw_send fail to creat\n");
		exit(1);
	}

	MSG("INFO: All threads created success.\n");
	SHOW_LINE;

	pthread_join(gw_recv_t, NULL);
	pthread_join(gw_send_t, NULL);

	/****************************/

	return SUCCESS;
}
