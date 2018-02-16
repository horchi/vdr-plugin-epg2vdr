/*
 * menu.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __EPG2VDR_MENU_H
#define __EPG2VDR_MENU_H

#include <vdr/osdbase.h>
#include <vdr/menuitems.h>

#include "lib/common.h"
#include "lib/db.h"
#include "lib/epgservice.h"
#include "lib/searchtimer.h"

#include "menu.h"
#include "ttools.h"
#include "parameters.h"

class cMenuEpgEditTimer;
class cTimerData;

//***************************************************************************
//
//***************************************************************************

#define NEWTIMERLIMIT    60 // seconds until the start time of a new timer created from the Schedule menu,
                            // within which it will go directly into the "Edit timer" menu to allow
                            // further parameter settings

#define MAXCHNAMWIDTH    16 // maximum number of characters of channels' short names shown in schedules menus

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#  define CHNUMWIDTH  (numdigits(cChannels::MaxNumber()) + 1)
#  define CHNAMWIDTH  (std::min(MAXCHNAMWIDTH, cChannels::MaxShortChannelNameLength() + 1))
#else
#  define CHNUMWIDTH  (numdigits(Channels.MaxNumber()) + 1)
#  define CHNAMWIDTH  (std::min(MAXCHNAMWIDTH, Channels.MaxShortChannelNameLength() + 1))
#endif

//***************************************************************************
// Class cMenuDb
//***************************************************************************

class cMenuDb : public cParameters
{
   friend class cMenuEpgEvent;
   friend class cMenuEpgEditTimer;
   friend class cMenuEpgWhatsOn;
   friend class cMenuEpgSchedule;
   friend class cMenuEpgTimers;
   friend class cMenuEpgSearchTimers;
   friend class cEpgMenuDones;
   friend class cEpgMenuSearchTimerEdit;
   friend class cMenuEpgMatchRecordings;
   friend class cEpgMenuSearchResult;
   friend class cMenuSetupEPG2VDR;
   friend class cMenuEpgScheduleItem;

   public:

      cMenuDb();
      virtual ~cMenuDb();

      int initDb();
      int exitDb();

      int initUserTimes();
      int initUserList(char**& userList, int& count, long int& loginEnabled);
      int lookupTimer(const cEvent* event, int& timerMatch, int& remote, int force = no);

      // timer db

      int createSwitchTimer(const cEvent* event);
      int createTimer(cDbRow* timerRow, const char* destUuid, int type = ttRecord);
      int modifyTimer(cDbRow* timerRow, const char* destUuid, char destType);
      int deleteTimer(long timerid);

      //

      int triggerVdrs(const char* trg, const char* uuid = "", const char* options = "");
      int triggerVdr(const char* uuid, const char* trg, const char* options = "");

   protected:

      virtual int dbConnected(int force = no)
      {
         return dbInitialized
            && connection
            && (!force || connection->check() == success);
      }

      // data

      long webLoginEnabled;
      std::string user;
      long startWithSched;

      cUserTimes* userTimes;
      char** vdrList;
      char** vdrUuidList;
      int vdrCount;

      // database

      int dbInitialized;
      cDbConnection* connection;

      cDbTable* timerDb;
      cDbTable* vdrDb;
      cDbTable* mapDb;
      cDbTable* timerDoneDb;
      cDbTable* userDb;
      cDbTable* searchtimerDb;
      cDbTable* recordingListDb;
      cDbTable* useeventsDb;

      cDbStatement* selectTimers;
      cDbStatement* selectEventById;
      cDbStatement* selectMaxUpdSp;
      cDbStatement* selectTimerById;
      cDbStatement* selectActiveVdrs;
      cDbStatement* selectAllVdrs;
      cDbStatement* selectDoneTimerByState;
      cDbStatement* selectAllUser;
      cDbStatement* selectSearchTimers;
      cDbStatement* selectSearchTimerByName;
      cDbStatement* selectDoneTimerByStateTitleOrder;
      cDbStatement* selectDoneTimerByStateTimeOrder;
      cDbStatement* selectRecordingForEvent;
      cDbStatement* selectRecordingForEventByLv;
      cDbStatement* selectChannelFromMap;

      cSearchTimer* search;

      cDbValue valueStartTime;
      cDbValue timerState;
      cDbValue timerAction;

      class cTimerInfo : public cListObject
      {
         public:

            uint timerid;
            char state;
            time_t starttime;
            uint eventid;
            char channelid[100];
            char uuid[sizeUuid+TB];
      };

      cList<cTimerInfo> timers;
      int timersCacheMaxUpdsp;
};

//***************************************************************************
// Class cMenuEditTimer
//***************************************************************************

class cMenuEpgEditTimer : public cOsdMenu
{
   public:

      cMenuEpgEditTimer(cMenuDb* db, cEpgTimer* Timer, bool New = no);
      virtual ~cMenuEpgEditTimer();

      virtual eOSState ProcessKey(eKeys Key);

   protected:

      eOSState SetFolder();
      void SetFirstDayItem();
      void SetHelpKeys();

   public:

      class cTimerData
      {
         public:

            cTimerData(cMenuDb* p)         { memset(this, 0, sizeof(cTimerData)); db = p; }
            ~cTimerData()                  { free(aux); }

            const char* getVdrName()       { return db->vdrList[vdrIndex]; }
            const char* getVdrUuid()       { return db->vdrUuidList[vdrIndex]; }
            const char* getLastVdrUuid()   { return lastVdrUuid; }
            long TimerId()                 { return timerid; }

            //**************************************************
            // From Timer
            //**************************************************

            void fromTimer(cEpgTimer* Timer, bool New = no)
            {
               weekdays = Timer->WeekDays();
               start = Timer->Start();
               stop = Timer->Stop();
               priority = Timer->Priority();
               lifetime = Timer->Lifetime();
               flags = Timer->Flags();
               channel = Timer->Channel();
               day = Timer->Day();
               strcpy(file, Timer->File());
               vdrIndex = 0;
               free(aux); aux = 0;
               createTime = Timer->CreateTime();
               modTime = Timer->ModTime();

               if (New)
                  flags |= tfActive;

               if (!isEmpty(Timer->Aux()))
                  aux = strdup(Timer->Aux());

               timerid = Timer->TimerId();
               eventid = Timer->EventId();
               state = Timer->State();
               sprintf(stateInfo, "%.300s", Timer->StateInfo());
               action = Timer->Action();
               type = Timer->Type();
               strcpy(lastVdrUuid, Timer->VdrUuid());

               for (int i = 0; i < db->vdrCount; i++)
                  if (strncmp(db->vdrList[i], Timer->VdrName(), strlen(db->vdrList[i])-1) == 0)
                     vdrIndex = i;

#ifdef WITH_PIN
               fskProtection = Timer->FskProtection();
#endif
            }

            int toRow(cDbRow* timerRow)
            {
               timerRow->clear();

               timerRow->setValue("ID", timerid);
               timerRow->setValue("VDRUUID", lastVdrUuid);
               timerRow->setValue("ACTIVE", (int)flags & tfActive);
               timerRow->setValue("CHILDLOCK", fskProtection);
               timerRow->setValue("CHANNELID", channel->GetChannelID().ToString());
               timerRow->setValue("FILE", file);
               timerRow->setValue("VPS", (int)flags & tfVps);
               timerRow->setValue("DAY", day);
               timerRow->setValue("WEEKDAYS", weekdays);
               timerRow->setValue("STARTTIME", start);
               timerRow->setValue("ENDTIME", stop);
               timerRow->setValue("PRIORITY", priority);
               timerRow->setValue("LIFETIME", lifetime);
               timerRow->setValue("EVENTID", eventid);
               //  don't set TYPE here !!! timerRow->setValue("TYPE", type); if needed we have to do it via lastType like VdrUuid

               return done;
            }

            long timerid;
            long eventid;
            int weekdays;                       // bitmask, lowest bits: SSFTWTM  (the 'M' is the LSB)
            int start;
            int stop;
            int priority;
            int fskProtection;
            int lifetime;
            uint flags;
            char state;
            char stateInfo[300+TB];
            char action;
            char type;
            int vdrIndex;
            int typeIndex;
            time_t createTime;
            time_t modTime;
            const cChannel* channel;
            mutable time_t day;
            mutable char file[NAME_MAX*2 + TB]; // *2 to be able to hold 'title' and 'episode', which can each be up to 255 characters long
            char* aux;

            cMenuDb* db;
            char lastVdrUuid[sizeUuid+TB];
      };

   protected:

      cMenuDb* menuDb;
      cTimerData data;
      int channelNr;
      cMenuEditStrItem* file;
      cMenuEditDateItem* day;
      cMenuEditDateItem* firstday;
};

//***************************************************************************
// class cEpgMenuTextItem
//***************************************************************************

class cEpgMenuTextItem : public cOsdItem
{
   public:

      cEpgMenuTextItem(long aId, const char* text)
      {
         cid = 0;
         id = aId;
         SetText(text);
      }

      cEpgMenuTextItem(const char* aId, const char* text)
      {
         cid = strdup(aId);
         SetText(text);
      }

      virtual ~cEpgMenuTextItem() { free(cid); }

      long getId() { return id; }
      const char* getCharId() { return cid; }

   protected:

      long id;
      char* cid;
};

//***************************************************************************
// Class cMenuEpgTimers
//***************************************************************************

class cMenuEpgTimers : public cOsdMenu
{
   public:

      cMenuEpgTimers();
      virtual ~cMenuEpgTimers();

      virtual eOSState ProcessKey(eKeys Key);
      int refresh();

   private:

      eOSState edit();
      eOSState create();
      eOSState remove();
      eOSState toggleState();
      eOSState info();
      cEpgTimer* currentTimer();
      void SetHelpKeys();

      cMenuDb* menuDb;
      int helpKeys;
      int timersMaxUpdsp;
};

//***************************************************************************
// Class cMenuEpgSearchTimers
//***************************************************************************

class cMenuEpgSearchTimers : public cOsdMenu
{
   public:

      cMenuEpgSearchTimers();
      virtual ~cMenuEpgSearchTimers();

      virtual eOSState ProcessKey(eKeys Key);
      int refresh();
      void setHelpKeys();

   private:

      cMenuDb* menuDb;
      // int helpKeys;
};

//***************************************************************************
// Class cMenuEpgEvent
//***************************************************************************

class cMenuEpgEvent : public cOsdMenu
{
   public:

      cMenuEpgEvent(cMenuDb* db, const cEvent* Event, const cSchedules* schedules,
                    int timerMatch, bool DispSchedule, bool CanSwitch = false);
      virtual void Display(void);
      virtual eOSState ProcessKey(eKeys Key);
      virtual const char* MenuKind() { return "MenuEvent"; }

   private:

      const cEvent* getNextPrevEvent(const cEvent* event, int step);
      int setEvent(const cEvent* Event, int timerMatch);

      cMenuDb* menuDb;
      const cSchedules* schedules;
      const cEvent* event;
      bool dispSchedule;
      bool canSwitch;

      std::string prevTime;
      std::string nextTime;
};

//***************************************************************************
// Class cMenuEpgScheduleItem
//***************************************************************************

class cMenuEpgScheduleItem : public cOsdItem
{
   public:

      enum eScheduleSortMode { ssmAllThis, ssmThisThis, ssmThisAll, ssmAllAll }; // "which event(s) on which channel(s)"

      cMenuEpgScheduleItem(cMenuDb* db, const cEvent* Event, const cChannel* Channel = 0, bool WithDate = no);
      ~cMenuEpgScheduleItem();

      static void SetSortMode(eScheduleSortMode SortMode)  { sortMode = SortMode; }
      static void IncSortMode()                            { sortMode = eScheduleSortMode((sortMode == ssmAllAll) ? ssmAllThis : sortMode + 1); }
      static eScheduleSortMode SortMode()                  { return sortMode; }

      virtual int Compare(const cListObject &ListObject) const;
      virtual bool Update(bool Force = false);
      virtual void SetMenuItem(cSkinDisplayMenu* DisplayMenu, int Index, bool Current, bool Selectable);

      const cEvent* event;
      const cChannel* channel;
      bool withDate;
      int timerMatch;

   private:

      cMenuDb* menuDb;
      static eScheduleSortMode sortMode;
};

//***************************************************************************
// Class cMenuEpgWhatsOn
//***************************************************************************

class cMenuEpgWhatsOn : public cOsdMenu
{
   public:

      cMenuEpgWhatsOn(const cEvent* aSearchEvent = 0);
      ~cMenuEpgWhatsOn();

      static int CurrentChannel()                   { return currentChannel; }
      static void SetCurrentChannel(int ChannelNr)  { currentChannel = ChannelNr; }
      virtual eOSState ProcessKey(eKeys Key);
      virtual void Display();

   private:

      int LoadAt();
      int LoadSearch(const cUserTimes::UserTime* userTime);
      int LoadSchedule();
      int LoadQuery(const cEvent* searchEvent);

      bool canSwitch;
      int helpKeys;
      time_t helpKeyTime;
      int helpKeyTimeMode;
      // int timerState;
      eOSState Record();
      eOSState Switch();
      bool Update();
      void SetHelpKeys();

      cMenuDb* menuDb;
      const cSchedules* schedules;
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey schedulesKey;
#else
      cSchedulesLock* schedulesLock;
#endif
      const cEvent* searchEvent;
      int dispSchedule;

      static int currentChannel;
      static const cEvent* scheduleEvent;
};

//***************************************************************************
// class cEpgMenuDones
//***************************************************************************

class cEpgMenuDones : public cOsdMenu
{
   public:

      enum JournalFilter
      {
         jfAll = 0,

         jfFirst = 1,
         jfRecorded = jfFirst,
         jfCreated,
         jfFailed,
         jfDeleted,

         jfCount
      };

      cEpgMenuDones();
      virtual ~cEpgMenuDones();
      virtual eOSState ProcessKey(eKeys Key);

   protected:

      int refresh();
      void setHelp();

      // data

      cMenuDb* menuDb;
      int journalFilter;
      int order;
};

//***************************************************************************
// class cMenuEpgMatchRecordings
//***************************************************************************

class cMenuEpgMatchRecordings : public cOsdMenu
{
   public:

      cMenuEpgMatchRecordings(cMenuDb* db, const cEvent* event);
      virtual ~cMenuEpgMatchRecordings() {};
      virtual eOSState ProcessKey(eKeys Key);
};

//***************************************************************************

#endif  // __EPG2VDR_MENU_H
