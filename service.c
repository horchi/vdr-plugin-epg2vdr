/*
 * service.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "service.h"
#include "plgconfig.h"

//***************************************************************************
// Class cEpgTimer
//***************************************************************************

cEpgTimer::cEpgTimer(bool Instant, bool Pause, const cChannel* Channel)
   : cEpgTimer_Interface_V1(Instant, Pause, Channel)
{
   timerid = na; eventid = na;
   vdrName = 0; vdrUuid = 0;
   vdrRunning = no;
   stateInfo = 0;
   local = yes;
   type = ttRecord;
   action = '-';
   createTime = 0;
   modTime = 0;
}

cEpgTimer::~cEpgTimer()
{
   free(vdrUuid);
   free(vdrName);
   free(stateInfo);
}

void cEpgTimer::setState(char s, const char* info)
{
   state = s;
   free(stateInfo);
   stateInfo = 0;

   if (!isEmpty(info))
      stateInfo = strdup(info);
}

void cEpgTimer::setVdr(const char* name, const char* uuid, int running)
{
   local = yes;   // the default
   free(vdrUuid);
   free(vdrName);
   vdrName = strdup(name);

   if (!isEmpty(uuid))
      vdrUuid = strdup(uuid);

   vdrRunning = running;

   if (!isEmpty(vdrUuid) && strcmp(vdrUuid, Epg2VdrConfig.uuid) != 0)
      local = no;
}
