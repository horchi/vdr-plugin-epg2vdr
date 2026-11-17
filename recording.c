/*
 * recording.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <set>
#include <dirent.h>
#include <vector>

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

//***************************************************************************
// Recordings Scan Active
//   VDR reads the video directory by the thread 'video directory scanner',
//   as long as it is running (e.g. at startup) the recordings list is incomplete!
//   The kernel truncates the thread name to 15 characters -> 'video directory'
//***************************************************************************

bool cUpdate::recordingsScanActive()
{
   DIR* dir = opendir("/proc/self/task");

   if (!dir)
      return false;

   bool active {false};

   while (dirent* entry = readdir(dir))
   {
      if (entry->d_name[0] == '.')
         continue;

      char comm[100] {};
      std::string path = std::string("/proc/self/task/") + entry->d_name + "/comm";
      FILE* fp = fopen(path.c_str(), "r");

      if (!fp)
         continue;

      if (fgets(comm, sizeof(comm), fp) && strncmp(comm, "video directory", 15) == 0)
         active = true;

      fclose(fp);

      if (active)
         break;
   }

   closedir(dir);

   return active;
}

int cUpdate::cleanupDeletedRecordings(int force)
{
   int delCnt = 0;
   md5Buf md5path;
   std::set<std::string> recMd5List;

   int recordingCount = 0;

   // don't check against an incomplete recordings list,
   //   all recordings not read yet would be marked as deleted

   if (recordingsScanActive())
   {
      tell(1, "Video directory scan in progress, cleanup of deleted recordings postponed");
      return done;
   }

   // --------------------------
   // create set of existing recordings (to improve speed),
   //   lock the recordings only for this, not during the database access

   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_RECORDINGS_READ;
      const cRecordings* recordings = Recordings;
#else
      const cRecordings* recordings = &Recordings;
#endif

      recordingCount = recordings->Count();

      if (recordingCount == lastRecordingCount && !force)
         return done;

      tell(0, "Cleanup deleted recordings at database%s", force ? " (forced)" : "");

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
   }  // release recordings lock

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
   lastRecordingCount = recordingCount;

   tell(0, "Info: Marked %d recordings as deleted", delCnt);

   return success;
}

//***************************************************************************
// Update Pending Recording Info Files
//***************************************************************************

int cUpdate::updatePendingRecordingInfoFiles()
{
   cEventDetails evd;
   int count = 0;
   std::vector<std::pair<std::string,tEventID>> pending;   // path, event id

   if (pendingNewRecordings.empty())
      return done;

   tell(1, "Updating recording info in info.epg2vdr");

   // lookup the pending recordings, lock only for the lookup

   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_RECORDINGS_READ;
      const cRecordings* recordings = Recordings;
#else
      const cRecordings* recordings = &Recordings;
#endif

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

         pending.push_back(std::make_pair(path, rec->Info()->GetEvent()->EventID()));
      }
   }

   // iterate over pending recordings, without holding a VDR lock

   for (const auto& p : pending)
   {
      const std::string& path = p.first;
      tEventID eventId = p.second;

      useeventsDb->clear();
      useeventsDb->setValue("USEID", (int)eventId);

      if (selectEventById->find())
      {
         evd.loadFromFs(path.c_str(), useeventsDb->getRow());
         evd.updateByRow(useeventsDb->getRow());

         if (evd.getChanges())
         {
            count++;
            evd.storeToFs(path.c_str());

            tell(1, "Updated/Created recording info for %d in info.epg2vdr with %d changes",
                 eventId, evd.getChanges());
         }
      }
      else
         tell(0, "Warning: Event %d not found in table", eventId);

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
   int imgCount = 0;

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

      // store the pictures to the filesystem

      // recordingImagesDb->clear();
      // recordingImagesDb->setValue("IMGID", recordingListDb->getStrValue("IMGID"));

      // for (int i = selectImagesOfRecording->find(); i && dbConnected(); i = selectImagesOfRecording->fetch())
      // {
      //    char* imgPath {};
      //    int lfn = recordingImagesDb->getIntValue("LFN");
      //    int size = imageSize.getIntValue();

      //    asprintf(&imgPath, "%s/epg2vdr_%d.jpg", path, lfn);

      //    if (!fileExists(imgPath) || fileSize(imgPath) != size)
      //    {
      //       tell(1, "Storing image of recording '%s'  to '%s'", recordingListDb->getStrValue("TITLE"), imgPath);

      //       if (fileExists(imgPath))
      //          removeFile(imgPath);

      //       storeToFile(imgPath, recordingImagesDb->getStrValue("IMAGE"), size);
      //       imgCount++;
      //    }

      //    free(imgPath);
      // }

      // selectImagesOfRecording->freeResult();

      free(path);
   }

   connection->commit();
   selectRecordings->freeResult();
   storeAllRecordingInfoFilesTrigger = no;

   tell(1, "Stored %d info.epg2vdr files and %d images", count, imgCount);

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

   struct RecInfo
   {
      std::string name;
      int hierarchyLevels {0};
      std::string fileName;
      std::string baseName;
      std::string folder;
      time_t start {0};
      int inUse {0};
      int length {0};
      std::string title;
      std::string subTitle;
      std::string description;
      std::string longdescription;
      std::string channelId;
      std::string channelName;
      int hasChannel {no};
      int eventId {0};
   };

   std::vector<RecInfo> recInfos;

   // first cleanup, at least to adjust count in 'lastRecordingCount'

   if (fullReload)
      recordingListDb->deleteWhere("vdruuid = '%s'", Epg2VdrConfig.uuid);
   else
      cleanupDeletedRecordings(yes);

   // ----------------
   // snapshot of the recordings, the VDR lists are locked only while copying
   //   (read lock is sufficient, sequence: Channels -> Recordings -> Schedules)

   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_CHANNELS_READ;
      const cChannels* channels = Channels;
      LOCK_RECORDINGS_READ;
      const cRecordings* recordings = Recordings;
#else
      cChannels* channels = &Channels;
      const cRecordings* recordings = &Recordings;
#endif

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304))
      cStateKey schedulesKey;
      const cSchedules* schedules = cSchedules::GetSchedulesRead(schedulesKey, 500/*ms*/);
#endif

      for (const cRecording* rec = recordings->First(); rec; rec = recordings->Next(rec))
      {
         RecInfo ri;
         const cRecordingInfo* recInfo = rec->Info();

         ri.name = notNull(rec->Name(), "");
         ri.hierarchyLevels = rec->HierarchyLevels();
         ri.fileName = rec->FileName();
         ri.baseName = notNull(rec->BaseName(), "");
         ri.folder = notNull(rec->Folder(), "");
         ri.start = rec->Start();
         ri.inUse = rec->IsInUse();
         ri.length = rec->LengthInSeconds() > 0 ? rec->LengthInSeconds() : 0;
         ri.title = ri.name;

         if (recInfo)
         {
            ri.channelId = *(recInfo->ChannelID().ToString());
            ri.subTitle = notNull(recInfo->ShortText(), "");
            ri.description = notNull(recInfo->Description(), "");

            if (const cChannel* channel = channels->GetByChannelID(recInfo->ChannelID()))
            {
               ri.hasChannel = yes;
               ri.channelName = notNull(channel->Name(), "");
            }

            if (recInfo->Title())
               ri.title = recInfo->Title();

            if (recInfo->GetEvent())
            {
               ri.eventId = recInfo->GetEvent()->EventID();

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304))
               const cChannel* channel = channels->GetByChannelID(recInfo->ChannelID());

               if (channel && schedules)
               {
                  cXml xml;
                  const cSchedule* s = (cSchedule*)schedules->GetSchedule(channel);

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20501))
                  const cEvent* event = s ? s->GetEventById(ri.eventId) : 0;
#else
                  const cEvent* event = s ? s->GetEvent(ri.eventId) : 0;
#endif

                  if (event && !isEmpty(event->Aux()) && xml.set(event->Aux()) == success)
                  {
                     if (XMLElement* element = xml.getElementByName("longdescription"))
                        if (!isEmpty(element->GetText()))
                           ri.longdescription = element->GetText();
                  }
               }
#endif
            }
         }

         recInfos.push_back(ri);
      }

#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304))
      if (schedules)
         schedulesKey.Remove();
#endif
   }

   // ----------------
   // update the database, without holding a VDR lock

   tell(0, "Updating recording list table");

   connection->startTransaction();

   recordingDirDb->deleteWhere("vdruuid = '%s'", Epg2VdrConfig.uuid);

   for (const auto& ri : recInfos)
   {
      int insert;
      int fsk;
      md5Buf md5path;
      const char* title = ri.title.c_str();
      const char* subTitle = ri.subTitle.c_str();
      int pathOffset = 0;
      int baseChanges = 0;

      if (!dbConnected())
         break;

      // check if directory is registered

      if (ri.hierarchyLevels > 0)
         dirCnt += updateRecordingDirectory(ri.name.c_str(), ri.hierarchyLevels);

      // check if recording is registered

      if (strncmp(ri.fileName.c_str(), videoBasePath, strlen(videoBasePath)) == 0)
      {
         pathOffset = strlen(videoBasePath);

         if (ri.fileName.c_str()[pathOffset] == '/')
            pathOffset++;
      }

      createMd5(ri.fileName.c_str()+pathOffset, md5path);

      fsk = isProtected(ri.fileName.c_str());

      recordingListDb->clear();

      recordingListDb->setValue("MD5PATH", md5path);
      recordingListDb->setValue("STARTTIME", ri.start);
      recordingListDb->setValue("OWNER", Epg2VdrConfig.useCommonRecFolder ? "" : Epg2VdrConfig.uuid);

      insert = !recordingListDb->find();
      recordingListDb->clearChanged();

      // base data ..

      recordingListDb->setValue("STATE", ri.inUse == ruTimer ? "R" : "F");
      recordingListDb->setValue("INUSE", ri.inUse);
      recordingListDb->setValue("PATH", ri.fileName.c_str()+pathOffset);
      recordingListDb->setValue("NAME", ri.baseName.c_str());
      recordingListDb->setValue("FOLDER", ri.folder.c_str());
      recordingListDb->setValue("DESCRIPTION", ri.description.c_str());
      recordingListDb->setValue("DURATION", ri.length);
      recordingListDb->setValue("EVENTID", ri.eventId);
      recordingListDb->setValue("CHANNELID", ri.channelId.c_str());
      recordingListDb->setValue("FSK", fsk);

      if (ri.longdescription.length())
         recordingListDb->setValue("LONGDESCRIPTION", ri.longdescription.c_str());

      // scraping relevand data ..

      baseChanges = recordingListDb->getChanges();

      recordingListDb->setValue("TITLE", title);
      recordingListDb->setValue("SHORTTEXT", subTitle);

      // load event details

      cEventDetails evd;
      if (ri.hasChannel) evd.setValue("CHANNELNAME", ri.channelName.c_str());
      evd.loadFromFs(ri.fileName.c_str(), recordingListDb->getRow(), no);
      evd.updateToRow(recordingListDb->getRow());

      // any scrap relevand data changed?

      if (recordingListDb->getChanges() > baseChanges)
      {
         recordingListDb->setValue("SCRNEW", yes);     // force scrap
         recordingListDb->setValue("SCRSP", time(0));  // force load from vdr
      }

      // don't toggle uuid if already set!

      if (recordingListDb->getValue("VDRUUID")->isNull())
          recordingListDb->setValue("VDRUUID", Epg2VdrConfig.uuid);

      if (recordingListDb->getValue("IMGID")->isEmpty())
      {
         md5Buf md5ImgId;
         std::string s = std::string(title) + std::string(subTitle);
         createMd5(s.c_str(), md5ImgId);
         recordingListDb->setValue("IMGID", md5ImgId);
      }

      if (insert || recordingListDb->getChanges())
      {
         insert ? insCnt++ : updCnt++;
         tell(2, "Info: '%s' recording '%s / %s' due to %d changes [%s]", insert ? "Insert" : "Update",
              title, subTitle, recordingListDb->getChanges(), recordingListDb->getChangedFields().c_str());
         recordingListDb->store();
      }

      // check recording image table

      {
         recordingImagesDb->clear();
         recordingImagesDb->setValue("IMGID", recordingListDb->getStrValue("IMGID"));
         recordingImagesDb->setValue("LFN", 0);

         if (!recordingImagesDb->find())
         {
            const char* ext = ".jpg";
            MemoryStruct data;
            struct dirent* dirent {};
            DIR* dir {};
            char* recdir {};
            int lfn {0};

            // we don't have a image, check filesystem

            asprintf(&recdir, "%s/%s", videoBasePath, recordingListDb->getStrValue("PATH"));
            dir = opendir(recdir);

            if (!dir)
            {
               tell(1, "Can't open directory '%s', '%s'", recdir, strerror(errno));
               continue;
            }

            free(recdir);

            while ((dirent = readdir(dir)))
            {
               // check extension

               if (strncmp(dirent->d_name + strlen(dirent->d_name) - strlen(ext), ext, strlen(ext)) != 0)
                  continue;

               char* imgPath {};
               asprintf(&imgPath, "%s/%s/%s", videoBasePath, recordingListDb->getStrValue("PATH"), dirent->d_name);

               tell(0, "%s'found image for '%s' [%s]", fileExists(imgPath) ? "" : "Don't ",
                    recordingListDb->getStrValue("TITLE"), imgPath);

               if (loadFromFile(imgPath, &data) == success)
               {
                  if (data.size < (unsigned long)imageDb->getField("IMAGE")->getSize())
                  {
                     recordingImagesDb->setValue("IMGID", recordingListDb->getStrValue("IMGID"));
                     recordingImagesDb->setValue("LFN", lfn++);
                     recordingImagesDb->setValue("IMAGE", data.memory, data.size);
                     recordingImagesDb->setValue("TITLE", recordingListDb->getStrValue("TITLE"));
                     recordingImagesDb->setValue("SHORTTEXT", recordingListDb->getStrValue("SHORTTEXT"));
                     recordingImagesDb->store();
                  }
               }

               free(imgPath);
            }

            closedir(dir);
         }

         recordingImagesDb->reset();
      }

      count++;
      recordingListDb->reset();
   }

   connection->commit();

   tell(0, "Info: Found %d recordings; %d inserted; %d updated and %d directories", count, insCnt, updCnt, dirCnt);

   // create info files for new recordings

   updatePendingRecordingInfoFiles();

   return success;
}

//***************************************************************************
// Update Recording Directory
//***************************************************************************

int cUpdate::updateRecordingDirectory(const char* name, int hierarchyLevels)
{
   int cnt = 0;
   char* dir = strdup(name);
   char* pos = strrchr(dir, '~');

   if (pos)
   {
      *pos = 0;

      for (int level = 0; level < hierarchyLevels; level++)
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
