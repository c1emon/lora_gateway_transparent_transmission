#include "parson_config.h"

#define ISIPINRANGE(n) (((n) >= 0) && ((n) <= 255))
#define ISPORT(n) (((n) >= 0) && ((n) <= 65535))

static STATUS parseJSON_checkType(const JSON_Object *rootObject, const char *name, JSON_Value_Type JSONtype)
{

	JSON_Value *val = NULL;
	val = json_object_get_value(rootObject, name);
	if (json_value_get_type(val) != JSONtype)
		return FAIL;
	else
		return SUCCESS;
}

//get object
JSON_Object *parseJSON_getRootObject(const char *FILE, const char *name)
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

JSON_Object *parseJSON_getObjectFromObject(const JSON_Object *rootObject, const char *name)
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

//get the board configure
void parseJSON_configBoard(JSON_Object *SX1301_configObject, struct lgw_conf_board_s *pSX1301_configBoard)
{

	/* -------------------------------------------------------------------------- */
	// memset(pSX1301_configBoard, 0, sizeof(lgw_conf_board)); //init the struct

	JSON_Value *valueJSON = NULL;
	valueJSON = json_object_get_value(SX1301_configObject, "lorawan_public"); /* fetch value (if possible) */
	if (json_value_get_type(valueJSON) == JSONBoolean)
	{
		pSX1301_configBoard->lorawan_public = (bool)json_value_get_boolean(valueJSON);
	}
	else
	{
		MSG("WARNING: Data type for lorawan_public seems wrong, please check\n");
		pSX1301_configBoard->lorawan_public = false;
	}

	valueJSON = json_object_get_value(SX1301_configObject, "clksrc"); /* fetch value (if possible) */
	if (json_value_get_type(valueJSON) == JSONNumber)
	{
		pSX1301_configBoard->clksrc = (uint8_t)json_value_get_number(valueJSON);
	}
	else
	{
		MSG("WARNING: Data type for clksrc seems wrong, please check\n");
		pSX1301_configBoard->clksrc = 0;
	}
	MSG("INFO: lorawan_public %d, clksrc %d\n", pSX1301_configBoard->lorawan_public, pSX1301_configBoard->clksrc);
	/* all parameters parsed, submitting configuration to the HAL */
}

void parseJSON_configTXlut(const JSON_Object *rootObject, struct lgw_tx_gain_lut_s *pSX1301_configTXlut)
{

	char paramName[32];
	// memset(&SX1301_configTXlut, 0, sizeof(SX1301_configTXlut));
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *TX_GAIN_LUT_Obj = NULL;

	for (int i = 0; i < TX_GAIN_LUT_SIZE; i++)
	{
		snprintf(paramName, 32, "tx_lut_%i", i); /* compose parameter path inside JSON structure */

		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			SHOW_LINE;
			continue;
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

		pSX1301_configTXlut->size++;

		pSX1301_configTXlut->lut[i].pa_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "pa_gain");
		pSX1301_configTXlut->lut[i].mix_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "mix_gain");
		pSX1301_configTXlut->lut[i].rf_power = (int)json_object_get_number(TX_GAIN_LUT_Obj, "rf_power");
		pSX1301_configTXlut->lut[i].dig_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "dig_gain");
		pSX1301_configTXlut->lut[i].dac_gain = (int)json_object_get_number(TX_GAIN_LUT_Obj, "dac_gain");
		//the value does not exist will be set to 0

		MSG("SX1301_configTXlut.lut[%d].pa_gain   = %d\n", i, pSX1301_configTXlut->lut[i].pa_gain);
		MSG("SX1301_configTXlut.lut[%d].mix_gain  = %d\n", i, pSX1301_configTXlut->lut[i].mix_gain);
		MSG("SX1301_configTXlut.lut[%d].rf_power  = %d\n", i, pSX1301_configTXlut->lut[i].rf_power);
		MSG("SX1301_configTXlut.lut[%d].dig_gain  = %d\n", i, pSX1301_configTXlut->lut[i].dig_gain);
		MSG("SX1301_configTXlut.lut[%d].dac_gain  = %d\n", i, pSX1301_configTXlut->lut[i].dac_gain);

		SHOW_LINE;
	}

	MSG("SX1301_configTXlut.size = %d\n", pSX1301_configTXlut->size);
	SHOW_LINE;
}

void parseJSON_configRFchain(const JSON_Object *rootObject, struct lgw_conf_rxrf_s pSX1301_configRXrf[])
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *RFchain_Obj = NULL;

	for (int i = 0; i < RF_Chain_SIZE; i++)
	{
		//memset(&SX1301_configRXrf[i], 0, sizeof(SX1301_configRXrf[0])); //init
		snprintf(&paramName, 32, "radio_%i", i); //set value
		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			continue;
		}

		RFchain_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

		pSX1301_configRXrf[i].enable = (bool)json_object_get_boolean(RFchain_Obj, "enable");
		MSG("INFO : radio_%d.enable = %d\n", i, pSX1301_configRXrf[i].enable);
		if (pSX1301_configRXrf[i].enable)
		{
			/*CHECK TYPE*/
			if (!strncmp(json_object_get_string(RFchain_Obj, "type"), "SX1255", 6))
			{
				pSX1301_configRXrf[i].type = LGW_RADIO_TYPE_SX1255;
				MSG("INFO : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_SX1255\n", i);
			}
			else if (!strncmp(json_object_get_string(RFchain_Obj, "type"), "SX1257", 6))
			{
				pSX1301_configRXrf[i].type = LGW_RADIO_TYPE_SX1257;
				MSG("INFO : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_SX1257\n", i);
			}
			else
			{
				pSX1301_configRXrf[i].type = LGW_RADIO_TYPE_NONE;
				MSG("WARNING : SX1301_configRXrf[%d].type = LGW_RADIO_TYPE_NONE\n", i);
			}
			/*OTHERS*/
			pSX1301_configRXrf[i].freq_hz = (uint32_t)json_object_get_number(RFchain_Obj, "freq");
			pSX1301_configRXrf[i].rssi_offset = json_object_get_number(RFchain_Obj, "rssi_offset");
			pSX1301_configRXrf[i].tx_enable = (bool)json_object_get_boolean(RFchain_Obj, "tx_enable");
			pSX1301_configRXrf[i].tx_notch_freq = (uint32_t)json_object_get_number(RFchain_Obj, "tx_notch_freq");

			// add tx_notch_freq

			MSG("INFO : SX1301_configRXrf[%d].freq_hz = %d\n", i, pSX1301_configRXrf[i].freq_hz);
			MSG("INFO : SX1301_configRXrf[%d].rssi_offset = %d\n", i, pSX1301_configRXrf[i].rssi_offset);
			MSG("INFO : SX1301_configRXrf[%d].tx_enable = %d\n", i, pSX1301_configRXrf[i].tx_enable);
			MSG("INFO : SX1301_configRXrf[%d].tx_notch_freq = %d\n", i, pSX1301_configRXrf[i].tx_notch_freq);

			SHOW_LINE;
		}
		else
			SHOW_LINE;
	}
}

void paresJSON_configMultiSF(const JSON_Object *rootObject, struct lgw_conf_rxif_s pSX1301_configRXif[])
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *MultiSF_Obj = NULL;

	for (int i = 0; i < Chan_MultiSF_SIZE; i++)
	{
		// memset(&SX1301_configRXif[i], 0, sizeof(SX1301_configRXif[0])); //init
		snprintf(&paramName, sizeof(paramName), "chan_multiSF_%i", i); //set value

		if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
		{
			MSG("WARNING : object %s does not exist\n", paramName);
			continue;
		}

		MultiSF_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

		pSX1301_configRXif[i].enable = (bool)json_object_get_boolean(MultiSF_Obj, "enable");
		MSG("INFO : chan_multiSF_%d.enable = %d\n", i, pSX1301_configRXif[i].enable);
		if (pSX1301_configRXif[i].enable)
		{
			pSX1301_configRXif[i].rf_chain = (uint8_t)json_object_get_number(MultiSF_Obj, "radio");
			pSX1301_configRXif[i].freq_hz = (int32_t)json_object_get_number(MultiSF_Obj, "if");
			MSG("INFO : SX1301_configRXif[%d].rf_chain = %d\n", i, pSX1301_configRXif[i].rf_chain);
			MSG("INFO : SX1301_configRXif[%d].freq_hz  = %d\n", i, pSX1301_configRXif[i].freq_hz);
			SHOW_LINE;
		}
		else
			SHOW_LINE;
	}
}

void parseJSON_configLoraStd(const JSON_Object *rootObject, struct lgw_conf_rxif_s *pSX1301_configLoraStd)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *LoraStd_Obj = NULL;

	// memset(&SX1301_configLoraStd, 0, sizeof(SX1301_configLoraStd));

	snprintf(&paramName, sizeof(paramName), "chan_Lora_std"); //set value

	if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
	{
		MSG("WARNING : object %s does not exist\n", paramName);
		return;
	}

	LoraStd_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

	pSX1301_configLoraStd->enable = (bool)json_object_get_boolean(LoraStd_Obj, "enable");
	MSG("INFO : chan_Lora_std.enable = %d\n", pSX1301_configLoraStd->enable);
	if (pSX1301_configLoraStd->enable)
	{
		pSX1301_configLoraStd->rf_chain = (uint8_t)json_object_get_number(LoraStd_Obj, "radio");
		pSX1301_configLoraStd->freq_hz = (int32_t)json_object_get_number(LoraStd_Obj, "if");

		//set bandwidth
		switch ((uint32_t)json_object_get_number(LoraStd_Obj, "bandwidth"))
		{
		case 500000:
			pSX1301_configLoraStd->bandwidth = BW_500KHZ;
			break;
		case 250000:
			pSX1301_configLoraStd->bandwidth = BW_250KHZ;
			break;
		case 125000:
			pSX1301_configLoraStd->bandwidth = BW_125KHZ;
			break;
		default:
			pSX1301_configLoraStd->bandwidth = BW_UNDEFINED;
		}

		//set datarate(spread_factor)
		switch ((uint32_t)json_object_get_number(LoraStd_Obj, "spread_factor"))
		{
		case 7:
			pSX1301_configLoraStd->datarate = DR_LORA_SF7;
			break;
		case 8:
			pSX1301_configLoraStd->datarate = DR_LORA_SF8;
			break;
		case 9:
			pSX1301_configLoraStd->datarate = DR_LORA_SF9;
			break;
		case 10:
			pSX1301_configLoraStd->datarate = DR_LORA_SF10;
			break;
		case 11:
			pSX1301_configLoraStd->datarate = DR_LORA_SF11;
			break;
		case 12:
			pSX1301_configLoraStd->datarate = DR_LORA_SF12;
			break;
		default:
			pSX1301_configLoraStd->datarate = DR_UNDEFINED;
		}

		//delete this
		// SX1301_configLoraStd.sync_word = (uint64_t)json_object_get_number(LoraStd_Obj, "sync_word");
		// SX1301_configLoraStd.sync_word_size = (uint8_t)json_object_get_number(LoraStd_Obj, "sync_word_size");

		MSG("INFO : pSX1301_configLoraStd->rf_chain   = %d\n", pSX1301_configLoraStd->rf_chain);
		MSG("INFO : pSX1301_configLoraStd->freq_hz    = %d\n", pSX1301_configLoraStd->freq_hz);
		MSG("INFO : pSX1301_configLoraStd->bandwidth  = %d\n", pSX1301_configLoraStd->bandwidth);
		MSG("INFO : pSX1301_configLoraStd->datarate   = %d\n", pSX1301_configLoraStd->datarate);
		MSG("INFO : pSX1301_configLoraStd->sync_word  = %d\n", pSX1301_configLoraStd->sync_word);
		MSG("INFO : pSX1301_configLoraStd->sync_word_size  = %d\n", pSX1301_configLoraStd->sync_word_size);

		SHOW_LINE;
	}
	else
		SHOW_LINE;
}

void parseJSON_configFSK(const JSON_Object *rootObject, struct lgw_conf_rxif_s *pSX1301_configFSK)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));
	JSON_Object *FSK_Obj = NULL;

	snprintf(&paramName, sizeof(paramName), "chan_FSK"); //set value

	if (parseJSON_checkType(rootObject, paramName, JSONObject) != SUCCESS)
	{
		MSG("WARNING : object %s does not exist\n", paramName);
		return;
	}

	FSK_Obj = parseJSON_getObjectFromObject(rootObject, paramName);

	pSX1301_configFSK->enable = (bool)json_object_get_boolean(FSK_Obj, "enable");
	MSG("INFO : chan_FSK.enable = %d\n", pSX1301_configFSK->enable);
	if (pSX1301_configFSK->enable)
	{
		pSX1301_configFSK->rf_chain = (uint8_t)json_object_get_number(FSK_Obj, "radio");
		pSX1301_configFSK->freq_hz = (int32_t)json_object_get_number(FSK_Obj, "if");
		pSX1301_configFSK->datarate = (uint32_t)json_object_get_number(FSK_Obj, "datarate");
		uint32_t bw = (uint32_t)json_object_get_number(FSK_Obj, "bandwidth");
		uint32_t fdev = (uint32_t)json_object_get_number(FSK_Obj, "freq_deviation");
		if ((bw == 0) && (fdev != 0))
		{
			bw = 2 * fdev + pSX1301_configFSK->datarate;
		}
		if (bw == 0)
			pSX1301_configFSK->bandwidth = BW_UNDEFINED;
		else if (bw <= 7800)
			pSX1301_configFSK->bandwidth = BW_7K8HZ;
		else if (bw <= 15600)
			pSX1301_configFSK->bandwidth = BW_15K6HZ;
		else if (bw <= 31200)
			pSX1301_configFSK->bandwidth = BW_31K2HZ;
		else if (bw <= 62500)
			pSX1301_configFSK->bandwidth = BW_62K5HZ;
		else if (bw <= 125000)
			pSX1301_configFSK->bandwidth = BW_125KHZ;
		else if (bw <= 250000)
			pSX1301_configFSK->bandwidth = BW_250KHZ;
		else if (bw <= 500000)
			pSX1301_configFSK->bandwidth = BW_500KHZ;
		else
			pSX1301_configFSK->bandwidth = BW_UNDEFINED;

		//SX1301_configFSK.sync_word 		= (uint64_t)json_object_get_number(FSK_Obj, "sync_word");
		//SX1301_configFSK.sync_word_size = (uint8_t) json_object_get_number(FSK_Obj, "sync_word_size");

		MSG("INFO : SX1301_configFSK.rf_chain   = %d\n", pSX1301_configFSK->rf_chain);
		MSG("INFO : SX1301_configFSK.freq_hz    = %d\n", pSX1301_configFSK->freq_hz);
		MSG("INFO : SX1301_configFSK.bandwidth  = %d\n", pSX1301_configFSK->bandwidth);
		MSG("INFO : SX1301_configFSK.datarate   = %d\n", pSX1301_configFSK->datarate);
		//MSG("INFO : SX1301_configFSK.sync_word  = %d\n", SX1301_configFSK.sync_word);
		//MSG("INFO : SX1301_configFSK.sync_word_size  = %d\n", SX1301_configFSK.sync_word_size);

		SHOW_LINE;
	}
	else
		SHOW_LINE;
}

static int isIPAddress(char *raw_str)
{

	char str[16] = {0};
	char *t;

	if ((strlen(raw_str) > 0) && (strlen(raw_str) <= 16))
	{
		strcpy(str, raw_str);
	}
	else
	{
		MSG("ERROR: Not a vailed ip address\n");
		return 1;
	}

	if (ISIPINRANGE(atoi(strtok(str, "."))))
	{
		while (t = strtok(NULL, "."))
		{
			if (ISIPINRANGE(atoi(t)))
			{
				//MSG("ip = %s", t);
			}
			else
			{
				return 1;
			}
			
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

void parseJSON_configNET(const JSON_Object *rootObject, struct net_info_s *pnet_info)
{

	char paramName[32];
	memset(&paramName, 0, sizeof(paramName));

	snprintf(&paramName, sizeof(paramName), "ip_addr"); //set value

	if (parseJSON_checkType(rootObject, paramName, JSONString) != SUCCESS)
	{
		MSG("ERROR: The json object ip_addr %s is not a right type\n", paramName);
		return;
	}

	
	if (isIPAddress(json_object_get_string(rootObject, paramName)))
	{
		MSG("ERROR: Not a vailed ip address\n");
		return;
	}
	strcpy(pnet_info->ip_addr, json_object_get_string(rootObject, paramName));

	if (ISPORT(json_object_get_number(rootObject, "port_down")))
	{
		pnet_info->port_down = (int32_t)json_object_get_number(rootObject, "port_down");
	}
	if (ISPORT(json_object_get_number(rootObject, "port_up")))
	{
		pnet_info->port_up = (int32_t)json_object_get_number(rootObject, "port_up");
	}

	MSG("INFO: Ip address : %s\n", pnet_info->ip_addr);
	MSG("INFO: port_down = %d\n", pnet_info->port_down);
	MSG("INFO: port_up = %d\n", pnet_info->port_up);
}