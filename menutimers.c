/*
 * menutimers.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/menu.h>
#include <vdr/interface.h>

#include "lib/vdrlocks.h"
#include "plgconfig.h"
#include "menu.h"

//***************************************************************************
// cMenuEpgEditTimer
//***************************************************************************

cMenuEpgEditTimer::cMenuEpgEditTimer(cMenuDb* db, cEpgTimer* Timer, bool New)
   : cOsdMenu(tr("Edit Timer"), 12),
     data(db)
{
   SetMenuCategory(mcTimerEdit);
   file = 0;
   day = firstday = 0;
   menuDb = db;

   if (Timer)
   {
      data.fromTimer(Timer);

      channelNr = data.channel->Number();
      Add(new cMenuEditBitItem(tr("Active"),       &data.flags, tfActive));

      if (menuDb->vdrCount)
         Add(new cMenuEditStraItem(tr("VDR"),      &data.vdrIndex, menuDb->vdrCount, menuDb->vdrList));

      Add(new cMenuEditChanItem(tr("Channel"),     &channelNr));
      Add(day = new cMenuEditDateItem(tr("Day"),   &data.day, &data.weekdays));
      Add(new cMenuEditTimeItem(tr("Start"),       &data.start));
      Add(new cMenuEditTimeItem(tr("Stop"),        &data.stop));
      Add(new cMenuEditBitItem(tr("VPS"),          &data.flags, tfVps));
      Add(new cMenuEditIntItem(tr("Priority"),     &data.priority, 0, MAXPRIORITY));
      Add(new cMenuEditIntItem(tr("Lifetime"),     &data.lifetime, 0, MAXLIFETIME));

#ifdef WITH_PIN
      if (cOsd::pinValid || !data.fskProtection)
         Add(new cMenuEditBoolItem(tr("Childlock"), &data.fskProtection));
      else
         Add(new cOsdItem(cString::sprintf("%s\t%s", tr("Childlock"), data.fskProtection ? trVDR("yes") : trVDR("no"))));
#endif

      Add(file = new cMenuEditStrItem(tr("File"),   data.file, sizeof(data.file)));
      SetFirstDayItem();

      Add(new cOsdItem(cString::sprintf("------- %s ----------", tr("Information"))));
      cList<cOsdItem>::Last()->SetSelectable(false);
      Add(new cOsdItem(cString::sprintf("Status:\t%s", toName((TimerState)data.state))));

      if (!isEmpty(data.stateInfo))
         Add(new cOsdItem(cString::sprintf("\t%s", data.stateInfo)));

      Add(new cOsdItem(cString::sprintf("Pending action:\t%s", toName((TimerAction)data.action, yes))));
   }

   SetHelpKeys();
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#else
   Timers.IncBeingEdited();
#endif
}

cMenuEpgEditTimer::~cMenuEpgEditTimer()
{
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#else
   Timers.DecBeingEdited();
#endif
}

void cMenuEpgEditTimer::SetHelpKeys(void)
{
   SetHelp(tr("Button$Folder"), data.weekdays ? tr("Button$Single") : tr("Button$Repeating"));
}

void cMenuEpgEditTimer::SetFirstDayItem(void)
{
   if (!firstday && data.weekdays)
   {
      Add(firstday = new cMenuEditDateItem(tr("First day"), &data.day));
      Display();
   }
   else if (firstday && !data.weekdays)
   {
      Del(firstday->Index());
      firstday = 0;
      Display();
   }
}

eOSState cMenuEpgEditTimer::SetFolder(void)
{
   if (cMenuFolder* mf = dynamic_cast<cMenuFolder*>(SubMenu()))
   {
      cString Folder = mf->GetFolder();
      const char *p = strrchr(data.file, FOLDERDELIMCHAR);

      if (p)
         p++;
      else
         p = data.file;

      if (!isempty(*Folder))
         strn0cpy(data.file, cString::sprintf("%s%c%s", *Folder, FOLDERDELIMCHAR, p), sizeof(data.file));
      else if (p != data.file)
         memmove(data.file, p, strlen(p) + 1);

      SetCurrent(file);
      Display();
   }

   return CloseSubMenu();
}

eOSState cMenuEpgEditTimer::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
         case kOk:
         {
            int recording = no;

            GET_CHANNELS_READ(channels);

#if APIVERSNUM >= 20301
            const cChannel* ch = channels->GetByNumber(channelNr);
#else
            cChannel* ch = channels.GetByNumber(channelNr);
#endif

            if (!ch)
            {
               Skins.Message(mtError, tr("*** Invalid Channel ***"));
               break;
            }

            data.channel = ch;

            if (!*data.file)
               strcpy(data.file, data.channel->ShortName(true));

            cDbRow timerRow("timers");

            data.toRow(&timerRow);

            // get actual timer state from database

            menuDb->timerDb->clear();
            menuDb->timerDb->setValue("ID", data.TimerId());
            menuDb->timerDb->setValue("VDRUUID", data.getLastVdrUuid());

            if (menuDb->timerDb->find() && menuDb->timerDb->hasCharValue("STATE", tsRunning))
               recording = yes;

            menuDb->timerDb->reset();

            // ask for confirmation if timer running!

            if (recording && strcmp(data.getVdrUuid(), data.getLastVdrUuid()) != 0)
            {
               if (!Interface->Confirm(tr("Timer still recording - really move to other VDR?")))
                  return osContinue;
            }

            menuDb->modifyTimer(&timerRow, data.getVdrUuid());

            return osBack;
         }

         case kRed:
            return AddSubMenu(new cMenuFolder(tr("Select folder"), &Folders, data.file));

         case kGreen:
         {
            if (day)
            {
               day->ToggleRepeating();
               SetCurrent(day);
               SetFirstDayItem();
               SetHelpKeys();
               Display();
            }

            return osContinue;
         }

         case kYellow:
         case kBlue:   return osContinue;

         default: break;
      }
   }
   else if (state == osEnd && HasSubMenu())
      state = SetFolder();

   if (Key != kNone)
      SetFirstDayItem();

   return state;
}

//***************************************************************************
// cMenuEpgTimerItem
//***************************************************************************

class cMenuEpgTimerItem : public cOsdItem
{
   public:

      cMenuEpgTimerItem(cEpgTimer* Timer);
      ~cMenuEpgTimerItem();

      virtual int Compare(const cListObject &ListObject) const;
      virtual void Set();
      cEpgTimer* Timer()  { return timer; }
      virtual void SetMenuItem(cSkinDisplayMenu* DisplayMenu, int Index,
                               bool Current, bool Selectable);

   private:

      cEpgTimer* timer;
};

cMenuEpgTimerItem::cMenuEpgTimerItem(cEpgTimer* Timer)
{
   timer = Timer;
   Set();
}

cMenuEpgTimerItem::~cMenuEpgTimerItem()
{
   delete timer;
}

int cMenuEpgTimerItem::Compare(const cListObject &ListObject) const
{
   return timer->Compare(*((cMenuEpgTimerItem*)&ListObject)->timer);
}

void cMenuEpgTimerItem::Set()
{
   cString day, dayName("");

   if (timer->WeekDays())
   {
      day = timer->PrintDay(0, timer->WeekDays(), false);
   }
   else if (timer->Day() - time(0) < 28 * SECSINDAY)
   {
      day = itoa(timer->GetMDay(timer->Day()));
      dayName = WeekDayName(timer->Day());
   }
   else
   {
      struct tm tm_r;
      time_t Day = timer->Day();
      localtime_r(&Day, &tm_r);
      char buffer[16];
      strftime(buffer, sizeof(buffer), "%Y%m%d", &tm_r);
      day = buffer;
   }

   const char* vdr = timer->VdrName();
   const char* File = Setup.FoldersInTimerMenu ? 0 : strrchr(timer->File(), FOLDERDELIMCHAR);

   if (File && strcmp(File + 1, TIMERMACRO_TITLE) && strcmp(File + 1, TIMERMACRO_EPISODE))
      File++;
   else
      File = timer->File();

   SetText(cString::sprintf("%c\t%s%c\t%d\t%s%s%s\t%02d:%02d\t%02d:%02d\t%s",
                            !(timer->HasFlags(tfActive)) ? ' ' : timer->FirstDay() ? '!' : timer->Recording() ? '#' : '>',
                            vdr, timer->isVdrRunning() ? '*' : ' ',
                            timer->Channel() ? timer->Channel()->Number() : na,
                            *dayName, *dayName && **dayName ? " " : "", *day,
                            timer->Start() / 100, timer->Start() % 100,
                            timer->Stop() / 100,  timer->Stop() % 100,
                            File));
}

void cMenuEpgTimerItem::SetMenuItem(cSkinDisplayMenu* DisplayMenu, int Index,
                                    bool Current, bool Selectable)
{
   if (!DisplayMenu->SetItemTimer(timer, Index, Current, Selectable))
      DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

//***************************************************************************
// Class cMenuEpgTimers
//***************************************************************************
//***************************************************************************
// Object
//***************************************************************************

cMenuEpgTimers::cMenuEpgTimers()
   : cOsdMenu(tr("Timers"), 2, CHNUMWIDTH, 10, 6, 6)
{
   helpKeys = -1;
   timersMaxUpdsp = 0;

   menuDb = new cMenuDb;

   SetMenuCategory(mcTimer);

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#else
   Timers.IncBeingEdited();
#endif

   refresh();
}

cMenuEpgTimers::~cMenuEpgTimers()
{
   delete menuDb;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
#else
   Timers.DecBeingEdited();
#endif
}

//***************************************************************************
// Refresh
//***************************************************************************

int cMenuEpgTimers::refresh()
{
   int current = Current();

   menuDb->timerDb->clear();
   menuDb->vdrDb->clear();

   menuDb->timerDb->clear();
   menuDb->selectMaxUpdSp->execute();
   timersMaxUpdsp = menuDb->timerDb->getIntValue("UPDSP");
   menuDb->selectMaxUpdSp->freeResult();

   Clear();

   menuDb->timerState.setValue("P,R,u");
   menuDb->timerAction.setValue("C,M,A,F,a");

   for (int f = menuDb->selectTimers->find(); f && menuDb->dbConnected(); f = menuDb->selectTimers->fetch())
   {
      cEpgTimer* t = newTimerObjectFromRow(menuDb->timerDb->getRow(), menuDb->vdrDb->getRow());

      if (t)
         Add(new cMenuEpgTimerItem(t));
   }

   menuDb->selectTimers->freeResult();

   Sort();
   SetCurrent(current >= 0 ? Get(current) : First());
   SetHelpKeys();
   Display();

   return done;
}

cEpgTimer* cMenuEpgTimers::currentTimer()
{
  cMenuEpgTimerItem* item = (cMenuEpgTimerItem*)Get(Current());
  return item ? item->Timer() : 0;
}

void cMenuEpgTimers::SetHelpKeys()
{
   int newHelpKeys = 0;
   cEpgTimer* timer = currentTimer();

   if (timer)
   {
      if (timer->Event())
         newHelpKeys = 2;
      else
         newHelpKeys = 1;
   }

   if (newHelpKeys != helpKeys)
   {
      helpKeys = newHelpKeys;
      SetHelp(helpKeys > 0 ? tr("Button$On/Off") : 0,
              tr("Button$New"),
              helpKeys > 0 ? tr("Button$Delete") : 0,
              helpKeys == 2 ? tr("Button$Info") : 0);
   }
}

eOSState cMenuEpgTimers::toggleState()
{
   if (HasSubMenu())
      return osContinue;

   cEpgTimer* timer = currentTimer();

   if (timer)
   {
      menuDb->timerDb->clear();
      menuDb->timerDb->setValue("ID", timer->TimerId());
      menuDb->timerDb->setValue("VDRUUID", timer->VdrUuid());

      if (menuDb->timerDb->find())
      {
         menuDb->timerDb->setCharValue("ACTION", taModify);
         menuDb->timerDb->getValue("STATE")->setNull();
         menuDb->timerDb->setValue("SOURCE", Epg2VdrConfig.uuid);
         menuDb->timerDb->setValue("ACTIVE", !menuDb->timerDb->getIntValue("ACTIVE"));
         menuDb->timerDb->update();
         menuDb->triggerVdrs("TIMERJOB");  // trigger all!

         tell(0, "Timer %s %sactivated", *timer->ToDescr(), menuDb->timerDb->getIntValue("ACTIVE") ? "" : "de");
      }
   }

   return osUser1;
}

eOSState cMenuEpgTimers::edit()
{
   if (HasSubMenu() || Count() == 0)
      return osContinue;

   tell(0, "Editing timer %s", *currentTimer()->ToDescr());

   return AddSubMenu(new cMenuEpgEditTimer(menuDb, currentTimer()));
}

eOSState cMenuEpgTimers::create()
{
   if (HasSubMenu())
      return osContinue;

   return AddSubMenu(new cMenuEpgEditTimer(menuDb, new cEpgTimer, yes));
}

eOSState cMenuEpgTimers::remove()
{
   int recording = no;
   cEpgTimer* ti = currentTimer();

   if (!ti)
      return osContinue;

   if (!Interface->Confirm(tr("Delete timer?")))
      return osContinue;

   // get actual state from database

   menuDb->timerDb->clear();
   menuDb->timerDb->setValue("ID", ti->TimerId());
   menuDb->timerDb->setValue("VDRUUID", ti->VdrUuid());

   if (menuDb->timerDb->find() && menuDb->timerDb->hasCharValue("STATE", tsRunning))
      recording = yes;

   menuDb->timerDb->reset();

   // check

   if (recording && !Interface->Confirm(tr("Timer still recording - really delete?")))
      return osContinue;

   // request timer deletion

   tell(0, "Deleting timer %s", *ti->ToDescr());

   menuDb->deleteTimer(currentTimer()->TimerId());

   return osUser1;
}

eOSState cMenuEpgTimers::info()
{
   if (HasSubMenu() || !Count())
      return osContinue;

   cEpgTimer* ti = currentTimer();

   if (ti && ti->Event())
   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_TIMERS_READ;
      LOCK_CHANNELS_READ;
      return AddSubMenu(new cMenuEvent(Timers, Channels, ti->Event()));
#else
      return AddSubMenu(new cMenuEvent(ti->Event()));
#endif
   }

   return osContinue;
}

eOSState cMenuEpgTimers::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
         case kInfo:
         case kBlue:   return info();

         case kOk:     state = edit();        break;
         case kGreen:  state = create();      break;
         case kYellow: state = remove();      break;
         case kRed:    state = toggleState(); break;

         case k1:      // search repetition of event
         {
            cEpgTimer* timer = currentTimer();

            if (timer->Event())
               return AddSubMenu(new cMenuEpgWhatsOn(timer->Event()));

            break;
         }

         default: break;
      }
   }

   if (!HasSubMenu() && Key != kNone && Key != kUp && Key != kDown)
   {
      int updSp = na;

      SetHelpKeys();

      if (state != osUser1)
      {
         menuDb->timerDb->clear();
         menuDb->selectMaxUpdSp->execute();
         updSp = menuDb->timerDb->getIntValue("UPDSP");
         menuDb->selectMaxUpdSp->freeResult();
      }
      else
      {
         usleep(300000);  // give VDRs a chance to process the last request
      }

      // new timer created or deleted ... ?

      if (state == osUser1 || (updSp != na && timersMaxUpdsp != updSp))
      {
         if (updSp != na)
            timersMaxUpdsp = updSp;

         refresh();

         tell(2, "DEBUG: maxUpdSp = %d; osUser1 = '%s'", timersMaxUpdsp,
              state == osUser1 ? "Y" : "N");
      }

      if (state == osUser1)
         state = osContinue;
   }

  return state;
}
