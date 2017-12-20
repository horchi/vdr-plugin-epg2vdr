/*
 * epg2vdr.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __EPG2VDR_H
#define __EPG2VDR_H

#include <list>

#include <vdr/plugin.h>
#include "plgconfig.h"
#include "service.h"
#include "HISTORY.h"

#include "lib/db.h"

class cUpdate;
class cEpg2VdrEpgHandler;
extern cUpdate* oUpdate;

//***************************************************************************
// Constants
//***************************************************************************

static const char* DESCRIPTION   = trNOOP("epg2vdr plugin");
static const char* MAINMENUENTRY = tr("EPG and Timer Service");

//***************************************************************************
// cPluginEPG2VDR
//***************************************************************************

class cPluginEPG2VDR : public cPlugin
{
   public:

      cPluginEPG2VDR(void);
      virtual ~cPluginEPG2VDR();

      virtual const char* Version(void)          { return VERSION; }
      virtual const char* VersionDate(void)      { return VERSION_DATE; }
      virtual const char* Description(void)      { return tr(DESCRIPTION); }
      virtual const char* CommandLineHelp(void);
      virtual bool Service(const char* id, void* data);
      virtual const char** SVDRPHelpPages(void);
      virtual cString SVDRPCommand(const char* Cmd, const char* Option, int& ReplyCode);
      virtual bool Initialize(void);
      virtual bool Start(void);
      virtual cString Active(void);
      virtual const char* MainMenuEntry(void)
      { return Epg2VdrConfig.mainmenuVisible ? MAINMENUENTRY : 0; }
      virtual cOsdObject* MainMenuAction(void);
      virtual cMenuSetupPage* SetupMenu(void);
      virtual bool SetupParse(const char* Name, const char* Value);
      virtual void Stop();
      virtual void DisplayMessage(const char* s);
      virtual time_t WakeupTime(void);

   protected:

      int initDb();
      int exitDb();

      int timerService(cEpgTimer_Service_V1* ts);
      int recordingDetails(cEpgRecording_Details_Service_V1* rd);

   private:

      int pluginInitialized;
      cDbConnection* connection;
      cDbTable* timerDb;
      cDbTable* vdrDb;
      cDbTable* useeventsDb;
      cDbTable* recordingListDb;
      cDbStatement* selectTimers;
      cDbStatement* selectEventById;
      cMutex mutexTimerService;
      cMutex mutexServiceWithDb;
};

//***************************************************************************
#endif // EPG2VDR_H
