/*
 * handler.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __HANDLER_H
#define __HANDLER_H

#include "update.h"

#define CHANNELMARKOBSOLETE "OBSOLETE"

//***************************************************************************
// Define tEventID again, to create a compiler error case it was defines different
//***************************************************************************

typedef u_int32_t tEventID;     // on error vdr >= 2.3.1 and patch is not applied!!

//***************************************************************************
// Mutex Try
//***************************************************************************

class cMutexTry
{
   public:

      cMutexTry()
      {
         locked = 0;

         pthread_mutexattr_t attr;
         pthread_mutexattr_init(&attr);
         pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
         pthread_mutex_init(&mutex, &attr);
      }

      ~cMutexTry()
      {
         pthread_mutex_destroy(&mutex);
      }

      int tryLock()
      {
         if (pthread_mutex_trylock(&mutex) == 0)
         {
            locked++;
            // tell(0, "[%d] got lock (%d) [%p]", cThread::ThreadId(), locked, this);

            return yes;
         }

         // tell(0, "[%d] don't got lock (%d) [%p]", cThread::ThreadId(), locked, this);

         return no;
      }

      void lock()
      {
         pthread_mutex_lock(&mutex);
         locked++;
      }

      void unlock()
      {
         if (locked)
         {
            // tell(0, "[%d] unlock (%d) [%p]", cThread::ThreadId(), locked, this);
            locked--;
            pthread_mutex_unlock(&mutex);
         }
      }

   private:

      pthread_mutex_t mutex;
      int locked;
};

//***************************************************************************
// EPG Handler
//***************************************************************************

class cEpgHandlerInstance
{
   public:

      cEpgHandlerInstance()
      {
         initialized = no;
         connection = 0;
         eventsDb = 0;
         compDb = 0;
         mapDb = 0;
         vdrDb = 0;
         endTime = 0;
         updateDelFlg = 0;
         selectDelFlg = 0;
         delCompOf = 0;
         selectMergeSp = 0;
         selectEventByStarttime = 0;

         tell(0, "Handler: Init handler instance for thread %d", cThread::ThreadId());
      }

      virtual ~cEpgHandlerInstance() { exitDb(); }

      virtual int dbConnected(int force = no)
      {
         return initialized
            && connection
            && (!force || connection->check() == success);
      }

      int checkConnection()
      {
         static int retry = 0;

         // check connection

         if (!dbConnected(yes))
         {
            // try to connect

            tell(0, "Handler: Trying to re-connect to database!");
            retry++;

            if (initDb() != success)
            {
               exitDb();

               tell(0, "Handler: Database re-connect failed!");
               return fail;
            }

            retry = 0;
            tell(0, "Handler: Connection established successfull!");
         }

         return success;
      }

      int isEpgdBusy()
      {
         int busy = no;

         if (!dbConnected())
            return true;

         vdrDb->clear();
         vdrDb->setValue("Uuid", EPGDNAME);

         if (vdrDb->find())
         {
            if (strcasecmp(vdrDb->getStrValue("State"), "busy (events)") == 0)
               busy = yes;
         }

         vdrDb->reset();

         return busy;
      }

      int initDb()
      {
         int status = success;

         exitDb();

         connection = new cDbConnection();

         vdrDb = new cDbTable(connection, "vdrs");
         if (vdrDb->open() != success) return fail;

         mapDb = new cDbTable(connection, "channelmap");
         if (mapDb->open() != success) return fail;

         eventsDb = new cDbTable(connection, "events");
         if (eventsDb->open() != success) return fail;

         compDb = new cDbTable(connection, "components");
         if (compDb->open() != success) return fail;

         // select
         //   * from events
         // where
         //   source = 'vdr'
         //   and starttime = ?
         //   and channelid = ?

         selectEventByStarttime = new cDbStatement(eventsDb);

         selectEventByStarttime->build("select ");
         selectEventByStarttime->bindAllOut(0, cDBS::ftAll);
         selectEventByStarttime->build(" from %s where source = 'vdr'", eventsDb->TableName());
         selectEventByStarttime->bind("STARTTIME", cDBS::bndIn | cDBS::bndSet, " and ");
         selectEventByStarttime->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet, " and ");

         status += selectEventByStarttime->prepare();

         // prepare statement to get mapsp
         // select
         //   mergesp from channelmap
         // where
         //   source != 'vdr'
         //   and channelid = ? limit 1

         selectMergeSp = new cDbStatement(mapDb);

         selectMergeSp->build("select ");
         selectMergeSp->bind("MergeSp", cDBS::bndOut);
         selectMergeSp->build(" from %s where source != 'vdr'", mapDb->TableName());
         selectMergeSp->bind("ChannelId", cDBS::bndIn | cDBS::bndSet, " and ");
         selectMergeSp->build(" limit 1");

         status += selectMergeSp->prepare();

         // prepare statement to mark wasted DVB events

         // update events set delflg = ?, updsp = ?
         //   where channelid = ? and source = ?
         //      and starttime+duration > ?
         //      and starttime < ?
         //      and (tableid > ? or (tableid = ? and version <> ?))

         endTime = new cDbValue("starttime+duration", cDBS::ffInt, 10);
         updateDelFlg = new cDbStatement(eventsDb);

         updateDelFlg->build("update %s set ", eventsDb->TableName());
         updateDelFlg->bind("DelFlg", cDBS::bndIn | cDBS::bndSet);
         updateDelFlg->bind("UpdFlg", cDBS::bndIn | cDBS::bndSet, ", ");
         updateDelFlg->bind("UpdSp", cDBS::bndIn | cDBS::bndSet, ", ");
         updateDelFlg->build(" where ");
         updateDelFlg->bind("ChannelId", cDBS::bndIn | cDBS::bndSet);
         updateDelFlg->bind("Source", cDBS::bndIn | cDBS::bndSet, " and ");
         updateDelFlg->bindCmp(0, endTime, ">" , " and ");
         updateDelFlg->bindCmp(0, "StartTime", 0, "<" ,  " and ");
         updateDelFlg->bindCmp(0, "TableId",   0, ">" ,  " and (");
         updateDelFlg->bindCmp(0, "TableId",   0, "=" ,  " or (");
         updateDelFlg->bindCmp(0, "Version",   0, "<>" , " and ");
         updateDelFlg->build("));");

         status += updateDelFlg->prepare();

         // we need the same as select :(

         selectDelFlg = new cDbStatement(eventsDb);

         selectDelFlg->build("select ");
         selectDelFlg->bind("EventId", cDBS::bndOut);
         selectDelFlg->build(" from %s where ", eventsDb->TableName());
         selectDelFlg->bind("ChannelId", cDBS::bndIn | cDBS::bndSet);
         selectDelFlg->bind("Source", cDBS::bndIn | cDBS::bndSet, " and ");
         selectDelFlg->bindCmp(0, endTime, ">" , " and ");
         selectDelFlg->bindCmp(0, "StartTime", 0, "<" ,  " and ");
         selectDelFlg->bindCmp(0, "TableId",   0, ">" ,  " and (");
         selectDelFlg->bindCmp(0, "TableId",   0, "=" ,  " or (");
         selectDelFlg->bindCmp(0, "Version",   0, "<>" , " and ");
         selectDelFlg->build("));");

         status += selectDelFlg->prepare();

         // -----------------
         // delete from components where eventid = ?;

         delCompOf = new cDbStatement(compDb);

         delCompOf->build("delete from %s where ", compDb->TableName());
         delCompOf->bind("EventId", cDBS::bndIn | cDBS::bndSet);
         delCompOf->build(";");

         status += delCompOf->prepare();

         if (status == success)
         {
            status += updateMemList();
            status += updateExternalIdsMap();
         }

         initialized = yes;
         return status;
      }

      int exitDb()
      {
         initialized = no;

         if (connection)
         {
            delete endTime;       endTime = 0;
            delete updateDelFlg;  updateDelFlg = 0;
            delete selectDelFlg;  selectDelFlg = 0;
            delete delCompOf;     delCompOf = 0;

            delete vdrDb;         vdrDb = 0;
            delete eventsDb;      eventsDb = 0;
            delete compDb;        compDb = 0;
            delete mapDb;         mapDb = 0;
            delete selectMergeSp; selectMergeSp = 0;
            delete selectEventByStarttime; selectEventByStarttime = 0;

            delete connection;    connection = 0;
         }

         return done;
      }

      int updateMemList()
      {
         time_t start = time(0);

         evtMemList.clear();

         // select eventid, channelid, version, tableid, delflg
         //   from events where source = 'vdr'

         cDbStatement* selectAllVdrEvents = new cDbStatement(eventsDb);

         selectAllVdrEvents->build("select ");
         selectAllVdrEvents->bind("EventId", cDBS::bndOut);
         selectAllVdrEvents->bind("ChannelId", cDBS::bndOut, ", ");
         selectAllVdrEvents->bind("Version", cDBS::bndOut, ", ");
         selectAllVdrEvents->bind("TableId", cDBS::bndOut, ", ");
         selectAllVdrEvents->bind("DelFlg", cDBS::bndOut, ", ");
         selectAllVdrEvents->build(" from %s where source = 'vdr'", eventsDb->TableName());

         if (selectAllVdrEvents->prepare() != success)
         {
            tell(0, "Handler: Aborted reading hashes from db due to prepare error");
            delete selectAllVdrEvents;
            return fail;
         }

         tell(1, "Handler: Start reading hashes from db");

         eventsDb->clear();

         for (int f = selectAllVdrEvents->find(); f; f = selectAllVdrEvents->fetch())
         {
            char evtKey[100];

            if (eventsDb->hasValue("DelFlg", "Y"))
               continue;

            sprintf(evtKey, "%ld:%s",
                    eventsDb->getIntValue("STARTTIME"),
                    eventsDb->getStrValue("CHANNELID"));

            evtMemList[evtKey].version = eventsDb->getIntValue("Version");
            evtMemList[evtKey].tableid = eventsDb->getIntValue("TableId");

            tell(4, "Handler: cInsert: '%s' with %d/%d", evtKey, evtMemList[evtKey].tableid, evtMemList[evtKey].version);
         }

         selectAllVdrEvents->freeResult();
         delete selectAllVdrEvents;

         tell(1, "Handler: Finished reading hashes from db, got %d hashes (in %ld seconds)",
              (int)evtMemList.size(), time(0)-start);

         return success;
      }

      //***************************************************************************
      // Ignore Channel
      //***************************************************************************

      virtual bool IgnoreChannel(const cChannel* Channel)
      {
         static time_t nextRetryAt = time(0);
         LogDuration l("IgnoreChannel", 5);

         // if this method is called the channel is
         // for the db an we have the active role!
         // (already checked by cEpg2VdrEpgHandler)

         // only check the DB connection here!!

         if (!dbConnected() && time(0) < nextRetryAt)
            return true;

         if (checkConnection() != success || externIdMap.size() < 1)
         {
            nextRetryAt = time(0) + 60;
            return true;
         }

         return false;
      }

      //***************************************************************************
      // Transaction Stuff
      //***************************************************************************

      virtual bool BeginSegmentTransfer(const cChannel* Channel, bool dummy)
      {
         // inital die channelid setzen

         channelId = Channel->GetChannelID();

         return false;
      }

      //***************************************************************************
      // Handled Externally
      //   hier wird festgelegt ob das Event via VDR im EPG landen soll
      //***************************************************************************

      virtual bool HandledExternally(const cChannel* Channel)
      {
         static int reportFirst = yes;

         LogDuration l("HandledExternally", 5);

         if (!channelId.Valid())
         {
            // if 'Channel' valid and 'channelId' not valid assume SegmentTransfer patch is missing
            //   -> report this ..

            if (Channel->GetChannelID().Valid() && reportFirst)
            {
               reportFirst = no;
               tell(0, "Handler: WARNING: Assume 'SegmentTransfer' patch is missing, "
                    "VDR < 2.1.0 have to be patched! Or disable the handler in the plugin setup");
            }

            return false;
         }

         // dbConnected() here okay -> if not connected we return false an the vdr will handle the event?!?

         if (dbConnected() && getExternalIdOfChannel(&channelId) != "")
            return true;

         return false;
      }

      virtual bool EndSegmentTransfer(bool Modified, bool dummy)
      {
         if (dummy || !dbConnected() || !connection->inTransaction())
            return false;

         if (Modified)
            connection->commit();
         else
            connection->rollback();

         if (Epg2VdrConfig.loglevel > 2)
            connection->showStat("handler");

         return false;
      }

      //***************************************************************************
      // Is Update
      //***************************************************************************

      virtual bool IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version)
      {
         char evtKey[100];
         LogDuration l("IsUpdate", 5);

         if (!dbConnected())
            return false;

         if (!isZero(getExternalIdOfChannel(&channelId).c_str()) && StartTime > time(0) + 4 * tmeSecondsPerDay)
            return false;

         sprintf(evtKey, "%ld:%s", StartTime, (const char*)channelId.ToString());

         if (evtMemList.find(evtKey) == evtMemList.end())
         {
            tell(4, "Handler: Handle insert (or starttime update) of event '%s' (%d) for channel '%s'",
                 evtKey, EventID, (const char*)channelId.ToString());

            if (!connection->inTransaction())
               connection->startTransaction();

            return true;
         }

         uchar currentTableId = std::max(uchar(evtMemList[evtKey].tableid), uchar(0x4E));

         // skip bigger ids as current

         if (TableID > currentTableId)
         {
            tell(4, "Handler: Ignoring update with older tableid (%d) for event '%s' (%d)(has tableid %d)",
                 TableID, evtKey, EventID, evtMemList[evtKey].tableid);
            return false;
         }

         // skip if version an tid identical

         if (currentTableId == TableID && evtMemList[evtKey].version == Version)
         {
            tell(4, "Handler: Ignoring 'non' update for event '%s' (%d), version still (%d)",
                 evtKey, EventID, Version);
            return false;
         }

         if (Epg2VdrConfig.loglevel > 3)
            tell(4, "Handler: Handle update of event '%s' (%d)  %d/%d - %d/%d", evtKey, EventID,
                 Version, TableID,
                 evtMemList[evtKey].version, evtMemList[evtKey].tableid);

         if (!connection->inTransaction())
            connection->startTransaction();

         return true;
      }

      //***************************************************************************
      // Handle Event
      //***************************************************************************

      virtual bool HandleEvent(cEvent* event)
      {
         int oldStartTime = 0;

         if (!dbConnected() || !event || !channelId.Valid())
            return false;

         LogDuration l("HandleEvent", 5);

         // External-ID:
         //     na  -> Kanal nicht konfiguriert -> wird ignoriert
         //    = 0  -> wird vom Sender genommen -> und in der DB abgelegt
         //    > 0  -> echte externe ID -> wird von extern ins epg übertragen,
         //            das Sender EPG wird nur (zusätzlich) in der DB gehalten

         // Events der Kanäle welche nicht in der map zu finden sind ignorieren

         if (getExternalIdOfChannel(&channelId) == "")
            return false;

         if (!connection->inTransaction())
         {
            tell(0, "Handler: Error missing tact in HandleEvent");
            return false;
         }

         std::string comp;

         // lookup the event ..
         //   first try by starttime

         eventsDb->clear();
         eventsDb->setValue("CHANNELID", channelId.ToString());
         eventsDb->setValue("STARTTIME", event->StartTime());

         int insert = !selectEventByStarttime->find();

         if (insert)
         {
            // try lookup by eventid

            eventsDb->setValue("CHANNELID", channelId.ToString());
            eventsDb->setBigintValue("EVENTID", (long)event->EventID());

            if (eventsDb->find())
            {
               // fount => NOT a insert, just a update of the starttime

               insert = no;
               oldStartTime = eventsDb->getIntValue("STARTTIME");
               eventsDb->setValue("STARTTIME", event->StartTime());
            }
         }

         // reinstate ??

         if (eventsDb->hasValue("DELFLG", "Y"))
         {
            char updFlg = Us::usPassthrough;

            mapDb->clear();
            mapDb->setValue("CHANNELID", channelId.ToString());

            if (selectMergeSp->find())
            {
               time_t mergesp = mapDb->getIntValue("MERGESP");
               long masterid = eventsDb->getIntValue("MASTERID");
               long useid = eventsDb->getIntValue("USEID");

               if (event->StartTime() > mergesp)
                  updFlg = Us::usRemove;
               else if (event->StartTime() <= mergesp && masterid == useid)
                  updFlg = Us::usActive;
               else if (event->StartTime() <= mergesp && masterid != useid)
                  updFlg = Us::usLink;
            }

            eventsDb->setCharValue("UPDFLG", updFlg);
            eventsDb->getValue("DELFLG")->setNull();
         }

         if (!insert && std::abs(event->StartTime() - eventsDb->getIntValue("StartTime")) > 6*tmeSecondsPerHour)
         {
            tell(3, "Handler: Info: Start time of %d/%s - '%s' moved %ld hours from %s to %s - '%s'",
                 event->EventID(), (const char*)channelId.ToString(),
                 eventsDb->getStrValue("Title"),
                 (event->StartTime() - eventsDb->getIntValue("StartTime")) / tmeSecondsPerHour,
                 l2pTime(eventsDb->getIntValue("StartTime")).c_str(),
                 l2pTime(event->StartTime()).c_str(),
                 event->Title());
         }

         time_t end = event->StartTime() + event->Duration();

         if (!insert
             && end < time(0) - 2*tmeSecondsPerHour
             && eventsDb->getIntValue("StartTime") >  time(0)
             && event->StartTime() < eventsDb->getIntValue("StartTime"))
         {
            tell(1, "Handler: Info: Got update of %d/%s with startime more than 2h in past "
                 "(%s/%d) before (%s), ignoring update, set delflg instead",
                 event->EventID(), (const char*)channelId.ToString(),
                 l2pTime(event->StartTime()).c_str(), event->Duration(),
                 l2pTime(eventsDb->getIntValue("StartTime")).c_str());

            eventsDb->setValue("DelFlg", "Y");
            eventsDb->setCharValue("UpdFlg", Us::usDelete);
         }
         else
         {
            eventsDb->setValue("StartTime", event->StartTime());
         }

         if (!insert && eventsDb->getIntValue("VPS") != event->Vps())
            tell(1, "Handler: Toggle vps flag for '%s' at '%s' from %s to %s",
                 event->Title(), (const char*)channelId.ToString(),
                 l2pTime(eventsDb->getIntValue("VPS")).c_str(), l2pTime(event->Vps()).c_str());

         eventsDb->setValue("Source", "vdr");
         eventsDb->setValue("TableId", event->TableID());
         eventsDb->setValue("Version", event->Version());
         eventsDb->setValue("Title", event->Title());
         eventsDb->setValue("LongDescription", event->Description());
         eventsDb->setValue("Duration", event->Duration());
         eventsDb->setValue("ParentalRating", event->ParentalRating());
         eventsDb->setValue("Vps", event->Vps());

         if (!isEmpty(event->ShortText()))
            eventsDb->setValue("ShortText", event->ShortText());

         // contents

         char contents[MaxEventContents * 10] = "";

         for (int i = 0; i < MaxEventContents; i++)
            if (event->Contents(i) > 0)
               sprintf(eos(contents), "0x%x,", event->Contents(i));

         if (!isEmpty(contents))
            eventsDb->setValue("CONTENTS", contents);

         // components ..

         compDb->clear();
         compDb->setBigintValue("EVENTID", eventsDb->getBigintValue("EVENTID"));
         delCompOf->execute();

         if (event->Components())
         {
            for (int i = 0; i < event->Components()->NumComponents(); i++)
            {
               tComponent* p = event->Components()->Component(i);

               compDb->clear();
               compDb->setBigintValue("EventId", eventsDb->getBigintValue("EVENTID"));
               compDb->setValue("ChannelId", channelId.ToString());
               compDb->setValue("Stream", p->stream);
               compDb->setValue("Type", p->type);
               compDb->setValue("Lang", p->language);
               compDb->setValue("Description", p->description ? p->description : "");
               compDb->store();
            }
         }

         // compressed ..

         if (event->Title())
         {
            comp = event->Title();
            prepareCompressed(comp);
            eventsDb->setValue("COMPTITLE", comp.c_str());
         }

         if (!isEmpty(event->ShortText()))
         {
            comp = event->ShortText();
            prepareCompressed(comp);
            eventsDb->setValue("COMPSHORTTEXT", comp.c_str());
         }

         if (!isEmpty(event->Description()))
         {
            comp = event->Description();
            prepareCompressed(comp);
            eventsDb->setValue("COMPLONGDESCRIPTION", comp.c_str());
         }

         if (insert)
         {
            eventsDb->setValue("UseId", 0L);
            eventsDb->setCharValue("UpdFlg", Us::usPassthrough); // default (vdr:000 events)

            mapDb->clear();
            mapDb->setValue("ChannelId", channelId.ToString());

            if (selectMergeSp->find())
            {
               // vdr event for merge with external event

               time_t mergesp = mapDb->getIntValue("MergeSp");
               eventsDb->setCharValue("UpdFlg", event->StartTime() > mergesp ? Us::usInactive : Us::usActive);
            }

            eventsDb->insert();
         }
         else
         {
            eventsDb->update();
         }

         if (!insert && oldStartTime)
         {
            char evtKey[100];
            sprintf(evtKey, "%ld:%s", (long)oldStartTime, (const char*)channelId.ToString());
            evtMemList.erase(evtKey);
            tell(4, "Handler: cRemove: '%s'  (due to starttime update)", evtKey);
         }

         // update hash map

         char evtKey[100];

         sprintf(evtKey, "%ld:%s", (long)event->StartTime(), (const char*)channelId.ToString());

         evtMemList[evtKey].version = event->Version();
         evtMemList[evtKey].tableid = event->TableID();

         tell(4, "Handler: cUpdate/cInsert: '%s' to %d/%d", evtKey, evtMemList[evtKey].tableid, evtMemList[evtKey].version);

         selectEventByStarttime->freeResult();

         return true;
      }

      //***************************************************************************
      // Drop Outdated
      //***************************************************************************

      virtual bool DropOutdated(cSchedule* Schedule, time_t SegmentStart,
                                time_t SegmentEnd, uchar TableID, uchar Version)
      {
         // we handle only vdr events here (provided by DVB)

         if (!dbConnected() || getExternalIdOfChannel(&channelId) == "")
            return false;

         if (!connection->inTransaction())
         {
            tell(0, "Handler: Error missing tact DropOutdated");
            return false;
         }

         if (SegmentStart <= 0 || SegmentEnd <= 0)
            return false;

         LogDuration l("DropOutdated", 5);

         eventsDb->clear();
         eventsDb->setValue("ChannelId", Schedule->ChannelID().ToString());
         eventsDb->setValue("Source", "vdr");
         eventsDb->setValue("UpdSp", time(0));
         eventsDb->setValue("StartTime", SegmentEnd);
         eventsDb->setValue("TableId", TableID);
         eventsDb->setValue("Version", Version);
         endTime->setValue(SegmentStart);

         // remove segment from cache

         for (int f = selectDelFlg->find(); f; f = selectDelFlg->fetch())
         {
            char evtKey[100];

            sprintf(evtKey, "%ld:%s",
                    eventsDb->getIntValue("STARTTIME"),
                    eventsDb->getStrValue("CHANNELID"));

            evtMemList.erase(evtKey);
            tell(4, "Handler: cRemove: '%s'", evtKey);
         }

         selectDelFlg->freeResult();

         eventsDb->setValue("DELFLG", "Y");
         eventsDb->setCharValue("UPDFLG", Us::usDelete);

         // mark segment as deleted

         updateDelFlg->execute();

         return true;
      }

   private:

      std::string getExternalIdOfChannel(tChannelID* channelId)
      {
         char* id = 0;
         std::string extid = "";

         if (externIdMap.size() < 1)
            return "";

         id = strdup(channelId->ToString());

         if (externIdMap.find(id) != externIdMap.end())
            extid = externIdMap[id];

         free(id);

         return extid;
      }

      int updateExternalIdsMap()
      {
         externIdMap.clear();
         tell(1, "Handler: Start reading external ids from db");
         mapDb->clear();

         // select extid, channelid
         //   from channelmap

         cDbStatement* selectAll = new cDbStatement(mapDb);

         selectAll->build("select ");
         selectAll->bind("ExternalId", cDBS::bndOut);
         selectAll->bind("ChannelId", cDBS::bndOut, ", ");
         selectAll->build(" from %s", mapDb->TableName());

         if (selectAll->prepare() != success)
         {
            tell(0, "Handler: Reading external id's from db aborted due to prepare error");
            delete selectAll;
            return fail;
         }

         for (int f = selectAll->find(); f; f = selectAll->fetch())
         {
            const char* extid = mapDb->getStrValue("ExternalId");
            const char* chan = mapDb->getStrValue("ChannelId");

            externIdMap[chan] = extid;
         }

         tell(1, "Handler: Finished reading external id's from db, got %d id's",
              (int)externIdMap.size());

         selectAll->freeResult();
         delete selectAll;

         return success;
      }

      struct MemMap
      {
         int version;
         int tableid;
      };

      std::map<std::string,MemMap> evtMemList;
      std::map<std::string,std::string> externIdMap;
      tChannelID channelId;

      int initialized;
      cDbConnection* connection;

      cDbTable* eventsDb;
      cDbTable* mapDb;
      cDbTable* vdrDb;
      cDbTable* compDb;

      cDbValue* endTime;
      cDbStatement* updateDelFlg;
      cDbStatement* selectDelFlg;
      cDbStatement* delCompOf;
      cDbStatement* selectMergeSp;
      cDbStatement* selectEventByStarttime;

      // cUpdate* update;
};

//***************************************************************************
// cEpg2VdrEpgHandler
//***************************************************************************

class cEpg2VdrEpgHandler : public cEpgHandler
{
   public:

      cEpg2VdrEpgHandler() : cEpgHandler() { active = no; }

      ~cEpg2VdrEpgHandler()
      {
         handlerMutex.lock();

         std::map<tThreadId,cEpgHandlerInstance*>::iterator it;

         for (it = handler.begin(); it != handler.end(); ++it)
         {
            delete handler[it->first];
            handler[it->first] = 0;
         }

         handlerMutex.unlock();
      }

      int getActive()           { return active; }
      void setActive(int state) { active = state; }

      //***************************************************************************
      // Ignore Channel
      //   - includes the NOEPG feature - so we don't need the noepg plugin
      //***************************************************************************

      virtual bool IgnoreChannel(const cChannel* Channel)
      {
         // IgnoreChannel ist der erste Anlaufpunkt des EIT handlers,
         // wird hier ignoriert bricht die Verarbeitung des Kanals direkt ab

         tChannelID tmp = Channel->GetChannelID();

         if (externIdMap.size() < 1)
            return true;

         // Kanäle welche nicht in der map konfiguriert sind (na) werden nicht in der DB verwaltet
         // abhängig von blacklist durchgelassen oder ignoriert

         if (getExternalIdOfChannel(&tmp) == "")
            return Epg2VdrConfig.blacklist;

         // ingnore - wenn nicht aktiv

         if (!active)
            return true;

         // Handler check - only to check if the DB connection is fine

         if (getHandler()->IgnoreChannel(Channel))
            return true;

         return false;
      }

      virtual bool HandledExternally(const cChannel* Channel)
      {
         return getHandler()->HandledExternally(Channel);
      }

      virtual bool BeginSegmentTransfer(const cChannel *Channel, bool dummy)
      {
         // solange die Datenbank mit einem anderen handler thread
         // beschäftigt ist (ergo wir den lock nicht bekommen) erst mal ignorieren

         if (!handlerMutex.tryLock())
            return false;

         getHandler()->BeginSegmentTransfer(Channel, dummy);

         return true;
      }

      virtual bool EndSegmentTransfer(bool Modified, bool dummy)
      {
         handlerMutex.unlock();
         getHandler()->EndSegmentTransfer(Modified, dummy);
         return false;
      }

      virtual bool IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version)
      {

         cEpgHandlerInstance* h = getHandler();
         return h->IsUpdate(EventID, StartTime, TableID, Version);
      }

      virtual bool HandleEvent(cEvent* event)
      {
         cEpgHandlerInstance* h = getHandler();
         return h->HandleEvent(event);
      }

      virtual bool DropOutdated(cSchedule* Schedule, time_t SegmentStart,
                                time_t SegmentEnd, uchar TableID, uchar Version)
      {
         cEpgHandlerInstance* h = getHandler();
         return h->DropOutdated(Schedule, SegmentStart, SegmentEnd, TableID, Version);
      }

      int updateExternalIdsMap(cDbTable* mapDb)
      {
         cMutexLock lock(&mapMutex);

         externIdMap.clear();
         tell(1, "Handler: Start reading external ids from db");
         mapDb->clear();

         // select extid, channelid
         //   from channelmap

         cDbStatement* selectAll = new cDbStatement(mapDb);

         selectAll->build("select ");
         selectAll->bind("EXTERNALID", cDBS::bndOut);
         selectAll->bind("CHANNELID", cDBS::bndOut, ", ");
         selectAll->bind("SOURCE", cDBS::bndOut, ", ");
         selectAll->bind("CHANNELNAME", cDBS::bndOut, ", ");
         selectAll->bind("FORMAT", cDBS::bndOut, ", ");
         selectAll->bind("MERGE", cDBS::bndOut, ", ");
         selectAll->build(" from %s", mapDb->TableName());

         if (selectAll->prepare() != success)
         {
            tell(0, "Handler: Reading external id's from db aborted due to prepare error");
            delete selectAll;
            return fail;
         }

         // channel lock scope
         {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
            LOCK_CHANNELS_READ;
            const cChannels* channels = Channels;
#else
            cChannels* channels = &Channels;
#endif

            for (int f = selectAll->find(); f; f = selectAll->fetch())
            {
               std::string extid = mapDb->getStrValue("ExternalId");

               const char* strChannelId = mapDb->getStrValue("ChannelId");
               const cChannel* channel = channels->GetByChannelID(tChannelID::FromString(strChannelId));

               // update channelname in channelmap

               if (channel && !isEmpty(channel->Name()) &&
                   (!mapDb->hasValue("CHANNELNAME", channel->Name()) || mapDb->getValue("FORMAT")->isNull()))
               {
                  mapDb->find();              // get all fields from table (needed for update)!

                  mapDb->setValue("CHANNELNAME", channel->Name());

                  if (strstr(channel->Name(), CHANNELMARKOBSOLETE))
                     mapDb->setValue("UNKNOWNATVDR", yes);

                  if (strstr(channel->Name(), "HD"))
                     mapDb->setValue("FORMAT", "HD");
                  else if (strstr(channel->Name(), "3D"))
                     mapDb->setValue("FORMAT", "3D");
                  else
                     mapDb->setValue("FORMAT", "SD");

                  mapDb->update();
                  mapDb->reset();
               }

               // we should get the merge > 1 channels already via a merge 1 entry of the channelmap!

               if (mapDb->getIntValue("Merge") > 1)
                  continue;

               // insert into map

               externIdMap[strChannelId] = extid;
            }
         }

         tell(1, "Handler: Finished reading external id's from db, got %d id's",
              (int)externIdMap.size());

         selectAll->freeResult();
         delete selectAll;

         return success;
      }

      static cEpg2VdrEpgHandler* getSingleton()
      {
         if (!singleton)
            singleton = new cEpg2VdrEpgHandler();

         return singleton;
      }

   private:

      std::string getExternalIdOfChannel(tChannelID* channelId)
      {
         char* id = 0;
         std::string extid = "";

         cMutexLock lock(&mapMutex);

         if (externIdMap.size() < 1)
            return "";

         id = strdup(channelId->ToString());

         if (externIdMap.find(id) != externIdMap.end())
            extid = externIdMap[id];

         free(id);

         return extid;
      }

      cEpgHandlerInstance* getHandler()
      {
         if (handler.find(cThread::ThreadId()) == handler.end())
            handler[cThread::ThreadId()] = new cEpgHandlerInstance();

         return handler[cThread::ThreadId()];
      }

      std::map<std::string,std::string> externIdMap;
      std::map<tThreadId,cEpgHandlerInstance*> handler;
      int active;
      cMutex mapMutex;
      cMutexTry handlerMutex;

      static cEpg2VdrEpgHandler* singleton;
};

//***************************************************************************
#endif // __HANDLER_H
