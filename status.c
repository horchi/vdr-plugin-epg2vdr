/*
 * status.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/tools.h>

#include "update.h"
#include "ttools.h"

//***************************************************************************
// ----> Copied from epgsearch ;-)
//***************************************************************************

#define LOC_INDEXFILESUFFIX "/index"

bool IsPesRecording(const cRecording *pRecording)
{
#if VDRVERSNUM < 10703
  return true;
#else
  return pRecording && pRecording->IsPesRecording();
#endif
}

#if VDRVERSNUM < 10703

int RecLengthInSecs(const cRecording* pRecording)
{
   struct stat buf;
   cString fullname = cString::sprintf("%s%s", pRecording->FileName(), "/index.vdr");

   if (stat(fullname, &buf) == 0)
   {
      struct tIndex { int offset; uchar type; uchar number; short reserved; };
      int delta = buf.st_size % sizeof(tIndex);

      if (delta)
      {
         delta = sizeof(tIndex) - delta;
         tell(0, "ERROR: invalid file size (%ld) in '%s'", buf.st_size, *fullname);
      }

      return (buf.st_size + delta) / sizeof(tIndex) / SecondsToFrames(1);
   }

   return -1;
}

#else

struct tIndexTs
{
   uint64_t offset:40; // up to 1TB per file (not using off_t here - must definitely be exactly 64 bit!)
   int reserved:7;     // reserved for future use
   int independent:1;  // marks frames that can be displayed by themselves (for trick modes)
   uint16_t number:16; // up to 64K files per recording

   tIndexTs(off_t Offset, bool Independent, uint16_t Number)
   {
      offset = Offset;
      reserved = 0;
      independent = Independent;
      number = Number;
   }
};

int RecLengthInSecs(const cRecording* pRecording)
{
  struct stat buf;
  cString fullname = cString::sprintf("%s%s", pRecording->FileName(), IsPesRecording(pRecording) ? LOC_INDEXFILESUFFIX ".vdr" : LOC_INDEXFILESUFFIX);

  if (pRecording->FileName() && *fullname && access(fullname, R_OK) == 0 && stat(fullname, &buf) == 0)
  {
     double frames = buf.st_size ? (buf.st_size - 1) / sizeof(tIndexTs) + 1 : 0;
     double Seconds = 0;
     modf((frames + 0.5) / pRecording->FramesPerSecond(), &Seconds);
     return Seconds;
  }

  return -1;
}

#endif

//***************************************************************************
//  Copied from epgsearch ;-)  <----------
//***************************************************************************

//***************************************************************************
// Notifications from VDRs Status Interface
//***************************************************************************
//***************************************************************************
// Timers Change Notification
//***************************************************************************

void cUpdate::TimerChange(const cTimer* Timer, eTimerChange Change)
{
   if (!Epg2VdrConfig.shareInWeb)
      return;

   tell(1, "Timer changed, trigger update. Action was (%d)", Change);

   timerTableUpdateTriggered = yes;
   waitCondition.Broadcast();         // wakeup
}

//***************************************************************************
// Recording Notification (cStatus::MsgRecording(....))
//***************************************************************************

void cUpdate::Recording(const cDevice* Device, const char* Name, const char* FileName, bool On)
{
   RecordingAction action;

   // Recording of 'Peter Hase' has 'started' [/srv/vdr/video.00/Peter_Hase/2014-10-08.11.05.18-0.rec]
   // Recording of '(null)' has 'stopped' [/srv/vdr/video.00/Peter_Hase/2014-10-08.11.05.18-0.rec]

   tell(1, "Recording of '%s' has '%s' [%s]", Name, On ? "started" : "stopped", FileName);

   // schedule this notification to perfrom it in oure context not in the cStatus Interface context
   //  due to the needed list locks!

   action.name = Name;
   action.fileName = FileName;
   action.cardIndex = Device->CardIndex();
   action.on = On;
   pendingRecordingActions.push(action);

   if (On)
      pendingNewRecordings.push(FileName);

   recordingStateChangedTrigger = yes;
   waitCondition.Broadcast();            // wakeup
}

//***************************************************************************
// Perform Pending Recording Notification
//  (got by cStatus::MsgRecording(....) above)
//***************************************************************************

int cUpdate::performRecordingActions()
{
   const int allowedBreakDuration = 2;

   GET_TIMERS_READ(timers);           // get timers lock
   GET_RECORDINGS_READ(recordings);   // recordings lock

   while (!pendingRecordingActions.empty())
   {
      cMutexLock lock(&runningRecMutex);

      RecordingAction action = pendingRecordingActions.front();
      pendingRecordingActions.pop();

      if (action.on && action.name.length())    // recording started ...
      {
         for (const cTimer* ti = timers->First(); ti; ti = timers->Next(ti))
         {
            if (ti->Recording())                     // timer nimmt gerade auf
            {
               cRunningRecording* recording = 0;

               // check if already known

               for (cRunningRecording* rr = runningRecordings.First(); rr;  rr = runningRecordings.Next(rr))
               {
                  if (rr->timer == ti)
                  {
                     recording = rr;
                     break;
                  }
               }

               if (recording) // already handled -> a resume?!
               {
                  tell(1, "Info: Detected resume of '%s' on device %d", action.name.c_str(), action.cardIndex);
                  continue;
               }

               int doneid = na;
               contentOfTag(ti, "doneid", doneid);

               recording = new cRunningRecording(ti, doneid);
               runningRecordings.Add(recording);
               tell(1, "Info: Recording '%s' with doneid %d added to running list", action.name.c_str(), doneid);
            }
         }
      }

      // recording stopped ...

      if (!action.on)
      {
         // loop over running recordings ..

         for (cRunningRecording* rr = runningRecordings.First(); rr;  rr = runningRecordings.Next(rr))
         {
            const cRecording* pRecording = recordings->GetByName(action.fileName.c_str());
            const cTimer* pendingTimer = 0;
            int complete;
            double recFraction = 100.0;
            long timerLengthSecs = rr->timer->StopTime() - rr->timer->StartTime();
            bool vpsUsed = rr->timer->HasFlags(tfVps) && rr->timer->Event() && rr->timer->Event()->Vps();

            // check if timer still exists

            for (pendingTimer = timers->First(); pendingTimer; pendingTimer = timers->Next(pendingTimer))
            {
               if (pendingTimer == rr->timer)
                  break;
            }

            // still recording :o ?

            if (pendingTimer && pendingTimer->Recording())
               continue;

            if (pRecording && timerLengthSecs)
            {
               int recLen = RecLengthInSecs(pRecording);
               recFraction = double(recLen) * 100 / timerLengthSecs;
            }

            // assure timer has reached it's end or at least 90% (vps) / 98% were recorded

            complete = recFraction >= (vpsUsed ? 90 : 98);

            if (complete)
               tell(1, "Info: Finished: '%s'; recorded %d%%; VPS %s",
                    rr->timer->File(), (int)round(recFraction), vpsUsed ? "Yes": "No");
            else
               tell(1, "Info: Finished: '%s' (not complete! - recorded only %d%%); VPS %s",
                    rr->timer->File(), (int)round(recFraction), vpsUsed ? "Yes": "No");

            if (complete)
               rr->lastBreak = 0;         // reset break
            else if (!rr->lastBreak)
               rr->lastBreak = time(0);   // store first break

            if (!rr->lastBreak || (time(0) - rr->lastBreak) > allowedBreakDuration)
            {
               char* infoTxt;

               asprintf(&infoTxt, "Recording '%s' finished - %s complete (%d%%)",
                        rr->timer->File(), complete ? "" : "NOT", (int)round(recFraction));

               tell(1, "Info: %s", infoTxt);

               rr->finished = yes;
               rr->failed = !complete;
               rr->setInfo(infoTxt);

               free(infoTxt);
            }
         }
      }
   }

   return done;
}

//***************************************************************************
// Channel Switch Notification
//***************************************************************************

void cUpdate::ChannelSwitch(const cDevice* Device, int ChannelNumber, bool LiveView)
{
   // to be implemented ... ???
}
