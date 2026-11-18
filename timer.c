/*
 * timer.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vector>

#include <vdr/tools.h>
#include <vdr/menu.h>

#include "update.h"
#include "ttools.h"
#include "service.h"

//***************************************************************************
// Timer Job Locks
//   - in VDR's sequence: Timers (write) -> Channels (read) -> Schedules (read)
//   - timers are locked with 'explicit modify', the state of the list is
//     only changed if SetModified() is called
//   - released by the destructor
//***************************************************************************

class cTimerJobLocks
{
   public:

      cTimerJobLocks()
      {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         timers = cTimers::GetTimersWrite(timersKey);
         timers->SetExplicitModify();
         channels = cChannels::GetChannelsRead(channelsKey);
         schedules = cSchedules::GetSchedulesRead(schedulesKey, 100/*ms*/);
#else
         timers = &Timers;
         channels = &Channels;
         schedulesLock = new cSchedulesLock(false, 100/*ms*/);
         schedules = cSchedules::Schedules(*schedulesLock);
#endif
      }

      ~cTimerJobLocks()
      {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         if (schedules)
            schedulesKey.Remove();

         channelsKey.Remove();
         timersKey.Remove();
#else
         delete schedulesLock;
#endif
      }

      bool ok() const { return schedules != nullptr; }

      cTimers* timers {};
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      const cChannels* channels {};
#else
      cChannels* channels {};
#endif
      const cSchedules* schedules {};

   private:

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey timersKey;
      cStateKey channelsKey;
      cStateKey schedulesKey;
#else
      cSchedulesLock* schedulesLock {};
#endif
};

//***************************************************************************
// Check Switch Timer
//***************************************************************************

int cUpdate::checkSwitchTimer()
{
   cMutexLock lock(&swTimerMutex);

   for (auto it = switchTimers.begin(); it != switchTimers.end(); )
   {
      SwitchTimer* swTimer = &(it->second);
      tChannelID channelId = tChannelID::FromString(swTimer->channelId.c_str());

      if (time(0) < swTimer->start)
      {
         if (!swTimer->notified && Epg2VdrConfig.switchTimerNotifyTime && time(0) >= swTimer->start - Epg2VdrConfig.switchTimerNotifyTime)
         {
            std::string channelName = swTimer->channelId;

            {
               GET_CHANNELS_READ(channels);

               if (const cChannel* channel = channels->GetByChannelID(channelId, true))
                  channelName = channel->Name();
            }

            char* buf;
            asprintf(&buf, tr("Switching in %ld seconds to '%s'"), swTimer->start-time(0), channelName.c_str());
            Skins.QueueMessage(mtInfo, buf);
            free(buf);
            swTimer->notified = yes;
         }

         it++;
         continue;
      }

      int found {no};
      int switched {no};

      {
         // channels lock only for the switch, not during the database access below

         GET_CHANNELS_READ(channels);

         if (const cChannel* channel = channels->GetByChannelID(channelId, true))
         {
            found = yes;
            tell(0, "Switching to channel '%s'", channel->Name());
            switched = cDevice::PrimaryDevice()->SwitchChannel(channel, true);
         }
      }

      if (!found)
         tell(0, "Switching to channel '%s' failed, channel not found!", swTimer->channelId.c_str());
      else if (!switched)
         Skins.QueueMessage(mtError, tr("Can't switch channel!"));

      timerDb->clear();
      timerDb->setValue("ID", it->first);
      timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

      if (timerDb->find())
      {
         timerDb->setCharValue("ACTION", taAssumed);
         timerDb->setCharValue("STATE", tsFinished);
         timerDb->getValue("INFO")->sPrintf("Swich %s", switched ? "succeeded" : "failed");
         timerDb->store();
      }
      else
      {
         tell(0, "Can't find timer '%ld', ignoring update of record", it->first);
      }

      timerDb->reset();

      it = switchTimers.erase(it);
   }

   switchTimerTrigger = no;

   return done;
}

//***************************************************************************
// Has Timer Changed
//***************************************************************************

int cUpdate::hasTimerChanged()
{
   int maxSp = 0;
   int changed = no;

   selectMaxUpdSp->execute();
   maxSp = timerDb->getIntValue("UPDSP");

   if (timersTableMaxUpdsp != maxSp)
   {
      timersTableMaxUpdsp = maxSp;
      changed = yes;

      cPluginManager::CallAllServices(EPG2VDR_TIMER_UPDATED, &timersTableMaxUpdsp);
   }

   selectMaxUpdSp->freeResult();

   return changed;
}

//***************************************************************************
// Perform Timer Jobs
//***************************************************************************

int cUpdate::performTimerJobs()
{
   int createCount = 0;
   int modifyCount = 0;
   int deleteCount = 0;
   uint64_t start = cTimeMs::Now();

   // switch timers ...

   takeSwitchTimer();

   tell(1, "Checking pending timer actions ..");

   // check if timer pending
   {
      timerDb->clear();
      timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

      if (!selectPendingTimerActions->find())
      {
         selectPendingTimerActions->freeResult();
         tell(1, ".. nothing to do");
         timerJobsUpdateTriggered = no;
         return done;
      }

      selectPendingTimerActions->freeResult();
   }

   // move this to cUpdate::init()

   std::string user = "@";
   long int webLoginEnabled = no;
   long int osdTimerNotify = no;
   int aborted = no;

   getParameter("webif", "needLogin", webLoginEnabled);

   if (webLoginEnabled)
      user += Epg2VdrConfig.user;

   getParameter(user.c_str(), "osdTimerNotify", osdTimerNotify);

   // iterate pending actions ...
   //   the VDR lists are locked for each job separately (not while fetching the next job)

   timerDb->clear();
   timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

   for (int f = selectPendingTimerActions->find(); f && dbConnected(); f = selectPendingTimerActions->fetch())
   {
      int timerid = timerDb->getIntValue("ID");
      int doneid = na;
      long eventid = timerDb->getValue("EVENTID")->isNull() ? na : timerDb->getIntValue("EVENTID");
      tChannelID channelId = tChannelID::FromString(timerDb->getStrValue("CHANNELID"));
      char requetedAction = timerDb->getStrValue("ACTION")[0];
      int deleted = no;               // -> finish delete request after releasing the locks
      int stored = no;                // -> finish create/modify request after releasing the locks
      int insert = no;
      std::string aux;

      if (!timerDb->getValue("DONEID")->isEmpty())
         doneid = timerDb->getIntValue("DONEID");

      tell(1, "DEBUG: Pending Action '%c' for timer (%d), event %ld, doneid %d", requetedAction, timerid, eventid, doneid);

      {
         cTimerJobLocks locks;

         if (!locks.ok())
         {
            tell(0, "Info: Can't get lock on schedules, skipping timer update!");
            aborted = yes;
            break;
         }

         cTimers* timers = locks.timers;
         auto channels = locks.channels;
         const cSchedules* schedules = locks.schedules;
         const cEvent* event = 0;
         cTimer* timer = getTimerById(timers, timerid);   // lookup VDRs timer object

         insert = !timer;

         // --------------------------------
         // Delete timer request

         if (requetedAction == taDelete || requetedAction == taReject)
         {
            if (timerDb->hasValue("VDRUUID", "any"))
            {
               tell(0, "Error: Ignoring delete/reject request of timer (%d) without VDRUUID", timerid);
               timerDb->getValue("INFO")->sPrintf("Error: Ignoring delete/reject request of timer (%d) without VDRUUID", timerid);
               timerDb->setCharValue("ACTION", taFailed);
               timerDb->setCharValue("STATE", tsError);
               timerDb->update();
               updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));
               continue;
            }

            // delete VDRs timer

            if (timer)
            {
               if (timer->Recording())
               {
                  timer->Skip();
#if defined (APIVERSNUM) && (APIVERSNUM >= 20302)
                  cRecordControls::Process(timers, time(0));
#else
                  cRecordControls::Process(time(0));
#endif
               }

               timers->Del(timer);
               timers->SetModified();
               deleteCount++;
               tell(0, "Deleted timer %d", timerid);
            }
            else
            {
               tell(0, "Info: Timer (%d) not found, ignoring delete/reject request", timerid);
            }

            deleted = yes;
         }

         // --------------------------------
         // Create or Modify timer request

         else if (requetedAction == taModify || requetedAction == taAdjust || requetedAction == taCreate)
         {
            cSchedule* s = 0;

            if (!timer && (requetedAction == taModify || requetedAction == taAdjust))
            {
               tell(0, "Fatal: Timer (%d) not found, skipping modify request", timerid);

               timerDb->getValue("INFO")->sPrintf("Fatal: Timer (%d) not found, skipping modify request", timerid);
               timerDb->setCharValue("ACTION", taFailed);
               timerDb->setCharValue("STATE", tsError);
               timerDb->update();
               updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));
               continue;
            }

            // get schedule (channel) and optional the event

            if (!(s = (cSchedule*)schedules->GetSchedule(channelId)))
            {
               const cChannel* channel = channels->GetByChannelID(channelId);

               tell(0, "Error: Time (%d), missing channel '%s' (%s) or channel not found, ignoring request",
                    timerid, channel ? channel->Name() : "", timerDb->getStrValue("CHANNELID"));

               timerDb->getValue("INFO")->sPrintf("Error: Timer, (%d), missing channel '%s' (%s) or channel not found, ignoring request",
                                                  timerid, channel ? channel->Name() : "", timerDb->getStrValue("CHANNELID"));
               timerDb->setCharValue("ACTION", taFailed);
               timerDb->setCharValue("STATE", tsError);
               timerDb->update();
               updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));

               // mark this channel as 'unknown'

               mapDb->clear();
               mapDb->setValue("CHANNELID", channelId.ToString());
               markUnknownChannel->execute();
               markUnknownChannel->freeResult();
               continue;
            }
#if APIVERSNUM > 20501
            if (eventid > 0 && !(event = s->GetEventById(eventid)))
#else
            if (eventid > 0 && !(event = s->GetEvent(eventid)))
#endif
            {
               const cChannel* channel = channels->GetByChannelID(channelId);

               tell(0, "Error: Timer (%d), missing event '%ld' on channel '%s' (%s), ignoring request",
                    timerid, eventid, channel ? channel->Name() : "", timerDb->getStrValue("CHANNELID"));

               timerDb->getValue("INFO")->sPrintf("Error: Timer (%d), missing event '%ld' on channel '%s' (%s), ignoring request",
                                                  timerid, eventid, channel ? channel->Name() : "", timerDb->getStrValue("CHANNELID"));
               timerDb->setCharValue("ACTION", taFailed);
               timerDb->setCharValue("STATE", tsError);
               timerDb->update();
               updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));

               // force reload of events

               tell(0, "Info: Trigger EPG full-reload due to missing event!");
               triggerEpgUpdate(yes);

               continue;
            }

            if (insert)
            {
               tell(1, "DEBUG: Create of timer %d for event %ld", timerid, eventid);

               // create timer ...

               if (event)
               {
                  // event should run at least more the 2 Minutes

                  if (getTimerByEvent(timers, event))
                     tell(0, "Warning: Timer for event (%d) '%s' already exist, creating additional timer due to request!",
                          event->EventID(), event->Title());

                  if (event->StartTime() + event->Duration() - (2 * tmeSecondsPerMinute) <= time(0))
                  {
                     tell(0, "Info: Event '%s' finished in the past, ignoring timer request!", event->Title());
                     timerDb->getValue("INFO")->sPrintf("Info: Event '%s' finished in the past, ignoring timer request!", event->Title());
                     timerDb->setCharValue("ACTION", taFailed);
                     timerDb->setCharValue("STATE", tsError);
                     timerDb->update();
                     updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));
                     continue;
                  }

                  timer = new cTimer(event);
               }
               else
               {
                  const cChannel* channel = channels->GetByChannelID(channelId);
                  timer = new cTimer(no, no, (cChannel*)channel);  // timer without a event
               }

               // reset error message in 'reason'

               if (!timerDb->getValue("INFO")->isEmpty())
                  timerDb->setValue("INFO", "");

               tell(1, "Create timer '%d'", timerid);

               if (timerDb->hasValue("VDRUUID", "any"))
               {
                  tell(0, "Took timer (%d) for uuid 'any', event (%ld)", timerid, eventid);

                  // mark 'any' record as assumed and create new with my UUID in primary key

                  timerDb->setCharValue("ACTION", taAssumed);
                  timerDb->setCharValue("STATE", tsIgnore);

                  timerDb->update();

                  // I take the timer -> new record will created!

                  timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);
                  timerDb->insert();

                  // update timerid !!!

                  int otid = timerid;
                  timerid = timerDb->getLastInsertId();
                  timerDb->setValue("ID", timerid);
                  tell(0, "DEBUG: Copied timer (%d/%ld) to (%d/%ld)", otid, eventid, timerid, timerDb->getIntValue("EVENTID"));
               }
            }
            else
            {
               // modify timer ...

               if (timerDb->hasValue("VDRUUID", "any"))
               {
                  tell(0, "Error: Ignoring modify request of timer (%d) without VDRUUID", timerid);
                  timerDb->getValue("INFO")->sPrintf("Error: Ignoring modify request of timer (%d) without VDRUUID", timerid);
                  timerDb->setCharValue("ACTION", taFailed);
                  timerDb->setCharValue("STATE", tsError);
                  timerDb->update();
                  updateTimerDone(timerid, doneid, tdsTimerCreateFailed, timerDb->getStrValue("INFO"));
                  continue;
               }

               tell(1, "Modify timer (%d), set event to (%d)", timerid, event ? (int)event->EventID() : na);

// #TODO ?!?!
//             if (event && timer->Event() != event)
//                timer->SetEvent(event);
            }

            // update the timer with data from timers table ...

            updateTimerObjectFromRow(timer, timerDb->getRow(), event);

            // add / store ...

            if (insert)
            {
               timers->Add(timer);
               createCount++;
            }
            else
            {
               if (requetedAction == taAdjust && event)
               {
                  // adjust time to given event ..

                  cTimer* dummyTimer = new cTimer(event);
                  timer->SetStart(dummyTimer->Start());
                  timer->SetStop(dummyTimer->Stop());
                  timer->SetDay(dummyTimer->Day());
                  delete dummyTimer;
               }

               modifyCount++;
            }

            timers->SetModified();
            aux = notNull(timer->Aux(), "");
            stored = yes;
         }
      }  // release the VDR locks

      // finish the request at the database

      if (deleted)
      {
         updateTimerDone(timerid, doneid, requetedAction == taDelete ? tdsTimerDeleted : tdsTimerRejected);
         timerDb->setCharValue("ACTION", taAssumed);
         timerDb->setCharValue("STATE", tsDeleted);
         timerDb->update();

         if (osdTimerNotify)
            Skins.QueueMessage(mtInfo, cString::sprintf("Timer '%s' deleted", timerDb->getStrValue("FILE")));
      }
      else if (stored)
      {
         if (insert)
            updateTimerDone(timerid, doneid, tdsTimerCreated);

         timerDb->setValue("AUX", aux.c_str());
         timerDb->setCharValue("ACTION", taAssumed);
         timerDb->setCharValue("STATE", tsPending);
         timerDb->store();

         if (osdTimerNotify)
            Skins.QueueMessage(mtInfo, cString::sprintf("Timer '%s' %s", timerDb->getStrValue("FILE"), insert ? "created" : "modified"));
      }
   }

   selectPendingTimerActions->freeResult();

   if (aborted)
      return fail;    // timerJobsUpdateTriggered stays set -> retry later

   timerJobsUpdateTriggered = no;

   tell(0, "Timer requests done, created %d, modified %d, deleted %d in %s",
        createCount, modifyCount, deleteCount, ms2Dur(cTimeMs::Now()-start).c_str());

   return success;
}

//***************************************************************************
// Take Switch Timer
//***************************************************************************

int cUpdate::takeSwitchTimer()
{
   cMutexLock lock(&swTimerMutex);

   tell(1, "Checking switch timer actions ..");

   // iterate pending switch timers ...

   timerDb->clear();
   timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

   for (int f = selectSwitchTimerActions->find(); f && dbConnected(); f = selectSwitchTimerActions->fetch())
   {
      long timerid = timerDb->getIntValue("ID");

      // to old?

      if (time(0) > timerDb->getIntValue("_STARTTIME"))
      {
         // to old, set to 'Finished'

         timerDb->setCharValue("ACTION", taAssumed);
         timerDb->setCharValue("STATE", tsFinished);
         timerDb->setValue("INFO", "Removed, to old");
         timerDb->store();
         continue;
      }

      // check state, did we have it already?

      auto it = switchTimers.find(timerid);

      // ACTION is delete?

      if (timerDb->hasCharValue("ACTION", taDelete))
      {
         if (it != switchTimers.end())
            switchTimers.erase(it);

         timerDb->setCharValue("ACTION", taAssumed);
         timerDb->setCharValue("STATE", tsDeleted);
         timerDb->store();
         continue;
      }

      // if already in map, ignore

      if (it != switchTimers.end())
         continue;

      // not in map, create it
      // independend if ACTION is 'pending', 'create' or 'modify' ore something else
      // that's special for switch timers since we have to get the 'pending' also after a vdr restart

      tell(1, "Got switch timer (%ld) for channel '%s' at '%s'",
           timerid, timerDb->getStrValue("CHANNELID"),
           l2pTime(timerDb->getIntValue("_STARTTIME")).c_str());

      switchTimers[timerid].eventId = timerDb->getIntValue("EVENTID");
      switchTimers[timerid].channelId = timerDb->getStrValue("CHANNELID");
      switchTimers[timerid].start = (timerDb->getIntValue("_STARTTIME") / 60) * 60;    // cut seconds
      switchTimers[timerid].notified = no;

      // and register timer for it

      timerThreads.push_back(new cTimerThread(&sendEvent, evtSwitchTimer,
                                              switchTimers[timerid].start, this));

      if (Epg2VdrConfig.switchTimerNotifyTime)
         timerThreads.push_back(new cTimerThread(&sendEvent, evtSwitchTimer,
                                                 switchTimers[timerid].start - Epg2VdrConfig.switchTimerNotifyTime,
                                                 this));

      // at  last confirm it

      timerDb->setCharValue("ACTION", taAssumed);
      timerDb->setCharValue("STATE", tsPending);
      timerDb->store();
   }

   selectSwitchTimerActions->freeResult();

   return done;
}

//***************************************************************************
// Timer Done to Failed
//***************************************************************************

int cUpdate::updateTimerDone(int timerid, int doneid, char state, const char* info)
{
   if (doneid == na)
      return done;

   timerDoneDb->clear();
   timerDoneDb->setValue("ID", doneid);

   if (timerDoneDb->find())
   {
      // don't toggle 'D'eleted by user to re'J'ected

      if (timerDoneDb->hasCharValue("STATE", tdsTimerDeleted) && state == tdsTimerRejected)
         return done;

      timerDoneDb->setValue("TIMERID", timerid);
      timerDoneDb->setCharValue("STATE", state);

      if (!isEmpty(info))
         timerDoneDb->setValue("INFO", info);

      timerDoneDb->update();
   }
   else
   {
      tell(0, "Warning: Id (%d) in timersdone for timer (%d) not found.",
           doneid, timerid);
   }

   return done;
}

//***************************************************************************
// Update Timer Table
//***************************************************************************

int cUpdate::updateTimerTable()
{
   cMutexLock lock(&timerMutex);
   int cnt = 0;
   int timerCount = 0;

   struct TimerInfo
   {
      const cTimer* timer {};       // only to identify the timer later, not accessed without lock
      int timerId {na};
      time_t startTime {0};
      std::string channelId;
      long eventId {na};
      std::string title;
      cDbRow* row {};               // timer data in timers table format
   };

   std::vector<TimerInfo> localTimers;
   std::vector<std::pair<const cTimer*,int>> newTimerIds;

   tell(1, "Updating table timers (and remove deleted and finished timers older than 2 days)");

   // --------------------------
   // snapshot of the local timers, lock only while copying
   //   (sequence Timers -> Channels -> Schedules, the timer accesses its channel and event)

   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_TIMERS_READ;
      LOCK_CHANNELS_READ;
      LOCK_SCHEDULES_READ;
      const cTimers* timers = Timers;
#else
      cTimers* timers = &Timers;
#endif

      timerCount = timers->Count();

      for (const cTimer* t = timers->First(); t; t = timers->Next(t))
      {
         if (!t->Local())
            continue;

         TimerInfo info;

         info.timer = t;
         info.timerId = getTimerIdOf(t);
         info.startTime = t->StartTime();
         info.channelId = *t->Channel()->GetChannelID().ToString();
         info.eventId = t->Event() ? (long)t->Event()->EventID() : na;
         info.title = t->Event() ? notNull(t->Event()->Title(), "") : notNull(t->File(), "");
         info.row = newRowFromTimer(t);

         localTimers.push_back(info);
      }
   }

   // --------------------------
   // database work without holding a VDR lock

   timerDb->deleteWhere("%s < unix_timestamp() - %d and %s in ('D','F','-')",
                        timerDb->getField("UPDSP")->getDbName(),
                        2 * tmeSecondsPerDay,
                        timerDb->getField("STATE")->getDbName());

   connection->startTransaction();

   // --------------------------
   // remove deleted timers

   timerDb->clear();
   timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

   for (int f = selectMyTimer->find(); f && dbConnected(); f = selectMyTimer->fetch())
   {
      int exist = no;

      // delete only assumed timers

      if (!timerDb->getValue("ACTION")->isEmpty() && !timerDb->hasCharValue("ACTION", taAssumed))
         continue;

      // ignore switch timer here

      if (timerDb->hasCharValue("TYPE", ttView))
         continue;

      // count my timers to detect truncated (epmty) table
      //  -> on empty table ignore known timer ids

      cnt++;

      for (const auto& info : localTimers)
      {
         // compare by timerid

         if (info.timerId != na && timerDb->getIntValue("ID") == info.timerId)
         {
            exist = yes;
            break;
         }

         // compare by start-time and channelid

         if (info.startTime == timerDb->getIntValue("STARTTIME") &&
             info.channelId == timerDb->getStrValue("CHANNELID"))
         {
            exist = yes;
            break;
         }
      }

      if (!exist)
      {
         int doneid = timerDb->getIntValue("DONEID");

         if (!timerDb->hasCharValue("STATE", tsFinished) &&
             !timerDb->hasCharValue("STATE", tsRunning) &&
             !timerDb->hasCharValue("STATE", tsError))
         {
            timerDb->setCharValue("STATE", tsDeleted);
            timerDb->update();
         }

         if (doneid > 0)
         {
            timerDoneDb->clear();
            timerDoneDb->setValue("ID", doneid);

            if (timerDoneDb->find() &&
                (timerDoneDb->hasCharValue("STATE", tdsTimerCreated) || timerDoneDb->hasCharValue("STATE", tdsTimerRequested)))
            {
               timerDoneDb->setCharValue("STATE", tdsTimerDeleted);
               timerDoneDb->update();
            }
         }
      }
   }

   selectMyTimer->freeResult();

   if (!cnt && timerCount)
      tell(0, "No timer of my uuid found, assuming cleared table and ignoring the known timerids");

   // --------------------------
   // update timers

   for (const auto& info : localTimers)
   {
      if (!dbConnected())
         break;

      int insert = yes;
      int timerId = info.timerId;

      // no timer id or not in table -> handle as insert!

      timerDb->clear();

      if (timerId != na && cnt)
      {
         timerDb->setValue("ID", timerId);
         timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

         insert = !timerDb->find();
         timerDb->clearChanged();
      }

      // update only assumed timers

      if (!insert)
      {
         if (!timerDb->getValue("ACTION")->isEmpty() && !timerDb->hasCharValue("ACTION", taAssumed))
            continue;
      }

      long dbEventId = insert ? 0 : timerDb->getIntValue("EVENTID");
      long dbEvtStart = insert ? 0 : timerDb->getIntValue("EVTSTARTTIME");
      std::string dbChannelId = insert ? "" : notNull(timerDb->getStrValue("CHANNELID"));

      timerDb->getRow()->copyValues(info.row, cDBS::ftAll);   // like updateRowByTimer() on the timer

      // VDR may assign the timer to an other event with (nearly) the same start time on the same channel,
      // e.g. one of the DVB EIT, keep the event of the database in this case - otherwise epgd don't find
      // the event anymore, rejects the timer and the search timer creates it again and again

      if (dbEventId > 0 && timerDb->getIntValue("EVENTID") != dbEventId
          && dbChannelId == notNull(timerDb->getStrValue("CHANNELID"))
          && std::abs(timerDb->getIntValue("EVTSTARTTIME") - dbEvtStart) <= 5 * tmeSecondsPerMinute)
      {
         tell(2, "Info: Timer (%d) assigned by VDR to event %ld, keeping event %ld of the database",
              timerId, timerDb->getIntValue("EVENTID"), dbEventId);
         timerDb->setValue("EVENTID", dbEventId);
         timerDb->setValue("EVTSTARTTIME", dbEvtStart);
      }

      if (insert)
      {
         timerDb->setCharValue("TYPE", ttRecord);
         timerDb->setCharValue("STATE", tsPending);
         timerDb->insert();

         // get timerid of auto increment field, it's stored to timers 'aux' data below

         timerId = timerDb->getLastInsertId();
         newTimerIds.push_back(std::make_pair(info.timer, timerId));
      }
      else if (!timerDb->getValue("STATE")->isEmpty() && strchr("DE", timerDb->getStrValue("STATE")[0]))
      {
         timerDb->setValue("STATE", "P");   // timer pending
      }

      if (insert || timerDb->getChanges())
      {
         timerDb->setValue("ID", timerId);  // set ID for update!!
         timerDb->update();                 // at least for aux (on insert case)

         tell(1, "'%s' timer for event %ld '%s' at database",
              insert ? "Insert" : "Update", info.eventId, info.title.c_str());
      }
      else
      {
         tell(3, "Nothing changed ... skipping db update");
      }

      timerDb->reset();
   }

   connection->commit();

   for (auto& info : localTimers)
      delete info.row;

   // --------------------------
   // store the timerid of new timers to the timers 'aux' data, short write lock

   if (!newTimerIds.empty())
   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_TIMERS_WRITE;
      Timers->SetExplicitModify();
      cTimers* timers = Timers;
#else
      cTimers* timers = &Timers;
#endif

      for (const auto& nt : newTimerIds)
      {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         if (!timers->Contains(nt.first))
         {
            tell(0, "Info: Timer for timerid (%d) was deleted in the meantime", nt.second);
            continue;
         }
#endif
         setTimerId((cTimer*)nt.first, nt.second);
         timers->SetModified();
      }
   }

   timerTableUpdateTriggered = no;

   tell(1, "Updating table timers done");

   return success;
}

//***************************************************************************
// Recording Changed
//***************************************************************************

int cUpdate::recordingChanged()
{
   cMutexLock lock(&runningRecMutex);

   cRunningRecording* rr = runningRecordings.First();

   tell(3, "recording changed; rr = %p", (void*)rr);

   while (rr && dbConnected())
   {
      int timerWasDeleted = no;

      // first: update timer state ..

      if (!rr->aux.empty())
      {
         int timerid = getTimerIdOf(rr->aux.c_str());
         timerDb->clear();
         timerDb->setValue("ID", timerid);
         timerDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

         tell(1, "Try to lookup timer with id %d", timerid);

         if (timerDb->find())
         {
            tell(1, "Found timer %ld", timerDb->getIntValue("ID"));

            if (timerDb->hasCharValue("STATE", tsDeleted))
               timerWasDeleted = yes;
            else
               timerDb->setCharValue("STATE", rr->failed ? tsError : rr->finished ? tsFinished : tsRunning);

            timerDb->setValue("INFO", rr->info.c_str());
            timerDb->store();
         }
         else
            tell(0, "Error: Can't lookup timer, id %d not found!", timerid);

         timerDb->reset();
      }

      // second: update done entry and remove from 'running' if 'finished' ..

      if (rr->finished)
      {
         if (rr->doneid != na)
         {
            timerDoneDb->clear();
            timerDoneDb->setValue("ID", rr->doneid);

            if (timerDoneDb->find())
            {
               if (timerWasDeleted)
                  timerDoneDb->setCharValue("STATE", tdsTimerDeleted);     // -> Recording deleted
               else if (rr->failed)
                  timerDoneDb->setCharValue("STATE", tdsRecordingFailed);  // -> Recording failed
               else
                  timerDoneDb->setCharValue("STATE", tdsRecordingDone);    // -> Recording done

               if (!timerWasDeleted && !rr->info.empty())
                  timerDoneDb->setValue("INFO", rr->info.c_str());          // e.g. "... NOT complete (63%)"

               tell(1, "Update 'done state' for doneid %ld to %s", rr->doneid, timerDoneDb->getStrValue("STATE"));

               timerDoneDb->update();
            }
            else
            {
               tell(0, "Error: Can't update timersdone state, doneid %ld not found!", rr->doneid);
            }
         }

         cRunningRecording* rrNext = runningRecordings.Next(rr);
         runningRecordings.Del(rr);
         rr = rrNext;

         continue;
      }

      rr = runningRecordings.Next(rr);
   }

   return done;
}
