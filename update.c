/*
 * update.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <locale.h>
#include <unordered_set>

#include <vdr/videodir.h>
#include <vdr/tools.h>

#include "lib/vdrlocks.h"
#include "lib/xml.h"
#include "epg2vdr.h"
#include "update.h"
#include "handler.h"

//***************************************************************************
// Events AUX Fields - stored as XML in cEvent:aux
//***************************************************************************

const char* cUpdate::auxFields[] =
{
// field name                type    max size

   "imagecount",          // int
   "scrseriesid",         // int
   "scrseriesepisode",    // int
   "scrmovieid",          // int
   "numrating",           // int

   "year",                // ascii     10
   "category",            // ascii     50
   "country",             // ascii     50
   "audio",               // ascii     50

   "txtrating",           // ascii    100
   "genre",               // ascii    100
   "flags",               // ascii    100
   "commentator",         // ascii    200
   "tipp",                // ascii    250
   "rating",              // ascii    250
   "moderator",           // ascii    250
   "music",               // ascii    250
   "screenplay",          // ascii    500
   "shortreview",         // ascii    500

   "guest",               // text    1000
   "producer",            // text    1000
   "camera",              // text    1000
   "director",            // text    1000
   "topic",               // ascii   1000

   "other",               // text    2000
   "shortdescription",    // mtext   3000
   "shorttext",           // ascii    300
   "actor",               // mtext   5000

   "episodename",         // ascii    100
   "episodeshortname",    // ascii    100
   "episodepartname",     // ascii    300
   "episodeextracol1",    // ascii    250
   "episodeextracol2",    // ascii    250
   "episodeextracol3",    // ascii    250
   "episodeseason",       // int
   "episodepart",         // int
   "episodeparts",        // int
   "episodenumber",       // int

   0
};

//***************************************************************************
// ctor
//***************************************************************************

cUpdate::cUpdate(cPluginEPG2VDR* aPlugin)
   : cThread("epg2vdr-update")
{
   // thread / update control

   plugin = aPlugin;
   connection = 0;
   loopActive = no;
   timerJobsUpdateTriggered = yes;
   timerTableUpdateTriggered = yes;
   recordingStateChangedTrigger = yes;
   updateRecFolderOptionTrigger = no;

   storeAllRecordingInfoFilesTrigger = no;
   recordingFullReloadTrigger = no;
   manualTrigger = no;
   videoBasePath = 0;
   dbReconnectTriggered = no;
   switchTimerTrigger = no;

   fullreload = no;
   epgdBusy = yes;
   epgdState = cEpgdState::esUnknown;
   mainActPending = no;
   eventsPending = no;
   nextEpgdUpdateAt = 0;

   lastUpdateAt = 0;
   lastEventsUpdateAt = 0;
   lastRecordingCount = 0;
   lastRecordingDeleteAt = 0;
   timersTableMaxUpdsp = 0;

   //

   compDb = 0;
   eventsDb = 0;
   useeventsDb = 0;
   fileDb = 0;
   imageDb = 0;
   imageRefDb = 0;
   episodeDb = 0;
   vdrDb = 0;
   mapDb = 0;
   timerDb = 0;
   timerDoneDb = 0;
   recordingDirDb = 0;
   recordingListDb = 0;
   recordingImagesDb = 0;

   selectMasterVdr = 0;
   selectAllImages = 0;
   selectUpdEvents = 0;
   selectEventById = 0;
   selectAllChannels = 0;
   selectChannelById = 0;
   markUnknownChannel = 0;
   selectComponentsOf = 0;
   deleteTimer = 0;
   selectMyTimer = 0;
   selectRecordings = 0;
   selectImagesOfRecording = 0;
   selectRecForInfoUpdate = 0;
   selectTimerByEvent = 0;
   selectTimerById = 0;
   selectTimerByDoneId = 0;
   selectMaxUpdSp = 0;
   selectPendingTimerActions = 0;
   selectSwitchTimerActions = 0;

   viewDescription = 0;
   viewMergeSource = 0;
   viewLongDescription = 0;

   //

   epgimagedir = 0;
   withutf8 = no;
   handlerMaster = no;

   // check/create uuid

   if (isEmpty(Epg2VdrConfig.uuid))
   {
      sstrcpy(Epg2VdrConfig.uuid, getUniqueId(), sizeof(Epg2VdrConfig.uuid));
      plugin->SetupStore("Uuid", Epg2VdrConfig.uuid);
      Setup.Save();

      tell(0, "Initially created uuid '%s'", Epg2VdrConfig.uuid);
   }
}

//***************************************************************************
// dtor
//***************************************************************************

cUpdate::~cUpdate()
{
   if (loopActive)
      Stop();

   free(epgimagedir);
}

//***************************************************************************
// Init
//***************************************************************************

int cUpdate::init()
{
   char* dictPath = 0;
   char* pdir;
   char* lang;

   lang = setlocale(LC_CTYPE, 0);

   if (lang)
   {
      tell(0, "Set locale to '%s'", lang);

      if ((strcasestr(lang, "UTF-8") != 0) || (strcasestr(lang, "UTF8") != 0))
      {
         tell(0, "detected UTF-8");
         withutf8 = yes;
      }
   }
   else
   {
      tell(0, "Reseting locale for LC_CTYPE failed.");
   }

   strcpy(imageExtension, "jpg");
   asprintf(&epgimagedir, "%s/epgimages", EPG2VDR_DATA_DIR);
   asprintf(&pdir, "%s/images", epgimagedir);

   if (!(DirectoryOk(pdir) || MakeDirs(pdir, true)))
      tell(0, "could not access or create Directory %s", pdir);

   free(pdir);

   // initialize the dictionary

   asprintf(&dictPath, "%s/epg.dat", cPlugin::ConfigDirectory("epg2vdr/"));

   if (dbDict.in(dictPath) != success)
   {
      tell(0, "Fatal: Dictionary not loaded, aborting!");
      return fail;
   }

   tell(0, "Dictionary '%s' loaded", dictPath);
   free(dictPath);

   // init database ...

   cDbConnection::setEncoding(withutf8 ? "utf8" : "latin1"); // mysql uses latin1 for ISO8851-1
   cDbConnection::setHost(Epg2VdrConfig.dbHost);
   cDbConnection::setPort(Epg2VdrConfig.dbPort);
   cDbConnection::setName(Epg2VdrConfig.dbName);
   cDbConnection::setUser(Epg2VdrConfig.dbUser);
   cDbConnection::setPass(Epg2VdrConfig.dbPass);
   cDbConnection::setConfPath(cPlugin::ConfigDirectory("epg2vdr/"));

   videoBasePath = cVideoDirectory::Name();

   return success;
}

//***************************************************************************
// Exit
//***************************************************************************

int cUpdate::exit()
{
   exitDb();

   return success;
}

cDbFieldDef imageSizeDef("image", "image", cDBS::ffUInt, 0, cDBS::ftData);

//***************************************************************************
// Init/Exit Database Connections
//***************************************************************************

int cUpdate::initDb()
{
   int status = success;

   if (!connection)
      connection = new cDbConnection();

   vdrDb = new cDbTable(connection, "vdrs");
   if (vdrDb->open() != success) return fail;

   // DB-API check

   vdrDb->clear();
   vdrDb->setValue("UUID", EPGDNAME);

   if (!vdrDb->find())
   {
      tell(0, "Can't lookup epgd information, start epgd to create the tables first! Aborting now.");
      return fail;
   }

   vdrDb->reset();

   if (vdrDb->getIntValue("DBAPI") != DB_API)
   {
      if (vdrDb->getIntValue("DBAPI") < DB_API)
         tell(0,  "Your database has version %d, epg2vdr expects version %d. Please make sure, epgd and "
              "epg2vdr use the same version and the database is properly updated",
              (int)vdrDb->getIntValue("DBAPI"), DB_API);
      else
         tell(0, "Found dbapi %d, expected %d, please update me! Aborting now.",
              (int)vdrDb->getIntValue("DBAPI"), DB_API);

      return fail;
   }

   // open tables ..

   mapDb = new cDbTable(connection, "channelmap");
   if (mapDb->open() != success) return fail;

   fileDb = new cDbTable(connection, "fileref");
   if (fileDb->open() != success) return fail;

   imageDb = new cDbTable(connection, "images");
   if (imageDb->open() != success) return fail;

   imageRefDb = new cDbTable(connection, "imagerefs");
   if (imageRefDb->open() != success) return fail;

   episodeDb = new cDbTable(connection, "episodes");
   if (episodeDb->open() != success) return fail;

   eventsDb = new cDbTable(connection, "events");
   if (eventsDb->open() != success) return fail;

   useeventsDb = new cDbTable(connection, "useevents");
   if (useeventsDb->open() != success) return fail;

   compDb = new cDbTable(connection, "components");
   if (compDb->open() != success) return fail;

   timerDb = new cDbTable(connection, "timers");
   if (timerDb->open() != success) return fail;

   timerDoneDb = new cDbTable(connection, "timersdone");
   if (timerDoneDb->open() != success) return fail;

   recordingDirDb = new cDbTable(connection, "recordingdirs");
   if (recordingDirDb->open() != success) return fail;

   recordingListDb = new cDbTable(connection, "recordinglist");
   if (recordingListDb->open() != success) return fail;

   recordingImagesDb = new cDbTable(connection, "recordingimages");
   if (recordingImagesDb->open() != success) return fail;

   if ((status = cParameters::initDb(connection)) != success)
      return status;

   // -------------------------------------------
   // init db values

   viewDescription = new cDbValue("description", cDBS::ffText, 50000);
   viewMergeSource = new cDbValue("mergesource", cDBS::ffAscii, 25);
   viewLongDescription = new cDbValue("longdescription", cDBS::ffText, 50000);

   // -------------------------------------------
   // init statements

   tell(2, "Prepare statements ...");

   selectMasterVdr = new cDbStatement(vdrDb);

   // select uuid, name from vdrs
   //    where upper(master) = 'Y' and uuid <>  ?;

   selectMasterVdr->build("select ");
   selectMasterVdr->bind("UUID", cDBS::bndOut);
   selectMasterVdr->bind("NAME", cDBS::bndOut, ", ");
   selectMasterVdr->build(" from %s where upper(%s) = 'Y' and ",
                          vdrDb->TableName(), vdrDb->getField("MASTER")->getDbName());
   selectMasterVdr->bindCmp(0, "Uuid", 0, "<>");

   status += selectMasterVdr->prepare();

   // all images

   selectAllImages = new cDbStatement(imageRefDb);

   // prepare fields

   imageSize.setField(&imageSizeDef);
   imageUpdSp.setField(imageDb->getField("UpdSp"));
   masterId.setField(eventsDb->getField("MasterId"));

   // select e.masterid, r.imagename, r.eventid, r.lfn, length(i.image)
   //      from imagerefs r, images i, events e
   //      where i.imagename = r.imagename
   //         and e.eventid = r.eventid
   //         and (i.updsp > ? or r.updsp > ?)

   selectAllImages->build("select ");
   selectAllImages->setBindPrefix("e.");
   selectAllImages->bind(&masterId, cDBS::bndOut);
   selectAllImages->setBindPrefix("r.");
   selectAllImages->bind("IMGNAME", cDBS::bndOut, ", ");
   selectAllImages->bind("IMGNAMEFS", cDBS::bndOut, ", ");
   selectAllImages->bind("EVENTID", cDBS::bndOut, ", ");
   selectAllImages->bind("LFN", cDBS::bndOut, ", ");
   selectAllImages->setBindPrefix("i.");
   selectAllImages->build(", length(");
   selectAllImages->bind(&imageSize, cDBS::bndOut);
   selectAllImages->build(")");
   selectAllImages->clrBindPrefix();
   selectAllImages->build(" from %s r, %s i, %s e where ",
                          imageRefDb->TableName(), imageDb->TableName(), eventsDb->TableName());
   selectAllImages->build("e.%s = r.%s and i.%s = r.%s and (",
                          eventsDb->getField("EVENTID")->getDbName(),
                          imageRefDb->getField("EVENTID")->getDbName(),
                          imageDb->getField("IMGNAME")->getDbName(),
                          imageRefDb->getField("IMGNAME")->getDbName());
   selectAllImages->bindCmp("i", &imageUpdSp, ">");
   selectAllImages->build(" or ");
   selectAllImages->bindCmp("r", "UPDSP", 0, ">");
   selectAllImages->build(")");

   status += selectAllImages->prepare();

   // select distinct channelid, channelname
   //   from channelmap;

   selectAllChannels = new cDbStatement(mapDb);

   selectAllChannels->build("select distinct ");
   selectAllChannels->bind("CHANNELID", cDBS::bndOut);
   selectAllChannels->bind("CHANNELNAME", cDBS::bndOut, ", ");
   selectAllChannels->build(" from %s", mapDb->TableName());

   status += selectAllChannels->prepare();

   // select distinct channelid, channelname
   //   from channelmap
   //    where channleid = ?;

   selectChannelById = new cDbStatement(mapDb);

   selectChannelById->build("select distinct ");
   selectChannelById->bind("CHANNELID", cDBS::bndOut);
   selectChannelById->bind("CHANNELNAME", cDBS::bndOut, ", ");
   selectChannelById->build(" from %s where ", mapDb->TableName());
   selectChannelById->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet);

   status += selectChannelById->prepare();

   // update channlemap
   //   set unknownatvdr = 1
   //   where channelid = ?

   markUnknownChannel = new cDbStatement(mapDb);

   markUnknownChannel->build("update %s set ", mapDb->TableName());
   markUnknownChannel->bind("UNKNOWNATVDR", cDBS::bndIn | cDBS::bndSet);
   markUnknownChannel->build(" where ");
   markUnknownChannel->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet);

   status += markUnknownChannel->prepare();

   // select changed events

   selectUpdEvents = new cDbStatement(eventsDb);

   // select useid, eventid, source, delflg, updflg, fileref,
   //        tableid, version, title, shorttext, starttime,
   //        duration, parentalrating, vps, description
   //    from eventsview
   //      where
   //        channelid = ?
   //        and updsp > ?
   //        and updflg in (.....)

   selectUpdEvents->build("select ");
   selectUpdEvents->bind("USEID", cDBS::bndOut);
   // selectUpdEvents->bind("MASTERID", cDBS::bndOut, ", ");
   selectUpdEvents->bind("EVENTID", cDBS::bndOut, ", ");
   selectUpdEvents->bind("SOURCE", cDBS::bndOut, ", ");
   selectUpdEvents->bind("DELFLG", cDBS::bndOut, ", ");
   selectUpdEvents->bind("UPDFLG", cDBS::bndOut, ", ");
   selectUpdEvents->bind("FILEREF", cDBS::bndOut, ", ");
   selectUpdEvents->bind("TABLEID", cDBS::bndOut, ", ");
   selectUpdEvents->bind("VERSION", cDBS::bndOut, ", ");
   selectUpdEvents->bind("TITLE", cDBS::bndOut, ", ");
   selectUpdEvents->bind("SHORTTEXT", cDBS::bndOut, ", ");
   selectUpdEvents->bind("STARTTIME", cDBS::bndOut, ", ");
   selectUpdEvents->bind("DURATION", cDBS::bndOut, ", ");
   selectUpdEvents->bind("PARENTALRATING", cDBS::bndOut, ", ");
   selectUpdEvents->bind("VPS",  cDBS::bndOut, ", ");
   selectUpdEvents->bind("CONTENTS",  cDBS::bndOut, ", ");
   selectUpdEvents->bind(viewDescription, cDBS::bndOut, ", ");
   selectUpdEvents->bind(viewMergeSource, cDBS::bndOut, ", ");
   selectUpdEvents->bind(viewLongDescription, cDBS::bndOut, ", ");
   selectUpdEvents->build(" from eventsview where ");
   selectUpdEvents->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet);
   selectUpdEvents->bindCmp(0, "UPDSP", 0, ">", " and ");
   selectUpdEvents->build(" and UPDFLG in (%s)", Us::getNeeded());

   status += selectUpdEvents->prepare();

   // select event by useid

   selectEventById = new cDbStatement(useeventsDb);

   // select * from eventsview
   //      where useid = ?
   //        and updflg in (.....)

   selectEventById->build("select ");
   selectEventById->bindAllOut();
   selectEventById->build(" from %s where ", useeventsDb->TableName());
   selectEventById->bind("USEID", cDBS::bndIn | cDBS::bndSet);
   selectEventById->build(" and %s in (%s)",
                          useeventsDb->getField("UPDFLG")->getDbName(),
                          Us::getNeeded());

   status += selectEventById->prepare();

   // select all active events

   selectAllEvents = new cDbStatement(useeventsDb);

   // select * from eventsview
   //      where useid = ?
   //        and updflg in (.....)

   selectAllEvents->build("select ");
   selectAllEvents->bind("USEID", cDBS::bndOut);
   selectAllEvents->build(" from %s where ", useeventsDb->TableName());
   selectAllEvents->build("%s in (%s)",
                          useeventsDb->getField("UPDFLG")->getDbName(), Us::getNeeded());

   status += selectAllEvents->prepare();

   // ...

   // select stream, type, lang, description
   //  from components where
   // eventid = ?;
   // channelid = ?;

   selectComponentsOf = new cDbStatement(compDb);

   selectComponentsOf->build("select ");
   selectComponentsOf->bind("Stream", cDBS::bndOut);
   selectComponentsOf->bind("Type", cDBS::bndOut, ", ");
   selectComponentsOf->bind("Lang", cDBS::bndOut, ", ");
   selectComponentsOf->bind("Description", cDBS::bndOut, ", ");
   selectComponentsOf->build(" from %s where ", compDb->TableName());
   selectComponentsOf->bind("EventId", cDBS::bndIn | cDBS::bndSet);
   selectComponentsOf->bind("ChannelId", cDBS::bndIn | cDBS::bndSet, " and ");

   status += selectComponentsOf->prepare();

   // select *
   //   from recordinglist where
   //      state <> 'D'

   selectRecordings = new cDbStatement(recordingListDb);

   selectRecordings->build("select ");
   selectRecordings->bindAllOut();
   selectRecordings->build(" from %s where ", recordingListDb->TableName());
   selectRecordings->build(" (%s <> 'D' or %s is null)",
                             recordingListDb->getField("STATE")->getDbName(),
                             recordingListDb->getField("STATE")->getDbName());

   status += selectRecordings->prepare();

   // select *
   //   from recordingimages where
   //      imgid = ?

   imageSizeRec.setField(&imageSizeDef);

   selectImagesOfRecording = new cDbStatement(recordingImagesDb);

   selectImagesOfRecording->build("select ");
   selectImagesOfRecording->bindAllOut();
   selectImagesOfRecording->build(", length(");
   selectImagesOfRecording->bind(&imageSize, cDBS::bndOut);
   selectImagesOfRecording->build(")");
   selectImagesOfRecording->build(" from %s where ", recordingImagesDb->TableName());
   selectImagesOfRecording->bind("IMGID", cDBS::bndIn | cDBS::bndSet);

   status += selectImagesOfRecording->prepare();

   // select srcmovieid, srcseriesid, scrseriesepisode
   //   from recordinglist where
   //      state <> 'D' or stete is null
   //      and (updsp > lastifoupd or lastifoupd is null)

   selectRecForInfoUpdate = new cDbStatement(recordingListDb);

   selectRecForInfoUpdate->build("select ");
   selectRecForInfoUpdate->bindAllOut();
   selectRecForInfoUpdate->build(" from %s where ", recordingListDb->TableName());
   selectRecForInfoUpdate->build(" (%s <> 'D' or %s is null)",
                                    recordingListDb->getField("STATE")->getDbName(),
                                    recordingListDb->getField("STATE")->getDbName());
   selectRecForInfoUpdate->build(" and (%s > %s or %s is null) ",
                                 recordingListDb->getField("UPDSP")->getDbName(),
                                 recordingListDb->getField("LASTIFOUPD")->getDbName(),
                                 recordingListDb->getField("LASTIFOUPD")->getDbName());

   status += selectRecForInfoUpdate->prepare();

   // select *
   //   from timers where
   //      state <> 'D'
   //      and vdruuid = ?

   selectMyTimer = new cDbStatement(timerDb);

   selectMyTimer->build("select ");
   selectMyTimer->bindAllOut();
   selectMyTimer->build(" from %s where ", timerDb->TableName());
   selectMyTimer->build(" %s <> 'D'", timerDb->getField("STATE")->getDbName());
   selectMyTimer->bind("VDRUUID", cDBS::bndIn | cDBS::bndSet, " and ");

   status += selectMyTimer->prepare();

   // select id, eventid, channelid, starttime, state, endtime
   //   from timers where
   //     eventid = ? and channelid = ? and vdruuid = ?

   selectTimerByEvent = new cDbStatement(timerDb);

   selectTimerByEvent->build("select ");
   selectTimerByEvent->bind("ID", cDBS::bndOut);
   selectTimerByEvent->bind("EVENTID", cDBS::bndOut, ", ");
   selectTimerByEvent->bind("CHANNELID", cDBS::bndOut, ", ");
   selectTimerByEvent->bind("STARTTIME", cDBS::bndOut, ", ");
   selectTimerByEvent->bind("STATE", cDBS::bndOut, ", ");
   selectTimerByEvent->bind("ENDTIME", cDBS::bndOut, ", ");
   selectTimerByEvent->build(" from %s where ", timerDb->TableName());
   selectTimerByEvent->bind("EVENTID", cDBS::bndIn | cDBS::bndSet);
   selectTimerByEvent->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet, " and ");
   selectTimerByEvent->bind("VDRUUID", cDBS::bndIn | cDBS::bndSet, " and ");

   status += selectTimerByEvent->prepare();

   // select *
   //   from timers where
   //     id = ?

   selectTimerById = new cDbStatement(timerDb);

   selectTimerById->build("select ");
   selectTimerById->bindAllOut();
   selectTimerById->build(" from %s where ", timerDb->TableName());
   selectTimerById->bind("ID", cDBS::bndIn | cDBS::bndSet);

   status += selectTimerById->prepare();

   // select *
   //   from timers where
   //     doneid = ?

   selectTimerByDoneId = new cDbStatement(timerDb);

   selectTimerByDoneId->build("select ");
   selectTimerByDoneId->bindAllOut();
   selectTimerByDoneId->build(" from %s where ", timerDb->TableName());
   selectTimerByDoneId->bind("DONEID", cDBS::bndIn | cDBS::bndSet);

   status += selectTimerByDoneId->prepare();

   //-----------
   // select
   //    max(updsp)
   //  from timers

   selectMaxUpdSp = new cDbStatement(timerDb);

   selectMaxUpdSp->build("select ");
   selectMaxUpdSp->bind("UPDSP", cDBS::bndOut, "max(");
   selectMaxUpdSp->build(") from %s", timerDb->TableName());

   status += selectMaxUpdSp->prepare();

   // select * from timers
   //  where
   //    action != 'A' and action != 'F' and action is not null   // !taAssumed and !taFailed
   //    and (vdruuid = ? or vdruuid = 'any')

   selectPendingTimerActions = new cDbStatement(timerDb);

   selectPendingTimerActions->build("select ");
   selectPendingTimerActions->bindAllOut();
   selectPendingTimerActions->build(" from %s where ", timerDb->TableName());
   selectPendingTimerActions->build("%s != 'A' and %s != 'F' and %s is not null",
                                    timerDb->getField("ACTION")->getDbName(),
                                    timerDb->getField("ACTION")->getDbName(),
                                    timerDb->getField("ACTION")->getDbName());
   selectPendingTimerActions->build(" and %s != '%c'",
                                    timerDb->getField("TYPE")->getDbName(), ttView);
   selectPendingTimerActions->bind("VDRUUID", cDBS::bndIn | cDBS::bndSet, " and (");
   selectPendingTimerActions->build(" or %s = 'any')", timerDb->getField("VDRUUID")->getDbName());

   status += selectPendingTimerActions->prepare();

   // select * from timers
   //  where
   //    action != 'A' and action != 'F' and action is not null   // !taAssumed and !taFailed
   //    and (vdruuid = ? or vdruuid = 'any')

   selectSwitchTimerActions = new cDbStatement(timerDb);

   selectSwitchTimerActions->build("select ");
   selectSwitchTimerActions->bindAllOut();
   selectSwitchTimerActions->build(" from %s where ", timerDb->TableName());
   selectSwitchTimerActions->build("%s != '%c' and %s != '%c'",
                                   timerDb->getField("STATE")->getDbName(), tsFinished,
                                   timerDb->getField("STATE")->getDbName(), tsDeleted);
   selectSwitchTimerActions->build(" and %s = '%c'", timerDb->getField("TYPE")->getDbName(), ttView);
   selectSwitchTimerActions->bind("VDRUUID", cDBS::bndIn | cDBS::bndSet, " and ");

   status += selectSwitchTimerActions->prepare();

   // delete from timers where
   //  eventid = ?

   deleteTimer = new cDbStatement(timerDb);

   deleteTimer->build("delete from %s where ", timerDb->TableName());
   deleteTimer->bind("EVENTID", cDBS::bndIn | cDBS::bndSet);
   deleteTimer->bind("CHANNELID", cDBS::bndIn | cDBS::bndSet, " and ");
   deleteTimer->bind("VDRUUID", cDBS::bndIn | cDBS::bndSet, " and ");

   status += deleteTimer->prepare();

   if (status == success)
   {
      // -------------------------------------------
      // lookback -> get last stamp

      vdrDb->clear();
      vdrDb->setValue("UUID", Epg2VdrConfig.uuid);

      if (vdrDb->find())
      {
         char buf[50+TB];

         lastUpdateAt = vdrDb->getIntValue("LastUpdate");
         lastEventsUpdateAt = lastUpdateAt;
         getParameter("uuid", "lastEventsUpdateAt", lastEventsUpdateAt);

         strftime(buf, 50, "%d.%m.%y %H:%M:%S", localtime(&lastUpdateAt));
         tell(0, "Info: Last update was at '%s'", buf);
      }

      // register me to the vdrs table

      int devCount = 0;
      char* v;

      for (int i = 0; i < cDevice::NumDevices(); i++)
      {
         const cDevice* device = cDevice::GetDevice(i);

         if (device && device->NumProvidedSystems())
            devCount ++;
      }

      asprintf(&v, "vdr %s epg2vdr %s (%s)", VDRVERSION, VERSION, VERSION_DATE);

      vdrDb->setValue("UUID", Epg2VdrConfig.uuid);
      vdrDb->setValue("IP", getIpOf(Epg2VdrConfig.netDevice));
      vdrDb->setValue("MAC", getMacOf(Epg2VdrConfig.netDevice));
      vdrDb->setValue("NAME", getHostName());
      vdrDb->setValue("DBAPI", DB_API);
      vdrDb->setValue("VERSION", v);
      vdrDb->setValue("STATE", "attached");
      vdrDb->setValue("MASTER", "n");
      vdrDb->setValue("TUNERCOUNT", devCount);
      vdrDb->setValue("SHAREINWEB", Epg2VdrConfig.shareInWeb);
      vdrDb->setValue("USECOMMONRECFOLDER", Epg2VdrConfig.useCommonRecFolder);

      int osd2WebPort = getOsd2WebPort();

      if (osd2WebPort != na)
         vdrDb->setValue("OSD2WEBP", osd2WebPort);

      // set svdrp port if uninitialized, we can't query ther actual port from VDR :(

      if (vdrDb->getIntValue("SVDRP") == 0)
         vdrDb->setValue("SVDRP", 6419);   // #TODO -> plugin-setup?!

      vdrDb->store();
      updateVdrData();
      free(v);
   }

   if (status == success)
      status += cEpg2VdrEpgHandler::getSingleton()->updateExternalIdsMap(mapDb);

   return status;
}

int cUpdate::exitDb()
{
   // de-register me at the vdrs table

   if (vdrDb && vdrDb->isConnected())
   {
      vdrDb->setValue("Uuid", Epg2VdrConfig.uuid);
      vdrDb->find();
      vdrDb->setValue("Master", "n");
      vdrDb->setValue("State", "detached");
      vdrDb->store();
   }

   cParameters::exitDb();

   delete selectAllImages;           selectAllImages = 0;
   delete selectUpdEvents;           selectUpdEvents = 0;
   delete selectEventById;           selectEventById = 0;
   delete selectAllEvents;           selectAllEvents = 0;
   delete selectAllChannels;         selectAllChannels = 0;
   delete selectChannelById;         selectChannelById = 0;
   delete markUnknownChannel;        markUnknownChannel = 0;
   delete selectComponentsOf;        selectComponentsOf = 0;
   delete selectMasterVdr;           selectMasterVdr = 0;
   delete deleteTimer;               deleteTimer = 0;
   delete selectMyTimer;             selectMyTimer = 0;
   delete selectRecordings;          selectRecordings = 0;
   delete selectImagesOfRecording;   selectImagesOfRecording = 0;
   delete selectRecForInfoUpdate;    selectRecForInfoUpdate = 0;
   delete selectTimerByEvent;        selectTimerByEvent = 0;
   delete selectTimerById;           selectTimerById = 0;
   delete selectTimerByDoneId;       selectTimerByDoneId = 0;
   delete selectMaxUpdSp;            selectMaxUpdSp = 0;
   delete selectPendingTimerActions; selectPendingTimerActions = 0;
   delete selectSwitchTimerActions;  selectSwitchTimerActions = 0;

   delete vdrDb;                  vdrDb = 0;
   delete mapDb;                  mapDb = 0;
   delete fileDb;                 fileDb = 0;
   delete imageDb;                imageDb = 0;
   delete imageRefDb;             imageRefDb = 0;
   delete episodeDb;              episodeDb = 0;
   delete eventsDb;               eventsDb = 0;
   delete useeventsDb;            useeventsDb = 0;
   delete compDb;                 compDb = 0;
   delete timerDb;                timerDb = 0;
   delete timerDoneDb;            timerDoneDb = 0;
   delete recordingDirDb;         recordingDirDb = 0;
   delete recordingListDb;        recordingListDb = 0;
   delete recordingImagesDb;      recordingImagesDb = 0;

   delete viewDescription;        viewDescription = 0;
   delete viewMergeSource;        viewMergeSource = 0;
   delete viewLongDescription;    viewLongDescription = 0;

   delete connection; connection = 0;

   return done;
}

//***************************************************************************
// Send Event
//***************************************************************************

void cUpdate::sendEvent(int event, void* userData)
{
   cUpdate* update = (cUpdate*)userData;
   cMutexLock lock(&update->eventHookMutex);

   update->eventHook.push(event);
   tell(3, "sendEvent(%d)", event);
   update->waitCondition.Broadcast();
}

//***************************************************************************
// Check Connection
//***************************************************************************

int cUpdate::checkConnection(int& timeout)
{
   static int retryCount = 0;

   timeout = retryCount < 5 ? 10 : 60;

   // reconnect requested ?

   if (dbReconnectTriggered)
      exitDb();

   dbReconnectTriggered = no;

   // check connection

   if (!dbConnected(yes))
   {
      // try to connect

      tell(0, "Trying to re-connect to database!");
      retryCount++;

      if (initDb() != success)
      {
         tell(0, "Retry #%d failed, retrying in %d seconds!", retryCount, timeout);
         exitDb();

         return fail;
      }

      retryCount = 0;
      tell(0, "Connection established successfull!");
   }

   return success;
}

//***************************************************************************
// Is Handler Master
//***************************************************************************

int cUpdate::getOsd2WebPort()
{
   Osd2Web_Port_v1_0 req;
   req.webPort = na;

   cPlugin* osd2webPlugin = cPluginManager::GetPlugin("osd2web");
   int osd2webPluginUuidService = osd2webPlugin && osd2webPlugin->Service(OSD2WEB_PORT_SERVICE, 0);

   if (!osd2webPluginUuidService)
      return na;

   if (!osd2webPlugin->Service(OSD2WEB_PORT_SERVICE, &req))
   {
      tell(0, "Error: Call of service '%s' failed.", OSD2WEB_PORT_SERVICE);
      return na;
   }

   tell(0, "Got webPort '%d' by osd2web", req.webPort);

   return req.webPort;
}

//***************************************************************************
// Is Handler Master
//***************************************************************************

int cUpdate::isHandlerMaster()
{
   static int initialized = no;

   char flag = 0;

/*
  wenn no    - handler ausschalten und db auf 'N'
  wenn auto  - aushandeln
  wenn yes   - handler anschalten und db auf 'Y'

*/
   if (!dbConnected())
      return no;

   // on first call detect state of epgd

   if (!initialized)
   {
      epgdBusy = no;

      vdrDb->clear();
      vdrDb->setValue("UUID", EPGDNAME);

      if (vdrDb->find())
      {
         nextEpgdUpdateAt = vdrDb->getIntValue("NEXTUPDATE");
         epgdState = cEpgdState::toState(vdrDb->getStrValue("State"));

         if (epgdState >= cEpgdState::esBusy && epgdState < cEpgdState::esBusyImages)
            epgdBusy = yes;

         tell(1, "Detected epgd state '%s' (%d)", vdrDb->getStrValue("State"), epgdState);
         initialized = yes;
      }
      else
      {
         tell(0, "Info: Can't detect epgd state");
         epgdBusy = yes;
      }
   }

   // update handler role

   if (Epg2VdrConfig.masterMode == mmAuto)
   {
      tell(3, "Auto check master role");

      // select where "uuid <> ? and upper(master) = 'Y'"

      vdrDb->clear();
      vdrDb->setValue("Uuid", Epg2VdrConfig.uuid);

      handlerMaster = !selectMasterVdr->find();

      if (!handlerMaster)
         tell(3, "Master found, uuid '%s' (%s)",
              vdrDb->getStrValue("UUID"),
              vdrDb->getStrValue("NAME"));

      selectMasterVdr->freeResult();

      flag = handlerMaster ? 'y' : 'n';
   }
   else if (Epg2VdrConfig.masterMode == mmYes)
   {
      flag = 'Y';
      handlerMaster = yes;
   }
   else
   {
      flag = 'n';
      handlerMaster = no;
   }

   // write again to force at least the updsp

   vdrDb->clear();
   vdrDb->setValue("UUID", Epg2VdrConfig.uuid);
   vdrDb->find();

   vdrDb->setValue("STATE", "attached");
   vdrDb->setCharValue("MASTER", flag);
   vdrDb->store();

   // set handler state

   int handlerState = !epgdBusy && handlerMaster;

   if (cEpg2VdrEpgHandler::getSingleton()->getActive() != handlerState)
   {
      tell(1, "Change handler state to '%s'", handlerState ? "active" : "standby");
      cEpg2VdrEpgHandler::getSingleton()->setActive(handlerState);
   }

   return handlerMaster;
}

// // ***************************************************************************
// // Initially Init Epgd State
// // ***************************************************************************

// void cUpdate::updateEpgdState()
// {
//    epgdBusy = no;

//    if (!dbConnected())
//       return;

//    vdrDb->clear();
//    vdrDb->setValue("UUID", EPGDNAME);

//    if (vdrDb->find())
//    {
//       nextEpgdUpdateAt = vdrDb->getIntValue("NEXTUPDATE");
//       epgdState = cEpgdState::toState(vdrDb->getStrValue("State"));

//       if (epgdState >= cEpgdState::esBusy && epgdState < cEpgdState::esBusyImages)
//          epgdBusy = yes;

//       tell(1, "Detected epgd state '%s' (%d)", vdrDb->getStrValue("State"), epgdState);
//    }
//    else
//       tell(0, "Info: Can't detect epgd state");

//    int handlerState = !epgdBusy && isHandlerMaster();

//    if (cEpg2VdrEpgHandler::getSingleton()->getActive() != handlerState)
//       tell(0, "Set handler state initially to '%s'", handlerState ? "active" : "standby");

//    cEpg2VdrEpgHandler::getSingleton()->setActive(handlerState);

//    vdrDb->reset();
// }

// ***************************************************************************
// Update VDR Data
// ***************************************************************************

void cUpdate::updateVdrData()
{
   int usedMb = 0;
   int freeMb = 0;

   cVideoDirectory::VideoDiskSpace(&freeMb, &usedMb);

   vdrDb->clear();
   vdrDb->setValue("UUID", Epg2VdrConfig.uuid);
   vdrDb->find();

   vdrDb->setValue("VIDEODIR", videoBasePath);
   vdrDb->setValue("VIDEOTOTAL", usedMb+freeMb);
   vdrDb->setValue("VIDEOFREE", freeMb);

   vdrDb->store();
}

//***************************************************************************
// Update Rec Folder Option
//***************************************************************************

int cUpdate::updateRecFolderOption()
{
   vdrDb->clear();
   vdrDb->setValue("UUID", Epg2VdrConfig.uuid);

   if (vdrDb->find())
   {
      vdrDb->setValue("USECOMMONRECFOLDER", Epg2VdrConfig.useCommonRecFolder);
      vdrDb->update();
   }

   updateRecFolderOptionTrigger = no;
   recordingStateChangedTrigger = yes;
   recordingFullReloadTrigger = yes;

   return done;
}

//***************************************************************************
// Trigger Update
//***************************************************************************

void cUpdate::triggerDbReconnect()
{
   dbReconnectTriggered = yes;
   waitCondition.Broadcast();
}

//***************************************************************************
// Trigger Update
//***************************************************************************

int cUpdate::triggerEpgUpdate(int reload)
{
   if (!loopActive)
   {
      tell(0, "Thread not running, try to recover ...");
      Cancel(3);
      Start();
   }

   fullreload = reload;
   mainActPending = yes;
   manualTrigger = yes;

   if (epgdBusy)
   {
      Skins.QueueMessage(mtInfo, cString::sprintf(tr("Skipping '%s' request, epgd busy. Trying again later"),
                                                  reload ? tr("reload") : tr("update")));

      return fail;
   }

   waitCondition.Broadcast();    // wakeup thread

   return success;
}

//***************************************************************************
// Trigger Recording Update
//***************************************************************************

int cUpdate::triggerRecUpdate()
{
   if (!loopActive)
   {
      tell(0, "Thread not running, try to recover ...");
      Cancel(3);
      Start();
   }

   recordingStateChangedTrigger = yes;
   waitCondition.Broadcast();    // wakeup thread

   return success;
}

int cUpdate::commonRecFolderOptionChanged()
{
   updateRecFolderOptionTrigger = yes;
   waitCondition.Broadcast();    // wakeup thread

   return done;
}

//***************************************************************************
// Trigger Store Info Files
//***************************************************************************

int cUpdate::triggerStoreInfoFiles()
{
   if (!loopActive)
   {
      tell(0, "Thread not running, try to recover ...");
      Cancel(3);
      Start();
   }

   storeAllRecordingInfoFilesTrigger = yes;
   waitCondition.Broadcast();                   // wakeup thread

   return success;
}

//***************************************************************************
// Trigger Timer Jobs
//***************************************************************************

void cUpdate::triggerTimerJobs()
{
   if (!loopActive)
   {
      tell(0, "Thread not running, try to recover ...");
      Cancel(3);
      Start();
   }

   timerTableUpdateTriggered = yes;
   timerJobsUpdateTriggered = yes;
   waitCondition.Broadcast();                  // wakeup thread
}

//***************************************************************************
// Epgd State Change
//  - called via SVDRP
//***************************************************************************

void cUpdate::epgdStateChange(const char* state)
{
   // state control

   if (!dbConnected())
      return;

   epgdState = cEpgdState::toState(state);
   epgdBusy = epgdState >= cEpgdState::esBusy && epgdState < cEpgdState::esBusyImages;

   tell(1, "Got epgd state '%s' (%d)", state, epgdState);

   if (epgdState == Es::esBusyMatch)
      eventsPending = yes;                 // events pending due to merge
   else if (epgdState == Es::esBusyEvents)
      mainActPending = yes;                // main action pending

   // epg handler control

   int handlerState = !epgdBusy && handlerMaster;

   if (cEpg2VdrEpgHandler::getSingleton()->getActive() != handlerState)
      tell(1, "Change handler state to '%s'", handlerState ? "active" : "standby");

   cEpg2VdrEpgHandler::getSingleton()->setActive(handlerState);

   // check loop state

   if (!loopActive)
   {
      tell(0, "Thread not running, try to recover ...");
      Cancel(3);
      Start();
   }

   if (!epgdBusy)
      waitCondition.Broadcast();    // wakeup thread
}

//***************************************************************************
// Stop Thread
//***************************************************************************

void cUpdate::Stop()
{
   loopActive = no;
   waitCondition.Broadcast();    // wakeup thread

   Cancel(10);                   // wait up to 10 seconds for thread was stopping
}

void cUpdate::processEvents()
{
   cMutexLock lock(&eventHookMutex);

   while (!eventHook.empty())
   {
      int event = eventHook.front();
      eventHook.pop();

      switch (event)
      {
         case evtSwitchTimer:
         {
            switchTimerTrigger = yes;
            break;
         }
      }
   }

   for (auto it = timerThreads.begin(); it != timerThreads.end(); )
   {
      if (!(*it)->isActive())
      {
         delete *it;
         it = timerThreads.erase(it);
      }
      else
      {
         it++;
      }
   }
}

//***************************************************************************
// Action
//***************************************************************************

void cUpdate::Action()
{
   // open tables - inside thread!

   if (initDb() != success)
      exitDb();

   // mutex gets ONLY unlocked when sleeping

   mutex.Lock();
   loopActive = yes;

   // main action loop ...

   while (loopActive && Running())
   {
      int reconnectTimeout;              // set by checkConnection()

      // wait 1 minute

      waitCondition.TimedWait(mutex, 60*1000);

      // first process events

      processEvents();

      // we pass here at least once per minute ...

      if (checkConnection(reconnectTimeout) != success)
         continue;

      // switch timer

      if (switchTimerTrigger)
         checkSwitchTimer();

      // recording stuff

      if (dbConnected() && updateRecFolderOptionTrigger)
         updateRecFolderOption();

      if (dbConnected() && recordingStateChangedTrigger)
      {
         if (!pendingRecordingActions.empty())
            performRecordingActions();

         if (Epg2VdrConfig.shareInWeb)
            recordingChanged();            // update timer state

         updateVdrData();                  // update video disk size/free, ...
         updateRecordingTable(recordingFullReloadTrigger);

         recordingFullReloadTrigger = no;
         recordingStateChangedTrigger = no;
      }
      else if (dbConnected() && lastRecordingDeleteAt+5*tmeSecondsPerMinute < time(0))
      {
         cleanupDeletedRecordings();
         updateRecordingInfoFiles();
         lastRecordingDeleteAt = time(0);
      }

      if (dbConnected() && storeAllRecordingInfoFilesTrigger)
         storeAllRecordingInfoFiles();

      // check handler role

      isHandlerMaster();

      if (Epg2VdrConfig.shareInWeb)
      {
         // update timer - even when epgd is busy!

         if (dbConnected() && timerTableUpdateTriggered)
           updateTimerTable();

         if (dbConnected())
            hasTimerChanged();
      }

      if (epgdBusy)
         continue;

      if (Epg2VdrConfig.shareInWeb)
      {
         // check timer distribution and take over 'my' timers

         if (dbConnected() && timerJobsUpdateTriggered)
         {
            tell(2, "Updating EPG prior to 'check of timer request'");
            refreshEpg(0, na);                  // refresh EPG before performing timer jobs!
            performTimerJobs();
         }
      }

      // if triggered externally or updates pending

      if (dbConnected(yes) && (mainActPending || eventsPending))
      {
         time_t lastUpdateStartAt = time(0);

         tell(mainActPending ? 0 : 2, "--- EPG '%s' started ---", fullreload ? "full-reload" : mainActPending ? "update" : "refresh");

         if (mainActPending)
         {
            // cleanup unreferenced image-files

            if (cleanupPictures() != success)
               continue;
         }

         if (refreshEpg() != success)
            continue;

         lastEventsUpdateAt = time(0);
         setParameter("uuid", "lastEventsUpdateAt", lastEventsUpdateAt);
         cSchedules::Cleanup(true);   // force VDR to store of epg.data to filesystem

         if (mainActPending)
         {
            // get pictures from database and copy to local FS

            if (storePicturesToFs() != success)
               continue;

            lastUpdateAt = lastUpdateStartAt;      // update lookback information (notice)

            vdrDb->clear();
            vdrDb->setValue("Uuid", Epg2VdrConfig.uuid);
            vdrDb->find();
            vdrDb->setValue("LastUpdate", lastUpdateAt);
            vdrDb->store();
         }

         tell(mainActPending ? 0 : 2, "--- EPG %s finished ---", fullreload ? "reload" : mainActPending ? "update" : "refresh");

         if (Epg2VdrConfig.loglevel > 2)
            connection->showStat("update");

         if (manualTrigger)
         {
            Skins.QueueMessage(mtInfo, cString::sprintf(tr("EPG '%s' done"), fullreload ? tr("reload") : tr("update")));
            manualTrigger = no;
         }

         mainActPending = no;
         eventsPending = no;
         fullreload = no;
      }
   }

   exit();     // don't call exit in dtor outside of thread!!

   loopActive = no;

   tell(0, "Update thread ended (tid=%i)", cThread::ThreadId());
}

//***************************************************************************
// Clear Epg
//***************************************************************************

void clearEpg()
{
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)

   LOCK_TIMERS_WRITE;
   LOCK_SCHEDULES_WRITE;

   for (cTimer* timer = Timers->First(); timer; timer = Timers->Next(timer))
      timer->SetEvent(0);      // processing all timers here (local *and* remote)

   for (cSchedule* schedule = Schedules->First(); schedule; schedule = Schedules->Next(schedule))
      schedule->Cleanup(INT_MAX);

   cEitFilter::SetDisableUntil(time(0) + 10);

#else

   while (!cSchedules::ClearAll())
      tell(0, "Warning: Clear EPG failed, can't get lock. Retrying ...");

#endif
}

// //***************************************************************************
// // Get Schedule Of
// //***************************************************************************

// int getScheduleOf(tChannelID channelId, const cSchedules* schedules, cSchedule*& s)
// {
//    cChannel* channel = 0;

//    s = 0;

//    // get channels lock

// #if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
//    cStateKey channelsKey;
//    cChannels* channels = cChannels::GetChannelsWrite(channelsKey, 500);
// #else
//    cChannels* channels = &Channels;
// #endif

//    if (!channels)
//       return fail;

//    // get channel and schedule of channel

//    if (channel = channels->GetByChannelID(channelId, true))
//       s = (cSchedule*)schedules->GetSchedule(channel, true);
//    else
//       tell(0, "Error: Channel with ID '%s' don't exist on this VDR", (const char*)channelId.ToString());

// #if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
//    channelsKey.Remove();
// #endif

//    return success;
// }

//***************************************************************************
// Refresh Epg
//***************************************************************************

int cUpdate::refreshEpg(const char* forChannelId, int maxTries)
{
   const cEvent* event;
   int tries = 0;
   int timerChanges = 0;
   int total = 0;
   int dels = 0;
   int channels = 0;
   uint64_t start = cTimeMs::Now();
   cDbStatement* select = 0;

   if (Epg2VdrConfig.loglevel >= 5)
      connection->showStat("before refresh");

   // lookback ...

   getParameter("uuid", "lastEventsUpdateAt", lastEventsUpdateAt);

   // full reload?

   if (fullreload)
   {
      tell(1, "Removing all events from epg");

      clearEpg();

      lastUpdateAt = 0;
      lastEventsUpdateAt = 0;
   }

   // iterate over all channels in channelmap

   mapDb->clear();

   if (forChannelId)
   {
      select = selectChannelById;
      mapDb->setValue("CHANNELID", forChannelId);

      tell(1, "Load EPG for channel '%s'", forChannelId);
   }
   else
   {
      select = selectAllChannels;

      if (lastEventsUpdateAt)
         tell(2, "Update EPG, loading changes since %s", l2pTime(lastEventsUpdateAt).c_str());
      else
         tell(2, "Update EPG, reloading all events");
   }

   for (int f = select->find(); f && dbConnected(yes); f = select->fetch())
   {
      int count = 0;
      cSchedule* s = 0;
      cChannel* channel = 0;
      tChannelID channelId = tChannelID::FromString(mapDb->getStrValue("ChannelId"));

      channels++;

      eventsDb->clear();
      eventsDb->setValue("UPDSP", forChannelId ? 0 : lastEventsUpdateAt);
      eventsDb->setValue("CHANNELID", mapDb->getStrValue("CHANNELID"));

      // #1 get timers lock

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey timersKey;
      tell(3, "-> Try to get timers lock");
      cTimers* timers = cTimers::GetTimersWrite(timersKey, 500/*ms*/);
#else
      cTimers* timers = &Timers;
#endif

      // #2 get channels lock

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey channelsKey;
      cChannels* channels = cChannels::GetChannelsWrite(channelsKey, 500);
#else
      cChannels* channels = &Channels;
#endif

      // #3 get schedules lock

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey schedulesKey;
      tell(3, "-> Try to get schedules lock");
      cSchedules* schedules = cSchedules::GetSchedulesWrite(schedulesKey, 500/*ms*/);
#else
      cSchedulesLock* schedulesLock = new cSchedulesLock(true, 500/*ms*/);
      cSchedules* schedules = (cSchedules*)cSchedules::Schedules(*schedulesLock);
      tell(3, "LOCK (refreshEpg)");
#endif

      // get channel and schedule of channel

      if (channels && schedules && (channel = channels->GetByChannelID(channelId, true)))
         s = (cSchedule*)schedules->GetSchedule(channel, true);
      else if (channels && schedules)
         tell(0, "Error: Channel with ID '%s' don't exist on this VDR", mapDb->getStrValue("ChannelId"));

      if (!schedules || !channels || !timers)
      {
         tell(3, "Info: Can't get write lock on '%s'", !schedules ? "schedules" : !timers ? "timers" : "channels");

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         if (schedules) schedulesKey.Remove();
         if (timers)    timersKey.Remove();
         if (channels) channelsKey.Remove();
#else
         delete schedulesLock;
#endif

         if (tries++ > maxTries)
         {
            select->freeResult();
            tell(3, "Warning: Aborting refresh after %d tries", tries);
            break;
         }

         tell(3, "Retrying in 3 seconds");
         sleep(3);

         continue;
      }

      tries = 0;

      // lookup schedules object

      if (s)
      {
         // -----------------------------------------
         // iterate over all events of this schedule

         for (int found = selectUpdEvents->find(); found && dbConnected(); found = selectUpdEvents->fetch())
         {
            cTimer* timer = 0;
            char updFlg = toupper(eventsDb->getStrValue("UPDFLG")[0]);

            updFlg = updFlg == 0 ? 'P' : updFlg;               // fix missing flag

            // ignore unneded event rows ..

            if (!Us::isNeeded(updFlg))
               continue;

            // get event / timer

            if ((event = s->GetEvent(eventsDb->getIntValue("USEID"))))
            {
               if (Us::isRemove(updFlg))
                  tell(2, "Remove event %uld of channel '%s' due to updflg %c",
                       event->EventID(), (const char*)event->ChannelID().ToString(), updFlg);

               if (event->HasTimer())
               {
                  for (timer = timers->First(); timer; timer = timers->Next(timer))
                     if (timer->Event() == event)
                        break;
               }

               if (timer)
                  timer->SetEvent(0);

               s->DelEvent((cEvent*)event);
            }

            if (!Us::isRemove(updFlg))
               event = s->AddEvent(createEventFromRow(eventsDb->getRow()));
            else if (event)
            {
               event = 0;
               dels++;
            }

            if (timer && event)
            {
               timer->SetEvent(event);
               timer->Matches(event);
               timerChanges++;
            }
            else if (timer)
            {
               tell(0, "Info: Timer '%s', has no event anymore", *timer->ToDescr());
            }

            count++;
         }

         selectUpdEvents->freeResult();

         // Kanal fertig machen ..

         s->Sort();
         s->SetModified();

         tell(2, "Processed channel '%s' - '%s' with %d updates",
              eventsDb->getStrValue("CHANNELID"),
              mapDb->getStrValue("CHANNELNAME"),
              count);

         total += count;
      }

      // schedules lock freigeben

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      schedulesKey.Remove();
      tell(3, "-> Released schedules lock");
      channelsKey.Remove();
      tell(3, "-> Released channels lock");
      timersKey.Remove();
      tell(3, "-> Released timers lock");
#else
      tell(3, "LOCK free (refreshEpg)");
      delete schedulesLock;
#endif
   }

   select->freeResult();

   if (timerChanges)
   {
      GET_TIMERS_WRITE(timers);
      timers->SetModified();
   }

   if (lastEventsUpdateAt)
      tell(1, "Updated changes since '%s'; %d channels, "
           "%d events (%d deletions) in %s",
           forChannelId ? "-" : l2pTime(lastEventsUpdateAt).c_str(),
           channels, total, dels, ms2Dur(cTimeMs::Now()-start).c_str());
   else
      tell(1, "Updated all %d channels, %d events (%d deletions) in %s",
           channels, total, dels, ms2Dur(cTimeMs::Now()-start).c_str());

   // print sql statistic for statement debugging

   if (Epg2VdrConfig.loglevel >= 5)
      connection->showStat("refresh");

   return dbConnected(yes) ? success : fail;
}

//***************************************************************************
// To/From Row
//***************************************************************************

cEvent* cUpdate::createEventFromRow(const cDbRow* row)
{
   cEvent* e = new cEvent(row->getIntValue("USEID"));

   e->SetTableID(row->getIntValue("TABLEID"));
   e->SetVersion(row->getIntValue("VERSION"));
   e->SetTitle(row->getStrValue("TITLE"));
   e->SetShortText(row->getStrValue("SHORTTEXT"));
   e->SetStartTime(row->getIntValue("STARTTIME"));
   e->SetDuration(row->getIntValue("DURATION"));
   e->SetParentalRating(row->getIntValue("PARENTALRATING"));
   e->SetVps(row->getIntValue("VPS"));
   e->SetDescription(viewDescription->getStrValue());
   e->SetComponents(0);

   // ------------
   // contents

   uchar contents[MaxEventContents] = { 0 };
   int numContents = 0;

   for (const char* p = row->getStrValue("CONTENTS"); p && numContents < MaxEventContents; p = strchr(p, ','))
   {
      if (*p == ',') p++;

      if (*p)
         contents[numContents++] = strtol(p, 0, 0);
   }

   e->SetContents(contents);

   // ------------
   // components

   if (row->hasValue("SOURCE", "vdr"))
   {
      cComponents* components = new cComponents;

      compDb->clear();
      compDb->setBigintValue("EVENTID", row->getBigintValue("EVENTID"));
      compDb->setValue("CHANNELID", row->getStrValue("CHANNELID"));

      for (int f = selectComponentsOf->find(); f; f = selectComponentsOf->fetch())
      {
         components->SetComponent(components->NumComponents(),
                                  compDb->getIntValue("STREAM"),
                                  compDb->getIntValue("TYPE"),
                                  compDb->getStrValue("LANG"),
                                  compDb->getStrValue("DESCRIPTION"));
      }

      selectComponentsOf->freeResult();

      if (components->NumComponents())
         e->SetComponents(components);      // event take ownership of components!
      else
         delete components;
   }

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304)) || (WITH_AUX_PATCH)

   // ------------
   // aux

   if (Epg2VdrConfig.extendedEpgData2Aux)
   {
      useeventsDb->clear();
      useeventsDb->setValue("USEID", row->getIntValue("USEID"));

      if (selectEventById->find())
      {
         cXml xml;

         xml.create("epg2vdr");

         for (int i = 0; auxFields[i]; i++)
         {
            cDbValue* value = useeventsDb->getValue(auxFields[i]);

            if (!value || value->isEmpty())
               continue;

            if (value->getField()->hasFormat(cDBS::ffAscii) || value->getField()->hasFormat(cDBS::ffText) || value->getField()->hasFormat(cDBS::ffMText))
               xml.appendElement(auxFields[i], value->getStrValue());
            else
               xml.appendElement(auxFields[i], value->getIntValue());
         }

         // finally add some fields of the view

         xml.appendElement("source", viewMergeSource->getStrValue());
         xml.appendElement("longdescription", viewLongDescription->getStrValue()); // the real original without view additions

         // set to events aux field

         e->SetAux(xml.toText());
      }

      selectEventById->freeResult();
   }

#endif // WITH_AUX_PATCH

   return e;
}

//***************************************************************************
// Store Pictures to local Filesystem
//***************************************************************************

int cUpdate::storePicturesToFs()
{
   int count = 0;
   int cntLinks = 0;
   int updated = 0;
   char* path = 0;
   time_t start = time(0);

   if (!Epg2VdrConfig.getepgimages)
      return done;

   asprintf(&path, "%s", epgimagedir);
   chkDir(path);
   free(path);
   asprintf(&path, "%s/images", epgimagedir);
   chkDir(path);
   free(path);

   tell(0, "Load images from database");

   imageRefDb->clear();
   imageRefDb->setValue("UPDSP", lastUpdateAt);
   imageUpdSp.setValue(lastUpdateAt);

   for (int res = selectAllImages->find(); res; res = selectAllImages->fetch())
   {
      int eventid = masterId.getIntValue();
      const char* imageNameFs = imageRefDb->getStrValue("IMGNAMEFS");
      int lfn = imageRefDb->getIntValue("LFN");
      char* newpath;
      char* linkdest = 0;
      char* destfile = 0;
      int forceLink = no;
      int size = imageSize.getIntValue();

      asprintf(&destfile, "%s/images/%s", epgimagedir, imageNameFs);

      // check target ... image changed?

      if (!fileExists(destfile) || fileSize(destfile) != size)
      {
         // get image

         imageDb->clear();
         imageDb->setValue("IMGNAME", imageRefDb->getStrValue("IMGNAME"));

         if (imageDb->find() && !imageDb->getRow()->getValue("IMAGE")->isNull())
         {
            count++;

            // remove existing target

            if (fileExists(destfile))
            {
               updated++;
               removeFile(destfile);
            }

            forceLink = yes;
            tell(2, "Store image '%s' with %d bytes", destfile, size);

            if (FILE* fh1 = fopen(destfile, "w"))
            {
               fwrite(imageDb->getStrValue("IMAGE"), 1, size, fh1);
               fclose(fh1);
            }
            else
            {
               tell(1, "Can't write image to '%s', error was '%s'", destfile, strerror(errno));
            }
         }

         imageDb->reset();
      }

      free(destfile);

      // create links ...

      asprintf(&linkdest, "./images/%s", imageNameFs);

#ifdef _IMG_LINK
      if (!lfn)
      {
         // for lfn 0 create additional link without "_?"

         asprintf(&newpath, "%s/%d.%s", epgimagedir, eventid, imageExtension);
         createLink(newpath, linkdest, forceLink);
         free(newpath);
      }
#endif

      // create link with index

      asprintf(&newpath, "%s/%d_%d.%s", epgimagedir, eventid, lfn, imageExtension);

      if (!fileExists(newpath))
         cntLinks++;

      createLink(newpath, linkdest, forceLink);
      free(newpath);

      // ...

      free(linkdest);

      if (!dbConnected())
         break;
   }

   selectAllImages->freeResult();

   tell(0, "Got %d images from database in %ld seconds (%d updates, %d new) and created %d links",
        count, time(0) - start, updated, count-updated, cntLinks);

   return dbConnected(yes) ? success : fail;
}

//***************************************************************************
// Remove Pictures
//***************************************************************************

int cUpdate::cleanupPictures()
{
   const char* ext {".jpg"};
   struct dirent* dirent {nullptr};
   DIR* dir {nullptr};
   char* pdir {nullptr};
   int iCount {0};
   int lCount {0};

   imageRefDb->countWhere("", iCount);

   if (iCount < 100)   // less than 100 image lines are suspicious ;)
   {
      tell(0, "Exit image cleanup to avoid deleting of all images on empty imagerefs table");
      return done;
   }

   tell(1, "Starting cleanup of images in '%s'", epgimagedir);

   // -----------------------
   // cleanup 'images' directory

   iCount = 0;

   // open directory

   asprintf(&pdir, "%s/images", epgimagedir);

   if (!(dir = opendir(pdir)))
   {
      tell(1, "Can't open directory '%s', '%s'", pdir, strerror(errno));
      free(pdir);
      return done;
   }

   free(pdir);

   if (!dbConnected(yes))
      return fail;

   cDbStatement stmt(imageRefDb);

   stmt.build("select ");
   stmt.bind("IMGNAMEFS", cDBS::bndOut);
   stmt.build(" from %s", imageRefDb->TableName());

   if (stmt.prepare() != success)
      return fail;

   std::unordered_set<std::string> usedRefs;

   imageRefDb->clear();

   for (int res = stmt.find(); res; res = stmt.fetch())
      usedRefs.insert(imageRefDb->getStrValue("IMGNAMEFS"));

   stmt.freeResult();

   while ((dirent = readdir(dir)))
   {
      // check extension

      if (strncmp(dirent->d_name + strlen(dirent->d_name) - strlen(ext), ext, strlen(ext)) != 0)
         continue;

      if (usedRefs.count(dirent->d_name))
      {
         asprintf(&pdir, "%s/images/%s", epgimagedir, dirent->d_name);
         tell(2, "Removing image '%s'", pdir);

         if (!removeFile(pdir))
            iCount++;

         free(pdir);
      }

      stmt.freeResult();
   }

   closedir(dir);

   // -----------------------
   // remove unused symlinks

   tell(1, "Cleanup %s symlinks", fullreload ? "all" : "old");

   std::unordered_set<uint> useIds;

   if (!fullreload)
   {
      if (!dbConnected(yes))
         return fail;

      for (int res = selectAllEvents->find(); res; res = selectAllEvents->fetch())
         useIds.insert(useeventsDb->getIntValue("USEID"));

      selectAllEvents->freeResult();
   }

   if (!(dir = opendir(epgimagedir)))
   {
      tell(1, "Can't open directory '%s', '%s'", epgimagedir, strerror(errno));
      return done;
   }

   while ((dirent = readdir(dir)))
   {
      asprintf(&pdir, "%s/%s", epgimagedir, dirent->d_name);

      if (isLink(pdir))
      {
         if (fullreload || !fileExists(pdir))
         {
            if (!removeFile(pdir))
               lCount++;
         }

         else if (useIds.count(atoi(dirent->d_name)) == 0)
         {
            if (!removeFile(pdir))
               lCount++;
         }
      }

      free(pdir);
   }

   closedir(dir);

   tell(1, "Cleanup finished, removed (%d) images and (%d) symlinks", iCount, lCount);

   return success;
}

