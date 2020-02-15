/*
 * update.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __UPDATE_H
#define __UPDATE_H

#include <mysql/mysql.h>
#include <queue>

#include <vdr/status.h>

#include "lib/common.h"
#include "lib/db.h"
#include "lib/epgservice.h"
#include "lib/vdrlocks.h"
#include "lib/xml.h"

#include "epg2vdr.h"
#include "parameters.h"

#define EPGDNAME "epgd"

#if defined(APIVERSNUM) && APIVERSNUM < 20304
#error "VDR-2.3.4 API version or greater is required!"
#endif

//***************************************************************************
// Running Recording
//***************************************************************************

class cRunningRecording : public cListObject
{
   public:

      cRunningRecording(const cTimer* t, long did = na)
      {
         id = t->Id();

         if (remote)
            free(remote);

         remote = t->Remote() ? strdup(t->Remote()) : nullptr;
         doneid = did;
         lastBreak = 0;
         info = 0;

         finished = no;
         failed = no;

         // copy until timer get waste ..

         aux = strdup(t->Aux() ? t->Aux() : "");
      }

      ~cRunningRecording()
      {
         free(aux);
         free(info);
      }

      void setInfo(const char* i) { info = strdup(i); }

      // data

      int id = 0;           // use timer->Id(), timer->Remote and GetById instead of pointer to a timer
      char* remote {nullptr};
      time_t lastBreak;
      int finished;
      int failed;
      long doneid;
      char* aux {nullptr};
      char* info {nullptr};
};

//***************************************************************************
// Event Details (Recording Info Files)
//***************************************************************************

class cEventDetails
{
   public:

      cEventDetails()     { changes = 0; }
      ~cEventDetails()    {}

      int getChanges()    { return changes; }
      void clearChanges() { changes = 0; }

      void setValue(const char* name, const char* value);
      void setValue(const char* name, int value);

      int storeToFs(const char* path);
      int loadFromFs(const char* path, cDbRow* row, int doClear = yes);
      int updateByRow(cDbRow* row);
      int updateToRow(cDbRow* row);

      static int row2Xml(cDbRow* row, cXml* xml);

   private:

      int changes;
      std::map<std::string, std::string> values;

      static const char* fields[];
};

//***************************************************************************
// Update
//***************************************************************************

class cUpdate : public cThread, public cStatus, public cParameters
{
   public:

      enum MasterMode
      {
         mmAuto,
         mmYes,
         mmNo,

         mmCount
      };

      enum Event
      {
         evtUnknown = na,
         evtSwitchTimer,
      };

      cUpdate(cPluginEPG2VDR* aPlugin);
      ~cUpdate();

      // interface

      int init();
      int exit();

      void Stop();
      int isEpgdBusy()             { return epgdBusy; }
      time_t getNextEpgdUpdateAt() { return nextEpgdUpdateAt; }
      void triggerDbReconnect();
      int triggerEpgUpdate(int reload = no);
      int triggerRecUpdate();
      int commonRecFolderOptionChanged();
      int triggerStoreInfoFiles();
      void triggerTimerJobs();
      void epgdStateChange(const char* state);

   protected:

      // notifications from VDRs status interface

      virtual void TimerChange(const cTimer* Timer, eTimerChange Change);
      virtual void Recording(const cDevice *Device, const char *Name, const char *FileName, bool On);
      virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);

   private:

      struct SwitchTimer
      {
         long eventId;
         std::string channelId;
         time_t start;
         int notified;
      };

      // struct to store a recording action delieverd by the status interface

      struct RecordingAction
      {
         std::string name;
         std::string fileName;
         int cardIndex;
         bool on;
      };

      // functions

      int initDb();
      int exitDb();

      void Action(void);
      void processEvents();
      int isHandlerMaster();
      void updateVdrData();
      int updateRecFolderOption();
      int dbConnected(int force = no)
      { return connection && connection->isConnected() && (!force || connection->check() == success); }
      int checkConnection(int& timeout);

      int refreshEpg(const char* channelid = 0, int maxTries = 5);
      cEvent* createEventFromRow(const cDbRow* row);
      int lookupVdrEventOf(int eId, const char* cId);
      int storePicturesToFs();
      int cleanupPictures();
      int getOsd2WebPort();

      tChannelID toChanID(const char* chanIdStr)
      {
         if (isEmpty(chanIdStr))
            return tChannelID::InvalidID;

         return tChannelID::FromString(chanIdStr);
      }

      // timer stuff

      int updateTimerTable();
      int performTimerJobs();
      int recordingChanged();
      int updateTimerDone(int timerid, int doneid, char state);
      int hasTimerChanged();
      int takeSwitchTimer();
      int checkSwitchTimer();

      // recording stuff

      int updateRecordingTable(int fullReload = no);
      int cleanupDeletedRecordings(int force = no);
      int updateRecordingDirectory(const cRecording* recording);
      int updatePendingRecordingInfoFiles(const cRecordings* recordings);
      int performRecordingActions();
      int storeAllRecordingInfoFiles();
      int updateRecordingInfoFiles();

      // data

      cDbConnection* connection;
      cPluginEPG2VDR* plugin;
      int handlerMaster;
      int loopActive;
      time_t nextEpgdUpdateAt;
      time_t lastUpdateAt;
      time_t lastEventsUpdateAt;
      time_t lastRecordingDeleteAt;
      int lastRecordingCount;
      char* epgimagedir;
      int withutf8;
      cCondVar waitCondition;
      cMutex mutex;
      int fullreload;
      char imageExtension[3+TB];

      cMutex timerMutex;
      cMutex swTimerMutex;
      int dbReconnectTriggered;
      int timerJobsUpdateTriggered;
      int timerTableUpdateTriggered;
      int manualTrigger;
      int recordingStateChangedTrigger;
      int recordingFullReloadTrigger;
      int storeAllRecordingInfoFilesTrigger;
      int updateRecFolderOptionTrigger;
      int switchTimerTrigger;
      cList<cRunningRecording> runningRecordings;
      cMutex runningRecMutex;

      Es::State epgdState;
      int epgdBusy;
      int eventsPending;
      int mainActPending;
      const char* videoBasePath;
      int timersTableMaxUpdsp;

      cDbTable* eventsDb;
      cDbTable* useeventsDb;
      cDbTable* fileDb;
      cDbTable* imageDb;
      cDbTable* imageRefDb;
      cDbTable* episodeDb;
      cDbTable* mapDb;
      cDbTable* timerDb;
      cDbTable* timerDoneDb;
      cDbTable* vdrDb;
      cDbTable* compDb;
      cDbTable* recordingDirDb;
      cDbTable* recordingListDb;
      cDbTable* recordingImagesDb;

      cDbStatement* selectMasterVdr;
      cDbStatement* selectAllImages;
      cDbStatement* selectUpdEvents;
      cDbStatement* selectAllEvents {nullptr};
      cDbStatement* selectEventById;
      cDbStatement* selectAllChannels;
      cDbStatement* selectChannelById;
      cDbStatement* markUnknownChannel;
      cDbStatement* selectComponentsOf;
      cDbStatement* deleteTimer;
      cDbStatement* selectMyTimer;
      cDbStatement* selectRecordings;
      cDbStatement* selectImagesOfRecording;
      cDbStatement* selectRecForInfoUpdate;
      cDbStatement* selectPendingTimerActions;
      cDbStatement* selectSwitchTimerActions;
      cDbStatement* selectTimerByEvent;
      cDbStatement* selectTimerById;
      cDbStatement* selectTimerByDoneId;
      cDbStatement* selectMaxUpdSp;

      cDbValue vdrEvtId;
      cDbValue extEvtId;
      cDbValue vdrStartTime;
      cDbValue extChannelId;
      cDbValue imageUpdSp;
      cDbValue imageSize;
      cDbValue imageSizeRec;
      cDbValue masterId;

      cDbValue* viewDescription;
      cDbValue* viewMergeSource;
      cDbValue* viewLongDescription;

      std::queue<std::string> pendingNewRecordings;        // recordings to store details (obsolete if pendingRecordingActions implemented finally)
      std::queue<RecordingAction> pendingRecordingActions; // recordings actions (start/stop)
      std::map<long,SwitchTimer> switchTimers;
      std::queue<int> eventHook;
      cMutex eventHookMutex;

      std::list<cTimerThread*> timerThreads;
      static void sendEvent(int event, void* userData);

      static const char* auxFields[];
};

//***************************************************************************
#endif //__UPDATE_H
