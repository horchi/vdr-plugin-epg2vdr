/*
 * demo.h
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "common.h"

#include "db.h"
#include "epgservice.h"

cDbConnection* connection = 0;
const char* logPrefix = "";

//***************************************************************************
// Init Connection
//***************************************************************************

void initConnection()
{
   cDbConnection::init();

   // cDbConnection::setHost("localhost");
   cDbConnection::setHost("192.168.200.101");
   cDbConnection::setPort(3306);
   cDbConnection::setName("epg2vdr");
   cDbConnection::setUser("epg2vdr");
   cDbConnection::setPass("epg");

   cDbConnection::setConfPath("/etc/epgd/");
   cDbConnection::setEncoding("utf8");

   connection = new cDbConnection();
}

void exitConnection()
{
   cDbConnection::exit();

   if (connection)
      delete connection;
}

//***************************************************************************
//
//***************************************************************************

int demoStatement()
{
   int status = success;

   cDbTable* eventsDb = new cDbTable(connection, "events");

   tell(0, "------------------- attach table ---------------");

   // open table (attach)

   if (eventsDb->open() != success)
      return fail;

   tell(0, "---------------- prepare select statement -------------");

   // vorbereiten (prepare) eines statement, am besten einmal bei programmstart!
   // ----------
   // select eventid, compshorttext, episodepart, episodelang
   //   from events
   //     where eventid > ?

   cDbStatement* selectByCompTitle = new cDbStatement(eventsDb);

   status += selectByCompTitle->build("select ");
   status += selectByCompTitle->bind("EventId", cDBS::bndOut);
   status += selectByCompTitle->bind("ChannelId", cDBS::bndOut, ", ");
   status += selectByCompTitle->bind("Title", cDBS::bndOut, ", ");
   status += selectByCompTitle->build(" from %s where ", eventsDb->TableName());
   status += selectByCompTitle->bindCmp(0, eventsDb->getField("EventId"), 0, ">");

   status += selectByCompTitle->prepare();   // prepare statement

   if (status != success)
   {
      // prepare sollte MySQL fehler ausgegeben haben!

      delete eventsDb;
      delete selectByCompTitle;

      return fail;
   }

   tell(0, "------------------ prepare done ----------------------");

   tell(0, "------------------ create some rows  ----------------------");

   eventsDb->clear();     // alle values löschen

   for (int i = 0; i < 10; i++)
   {
      char* title;
      asprintf(&title, "title %d", i);

      eventsDb->setValue(eventsDb->getField("EventId"), 800 + i * 100);
      eventsDb->setValue(eventsDb->getField("ChannelId"), "xxx-yyyy-zzz");
      eventsDb->setValue(eventsDb->getField("Title"), title);

      eventsDb->store();                // store -> select mit anschl. update oder insert je nachdem ob dier PKey bereits vorhanden ist
      // eventsDb->insert();            // sofern man schon weiß das es ein insert ist
      // eventsDb->update();            // sofern man schon weiß das der Datensatz vorhanden ist

      free(title);
   }

   tell(0, "------------------ done  ----------------------");

   tell(0, "-------- select all where eventid > 1000 -------------");

   eventsDb->clear();     // alle values löschen
   eventsDb->setValue(eventsDb->getField("EventId"), 1000);

   for (int f = selectByCompTitle->find(); f; f = selectByCompTitle->fetch())
   {
      tell(0, "id: %ld", eventsDb->getIntValue(eventsDb->getField("EventId")));
      tell(0, "channel: %s", eventsDb->getStrValue(eventsDb->getField("ChannelId")));
      tell(0, "titel: %s", eventsDb->getStrValue(eventsDb->getField("Title")));
   }

   // freigeben der Ergebnissmenge !!

   selectByCompTitle->freeResult();

   // folgendes am programmende

   delete eventsDb;             // implizietes close (detach)
   delete selectByCompTitle;    // statement freigeben (auch gegen die DB)

   return success;
}

//***************************************************************************
// Join
//***************************************************************************

// #define F_INIT(a,b) cDbFieldDef* a##b = 0; dbDict.init(a##b, #a, #b)

int joinDemo()
{
   int status = success;

   // grundsätzlich genügt hier auch eine Tabelle, für die anderen sind cDbValue Instanzen außreichend
   // so ist es etwas einfacher die cDbValues zu initialisieren.
   // Ich habe statische "virtual FieldDef* getFieldDef(int f)" Methode in der Tabellenklassen geplant
   // um ohne Instanz der cTable ein Feld einfach initialisieren zu können

   cDbTable* eventsDb = new cDbTable(connection, "events");
   cDbTable* imageRefDb = new cDbTable(connection, "imagerefs");
   cDbTable* imageDb = new cDbTable(connection, "images");

   cDbTable* timerDb = new cDbTable(connection, "timers");
   timerDb->open(yes);
   delete timerDb;

   // init dict fields as needed (normaly done once at programm start)
   //   init and using the pointer improve the speed since the lookup via
   //   the name is dine only once

   // F_INIT(events, EventId); // ergibt: cDbFieldDef* eventsEventId; dbDict.init(eventsEventId, "events", "EventId");

   tell(0, "----------------- open table connection ---------------");

   // open tables (attach)

   if (eventsDb->open() != success)
      return fail;

   if (imageDb->open() != success)
      return fail;

   if (imageRefDb->open() != success)
      return fail;

   tell(0, "---------------- prepare select statement -------------");

   // all images

   cDbStatement* selectAllImages = new cDbStatement(imageRefDb);

   // prepare fields

   cDbValue imageUpdSp;
   cDbValue imageSize;
   cDbValue masterId;

   cDbFieldDef imageSizeDef("image", "image", cDBS::ffUInt,  999, cDBS::ftData, 0);  // eine Art ein Feld zu erzeugen
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
   selectAllImages->bind("ImgName", cDBS::bndOut, ", ");
   selectAllImages->bind("EventId", cDBS::bndOut, ", ");
   selectAllImages->bind("Lfn", cDBS::bndOut, ", ");
   selectAllImages->setBindPrefix("i.");
   selectAllImages->build(", length(");
   selectAllImages->bind(&imageSize, cDBS::bndOut);
   selectAllImages->build(")");
   selectAllImages->clrBindPrefix();
   selectAllImages->build(" from %s r, %s i, %s e where ",
                          imageRefDb->TableName(), imageDb->TableName(), eventsDb->TableName());
   selectAllImages->build("e.%s = r.%s and i.%s = r.%s and (",
                          "EventId",        // eventsEventId->getDbName(),
                          imageRefDb->getField("EventId")->getDbName(),
                          "imagename",      // direkt den DB Feldnamen verwenden -> nicht so schön da nicht dynamisch
                          imageRefDb->getField("ImgName")->getDbName()); // ordentlich via dictionary übersetzt -> schön ;)
   selectAllImages->bindCmp("i", &imageUpdSp, ">");
   selectAllImages->build(" or ");
   selectAllImages->bindCmp("r", imageRefDb->getField("UpdSp"), 0, ">");
   selectAllImages->build(")");

   status += selectAllImages->prepare();

   if (status != success)
   {
      // prepare sollte MySQL Fehler ausgegeben haben!

      delete eventsDb;
      delete imageDb;
      delete imageRefDb;
      delete selectAllImages;

      return fail;
   }

   tell(0, "------------------ prepare done ----------------------");


   tell(0, "------------------ select ----------------------");

   time_t since = 0; //time(0) - 60 * 60;
   imageRefDb->clear();
   imageRefDb->setValue(imageRefDb->getField("UpdSp"), since);
   imageUpdSp.setValue(since);

   for (int res = selectAllImages->find(); res; res = selectAllImages->fetch())
   {
      // so kommst du an die Werte der unterschiedlichen Tabellen

      int eventid = masterId.getIntValue();
      const char* imageName = imageRefDb->getStrValue("ImgName");
      int lfn = imageRefDb->getIntValue(imageRefDb->getField("Lfn"));
      int size = imageSize.getIntValue();

      tell(0, "Found (%d) '%s', %d, %d", eventid, imageName, lfn, size);
   }

   tell(0, "------------------ done ----------------------");

   // freigeben der Ergebnissmenge !!

   selectAllImages->freeResult();


   // folgendes am programmende

   delete eventsDb;             // implizites close (detach)
   delete imageDb;
   delete imageRefDb;
   delete selectAllImages;      // statement freigeben (auch gegen die DB)

   return success;
}

//***************************************************************************
//
//***************************************************************************

int insertDemo()
{
   cDbTable* timerDb = new cDbTable(connection, "timers");
   if (timerDb->open() != success) return fail;

   timerDb->clear();
   timerDb->setValue("EventId", 4444444);

   if (timerDb->insert() == success)
   {
      tell(0, "Insert succeeded with ID %d", timerDb->getLastInsertId());
   }

   delete timerDb;

   return done;
}

//***************************************************************************
//
//***************************************************************************

int findUseEvent()
{
   cDbTable* useeventsDb = new cDbTable(connection, "useevents");
   if (useeventsDb->open() != success) return fail;

   // select event by useid

   tell(0, "========================================");

   cDbStatement* selectEventById = new cDbStatement(useeventsDb);

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

   selectEventById->prepare();

   tell(0, "========================================");

   useeventsDb->clear();
   useeventsDb->setValue("USEID", 1146680);

   const char* flds[] =
   {
      "ACTOR",
      "AUDIO",
      "CATEGORY",
      "COUNTRY",
      "DIRECTOR",
      "FLAGS",
      "GENRE",
      "INFO",
      "MUSIC",
      "PRODUCER",
      "SCREENPLAY",
      "SHORTREVIEW",
      "TIPP",
      "TOPIC",
      "YEAR",
      "RATING",
      "NUMRATING",
      "MODERATOR",
      "OTHER",
      "GUEST",
      "CAMERA",

      0
   };

   if (selectEventById->find())
   {
      FILE* f;
      char* fileName = 0;

      asprintf(&fileName, "%s/info.epg2vdr", ".");

      if (!(f = fopen(fileName, "w")))
      {
         selectEventById->freeResult();
         tell(0, "Error opening file '%s' failed, %s", fileName, strerror(errno));
         free(fileName);

         return fail;
      }

      tell(0, "Storing event details to '%s'", fileName);

      for (int i = 0; flds[i]; i++)
      {
         cDbFieldDef* field = useeventsDb->getField(flds[i]);

         if (!field)
         {
            tell(0, "Warning: Field '%s' not found at table '%s'", flds[i], useeventsDb->TableName());
            continue;
         }

         if (field->getFormat() == cDbService::ffAscii ||
             field->getFormat() == cDbService::ffText ||
             field->getFormat() == cDbService::ffMText)
         {
            fprintf(f, "%s: %s\n", flds[i], useeventsDb->getStrValue(flds[i]));
         }
         else
         {
            fprintf(f, "%s: %ld\n", flds[i], useeventsDb->getIntValue(flds[i]));
         }
      }

   selectEventById->freeResult();
      free(fileName);
      fclose(f);
   }

   tell(0, "========================================");

   selectEventById->freeResult();

   return done;
}

//***************************************************************************
//
//***************************************************************************

int updateRecordingDirectory()
{
   int ins = 0;

   cDbTable* recordingDirDb = new cDbTable(connection, "recordingdirs");
   if (recordingDirDb->open() != success) return fail;

   // char* dir = strdup("more~Marvel's Agents of S.H.I.E.L.D.~xxx.ts");
   char* dir = strdup("aaaaa~bbbbbb~ccccc.ts");
   char* pos = strrchr(dir, '~');

   if (pos)
   {
      *pos = 0;

      for (int level = 0; level < 3; level++)
      {
         recordingDirDb->clear();

         recordingDirDb->setValue("VDRUUID", "foobar");
         recordingDirDb->setValue("DIRECTORY", dir);

         if (!recordingDirDb->find())
         {
            ins++;
            recordingDirDb->store();
         }

         recordingDirDb->reset();

         char* pos = strrchr(dir, '~');
         if (pos) *pos=0;
      }
   }

   tell(0, "inserted %d directories", ins);

   delete recordingDirDb;
   free(dir);

   return ins;
}

//***************************************************************************
// Main
//***************************************************************************

int main(int argc, char** argv)
{
   cEpgConfig::logstdout = yes;
   cEpgConfig::loglevel = 2;

   char* str = strdup("https://cellular.images.dvbdata.com:1234/4998510/4998510/320x240.jpg");
   replaceChars(str, "<>:\"/\\:|?*", '_');
   tell(0, "%s", str);
   free(str);

   const char* path = "/var/lib/vdr/plugins/epg2vdr/epg.dat";

   if (argc > 1)
      path = argv[1];

   cUserTimes userTimes;

   tell(0, "--------------");
   //userTimes.clear();
   tell(0, "--------------");

   userTimes.add("@Now", "What's on now?") ;
   userTimes.add("@Next", "What's on next?" );
   userTimes.add("!20:15", "prime time");
   userTimes.add("22:20", "late night");
   userTimes.add("00:00");

   tell(0, "--------------");

   for (int i = 0; i < 6; i++)
   {
      cUserTimes::UserTime* t = userTimes.next();

      tell(0, "%d - %s (%s) %s", t->getHHMM(), t->getTitle(), t->getHHMMStr(), t->isHighlighted() ? "highlighted" : "");
   }

   // read dictionary

   dbDict.setFilterFromNameFct(toFieldFilter);

   if (dbDict.in(path, ffEpgd) != success)
   {
      tell(0, "Invalid dictionary configuration, aborting!");
      return 1;
   }

   // dbDict.show();

   initConnection();

   // demoStatement();
   // joinDemo();
   // insertDemo();

   tell(0, "uuid: '%s'", getUniqueId());

   tell(0, "- - - - - - - - - - - - - - - - - ");

   // updateRecordingDirectory();
   // findUseEvent();

   cDbTable* vdrDb = new cDbTable(connection, "vdrs");
   if (vdrDb->open() != success) return fail;

   // DB-API check

   vdrDb->clear();
   vdrDb->setValue("UUID", "epgd");

   if (!vdrDb->find())
   {
      tell(0, "Can't lookup epgd information, start epgd to create the tables first! Aborting now.");
      return fail;
   }

   vdrDb->reset();

   const int DB_API = 8;

   tell(0, "Found DBAPI '%s' verion %ld", vdrDb->getStrValue("UUID"), vdrDb->getIntValue("DBAPI"));

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

   tell(0, "- - - - - - - - - - - - - - - - - ");

   exitConnection();

   return 0;
}
