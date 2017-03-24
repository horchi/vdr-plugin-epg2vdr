/*
 * config.c:
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "plgconfig.h"

cEpg2VdrConfig Epg2VdrConfig;

//***************************************************************************
// cEpg2VdrConfig
//***************************************************************************

cEpg2VdrConfig::cEpg2VdrConfig()
   : cEpgConfig()
{
   mainmenuVisible = yes;
   mainmenuFullupdate = 0;
   blacklist = no;

   activeOnEpgd = no;
   scheduleBoot = no;
   masterMode = 0;
   shareInWeb = yes;
   createTimerLocal = no;
   useCommonRecFolder = yes;
   xchgOkBlue = no;

   replaceScheduleMenu = no;
   replaceTimerMenu = no;
   userIndex = 0;
   *user = 0;
   showEmptyChannels = no;
   extendedEpgData2Aux = no;
}
