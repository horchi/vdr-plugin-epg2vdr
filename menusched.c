/*
 * menusched.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string>

#include <vdr/menuitems.h>
#include <vdr/status.h>
#include <vdr/menu.h>

#include "plgconfig.h"
#include "menu.h"
#include "ttools.h"

class cMenuEpgScheduleItem;

//***************************************************************************
// Class cEpgCommandMenu
//***************************************************************************

class cEpgCommandMenu : public cOsdMenu
{
   public:

      cEpgCommandMenu(const char* title, cMenuDb* db, const cMenuEpgScheduleItem* aItem);
      virtual ~cEpgCommandMenu() { };

      virtual eOSState ProcessKey(eKeys key);

   protected:

      const cMenuEpgScheduleItem* item;
      cMenuDb* menuDb;
};

cEpgCommandMenu::cEpgCommandMenu(const char* title, cMenuDb* db, const cMenuEpgScheduleItem* aItem)
   : cOsdMenu(title)
{
   item = aItem;
   menuDb = db;

   SetMenuCategory(mcMain);
   SetHasHotkeys(yes);
   Clear();

   cOsdMenu::Add(new cOsdItem(hk(tr("Search matching Events")),     osUser1));
   cOsdMenu::Add(new cOsdItem(hk(tr("Search matching Recordings")), osUser2));
   cOsdMenu::Add(new cOsdItem(hk(tr("Searchtimers")),               osUser3));

   SetHelp(0, 0, 0, 0);

   Display();
}

eOSState cEpgCommandMenu::ProcessKey(eKeys key)
{
   eOSState state = cOsdMenu::ProcessKey(key);

   switch (state)
   {
      case osUser1:
      {
         if (item)
            return AddSubMenu(new cMenuEpgWhatsOn(item->epgEvent));

         break;
      }

      case osUser2:
      {
         if (item)
            return AddSubMenu(new cMenuEpgMatchRecordings(menuDb, item->epgEvent));

         break;
      }

      case osUser3: return AddSubMenu(new cMenuEpgSearchTimers());

      default: ;
   }

   return state;
}

//***************************************************************************
// Class cMenuEpgMatchRecordings
//***************************************************************************

cMenuEpgMatchRecordings::cMenuEpgMatchRecordings(cMenuDb* db, const cEvent* event)
   : cOsdMenu(tr("Matching recordings"), 30, 30, 30, 40)
{
   cMenuDb* menuDb = db;

   Add(new cOsdItem(tr("Direct Matches:")));
   cList<cOsdItem>::Last()->SetSelectable(no);

   tell(0, "Search recordings for '%s' '%s'", event->Title(), event->ShortText());

   menuDb->recordingListDb->clear();
   menuDb->recordingListDb->setValue("TITLE", event->Title());
   menuDb->recordingListDb->setValue("SHORTTEXT", event->ShortText());

   for (int f = menuDb->selectRecordingForEvent->find(); f; f = menuDb->selectRecordingForEvent->fetch())
   {
      const char* vdr = 0;

#ifdef WITH_PIN
      if (!cOsd::pinValid && menuDb->recordingListDb->getIntValue("FSK"))
         continue;
#endif

      if (!menuDb->recordingListDb->getValue("OWNER")->isEmpty())
      {
         menuDb->vdrDb->clear();
         menuDb->vdrDb->setValue("UUID", menuDb->recordingListDb->getStrValue("OWNER"));

         if (menuDb->vdrDb->find())
            vdr = menuDb->vdrDb->getStrValue("NAME");
      }

      Add(new cEpgMenuTextItem(menuDb->recordingListDb->getStrValue("MD5PATH"),
                               cString::sprintf("%s%s%s\t%s\t%s/%s", vdr ? vdr : "", vdr ? "\t" : "",
                                                menuDb->recordingListDb->getStrValue("TITLE"),
                                                menuDb->recordingListDb->getStrValue("SHORTTEXT"),
                                                menuDb->recordingListDb->getStrValue("FOLDER"),
                                                menuDb->recordingListDb->getStrValue("NAME"))));
   }

   menuDb->selectRecordingForEvent->freeResult();

   Add(new cOsdItem(tr("All Matches:")));
   cList<cOsdItem>::Last()->SetSelectable(no);

   menuDb->recordingListDb->clear();
   menuDb->recordingListDb->setValue("TITLE", event->Title());

   for (int f = menuDb->selectRecordingForEventByLv->find(); f; f = menuDb->selectRecordingForEventByLv->fetch())
   {
      const char* vdr = 0;

#ifdef WITH_PIN
         if (!cOsd::pinValid && menuDb->recordingListDb->getIntValue("FSK"))
            continue;
#endif

      if (!menuDb->recordingListDb->getValue("OWNER")->isEmpty())
      {
         menuDb->vdrDb->clear();
         menuDb->vdrDb->setValue("UUID", menuDb->recordingListDb->getStrValue("OWNER"));

         if (menuDb->vdrDb->find())
            vdr = menuDb->vdrDb->getStrValue("NAME");
      }

      Add(new cEpgMenuTextItem(menuDb->recordingListDb->getStrValue("MD5PATH"),
                               cString::sprintf("%s%s%s\t%s\t%s/%s", vdr ? vdr : "", vdr ? "\t" : "",
                                                menuDb->recordingListDb->getStrValue("TITLE"),
                                                menuDb->recordingListDb->getStrValue("SHORTTEXT"),
                                                menuDb->recordingListDb->getStrValue("FOLDER"),
                                                menuDb->recordingListDb->getStrValue("NAME"))));
   }

   menuDb->selectRecordingForEventByLv->freeResult();

   Display();
}

eOSState cMenuEpgMatchRecordings::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      return osContinue;
   }

   return state;
}

//***************************************************************************
//***************************************************************************
// Class cMenuEpgScheduleItem
//***************************************************************************

cMenuEpgScheduleItem::eScheduleSortMode cMenuEpgScheduleItem::sortMode = ssmAllThis;

//***************************************************************************
// Object
//***************************************************************************

cMenuEpgScheduleItem::cMenuEpgScheduleItem(cMenuDb* db, const cEvent* Event,
                                           const cChannel* Channel, bool WithDate)
{
   epgEvent = 0;
   eventReady = no;
   menuDb = db;
   vdrEvent = Event;

   if (Event)
   {
      FILE* inMem = 0;
      char* bp;
      size_t size;

      if ((inMem = open_memstream(&bp, &size)))
      {
         Event->Dump(inMem, "", yes);
         fflush(inMem);
         fclose(inMem);

         epgEvent = new cEpgEvent(Event->EventID());
         inMem = fmemopen(bp, strlen(bp), "r");
         epgEvent->Read(inMem);
         fclose(inMem);
      }
   }

   channel = Channel;
   withDate = WithDate;
   timerMatch = tmNone;
   Update(yes);
}

cMenuEpgScheduleItem::~cMenuEpgScheduleItem()
{
   delete epgEvent;
}

//***************************************************************************
// Compare
//***************************************************************************

int cMenuEpgScheduleItem::Compare(const cListObject &ListObject) const
{
   cMenuEpgScheduleItem* p = (cMenuEpgScheduleItem*)&ListObject;
   int r = -1;

   if (sortMode != ssmAllThis)
      r = strcoll(vdrEvent->Title(), p->vdrEvent->Title());

   if (sortMode == ssmAllThis || r == 0)
      r = vdrEvent->StartTime() - p->vdrEvent->StartTime();

   return r;
}

//***************************************************************************
// Update
//***************************************************************************

bool cMenuEpgScheduleItem::Update(bool Force)
{
   static const char* TimerMatchChars = " tT";
   static const char* TimerMatchCharsRemote = " sS";

   bool result = false;
   int oldTimerMatch = timerMatch;
   int remote = no;

   if (epgEvent)
      menuDb->lookupTimer(epgEvent, timerMatch, remote); // -> loads timerDb, vdrDb and set ther timerMatch

   if (Force || timerMatch != oldTimerMatch)
   {
      if (timerMatch && epgEvent)
         tell(0, "Timer match for '%s'", epgEvent->Title());

      cString buffer;
      char t = !remote ? TimerMatchChars[timerMatch] : TimerMatchCharsRemote[timerMatch];
      char v = !epgEvent ? ' ' : epgEvent->Vps() && (epgEvent->Vps() - epgEvent->StartTime()) ? 'V' : ' ';
      char r = !epgEvent ? ' ' : epgEvent->SeenWithin(30) && epgEvent->IsRunning() ? '*' : ' ';
      const char* csn = channel ? channel->ShortName(true) : 0;
      cString eds = !epgEvent ? "" : epgEvent->GetDateString();

      if (channel && withDate)
         buffer = cString::sprintf("%d\t%.*s\t%.*s\t%s\t%c%c%c\t%s", channel->Number(),
                                   Utf8SymChars(csn, 999), csn, Utf8SymChars(eds, 6),
                                   *eds,
                                   !epgEvent ? "" : *epgEvent->GetTimeString(),
                                   t, v, r,
                                   !epgEvent ? "" : epgEvent->Title());
      else if (channel)
         buffer = cString::sprintf("%d\t%.*s\t%s\t%c%c%c\t%s", channel->Number(),
                                   Utf8SymChars(csn, 999), csn,
                                   !epgEvent ? "" : *epgEvent->GetTimeString(),
                                   t, v, r,
                                   !epgEvent ? "" : epgEvent->Title());
      else
         buffer = cString::sprintf("%.*s\t%s\t%c%c%c\t%s", Utf8SymChars(eds, 6), *eds,
                                   !epgEvent ? "" : *epgEvent->GetTimeString(),
                                   t, v, r, !epgEvent ? "" :
                                   !epgEvent ? "" : epgEvent->Title());

      SetText(buffer);
      result = true;
   }

   return result;
}

//***************************************************************************
// Set Menu Item
//***************************************************************************

void cMenuEpgScheduleItem::SetMenuItem(cSkinDisplayMenu* DisplayMenu,
                                       int Index, bool Current, bool Selectable)
{
   if (epgEvent && !eventReady)
   {
      // lookup and enrich event with data of events table

      menuDb->useeventsDb->clear();
      menuDb->useeventsDb->setValue("USEID", (int)epgEvent->EventID());

      enrichEvent(epgEvent, menuDb->useeventsDb, menuDb->selectEventById);
      eventReady = yes;
   }

   if (!DisplayMenu->SetItemEvent(epgEvent, Index, Current, Selectable, channel,
                                  withDate, (eTimerMatch)timerMatch))
   {
      DisplayMenu->SetItem(Text(), Index, Current, Selectable);
   }
}

//***************************************************************************
//***************************************************************************
// cMenuEpgScheduleSepItem
//***************************************************************************

class cMenuEpgScheduleSepItem : public cOsdItem
{
   public:

      cMenuEpgScheduleSepItem(const cChannel* Channel, const cEvent* Event);
      ~cMenuEpgScheduleSepItem();

      virtual bool Update(bool Force = false);
      virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);

   private:

      const cChannel* channel;
      const cEvent* event;
      cEvent* tmpEvent;
};

cMenuEpgScheduleSepItem::cMenuEpgScheduleSepItem(const cChannel* Channel, const cEvent* Event)
{
   channel = Channel;
   event = Event;
   tmpEvent = 0;

   SetSelectable(false);
   Update(true);
}

cMenuEpgScheduleSepItem::~cMenuEpgScheduleSepItem()
{
   delete tmpEvent;
}

bool cMenuEpgScheduleSepItem::Update(bool Force)
{
   if (channel)
   {
      SetText(cString::sprintf("-----\t %s -----", channel ? channel->Name() : *event->GetDateString()));
   }
   else if (event)
   {
      tmpEvent = new cEvent(0);
      tmpEvent->SetTitle(cString::sprintf("-----\t %s -----", *event->GetDateString()));
      SetText(tmpEvent->Title());
   }

   return true;
}

void cMenuEpgScheduleSepItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
   if (!DisplayMenu->SetItemEvent(tmpEvent, Index, Current, Selectable, channel, no, tmNone))
      DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

//***************************************************************************
// cMenuEpgEvent
//***************************************************************************

cMenuEpgEvent::cMenuEpgEvent(cMenuDb* db, const cEvent* Event, const cSchedules* Schedules,
                             int timerMatch, bool DispSchedule, bool CanSwitch)
   : cOsdMenu(tr("Event"))
{
   menuDb = db;
   dispSchedule = DispSchedule;
   schedules = Schedules;
   canSwitch = CanSwitch;
   SetMenuCategory(mcEvent);
   setEvent(Event, timerMatch);
}

//***************************************************************************
// Set Event
//***************************************************************************

int cMenuEpgEvent::setEvent(const cEvent* Event, int timerMatch)
{
   if (Event)
   {
      event = Event;

#if APIVERSNUM >= 20301
      LOCK_CHANNELS_READ;
      const cChannels* channels = Channels;
      const cChannel* channel = channels->GetByChannelID(event->ChannelID(), true);
#else
      cChannels* channels = &Channels;
      cChannel* channel = channels->GetByChannelID(event->ChannelID(), true);
#endif

      if (channel)
      {
         const cChannel* prevChannel = 0;
         const cChannel* nextChannel = 0;

         prevTime = "";
         nextTime = "";

         SetTitle(channel->Name());

         if (menuDb->userTimes->current()->getMode() != cUserTimes::mSearch)
         {
            const cEvent* e;

            if ((e = getNextPrevEvent(event, -1)))
            {
               if (dispSchedule)
                  prevTime = l2pTime(e->StartTime(), "%H:%M");
               else
                  prevChannel = channels->GetByChannelID(e->ChannelID(), true);
            }

            if ((e = getNextPrevEvent(event, +1)))
            {
               if (dispSchedule)
                  nextTime = l2pTime(e->StartTime(), "%H:%M");
               else
                  nextChannel = channels->GetByChannelID(e->ChannelID(), true);
            }
         }

         SetHelp(timerMatch == tmFull ? tr("Button$Timer") : tr("Button$Record"),
                 dispSchedule ? prevTime.c_str() : (prevChannel ? prevChannel->Name() : 0),
                 dispSchedule ? nextTime.c_str() : (nextChannel ? nextChannel->Name() : 0),
                 canSwitch ? tr("Button$Switch") : 0);
      }

      Display();
   }

   return success;
}

void cMenuEpgEvent::Display()
{
   cOsdMenu::Display();
   DisplayMenu()->SetEvent(event);

#ifdef WITH_GTFT
   cStatus::MsgOsdSetEvent(event);
#endif

   if (event->Description())
      cStatus::MsgOsdTextItem(event->Description());
}

//***************************************************************************
// Get Next Prev Event (related to current)
//***************************************************************************

const cEvent* cMenuEpgEvent::getNextPrevEvent(const cEvent* event, int step)
{
#if APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
   const cChannel* channel = channels->GetByChannelID(event->ChannelID(), true);
#else
   cChannels* channels = &Channels;
   cChannel* channel = channels->GetByChannelID(event->ChannelID(), true);
#endif

   const cEvent* e = 0;

   if (dispSchedule)
   {
      const cSchedule* schedule = schedules->GetSchedule(channel);

      if (schedule && step == -1)
         e = schedule->Events()->Prev(event);
      else if (schedule && step == +1)
         e = schedule->Events()->Next(event);
   }
   else
   {
      while (!e)
      {
         int cn = channel->Number() + step;

         if (cn < 1)
            cn = channels->MaxNumber();
         else if (cn > channels->MaxNumber())
            cn = 1;

         if (!(channel = channels->GetByNumber(cn)))
            return 0;

         const cSchedule* schedule = schedules->GetSchedule(channel);

         if (schedule)
         {
            cUserTimes::UserTime* userTime = menuDb->userTimes->current();

            switch (userTime->getMode())
            {
               case cUserTimes::mNow:  e = schedule->GetPresentEvent();    break;
               case cUserTimes::mNext: e = schedule->GetFollowingEvent();  break;
               case cUserTimes::mTime: e = schedule->GetEventAround(userTime->getTime()); break;
            }
         }
      }
   }

   return e;
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cMenuEpgEvent::ProcessKey(eKeys Key)
{
   switch (int(Key))
   {
      case kUp|k_Repeat:
      case kUp:
      case kDown|k_Repeat:
      case kDown:
      case kLeft|k_Repeat:
      case kLeft:
      case kRight|k_Repeat:
      case kRight:
      {
         DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
         cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft);
         return osContinue;
      }

      case kInfo:   return osBack;
      default: break;
   }

   if (Key == kGreen || Key == kYellow)
   {
     if (menuDb->userTimes->current()->getMode() == cUserTimes::mSearch)
         return osContinue;

      const cEvent* e = getNextPrevEvent(event, Key == kGreen ? -1 : +1);
      int remote = no;
      int timerMatch = tmNone;

      // get remote and timerMatch info

      menuDb->lookupTimer(e, timerMatch, remote);
      setEvent(e, timerMatch);

      return osContinue;
   }

   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
         case kOk:     return osBack;
         default: break;
      }
   }

   return state;
}

//***************************************************************************
//***************************************************************************
// cMenuEpgWhatsOn
//***************************************************************************

int cMenuEpgWhatsOn::currentChannel = 0;
const cEvent* cMenuEpgWhatsOn::scheduleEvent = 0;

//***************************************************************************
// Object
//***************************************************************************

cMenuEpgWhatsOn::cMenuEpgWhatsOn(const cEvent* aSearchEvent)
   : cOsdMenu("", CHNUMWIDTH, CHNAMWIDTH, 6, 4)
{
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#else
   schedulesLock = 0;
#endif
   schedules = 0;
   dispSchedule = no;
   canSwitch = no;
   helpKeys = 0;
   helpKeyTime = 0;
   helpKeyTimeMode = na;
//   timerState = 0;
   searchEvent = aSearchEvent;
   menuDb = new cMenuDb;

#if APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
   const cChannel* channel = channels->GetByNumber(cDevice::CurrentChannel());
#else
   cChannels* channels = &Channels;
   const cChannel* channel = channels->GetByNumber(cDevice::CurrentChannel());
#endif

   if (channel)
   {
      currentChannel = channel->Number();
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      schedules = cSchedules::GetSchedulesRead(schedulesKey);
#else
      schedulesLock = new cSchedulesLock(false);
      schedules = (cSchedules*)cSchedules::Schedules(*schedulesLock);
#endif
   }

//   Timers.Modified(timerState);

   if (menuDb->dbConnected())
   {
      menuDb->userTimes->first();     // 'whats on now' should be the first

      if (searchEvent)
         LoadQuery(searchEvent);
      else if (menuDb->startWithSched)
         LoadSchedule();
      else
         LoadAt();
   }
}

cMenuEpgWhatsOn::~cMenuEpgWhatsOn()
{
   delete menuDb;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   if (schedules)
      schedulesKey.Remove();
#else
   tell(3, "LOCK free (cMenuEpgWhatsOn)");
   delete schedulesLock;
#endif
}

//***************************************************************************
// Load At
//***************************************************************************

int cMenuEpgWhatsOn::LoadAt()
{
   dispSchedule = no;
   cUserTimes::UserTime* userTime = menuDb->userTimes->current();
   cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

   if (item)
   {
      scheduleEvent = item->epgEvent;

      if (item->channel)
         currentChannel = item->channel->Number();
   }

   if (userTime->getMode() == cUserTimes::mSearch)
      return LoadSearch(userTime);

   Clear();
//   Timers.Modified(timerState);

   SetMenuCategory(userTime->getMode() == cUserTimes::mNow ? mcScheduleNow : mcScheduleNext);

   if (isEmpty(userTime->getHHMMStr()))
      SetTitle(cString::sprintf(tr("Overview - %s"), userTime->getTitle()));
   else
      SetTitle(cString::sprintf(tr("Overview - %s (%s)"), userTime->getTitle(), userTime->getHHMMStr()));

   tell(2, "DEBUG: Loading events for '%s' %s", userTime->getTitle(), !isEmpty(userTime->getHHMMStr()) ? userTime->getHHMMStr() : "");

#if APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
#else
   const cChannels* channels = &Channels;
#endif

   // events

   for (const cChannel* Channel = channels->First(); Channel; Channel = channels->Next(Channel))
   {
      if (!Channel->GroupSep())
      {
         const cSchedule* Schedule = schedules->GetSchedule(Channel);
         const cEvent* Event = 0;

         if (Schedule)
         {
            switch (userTime->getMode())
            {
               case cUserTimes::mNow:  Event = Schedule->GetPresentEvent();                    break;
               case cUserTimes::mNext: Event = Schedule->GetFollowingEvent();                  break;
               case cUserTimes::mTime: Event = Schedule->GetEventAround(userTime->getTime());  break;
            }
         }

         // #TODO
         // hier ein cEpgEvent für das Event aus der Row erzeugen und dieses anstelle von cEvent für das Menü verwenden

         if (!Event && !Epg2VdrConfig.showEmptyChannels)
            continue;

         Add(new cMenuEpgScheduleItem(menuDb, Event, Channel), Channel->Number() == currentChannel);
      }
      else
      {
         if (strlen(Channel->Name()))
            Add(new cMenuEpgScheduleSepItem(Channel, 0));
      }
   }

   Display();
   SetHelpKeys();

   return done;
}

//***************************************************************************
// Load Search
//***************************************************************************

int cMenuEpgWhatsOn::LoadSearch(const cUserTimes::UserTime* userTime)
{
   cDbStatement* select = 0;

   Clear();
   SetMenuCategory(mcSchedule);
   SetTitle(cString::sprintf(tr("Overview - %s"), userTime->getTitle()));
   tell(2, "DEBUG: Loading events for search '%s'", userTime->getSearch());

   menuDb->searchtimerDb->clear();
   menuDb->searchtimerDb->setValue("NAME", userTime->getSearch());

   if (!menuDb->selectSearchTimerByName->find())
      return done;

   if (!(select = menuDb->search->prepareSearchStatement(menuDb->searchtimerDb->getRow(), menuDb->useeventsDb)))
      return fail;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   cChannelsLock channelsLock(false);
   const cChannels* channels = channelsLock.Channels();
#else
   cChannels* channels = &Channels;
#endif

   menuDb->useeventsDb->clear();

   for (int res = select->find(); res; res = select->fetch())
   {
      menuDb->useeventsDb->find();  // get all fields ..

      if (!menuDb->search->matchCriterias(menuDb->searchtimerDb->getRow(), menuDb->useeventsDb->getRow()))
         continue;

      //

      const char* strChannelId = menuDb->useeventsDb->getStrValue("CHANNELID");
      const cChannel* channel = channels->GetByChannelID(tChannelID::FromString(strChannelId));
      const cSchedule* schedule = schedules->GetSchedule(channel);
      const cEvent* event = !schedule ? 0 :  schedule->GetEvent(menuDb->useeventsDb->getIntValue("USEID"));

      if (event)
         Add(new cMenuEpgScheduleItem(menuDb, event, channel, yes));
  }

   select->freeResult();
   menuDb->selectSearchTimerByName->freeResult();

   Display();
   SetHelpKeys();

   return Count();
}

//***************************************************************************
// Load Schedule
//***************************************************************************

int cMenuEpgWhatsOn::LoadSchedule()
{
   Clear();
   SetCols(7, 6, 4);
   dispSchedule = yes;

#if APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
   const cChannel* channel = channels->GetByNumber(currentChannel);
#else
   cChannels* channels = &Channels;
   const cChannel* channel = channels->GetByNumber(currentChannel);
#endif

   cMenuEpgScheduleItem::SetSortMode(cMenuEpgScheduleItem::ssmAllThis);
   SetMenuCategory(mcSchedule);
   SetTitle(cString::sprintf(tr("Schedule - %s"), channel->Name()));

   if (schedules && channel)
   {
      const cSchedule* Schedule = schedules->GetSchedule(channel);

      if (Schedule)
      {
         int lastDay = 0;
         const cEvent* PresentEvent = scheduleEvent ? scheduleEvent : Schedule->GetPresentEvent();
         time_t now = time(0) - Setup.EPGLinger * 60;

         for (const cEvent* ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev))
         {
            time_t stime = ev->StartTime();
            struct tm tmSTime;
            localtime_r(&stime, &tmSTime);

            if (lastDay != 0 && tmSTime.tm_mday != lastDay)
               Add(new cMenuEpgScheduleSepItem(0, ev));

            if (ev->EndTime() > now || ev == PresentEvent)
               Add(new cMenuEpgScheduleItem(menuDb, ev, channel), ev == PresentEvent);

            lastDay = tmSTime.tm_mday;
         }
      }
   }

   Display();
   SetHelpKeys();

   return done;
}

//***************************************************************************
// Load Query
//***************************************************************************

int cMenuEpgWhatsOn::LoadQuery(const cEvent* searchEvent)
{
   Clear();
   SetMenuCategory(mcSchedule);
   cMenuEpgScheduleItem::SetSortMode(cMenuEpgScheduleItem::ssmAllThis);

#if APIVERSNUM >= 20301
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
#else
   const cChannels* channels = &Channels;
#endif

   tell(1, "Lookup events with title '%s'", searchEvent->Title());

   for (const cChannel* channel = channels->First(); channel; channel = channels->Next(channel))
   {
      const cSchedule* schedule = schedules->GetSchedule(channel);

      if (schedule)
      {
         for (const cEvent* event = schedule->Events()->First(); event; event = schedule->Events()->Next(event))
         {
            if (strcasecmp(event->Title(), searchEvent->Title()) == 0 &&
                event->StartTime() > time(0))
               Add(new cMenuEpgScheduleItem(menuDb, event, channel, yes));
         }
      }
   }

   Sort();
   SetTitle(cString::sprintf("%d %s - %s", Count(), tr("Search results"), searchEvent->Title()));

   Display();
   SetHelpKeys();

   return Count();
}

//***************************************************************************
// Display
//***************************************************************************

void cMenuEpgWhatsOn::Display()
{
   cOsdMenu::Display();

#ifdef WITH_GTFT
   if (Count() > 0)
   {
      int ni = 0;

      for (cOsdItem *item = First(); item; item = Next(item))
         cStatus::MsgOsdEventItem(((cMenuEpgScheduleItem*)item)->epgEvent, item->Text(), ni++, Count());
   }
#endif
}

//***************************************************************************
// Update
//***************************************************************************

bool cMenuEpgWhatsOn::Update()
{
   bool result = false;

   for (cOsdItem* item = First(); item; item = Next(item))
   {
      if (((cMenuEpgScheduleItem*)item)->Update())
         result = true;
   }

   return result;
}

//***************************************************************************
// SetHelpKeys
//***************************************************************************

void cMenuEpgWhatsOn::SetHelpKeys()
{
   cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem *)Get(Current());
   canSwitch = no;
   int NewHelpKeys = 0;

   if (item)
   {
      if (item->timerMatch == tmFull)
         NewHelpKeys |= 0x02;    // "Timer"
      else
         NewHelpKeys |= 0x01;    // "Record"

      if (!dispSchedule)
         NewHelpKeys |= 0x04;    // "Schedule"

      if (item->channel)
      {
         if (item->channel->Number() != cDevice::CurrentChannel())
         {
            NewHelpKeys |= 0x10; // "Switch"
            canSwitch = yes;
         }
      }
   }

   cUserTimes::UserTime* userTimeGreen = !dispSchedule ? menuDb->userTimes->getNext() : menuDb->userTimes->current();

   if (NewHelpKeys != helpKeys || helpKeyTime != userTimeGreen->getTime() || helpKeyTimeMode != userTimeGreen->getMode())
   {
      const char* Red[] = { 0, tr("Button$Record"), tr("Button$Timer") };

      if (searchEvent)
      {
         helpKeys = 0;
         helpKeyTime = 0;
         helpKeyTimeMode = na;

         SetHelp(Red[NewHelpKeys & 0x03], 0, 0, 0);
      }
      else
      {
         SetHelp(Red[NewHelpKeys & 0x03],                                   // red
                 userTimeGreen->getHelpKey(),                               // green
                 !dispSchedule
                 ? tr("Button$Schedule")                                    // yellow
                 : menuDb->userTimes->current() == menuDb->userTimes->getFirst()
                 ? tr(menuDb->userTimes->getNext()->getHelpKey())           // yellow
                 : tr(menuDb->userTimes->getFirst()->getHelpKey()),         // yellow
                 Epg2VdrConfig.xchgOkBlue ? tr("Button$Info") : canSwitch ? tr("Button$Switch") : 0);  // blue

         helpKeyTime = userTimeGreen->getTime();
         helpKeyTimeMode = userTimeGreen->getMode();
         helpKeys = NewHelpKeys;
      }
   }
}

//***************************************************************************
// Switch
//***************************************************************************

eOSState cMenuEpgWhatsOn::Switch()
{
   cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

   if (cDevice::PrimaryDevice()->SwitchChannel(item->channel, true))
      return osEnd;

   Skins.Message(mtError, tr("Can't switch channel!"));

   return osContinue;
}

//***************************************************************************
// Record
//***************************************************************************

eOSState cMenuEpgWhatsOn::Record()
{
   int timerMatch = tmNone;
   cEpgTimer* timer = 0;
   long timerid = 0;
   int remote = no;
   cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

   if (!item)
      return osContinue;

   if ((timerid = menuDb->lookupTimer(item->epgEvent, timerMatch, remote))) // -> loads timerDb and vdrDb
   {
      menuDb->timerDb->clear();
      menuDb->timerDb->setValue("ID", timerid);

      if (!menuDb->selectTimerById->find())
      {
         tell(0, "Fatal, Can't find timer (%ld)", timerid);
         return osContinue;
      }

      timer = newTimerObjectFromRow(menuDb->timerDb->getRow(), menuDb->vdrDb->getRow());
   }

   if (timer)
      return AddSubMenu(new cMenuEpgEditTimer(menuDb, timer));

   // neuen Timer anlegen

   cDbRow* timerRow = newTimerRowFromEvent(item->vdrEvent);
   char timerDefaultVDRuuid[150+TB] = "";

   menuDb->getParameter(menuDb->user.c_str(), "timerDefaultVDRuuid", timerDefaultVDRuuid);

   // Menü bei 'aktuellem' Event Timer Dialog öffen -> #TODO

   if (timer && timer->Event() && timer->Event()->StartTime() < time(0) + NEWTIMERLIMIT)
   {
      // timer = newTimerObjectFromRow(timerRow, xxxxx);
      // return AddSubMenu(new cMenuEpgEditTimer(menuDb, timer));
   }

   // ansonsten direkt anlegen

   menuDb->createTimer(timerRow, isEmpty(timerDefaultVDRuuid) || Epg2VdrConfig.createTimerLocal ? Epg2VdrConfig.uuid : timerDefaultVDRuuid);
   delete timerRow;

   if (HasSubMenu())
      CloseSubMenu();

   sleep(1);

   if (Update())
      Display();

   SetHelpKeys();

   return osContinue;
}

//***************************************************************************
// ProcessKey
//***************************************************************************

eOSState cMenuEpgWhatsOn::ProcessKey(eKeys Key)
{
   bool HadSubMenu = HasSubMenu();
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (!menuDb->dbConnected())
      return state;

   if (state == osUnknown)
   {
      if (searchEvent && Key != kOk && Key != kRed && Key != kRecord)
         return osContinue;

      if (Epg2VdrConfig.xchgOkBlue && !HasSubMenu())
      {
         if      (Key == kBlue) Key = kOk;
         else if (Key == kOk)   Key = kBlue;
      }

      switch (Key)
      {
         case k0:       // command menu
         {
            cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

            return AddSubMenu(new cEpgCommandMenu("Commands", menuDb, item));
         }

         case k1:       // search repeat of event
         {
            if (Get(Current()))
            {
               cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

               if (item)
                  return AddSubMenu(new cMenuEpgWhatsOn(item->epgEvent));
            }

            break;
         }

         case k2:      // search matching recordings
         {
            if (Get(Current()))
            {
               cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

               if (item)
                  return AddSubMenu(new cMenuEpgMatchRecordings(menuDb, item->epgEvent));
            }

            break;
         }

         case k3:  // search timer dialog
         {
            return AddSubMenu(new cMenuEpgSearchTimers());
         }

         case k4:  // Umschalt Timer erstellen
         {
            break;
         }

         case kRecord:
         case kRed:
            return Record();

         case kYellow:
         {
            if (!dispSchedule && Get(Current()))
            {
               cMenuEpgScheduleItem* mi = (cMenuEpgScheduleItem*)Get(Current());

               if (mi)
               {
                  scheduleEvent = mi->epgEvent;
                  currentChannel = mi->channel->Number();
                  LoadSchedule();
               }
            }
            else
            {
               if (menuDb->userTimes->current() == menuDb->userTimes->getFirst())
                  menuDb->userTimes->next();
               else
                  menuDb->userTimes->first();

               LoadAt();
            }

            return osContinue;
         }
         case kGreen:
         {
            if (!dispSchedule)
               menuDb->userTimes->next();

            LoadAt();

            return osContinue;
         }

         case kBlue:
         {
            if (canSwitch)
               return Switch();

            break;
         }

         case kInfo:
         case kOk:
         {
            if (Get(Current()))
            {
               cMenuEpgScheduleItem* item = (cMenuEpgScheduleItem*)Get(Current());

               if (item)
               {
                  const cEvent* event = item->epgEvent;

                  if (Count() && event)
                     return AddSubMenu(new cMenuEpgEvent(menuDb, event, schedules,
                                                         item->timerMatch, dispSchedule, canSwitch));
               }
            }

            break;
         }

         default:
            break;
      }
   }

    if (!HasSubMenu())
   {
      if (HadSubMenu && Update())
         Display();

      if (Key != kNone)
         SetHelpKeys();
   }

   return state;
}

//***************************************************************************
