/*
 * recording.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <set>

#include <vdr/videodir.h>

#include "lib/common.h"
#include "update.h"

//***************************************************************************
// Is Protected
//***************************************************************************

int isProtected(const char* path)
{
   char* dir;
   int fsk = no;
   char* file = 0;
   char* p = 0;

   const char* videoDir = cVideoDirectory::Name();

   if (strncmp(path, videoDir, strlen(videoDir)) != 0)
   {
      tell(0, "Fatal: Unexpected video dir in '%s'", path);
      return no;
   }

   dir = strdup(path + strlen(videoDir));

   do
   {
      if (p) *p = 0;

      asprintf(&file, "%s/%s/protection.fsk", videoDir, dir);
      fsk = fileExists(file);
      free(file);

   } while (!fsk && (p = strrchr(dir, '/')));

   tell(3, "'%s' is %sprotected", path, fsk ? "" : "not ");
   free(dir);

   return fsk;
}

//***************************************************************************
// Cleanup Deleted Recordings from Table
//***************************************************************************

int cUpdate::cleanupDeletedRecordings(int force)
{
   int delCnt = 0;
   md5Buf md5path;
   std::set<std::string> recMd5List;

   // --------------------------
   // get recordings lock

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   LOCK_RECORDINGS_READ;
   const cRecordings* recordings = Recordings;
#else
   const cRecordings* recordings = &Recordings;
#endif

   if (recordings->Count() == lastRecordingCount && !force)
      return done;

   tell(0, "Cleanup deleted recordings at database%s", force ? " (forced)" : "");

   // create set of existing recordings (to improve speed)

   for (const cRecording* rec = recordings->First(); rec; rec = recordings->Next(rec))
   {
      int pathOffset = 0;

      if (strncmp(rec->FileName(), videoBasePath, strlen(videoBasePath)) == 0)
      {
         pathOffset = strlen(videoBasePath);

         if (*(rec->FileName()+pathOffset) == '/')
            pathOffset++;
      }

      createMd5(rec->FileName()+pathOffset, md5path);

      tell(5, "DEBUG: Recording: '%s' [%s]", rec->Title(), rec->FileName());
      recMd5List.insert(md5path);
   }

   connection->startTransaction();

   recordingListDb->clear();
   recordingListDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

   for (int f = selectRecordings->find(); f && dbConnected(); f = selectRecordings->fetch())
   {
      // bin 'ich' zuständig?

      if (!recordingListDb->hasValue("VDRUUID", Epg2VdrConfig.uuid))
      {
         if (!recordingListDb->hasValue("OWNER", "") || !Epg2VdrConfig.useCommonRecFolder)
            continue;

         // sonderlocke, das mounted ggf. nicht jeder überall

         if (recordingListDb->getIntValue("FSK"))
            continue;
      }

      if (recMd5List.find(recordingListDb->getStrValue("MD5PATH")) == recMd5List.end())
      {
         // not found, mark as deleted

         delCnt++;
         recordingListDb->setValue("STATE", "D");
         recordingListDb->update();
      }
   }

   selectRecordings->freeResult();

   connection->commit();
   lastRecordingCount = recordings->Count();

   tell(0, "Info: Marked %d recordings as deleted", delCnt);

   return success;
}

//***************************************************************************
// Update Pending Recording Info Files
//***************************************************************************

int cUpdate::updatePendingRecordingInfoFiles(const cRecordings* recordings)
{
   cEventDetails evd;
   int count = 0;

   if (pendingNewRecordings.empty())
      return done;

   tell(1, "Updating recording info in info.epg2vdr");

   // iterate over pending recordings

   while (!pendingNewRecordings.empty())
   {
      std::string path = pendingNewRecordings.front();
      const cRecording* rec = ((cRecordings*)recordings)->GetByName(path.c_str());

      pendingNewRecordings.pop();

      if (!rec || !rec->Info() || !rec->Info()->GetEvent())
      {
         tell(0, "Warning: Recording '%s' not found or missin recording info,"
              " can't create info.epg2vdr", path.c_str());
         continue;
      }

      useeventsDb->clear();
      useeventsDb->setValue("USEID", (int)rec->Info()->GetEvent()->EventID());

      if (selectEventById->find())
      {
         evd.loadFromFs(path.c_str(), useeventsDb->getRow());
         evd.updateByRow(useeventsDb->getRow());

         if (evd.getChanges())
         {
            count++;
            evd.storeToFs(path.c_str());

            tell(1, "Updated/Created recording info for %d in info.epg2vdr with %d changes",
                 rec->Info()->GetEvent()->EventID(), evd.getChanges());
         }
      }
      else
         tell(0, "Warning: Event %d not found in table", rec->Info()->GetEvent()->EventID());

      selectEventById->freeResult();
   }

   tell(1, "Updated %d pending info.epg2vdr files", count);

   return done;
}

//***************************************************************************
// Store All Recording Info Files (only used by manual trigger)
//***************************************************************************

int cUpdate::storeAllRecordingInfoFiles()
{
   cEventDetails evd;
   int count = 0;

   tell(1, "Store info.epg2vdr for all recordings");

   connection->startTransaction();
   recordingListDb->clear();
   recordingListDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

   for (int f = selectRecordings->find(); f && dbConnected(); f = selectRecordings->fetch())
   {
      char* path;

      // bin 'ich' zuständig?

      if (!recordingListDb->hasValue("VDRUUID", Epg2VdrConfig.uuid))
      {
         // nicht zuständig wenn OWNER nicht leer oder ich nicht am NAS Verbund teilnehme

         if (!recordingListDb->hasValue("OWNER", "") || !Epg2VdrConfig.useCommonRecFolder)
            continue;
      }

      asprintf(&path, "%s/%s", videoBasePath, recordingListDb->getStrValue("PATH"));

      evd.loadFromFs(path, recordingListDb->getRow());
      evd.updateByRow(recordingListDb->getRow());

      if (evd.getChanges())
      {
         count++;
         evd.storeToFs(path);

         tell(2, "Store recording info for %ld in info.epg2vdr with %d changes",
              recordingListDb->getIntValue("EVENTID"), evd.getChanges());
      }

      time_t sp = time(0);
      recordingListDb->setValue("LASTIFOUPD", sp);
      recordingListDb->update(sp);

      free(path);
   }

   connection->commit();
   selectRecordings->freeResult();
   storeAllRecordingInfoFilesTrigger = no;

   tell(1, "Stored %d info.epg2vdr files", count);

   return done;
}

//***************************************************************************
// Update Recording Info Files
//***************************************************************************

int cUpdate::updateRecordingInfoFiles()
{
   cEventDetails evd;
   int count = 0;

   tell(1, "Update info.epg2vdr recordings");

   connection->startTransaction();
   recordingListDb->clear();

   for (int f = selectRecForInfoUpdate->find(); f && dbConnected(); f = selectRecForInfoUpdate->fetch())
   {
      char* path;

      asprintf(&path, "%s/%s", videoBasePath, recordingListDb->getStrValue("PATH"));

      // only update if this VDR can access the recording folder ...

      if (folderExists(path))
      {
         evd.loadFromFs(path, recordingListDb->getRow());
         evd.updateByRow(recordingListDb->getRow());

         if (evd.getChanges())
         {
            count++;
            evd.storeToFs(path);

            tell(1, "Update recording info for %ld in info.epg2vdr with %d changes",
                 recordingListDb->getIntValue("EVENTID"), evd.getChanges());
         }

         time_t sp = time(0);
         recordingListDb->setValue("LASTIFOUPD", sp);
         recordingListDb->update(sp);                     // force same stamp!
      }
      else
      {
         tell(3, "Info: Folder '%s' not found, suppose it belong to another vdr", path);
      }

      free(path);
   }

   connection->commit();
   selectRecForInfoUpdate->freeResult();
   tell(1, "Updated %d info.epg2vdr files", count);

   return done;
}

//***************************************************************************
// Update Recording Table
//***************************************************************************

int cUpdate::updateRecordingTable(int fullReload)
{
   int count = 0, insCnt = 0, updCnt = 0, dirCnt = 0;

   // first cleanup, at least to adjust count in 'lastRecordingCount'

   if (fullReload)
      recordingListDb->deleteWhere("vdruuid = '%s'", Epg2VdrConfig.uuid);
   else
      cleanupDeletedRecordings(yes);

   // get channel and recordings lock

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
#else
   cChannels* channels = &Channels;
#endif

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   LOCK_RECORDINGS_WRITE;
   cRecordings* recordings = Recordings;
#else
   const cRecordings* recordings = &Recordings;
#endif

   // update

   tell(0, "Updating recording list table");

   connection->startTransaction();

   recordingDirDb->deleteWhere("vdruuid = '%s'", Epg2VdrConfig.uuid);

   // ----------------
   // update ...

   for (cRecording* rec = recordings->First(); rec; rec = recordings->Next(rec))
   {
      int insert;
      int fsk;
      md5Buf md5path;
      const char* subTitle = "";
      int eventId = 0;
      std::string channelId = "";
      const char* description = "";
      std::string longdescription = "";
      const char* title = rec->Name();
      const cRecordingInfo* recInfo = rec->Info();
      int pathOffset = 0;
      const cChannel* channel = 0;
      int baseChanges = 0;

      // check if directory is registered

      if (rec->HierarchyLevels() > 0)
         dirCnt += updateRecordingDirectory(rec);

      // check if recording is registered

      if (strncmp(rec->FileName(), videoBasePath, strlen(videoBasePath)) == 0)
      {
         pathOffset = strlen(videoBasePath);

         if (*(rec->FileName()+pathOffset) == '/')
            pathOffset++;
      }

      createMd5(rec->FileName()+pathOffset, md5path);

      if (recInfo)
      {
         channelId = *(recInfo->ChannelID().ToString());
         subTitle = recInfo->ShortText() ? recInfo->ShortText() : "";
         description = recInfo->Description() ? recInfo->Description() : "";
         channel = channels->GetByChannelID(recInfo->ChannelID());

         if (recInfo->Title())
            title = recInfo->Title();

         if (recInfo->GetEvent())
         {
            eventId = recInfo->GetEvent()->EventID();

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304))
            if (channel)
            {
               cXml xml;
               cStateKey schedulesKey;
               const cSchedules* schedules = cSchedules::GetSchedulesRead(schedulesKey, 500/*ms*/);
               const cSchedule* s = schedules ? (cSchedule*)schedules->GetSchedule(channel) : 0;
               const cEvent* event = s ? s->GetEvent(eventId) : 0;

               if (event && !isEmpty(event->Aux()) && xml.set(event->Aux()) == success)
               {
                  if (XMLElement* element = xml.getElementByName("longdescription"))
                     longdescription = element->GetText();
               }

               if (schedules)
                  schedulesKey.Remove();
            }
#endif
         }
      }

      fsk = isProtected(rec->FileName());

      recordingListDb->clear();

      recordingListDb->setValue("MD5PATH", md5path);
      recordingListDb->setValue("STARTTIME", rec->Start());
      recordingListDb->setValue("OWNER", Epg2VdrConfig.useCommonRecFolder ? "" : Epg2VdrConfig.uuid);

      insert = !recordingListDb->find();
      recordingListDb->clearChanged();

      // base data ..

      recordingListDb->setValue("STATE", rec->IsInUse() == ruTimer ? "R" : "F");
      recordingListDb->setValue("INUSE", rec->IsInUse());
      recordingListDb->setValue("PATH", rec->FileName()+pathOffset);
      recordingListDb->setValue("NAME", rec->BaseName());
      recordingListDb->setValue("FOLDER", rec->Folder());
      recordingListDb->setValue("DESCRIPTION", description);
      recordingListDb->setValue("DURATION", rec->LengthInSeconds() > 0 ? rec->LengthInSeconds() : 0);
      recordingListDb->setValue("EVENTID", eventId);
      recordingListDb->setValue("CHANNELID", channelId.c_str());
      recordingListDb->setValue("FSK", fsk);

      if (longdescription.length())
         recordingListDb->setValue("LONGDESCRIPTION", longdescription.c_str());

      // scraping relevand data ..

      baseChanges = recordingListDb->getChanges();

      recordingListDb->setValue("TITLE", title);
      recordingListDb->setValue("SHORTTEXT", subTitle);

      // load event details

      cEventDetails evd;
      if (channel) evd.setValue("CHANNELNAME", channel->Name());
      evd.loadFromFs(rec->FileName(), recordingListDb->getRow(), no);

   tell(0, "CCCC");
      evd.updateToRow(recordingListDb->getRow());
   tell(0, "DDDD");

      // any scrap relevand data changed?

      if (recordingListDb->getChanges() > baseChanges)
      {
         recordingListDb->setValue("SCRNEW", yes);     // force scrap
         recordingListDb->setValue("SCRSP", time(0));  // force load from vdr
      }

      // don't toggle uuid if already set!

      if (recordingListDb->getValue("VDRUUID")->isNull())
          recordingListDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

      if (insert || recordingListDb->getChanges())
      {
         insert ? insCnt++ : updCnt++;
         tell(2, "Info: '%s' recording '%s / %s' due to %d changes [%s]",
              insert ? "Insert" : "Update", title, subTitle,
              recordingListDb->getChanges(), recordingListDb->getChangedFields().c_str());
         recordingListDb->store();
      }

      count++;
      recordingListDb->reset();
   }

   connection->commit();

   tell(0, "Info: Found %d recordings; %d inserted; %d updated "
        "and %d directories", count, insCnt, updCnt, dirCnt);

   // create info files for new recordings

   updatePendingRecordingInfoFiles(recordings);

   return success;
}

//***************************************************************************
// Update Recording Directory
//***************************************************************************

int cUpdate::updateRecordingDirectory(const cRecording* recording)
{
   int cnt = 0;
   char* dir = strdup(recording->Name());
   char* pos = strrchr(dir, '~');

   if (pos)
   {
      *pos = 0;

      for (int level = 0; level < recording->HierarchyLevels(); level++)
      {
         recordingDirDb->clear();
         recordingDirDb->setValue("VDRUUID", Epg2VdrConfig.uuid);
         recordingDirDb->setValue("DIRECTORY", dir);

         if (!recordingDirDb->find())
         {
            cnt++;
            recordingDirDb->store();
         }

         recordingDirDb->reset();

         char* pos = strrchr(dir, '~');
         if (pos) *pos=0;
      }
   }

   free(dir);

   return cnt;
}
