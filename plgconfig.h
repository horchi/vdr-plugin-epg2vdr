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

      cEpg2VdrConfig() : cEpgConfig() {};

      int mainmenuVisible {true};
      int mainmenuFullupdate {0};
      int activeOnEpgd {false};
      int scheduleBoot {false};
      int blacklist {false};            // to enable noepg feature
      int masterMode {0};
      int shareInWeb {true};            // 'am verbund teilnehmen'
      int createTimerLocal {false};
      int useCommonRecFolder {true};    // NAS
      int xchgOkBlue {false};

      int replaceScheduleMenu {false};
      int replaceTimerMenu {false};
      int userIndex {0};
      char user[100+TB] {};
      int showEmptyChannels {false};
      int extendedEpgData2Aux {false};
      int switchTimerNotifyTime {0};
      int closeOnSwith {false};
};

extern cEpg2VdrConfig Epg2VdrConfig;
