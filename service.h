/*
 * service.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <vdr/timers.h>
#include <vdr/epg.h>

#include <list>
#include <map>

//***************************************************************************
// Event - Skin Interface
//***************************************************************************

class cEpgEvent_Interface_V1 : public cEvent
{
   public:

      cEpgEvent_Interface_V1(tEventID EventID) : cEvent(EventID) {}

      const char* getValue(const char* name)                     { return epg2vdrData[name].c_str(); }
      const std::map<std::string,std::string>* getValues() const { return &epg2vdrData; }

   protected:

      std::map<std::string,std::string> epg2vdrData;
};

//***************************************************************************
// Timer - Skin Interface
//***************************************************************************

class cEpgTimer_Interface_V1 : public cTimer
{
   public:

      cEpgTimer_Interface_V1(bool Instant = false, bool Pause = false, const cChannel* Channel = 0)
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         : cTimer(Instant, Pause, Channel) {}
#else
         : cTimer(Instant, Pause, (cChannel*)Channel) {}
#endif

      long TimerId()                { return timerid; }
      long EventId()                { return eventid; }
      const char* VdrName()         { return vdrName ? vdrName : ""; }
      const char* VdrUuid()         { return vdrUuid ? vdrUuid : ""; }
      int isVdrRunning()            { return vdrRunning; }
      int isLocal()                 { return local; }
      int isRemote()                { return !isLocal(); }

      char State()                  { return state; }
      int  hasState(char s)   const { return state == s; }
      const char* StateInfo()       { return stateInfo ? stateInfo : ""; }
      char Action()                 { return action; }

   protected:

      long timerid;
      long eventid;

      char* vdrName;
      char* vdrUuid;
      int local;
      int vdrRunning;

      char state;
      char* stateInfo;
      char action;
};

//***************************************************************************
// Service Interface
//***************************************************************************

struct cEpgTimer_Service_V1
{
   std::list<cEpgTimer_Interface_V1*> epgTimers;
};

#define EPG2VDR_TIMER_UPDATED "Epg2Vdr_Timer_Updated-v1.0"
#define EPG2VDR_TIMER_SERVICE "Epg2Vdr_Timer_Service-v1.0"

#ifdef EPG2VDR

//***************************************************************************
// Internal
//***************************************************************************

//***************************************************************************
// Class cEpgEvent
//***************************************************************************

class cEpgEvent : public cEpgEvent_Interface_V1
{
   public:

      cEpgEvent(tEventID EventID);
      virtual ~cEpgEvent() {}

      bool Read(FILE *f);

      void setValue(const char* name, const char* value) { epg2vdrData[name] = value; }
      void setValue(const char* name, long value)        { epg2vdrData[name] = std::to_string(value); }
};

//***************************************************************************
// Class cEpgTimer
//***************************************************************************

class cEpgTimer : public cEpgTimer_Interface_V1
{
   public:

      cEpgTimer(bool Instant = false, bool Pause = false, const cChannel* Channel = 0);
      virtual ~cEpgTimer();

      void setTimerId(long id)    { timerid = id; }
      void setEventId(long id)    { eventid = id; }
      void setState(char s, const char* info);
      void setAction(char a);
      void setVdr(const char* name, const char* uuid = 0, int running = 0);
};

#endif // EPG2VDR

//***************************************************************************

#endif // _SERVICE_H_
