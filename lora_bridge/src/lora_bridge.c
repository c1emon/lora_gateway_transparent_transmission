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
#include <signal.h> /* sigaction */
#include <unistd.h> /* getopt access */
#include <stdlib.h> /* exit codes */
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "parson.h" /* parson json */

pthread_mutex_t work_mutex;

/* -------------------------------------------------------------------------- */
/* ----- type --------------------------------------------------------------- */
typedef enum status
{
	SUCCESS = 0,
	FAIL = -1
} STATUS;

const char global_conf_Path[] = "global_conf.json";
const char conf_obj_name[] = "SX1301_conf";

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */
#define SHOW_LINE MSG("\/* -------------------------------------------------------------------------- *\/\n")
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) //can not use for a pointer!

#define MSG(args...) fprintf(stderr, args) /* message that is destined to the user */
#define CHECK_NULL(a) (a == NULL)

#define TX_GAIN_LUT_SIZE 16
#define Chan_MultiSF_SIZE 8
#define RF_Chain_SIZE 2

#define RADIOA 0
#define RADIOB 1

/* -------------------------------------------------------------------------- */
struct lgw_conf_board_s SX1301_configBoard;  //configure struct of sx1301
struct lgw_tx_gain_lut_s SX1301_configTXlut; //configure struct of sx1301 tx
struct lgw_conf_rxrf_s SX1301_configRXrf[2];
struct lgw_conf_rxif_s SX1301_configRXif[Chan_MultiSF_SIZE];
struct lgw_conf_rxif_s SX1301_configLoraStd;
struct lgw_conf_rxif_s SX1301_configFSK;

struct lgw_pkt_rx_s rxpkt[16] = {0}; /* array containing up to 16 inbound packets metadata */
/* -------------------------------------------------------------------------- */
/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

static void sig_handler(int sigio)
{
	if (sigio == SIGQUIT)
	{
		quit_sig = 1;		
	}
	else if ((sigio == SIGINT) || (sigio == SIGTERM))
	{
		exit_sig = 1;
	}
}
/* -------------------------------------------------------------------------- */

//get object
static JSON_Object *parseJSON_getRootObject(const char *FILE, const char *name)
{

	JSON_Value *rootValue = NULL;
	JSON_Object *nameObject = NULL;

	/* try to parse JSON */
	rootValue = json_parse_file_with_comments(FILE);
	if (rootValue == NULL)
	{
		MSG("ERROR: %s is not a valid JSON file\n", FILE);
		exit(EXIT_FAILURE);
	}
	/* point to the gateway configuration object */
	nameObject = json_object_get_object(json_value_get_object(rootValue), name);
	if (nameObject == NULL)
	{
		MSG("INFO: the JSON object named %s does not exist\n", name);
		return NULL;
	}
	else
	{
		MSG("INFO: get the JSON object named %s, parsing SX1301 parameters\n", name);
		return nameObject;
	}
}

static JSON_Object *parseJSON_getObjectFromObject(const JSON_Object *rootObject, const char *name)
{

	JSON_Object *nameObject = NULL;

	/* point to the gateway configuration object */
	nameObject = json_object_get_object(rootObject, name);
	if (nameObject == NULL)
	{
		MSG("INFO: the JSON object named %s does not exist\n", name);
		return NULL;
	}
	else
	{
		MSG("INFO: get the JSON object named %s, parsing SX1301 parameters\n", name);
		return nameObject;
	}
}

static STATUS parseJSON_checkType(const JSON_Object *rootObject, const char *name, JSON_Value_Type JSONtype)
{

	JSON_Value *val = NULL;
	val = json_object_get_value(rootObject, name);
	if (json_value_get_type(val) != JSONtype)
		return FAIL;
	else
		return SUCCESS;
}

//get the board configure
static void parseJSON_configBoard(JSON_Object *SX1301_configObject)
{

	/* -------------------------------------------------------------------------- */
	memset(&SX1301_configBoard, 0, sizeof(SX1301_configBoard)); //init the struct

	JSON_Value *valueJSON = NULL;
	valueJSON = json_object_get_value(SX1301_configObject, "lorawan_public"); /* fetch value (if possible) */
	if (json_value_get_type(valueJSON) == JSONBoolean)
	{
		SX1301_configBoard.lorawan_public = (bool)json_value_get_boolean(valueJSON);
	}
	else
	{
		MSG("WARNING: Data type for lorawan_public seems wrong, please check\n");
		SX1301_configBoard.lorawan_public = false;
	}

	valueJSON = json_object_get_value(SX1301_configObject, "clksrc"); /* fetch value (if possible) */
	if (json_value_get_type(valueJSON) == JSONNumber)
	{
		SX1301_configBoard.clksrc = (uint8_t)json_value_get_number(valueJSON);
	}
	else
	{
		MSG("WARNING: Data type for clksrc seems wrong, please check\n");
		SX1301_configBoard.clksrc = 0;
	}
	MSG("INFO: lorawan_public %d, clksrc %d\n", SX1301_configBoard.lorawan_public, SX1301_configBoard.clksrc);
	/* all parameters parsed, submitting configuration to the HAL */
}

static void parseJSON_configTXlut(const JSON_Object *rootObject)
{

	char paramName[32];
	memset(&SX1301_configTXlut, 0, sizeof(SX1301_configTXlut));
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *TX_GAIN_LUT_Obj = NULL;

	//int i = 0;
	for (int i = 0; i < TX_GAIN_LUT_SIZE; i++)
	{
		snprintf(paramName, 32, "tx_lut_%i", i); /* compose parameter path inside JSON structure */

		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			SHOW_LINE;
			continue;
			//return;
		}

		TX_GAIN_LUT_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

		if (parseJSON_checkType(TX_GAIN_LUT_Obj, "pa_gain", JSONNumber) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", "pa_gain");
		}
		if (parseJSON_checkType(TX_GAIN_LUT_Obj, "mix_gain", JSONNumber) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", "mix_gain");
		}
		if (parseJSON_checkType(TX_GAIN_LUT_Obj, "rf_power", JSONNumber) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", "rf_power");
		}
		if (parseJSON_checkType(TX_GAIN_LUT_Obj, "dig_gain", JSONNumber) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", "dig_gain");
		}
		if (parseJSON_checkType(TX_GAIN_LUT_Obj, "dac_gain", JSONNumber) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", "dac_gain");
		}

		SX1301_configTXlut.size++;

		SX1301_configTXlut.lut[i].pa_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "pa_gain");
		SX1301_configTXlut.lut[i].mix_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "mix_gain");
		SX1301_configTXlut.lut[i].rf_power = (int)json_object_get_number(TX_GAIN_LUT_Obj, "rf_power");
		SX1301_configTXlut.lut[i].dig_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "dig_gain");
		SX1301_configTXlut.lut[i].dac_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "dac_gain");
		//the value does not exist will be set to 0

		MSG("SX1301_configTXlut.lut[%d].pa_gain   = %d\n", i, SX1301_configTXlut.lut[i].pa_gain);
		MSG("SX1301_configTXlut.lut[%d].mix_gain  = %d\n", i, SX1301_configTXlut.lut[i].mix_gain);
		MSG("SX1301_configTXlut.lut[%d].rf_power  = %d\n", i, SX1301_configTXlut.lut[i].rf_power);
		MSG("SX1301_configTXlut.lut[%d].dig_gain  = %d\n", i, SX1301_configTXlut.lut[i].dig_gain);
		MSG("SX1301_configTXlut.lut[%d].dac_gain  = %d\n", i, SX1301_configTXlut.lut[i].dac_gain);

		SHOW_LINE;
	}
	MSG("SX1301_configTXlut.size = %d\n", SX1301_configTXlut.size);
	SHOW_LINE;
}

static void parseJSON_configRFchain(const JSON_Object *rootObject)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *RFchain_Obj = NULL;

	for (int i = 0; i < RF_Chain_SIZE; i++)
	{
		memset(&SX1301_configRXrf[i], 0, sizeof(SX1301_configRXrf[0])); //init
		snprintf(&paramName, 32, "radio_%i", i);						//set value
		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			continue;
		}

		RFchain_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

		SX1301_configRXrf[i].enable = (bool)json_object_get_boolean(RFchain_Obj, "enable");
		MSG("INFO : radio_%d.enable = %d\n", i, SX1301_configRXrf[i].enable);
		if (SX1301_configRXrf[i].enable)
		{
			/*CHECK TYPE*/
			if (!strncmp(json_object_get_string(RFchain_Obj, "type"), "SX1255", 6))
			{
				SX1301_configRXrf[i].type = LGW_RADIO_TYPE_SX1255;
				MSG("INFO : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_SX1255\n", i);
			}
			else if (!strncmp(json_object_get_string(RFchain_Obj, "type"), "SX1257", 6))
			{
				SX1301_configRXrf[i].type = LGW_RADIO_TYPE_SX1257;
				MSG("INFO : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_SX1257\n", i);
			}
			else
			{
				SX1301_configRXrf[i].type = LGW_RADIO_TYPE_NONE;
				MSG("WARNING : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_NONE\n", i);
			}
			/*OTHERS*/
			SX1301_configRXrf[i].freq_hz = (uint32_t)json_object_get_number(RFchain_Obj, "freq");
			SX1301_configRXrf[i].rssi_offset = json_object_get_number(RFchain_Obj, "rssi_offset");
			SX1301_configRXrf[i].tx_enable = (bool)json_object_get_boolean(RFchain_Obj, "tx_enable");

			// add tx_notch_freq

			MSG("INFO : SX1301_configRXrf[%d].freq_hz = %d\n", i, SX1301_configRXrf[i].freq_hz);
			MSG("INFO : SX1301_configRXrf[%d].rssi_offset = %d\n", i, SX1301_configRXrf[i].rssi_offset);
			MSG("INFO : SX1301_configRXrf[%d].tx_enable = %d\n", i, SX1301_configRXrf[i].tx_enable);

			SHOW_LINE;
		}
		else
			SHOW_LINE;
	}
}

static void paresJSON_configMultiSF(const JSON_Object *rootObject)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *MultiSF_Obj = NULL;

	for (int i = 0; i < Chan_MultiSF_SIZE; i++)
	{
		memset(&SX1301_configRXif[i], 0, sizeof(SX1301_configRXif[0])); //init
		snprintf(&paramName, sizeof(paramName), "chan_multiSF_%i", i);  //set value

		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			continue;
		}

		MultiSF_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

		SX1301_configRXif[i].enable = (bool)json_object_get_boolean(MultiSF_Obj, "enable");
		MSG("INFO : chan_multiSF_%d.enable = %d\n", i, SX1301_configRXif[i].enable);
		if (SX1301_configRXif[i].enable)
		{
			SX1301_configRXif[i].rf_chain = (uint8_t)json_object_get_number(MultiSF_Obj, "radio");
			SX1301_configRXif[i].freq_hz = (int32_t)json_object_get_number(MultiSF_Obj, "if");
			MSG("INFO : SX1301_configRXif[%d].rf_chain = %d\n", i, SX1301_configRXif[i].rf_chain);
			MSG("INFO : SX1301_configRXif[%d].freq_hz  = %d\n", i, SX1301_configRXif[i].freq_hz);
			SHOW_LINE;
		}
		else
			SHOW_LINE;
	}
}

static void parseJSON_configLoraStd(const JSON_Object *rootObject)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *LoraStd_Obj = NULL;

	memset(&SX1301_configLoraStd, 0, sizeof(SX1301_configLoraStd));

	snprintf(&paramName, sizeof(paramName), "chan_Lora_std"); //set value

	if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
	{
		MSG("WARNING : object %s does not exist\n", paramName);
		return;
	}

	LoraStd_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

	SX1301_configLoraStd.enable = (bool)json_object_get_boolean(LoraStd_Obj, "enable");
	MSG("INFO : chan_Lora_std.enable = %d\n", SX1301_configLoraStd.enable);
	if (SX1301_configLoraStd.enable)
	{
		SX1301_configLoraStd.rf_chain = (uint8_t)json_object_get_number(LoraStd_Obj, "radio");
		SX1301_configLoraStd.freq_hz = (int32_t)json_object_get_number(LoraStd_Obj, "if");

		//set bandwidth
		switch ((uint32_t)json_object_get_number(LoraStd_Obj, "bandwidth"))
		{
		case 500000:
			SX1301_configLoraStd.bandwidth = BW_500KHZ;
			break;
		case 250000:
			SX1301_configLoraStd.bandwidth = BW_250KHZ;
			break;
		case 125000:
			SX1301_configLoraStd.bandwidth = BW_125KHZ;
			break;
		default:
			SX1301_configLoraStd.bandwidth = BW_UNDEFINED;
		}

		//set datarate(spread_factor)
		switch ((uint32_t)json_object_get_number(LoraStd_Obj, "spread_factor"))
		{
		case 7:
			SX1301_configLoraStd.datarate = DR_LORA_SF7;
			break;
		case 8:
			SX1301_configLoraStd.datarate = DR_LORA_SF8;
			break;
		case 9:
			SX1301_configLoraStd.datarate = DR_LORA_SF9;
			break;
		case 10:
			SX1301_configLoraStd.datarate = DR_LORA_SF10;
			break;
		case 11:
			SX1301_configLoraStd.datarate = DR_LORA_SF11;
			break;
		case 12:
			SX1301_configLoraStd.datarate = DR_LORA_SF12;
			break;
		default:
			SX1301_configLoraStd.datarate = DR_UNDEFINED;
		}

		//delete this
		// SX1301_configLoraStd.sync_word = (uint64_t)json_object_get_number(LoraStd_Obj, "sync_word");
		// SX1301_configLoraStd.sync_word_size = (uint8_t)json_object_get_number(LoraStd_Obj, "sync_word_size");

		MSG("INFO : SX1301_configLoraStd.rf_chain   = %d\n", SX1301_configLoraStd.rf_chain);
		MSG("INFO : SX1301_configLoraStd.freq_hz    = %d\n", SX1301_configLoraStd.freq_hz);
		MSG("INFO : SX1301_configLoraStd.bandwidth  = %d\n", SX1301_configLoraStd.bandwidth);
		MSG("INFO : SX1301_configLoraStd.datarate   = %d\n", SX1301_configLoraStd.datarate);
		MSG("INFO : SX1301_configLoraStd.sync_word  = %d\n", SX1301_configLoraStd.sync_word);
		MSG("INFO : SX1301_configLoraStd.sync_word_size  = %d\n", SX1301_configLoraStd.sync_word_size);

		SHOW_LINE;
	}
	else
		SHOW_LINE;
}

static void parseJSON_configFSK(const JSON_Object *rootObject)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *FSK_Obj = NULL;

	memset(&SX1301_configFSK, 0, sizeof(SX1301_configFSK));

	snprintf(&paramName, sizeof(paramName), "chan_FSK"); //set value

	if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
	{
		MSG("WARNING : object %s does not exist\n", paramName);
		return;
	}

	FSK_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

	SX1301_configFSK.enable = (bool)json_object_get_boolean(FSK_Obj, "enable");
	MSG("INFO : chan_FSK.enable = %d\n", SX1301_configFSK.enable);
	if (SX1301_configFSK.enable)
	{
		SX1301_configFSK.rf_chain = (uint8_t)json_object_get_number(FSK_Obj, "radio");
		SX1301_configFSK.freq_hz = (int32_t)json_object_get_number(FSK_Obj, "if");
		SX1301_configFSK.datarate = (uint32_t)json_object_get_number(FSK_Obj, "datarate");
		uint32_t bw = (uint32_t)json_object_get_number(FSK_Obj, "bandwidth");
		uint32_t fdev = (uint32_t)json_object_get_number(FSK_Obj, "freq_deviation");
		if ((bw == 0) && (fdev != 0))
		{
			bw = 2 * fdev + SX1301_configFSK.datarate;
		}
		if (bw == 0)
			SX1301_configFSK.bandwidth = BW_UNDEFINED;
		else if (bw <= 7800)
			SX1301_configFSK.bandwidth = BW_7K8HZ;
		else if (bw <= 15600)
			SX1301_configFSK.bandwidth = BW_15K6HZ;
		else if (bw <= 31200)
			SX1301_configFSK.bandwidth = BW_31K2HZ;
		else if (bw <= 62500)
			SX1301_configFSK.bandwidth = BW_62K5HZ;
		else if (bw <= 125000)
			SX1301_configFSK.bandwidth = BW_125KHZ;
		else if (bw <= 250000)
			SX1301_configFSK.bandwidth = BW_250KHZ;
		else if (bw <= 500000)
			SX1301_configFSK.bandwidth = BW_500KHZ;
		else
			SX1301_configFSK.bandwidth = BW_UNDEFINED;

		//SX1301_configFSK.sync_word 		= (uint64_t)json_object_get_number(FSK_Obj, "sync_word");
		//SX1301_configFSK.sync_word_size = (uint8_t) json_object_get_number(FSK_Obj, "sync_word_size");

		MSG("INFO : SX1301_configFSK.rf_chain   = %d\n", SX1301_configFSK.rf_chain);
		MSG("INFO : SX1301_configFSK.freq_hz    = %d\n", SX1301_configFSK.freq_hz);
		MSG("INFO : SX1301_configFSK.bandwidth  = %d\n", SX1301_configFSK.bandwidth);
		MSG("INFO : SX1301_configFSK.datarate   = %d\n", SX1301_configFSK.datarate);
		//MSG("INFO : SX1301_configFSK.sync_word  = %d\n", SX1301_configFSK.sync_word);
		//MSG("INFO : SX1301_configFSK.sync_word_size  = %d\n", SX1301_configFSK.sync_word_size);

		SHOW_LINE;
	}
	else
		SHOW_LINE;
}

static void parseAllConfig(void)
{
	JSON_Object *rootObj = parseJSON_getRootObject(global_conf_Path, conf_obj_name);
	parseJSON_configBoard(rootObj);
	parseJSON_configLoraStd(rootObj);
	parseJSON_configFSK(rootObj);
	parseJSON_configRFchain(rootObj);
	paresJSON_configMultiSF(rootObj);
	parseJSON_configTXlut(rootObj);
}
/***********************************************************************************************/
/* config of gateway */
//设置网关
static void loraGW_Init(void)
{
	/* board config */
	lgw_board_setconf(SX1301_configBoard);
}

//rxrf
static void loraGW_RxrfSetconf(void)
{
	lgw_rxrf_setconf(RADIOA, SX1301_configRXrf[0]);
	lgw_rxrf_setconf(RADIOB, SX1301_configRXrf[1]);
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

//启动网关
static STATUS loraGW_Start(void)
{
	int s, j = 5;
	MSG("INFO : set all valuse...\n");
	loraGW_Init();
	loraGW_RxrfSetconf();
	loraGW_RxifSetconf();
	loraGW_LoraStdSetconf();
	loraGW_FSKSetconf();
	loraGWTxLutSetconf();

	do
	{
		s = lgw_start();
		MSG("statring...\n");
		j--;
		wait_ms(1000);
	} while ((s != LGW_HAL_SUCCESS) && j);

	if (s == LGW_HAL_SUCCESS)
	{
		MSG("INFO: success to start \n");
		return SUCCESS;
	}
	else
	{
		MSG("ERROR: fail to start\n");
		return FAIL;
	}
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
		MSG("ERR:发送数据\"%s\" 失败\n", txpkt.payload);
		return FAIL;
	}
	MSG("INFO:正在发送\"%s\"...", txpkt.payload);
	do
	{
		wait_ms(5);
		lgw_status(TX_STATUS, &status_var); /* get TX status */
	} while (status_var != TX_FREE);
	MSG("完成\n");
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
	addr_sevr.sin_port = htons(8011);
	addr_sevr.sin_addr.s_addr = inet_addr("127.0.0.1");

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
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	/****************************/
	parseAllConfig();			   //parse
	if (loraGW_Start() != SUCCESS) //satrt
		return -1;

	/****************************/

	pthread_mutex_init(&work_mutex, NULL);

	pthread_t gw_send_t, gw_recv_t;

	/****************************/
	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		perror("socket build fail");
		exit(1);
	}

	struct sockaddr_in addr_sevr;
	memset(&addr_sevr, 0, sizeof(addr_sevr));
	addr_sevr.sin_family = AF_INET;
	addr_sevr.sin_port = htons(8010);
	addr_sevr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(socket_fd, (struct sockaddr *)&addr_sevr, sizeof(addr_sevr)) < 0)
	{
		perror("bind fail");
		exit(1);
	}

	if (pthread_create(&gw_recv_t, NULL, &thread_gw_recv, &socket_fd))
	{
		perror("thread_gw_recv creat err");
		exit(1);
	}

	if (pthread_create(&gw_send_t, NULL, &thread_gw_send, &socket_fd))
	{
		perror("thread_gw_send creat err");
		exit(1);
	}

	pthread_join(gw_recv_t, NULL);
	pthread_join(gw_send_t, NULL);

	/****************************/
	if ((quit_sig == 1) || (exit_sig == 1))
	{
		lgw_stop();
		close(socket_fd);
		pthread_mutex_destroy(&work_mutex);

		printf("退出\n");

		exit(1);
		return SUCCESS;
	}

	/* clean up before leaving */

	return SUCCESS;
}
