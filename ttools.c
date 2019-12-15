/*
 * ttools.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string>
#include <regex>

#include "update.h"
#include "ttools.h"

using namespace std;

//***************************************************************************
// Content Of Tag
//***************************************************************************

int contentOfTag(const char* tag, const char* xml, char* buf, int size)
{
   string sTag = "<" + string(tag) + ">";
   string eTag = "</" + string(tag) + ">";

   const char* s;
   const char* e;

   if (buf)
      *buf = 0;

   if ((s = strstr(xml, sTag.c_str())) && (e = strstr(xml, eTag.c_str())))
   {
      s += strlen(sTag.c_str());

      if (buf)
         sprintf(buf, "%.*s", (int)(e-s), s);

      return success;
   }

   return fail;
}

//***************************************************************************
// Content Value Of Tag
//***************************************************************************

int contentOfTag(const cTimer* timer, const char* tag, char* buf, int size)
{
   char epgaux[512+TB];

   if (!timer || isEmpty(timer->Aux()))
      return fail;

   if (contentOfTag("epgd", timer->Aux(), epgaux, 512) != success)
      return fail;

   if (contentOfTag(tag, epgaux, buf, size) != success)
      return fail;

   return success;
}

int contentOfTag(const cTimer* timer, const char* tag, int& value)
{
   char epgaux[512+TB];
   char buf[100+TB];

   if (!timer || isEmpty(timer->Aux()))
      return fail;

   if (contentOfTag("epgd", timer->Aux(), epgaux, 512) != success)
      return fail;

   if (contentOfTag(tag, epgaux, buf, 100) != success)
      return fail;

   value = atoi(buf);

   return success;
}

//***************************************************************************
// Get Timer Id Of
//***************************************************************************

int getTimerIdOf(const cTimer* timer)
{
   char tid[100+TB];

   if (!timer || isEmpty(timer->Aux()))
      return na;

   if (contentOfTag(timer, "timerid", tid, 100) != success)
      return na;

   return atoi(tid);
}

int getTimerIdOf(const char* aux)
{
   char tid[100+TB];
   char epgaux[512+TB];

   if (isEmpty(aux))
      return na;

   if (contentOfTag("epgd", aux, epgaux, 512) != success)
      return fail;

   if (contentOfTag("timerid", epgaux, tid, 100) != success)
      return fail;

   return atoi(tid);
}

//***************************************************************************
// Remove Tag
//***************************************************************************

void removeTag(char* xml, const char* tag)
{
   string sTag = "<" + string(tag) + ">";
   string eTag = "</" + string(tag) + ">";

   const char* s;
   const char* e;

   if ((s = strstr(xml, sTag.c_str())) && (e = strstr(xml, eTag.c_str())))
   {
      char tmp[10000+TB];

      e += strlen(eTag.c_str());

      // sicher ist sicher ;)

      if (e <= s)
         return;

      sprintf(tmp, "%.*s%s", int(s-xml), xml, e);

      strcpy(xml, tmp);
   }
}

//***************************************************************************
// Insert Tag
//***************************************************************************

int insertTag(char* xml, const char* parent, const char* tag, int value)
{
   char* tmp;
   string sTag = "<" + string(parent) + ">";
   const char* s;

   if ((s = strstr(xml, sTag.c_str())))
   {
      s += strlen(sTag.c_str());
      asprintf(&tmp, "%.*s<%s>%d</%s>%s", int(s-xml), xml, tag, value, tag, s);
   }
   else
   {
      asprintf(&tmp, "%s<%s><%s>%d</%s></%s>", xml, parent, tag, value, tag, parent);
   }

   strcpy(xml, tmp);
   free(tmp);

   return success;
}

int insertTag(char* xml, const char* parent, const char* tag, const char* value)
{
   char* tmp;
   string sTag = "<" + string(parent) + ">";
   const char* s;

   if ((s = strstr(xml, sTag.c_str())))
   {
      s += strlen(sTag.c_str());
      asprintf(&tmp, "%.*s<%s>%s</%s>%s", int(s-xml), xml, tag, value, tag, s);
   }
   else
   {
      asprintf(&tmp, "%s<%s><%s>%s</%s></%s>", xml, parent, tag, value, tag, parent);
   }

   strcpy(xml, tmp);
   free(tmp);

   return success;
}

//***************************************************************************
// Set Tag To
//***************************************************************************

int setTagTo(cTimer* timer, const char* tag, int value)
{
   char aux[10000+TB] = "";

   if (!isEmpty(timer->Aux()))
      strcpy(aux, timer->Aux());

   removeTag(aux, tag);
   insertTag(aux, "epgd", tag, value);

   timer->SetAux(aux);

   return done;
}

int setTagTo(cTimer* timer, const char* tag, const char* value)
{
   char aux[10000+TB] = "";

   if (!isEmpty(timer->Aux()))
      strcpy(aux, timer->Aux());

   removeTag(aux, tag);
   insertTag(aux, "epgd", tag, value);

   timer->SetAux(aux);

   return done;
}

//***************************************************************************
// Set Timer Id
//***************************************************************************

int setTimerId(cTimer* timer, int tid)
{
   char aux[10000+TB] = "";

   if (!isEmpty(timer->Aux()))
      strcpy(aux, timer->Aux());

   // remove old timerid - if exist

   removeTag(aux, "timerid");
   insertTag(aux, "epgd", "timerid", tid);

   timer->SetAux(aux);

   return done;
}

//***************************************************************************
// Get Timer By Id
//***************************************************************************

cTimer* getTimerById(cTimers* timers, int timerid)
{
   for (cTimer* t = timers->First(); t; t = timers->Next(t))
      if (timerid == getTimerIdOf(t))
         return t;

   return 0;
}

//***************************************************************************
// Get Timer By Event
//***************************************************************************

cTimer* getTimerByEvent(cTimers* timers, const cEvent* event)
{
   cTimer* timer = 0;

   eTimerMatch tm = tmNone;

   timer = timers->GetMatch(event, &tm);

   if (tm != tmFull)
      timer = 0;

   return timer;
}

//***************************************************************************
// New Row From Event
//***************************************************************************

cDbRow* newTimerRowFromEvent(const cEvent* event)
{
   cTimer* timer = new cTimer(event);
   cDbRow* timerRow = newRowFromTimer(timer);

   delete timer;

   return timerRow;
}

//***************************************************************************
// New Row From Timer
//***************************************************************************

cDbRow* newRowFromTimer(const cTimer* timer)
{
   cDbRow* timerRow = new cDbRow("timers");

   updateRowByTimer(timerRow, timer);

   return timerRow;
}

//***************************************************************************
// Update Row By Timer
//***************************************************************************

int updateRowByTimer(cDbRow* timerRow, const cTimer* t)
{
   int autotimerid = na;
   int autotimerinssp = na;
   int doneid = na;
   int namingmode = na;
   char tmplExpression[100+TB] = "";
   int childLock = no;
   int epgs = no;
   char directory[512+TB] = "";
   char source[40+TB] = "";
   cString channelId = t->Event() ? t->Event()->ChannelID().ToString() : t->Channel()->GetChannelID().ToString();

   contentOfTag(t, "autotimerid", autotimerid);
   contentOfTag(t, "autotimerinssp", autotimerinssp);
   contentOfTag(t, "doneid", doneid);
   contentOfTag(t, "namingmode", namingmode);
   contentOfTag(t, "template", tmplExpression, 100);
   contentOfTag(t, "directory", directory, 512);
   contentOfTag(t, "source", source, 40);

   timerRow->setValue("VDRUUID", Epg2VdrConfig.uuid);
   timerRow->setValue("EVENTID", t->Event() ? (long)t->Event()->EventID() : 0);
   timerRow->setValue("_STARTTIME", t->Event() ? t->Event()->StartTime() : 0);
   timerRow->setValue("_ENDTIME", t->Event() ? t->StopTime() : 0);
   timerRow->setValue("CHANNELID", channelId);
   timerRow->setValue("DAY", t->Day());
   timerRow->setValue("STARTTIME", t->Start());
   timerRow->setValue("ENDTIME", t->Stop());
   timerRow->setValue("WEEKDAYS", t->WeekDays());
   timerRow->setValue("PRIORITY", t->Priority());
   timerRow->setValue("LIFETIME", t->Lifetime());
   timerRow->setValue("VPS", t->HasFlags(tfVps) ? yes : no);
   timerRow->setValue("ACTIVE", t->HasFlags(tfActive) ? yes : no);

   timerRow->setValue("DIRECTORY", directory);

   if (!isEmpty(directory) && strncmp(t->File(), directory, strlen(directory)) == 0)
   {
      int len = strlen(directory);

      if (t->File()[len] == '~')
         len++;

      timerRow->setValue("FILE", t->File()+len);
   }
   else
      timerRow->setValue("FILE", t->File());

   if (namingmode != na)
      timerRow->setValue("NAMINGMODE", namingmode);

   if (!isEmpty(tmplExpression))
      timerRow->setValue("TEMPLATE", tmplExpression);

   if (autotimerinssp != na)
      timerRow->setValue("AUTOTIMERINSSP", autotimerinssp);

   if (autotimerid != na)
      timerRow->setValue("AUTOTIMERID", autotimerid);

   if (doneid != na)
      timerRow->setValue("DONEID", doneid);

   // AUX: "<epgsearch><channel>62 - TNTSerieHD</channel><searchtimer>Falling Skies</searchtimer><start>1411498680</start><stop>1411502100</stop><s-id>3</s-id><eventid>565129</eventid></epgsearch>
   // AUX:   <epgd>......</epgd><pin-plugin><protected>yes</protected></pin-plugin>"

   if (t->Aux())
   {
      epgs = strstr(t->Aux(), "<epgsearch>") != 0;
      childLock = strstr(t->Aux(), "<pin-plugin><protected>yes</protected></pin-plugin>") != 0;
   }

   timerRow->setValue("SOURCE", !isEmpty(source) ? source : epgs ? "epgs" : Epg2VdrConfig.uuid);
   timerRow->setValue("CHILDLOCK", childLock);
   timerRow->setValue("AUX", t->Aux());                    // update aux also in table

   return done;
}

//***************************************************************************
// New Timer From Row
//***************************************************************************

cEpgTimer* newTimerObjectFromRow(cDbRow* timerRow, cDbRow* vdrRow)
{
   cEpgTimer* timer = 0;
   const cEvent* event = 0;
   cString buf;
   tChannelID channelId = tChannelID::FromString(timerRow->getStrValue("CHANNELID"));
   uint flags = tfNone;
   char* file;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
   const cChannel* channel = channels->GetByChannelID(channelId);
#else
   cChannels* channels = &Channels;
   const cChannel* channel = channels->GetByChannelID(channelId);
#endif

   const char* dir = timerRow->getStrValue("DIRECTORY");

   file = strdup(timerRow->getStrValue("FILE"));
   strReplace(file, ':', '|');

   if (timerRow->getIntValue("VPS"))
      flags |= tfVps;

   if (timerRow->getIntValue("ACTIVE"))
      flags |= tfActive;

   if (timerRow->hasCharValue("STATE", tsRunning))
      flags |= tfRecording;

   timer = new cEpgTimer(no, no, channel);

   buf = cString::sprintf("%d:%d:%s:%04d:%04d:%d:%d:%s%s%s:%s",
                          flags,
                          channel ? channel->Number() : 0,
                          (const char*)cTimer::PrintDay(timerRow->getIntValue("DAY"),
                                                        timerRow->getIntValue("WEEKDAYS"), yes),
                          (int)timerRow->getIntValue("STARTTIME"),
                          (int)timerRow->getIntValue("ENDTIME"),
                          (int)timerRow->getIntValue("PRIORITY"),
                          (int)timerRow->getIntValue("LIFETIME"),
                          !isEmpty(dir) ? dir : "",
                          !isEmpty(dir) && !isEmpty(file) ? "~" : "",
                          file,
                          timerRow->getStrValue("AUX"));

   free(file);
   timer->Parse(buf);

   if (channel)
   {
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      cStateKey schedulesKey;
      const cSchedules* schedules = cSchedules::GetSchedulesRead(schedulesKey);
#else
      cSchedulesLock* schedulesLock = new cSchedulesLock(false);
      const cSchedules* schedules = (cSchedules*)cSchedules::Schedules(*schedulesLock);
#endif
      if (schedules)
      {
         const cSchedule* schedule = schedules->GetSchedule(channel);

         if (schedule)
            event = schedule->GetEvent(timerRow->getIntValue("EVENTID"));
      }

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      if (schedules) schedulesKey.Remove();
#else
      delete schedulesLock;
#endif
   }

   if (event)
      timer->SetEvent(event);

   timer->setTimerId(timerRow->getIntValue("ID"));
   timer->setEventId(timerRow->getIntValue("EVENTID"));
   timer->setType(!timerRow->getValue("TYPE")->isNull() ? timerRow->getStrValue("TYPE")[0] : ttRecord);
   timer->setAction(!timerRow->getValue("ACTION")->isNull() ? timerRow->getStrValue("ACTION")[0] : ' ');
   timer->setVdr(vdrRow->getStrValue("NAME"), vdrRow->getStrValue("UUID"), vdrRow->hasValue("STATE", "attached"));
   timer->setState(!timerRow->getValue("STATE")->isNull() ? timerRow->getStrValue("STATE")[0] : ' ', timerRow->getStrValue("INFO"));
   timer->setCreateTime(timerRow->getIntValue("INSSP"));
   timer->setModTime(timerRow->getIntValue("UPDSP"));

#ifdef WITH_PIN
   timer->SetFskProtection(timerRow->getIntValue("CHILDLOCK"));
#endif

   return timer;
}

//***************************************************************************
// update Timer Object From Row
//***************************************************************************

int updateTimerObjectFromRow(cTimer* timer, cDbRow* timerRow, const cEvent* event)
{
   if (!timerRow->getValue("FILE")->isEmpty())
   {
      string path = "";

      if (!timerRow->getValue("DIRECTORY")->isEmpty())
         path = timerRow->getStrValue("DIRECTORY") + string("~");

      if (!timerRow->getValue("FILE")->isEmpty())
         path += timerRow->getStrValue("FILE");

      timer->SetFile(path.c_str());
   }
   else if (!timerRow->getValue("DIRECTORY")->isEmpty())
   {
      string path = timerRow->getStrValue("DIRECTORY") + string("~") + string(timer->File());
      timer->SetFile(path.c_str());
   }
   else if (!event)
   {
#if APIVERSNUM >= 20301
      LOCK_CHANNELS_READ;
      const cChannels* channels = Channels;
#else
      cChannels* channels = &Channels;
#endif

      tChannelID channelId = tChannelID::FromString(timerRow->getStrValue("CHANNELID"));
      const cChannel* channel = channels->GetByChannelID(channelId);

      timer->SetFile(channel->Name());
      tell(0, "Missing file, using channel name instead '%s'", channel->Name());
   }

   if (!timerRow->getValue("DAY")->isNull())
      timer->SetDay(timerRow->getIntValue("DAY"));

   if (!timerRow->getValue("WEEKDAYS")->isNull())
      timer->SetWeekDays(timerRow->getIntValue("WEEKDAYS"));

   if (!timerRow->getValue("STARTTIME")->isNull())
      timer->SetStart(timerRow->getIntValue("STARTTIME"));

   if (!timerRow->getValue("ENDTIME")->isNull())
      timer->SetStop(timerRow->getIntValue("ENDTIME"));

   if (!timerRow->getValue("PRIORITY")->isNull())
      timer->SetPriority(timerRow->getIntValue("PRIORITY"));

   if (!timerRow->getValue("LIFETIME")->isNull())
      timer->SetLifetime(timerRow->getIntValue("LIFETIME"));

   if (!timerRow->getValue("VPS")->isNull())
   {
      if (timerRow->getIntValue("VPS"))
         timer->SetFlags(tfVps);
      else
         timer->ClrFlags(tfVps);
   }

   if (timerRow->getIntValue("ACTIVE"))
      timer->SetFlags(tfActive);
   else
      timer->ClrFlags(tfActive);

   setTagTo(timer, "timerid", timerRow->getIntValue("ID"));

   if (!timerRow->getValue("SOURCE")->isNull())
      setTagTo(timer, "source", timerRow->getStrValue("SOURCE"));

   if (!timerRow->getValue("DIRECTORY")->isEmpty())
      setTagTo(timer, "directory", timerRow->getStrValue("DIRECTORY"));

   if (!timerRow->getValue("NAMINGMODE")->isNull())
      setTagTo(timer, "namingmode", timerRow->getIntValue("NAMINGMODE"));

   if (!timerRow->getValue("TEMPLATE")->isNull())
      setTagTo(timer, "template", timerRow->getStrValue("TEMPLATE"));

   if (!timerRow->getValue("AUTOTIMERID")->isNull())
      setTagTo(timer, "autotimerid", timerRow->getIntValue("AUTOTIMERID"));

   if (!timerRow->getValue("DONEID")->isNull())
      setTagTo(timer, "doneid", timerRow->getIntValue("DONEID"));

   if (!timerRow->getValue("AUTOTIMERINSSP")->isNull())
      setTagTo(timer, "autotimerinssp", timerRow->getStrValue("AUTOTIMERINSSP"));

   if (!timerRow->getValue("EXPRESSION")->isNull())
      setTagTo(timer, "expression", timerRow->getStrValue("EXPRESSION"));

   timer->Matches();  // adjust times of timer

   return done;
}
