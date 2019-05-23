#ifndef parson_config_H
#define parson_config_H

#include "parson.h"


#include <stdint.h>  /* C99 types */
#include <stdbool.h> /* bool type */
#include <stdio.h>   /* printf fprintf sprintf fopen fputs */

#include <string.h> /* memset */
#include <unistd.h> /* getopt access */
#include <stdlib.h> /* exit codes */

#include <loragw_hal.h>
#include <loragw_aux.h>

#define TX_GAIN_LUT_SIZE 16
#define Chan_MultiSF_SIZE 8
#define RF_Chain_SIZE 2

#define RADIOA 0
#define RADIOB 1


struct net_info_s
{
    char ip_addr[16];
    long port_up;
    long port_down;
};


typedef enum status
{
	SUCCESS = 0,
	FAIL = -1
} STATUS;

#define SHOW_LINE MSG("* -------------------------------------------------------------------------- *\n")
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) //can not use for a pointer!

#define MSG(args...) fprintf(stderr, args) /* message that is destined to the user */
#define CHECK_NULL(a) (a == NULL)

JSON_Object *parseJSON_getRootObject(const char *FILE, const char *name);
JSON_Object *parseJSON_getObjectFromObject(const JSON_Object *rootObject, const char *name);
void parseJSON_configBoard(JSON_Object *SX1301_configObject ,struct lgw_conf_board_s *pSX1301_configBoard);
void parseJSON_configTXlut(const JSON_Object *rootObject, struct lgw_tx_gain_lut_s *pSX1301_configTXlut);
void parseJSON_configRFchain(const JSON_Object *rootObject, struct lgw_conf_rxrf_s pSX1301_configRXrf[]);
void paresJSON_configMultiSF(const JSON_Object *rootObject, struct lgw_conf_rxif_s pSX1301_configRXif[]);
void parseJSON_configLoraStd(const JSON_Object *rootObject, struct lgw_conf_rxif_s *pSX1301_configLoraStd);
void parseJSON_configFSK(const JSON_Object *rootObject, struct lgw_conf_rxif_s *pSX1301_configFSK);

void parseJSON_configNET(const JSON_Object *rootObject, struct net_info_s *pnet_info);


#endif
