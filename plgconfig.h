/*
 * plgconfig.h:
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: config.h,v 1.2 2012/10/26 08:44:13 wendel Exp $
 */

#pragma once

#include "lib/config.h"

//***************************************************************************
// Config
//***************************************************************************

struct cEpg2VdrConfig : public cEpgConfig
{
   public:

      cEpg2VdrConfig();

      int mainmenuVisible;
      int mainmenuFullupdate;
      int activeOnEpgd;
      int scheduleBoot;
      int blacklist;            // to enable noepg feature
      int masterMode;
      int shareInWeb;           // 'am verbund teilnehmen'
      int createTimerLocal;
      int useCommonRecFolder;   // NAS
      int xchgOkBlue;

      int replaceScheduleMenu;
      int replaceTimerMenu;
      int userIndex;
      char user[100+TB];
      int showEmptyChannels;
      int extendedEpgData2Aux;
      int switchTimerNotifyTime;
      int closeOnSwith {yes};
};

extern cEpg2VdrConfig Epg2VdrConfig;
