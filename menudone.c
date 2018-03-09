/*
 * menudone.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/interface.h>

#include "epg2vdr.h"
#include "update.h"
#include "menu.h"

#include "svdrpclient.h"

//***************************************************************************
// Object
//***************************************************************************

cEpgMenuDones::cEpgMenuDones()
   : cOsdMenu("", 2, 20, 35, 40)
{
   journalFilter = jfAll;
   order = 0;

   menuDb = new cMenuDb;

   refresh();
}

cEpgMenuDones::~cEpgMenuDones()
{
   delete menuDb;
}

//***************************************************************************
// Set Help
//***************************************************************************

void cEpgMenuDones::setHelp()
{
   const char* btGreen = "";

   switch (journalFilter)
   {
      case jfAll:      btGreen = tr("Recorded");  break;
      case jfRecorded: btGreen = tr("Created");   break;
      case jfCreated:  btGreen = tr("Failed");    break;
      case jfFailed:   btGreen = tr("Deleted");   break;
      case jfDeleted:  btGreen = tr("Recorded");  break;
   }

   SetHelp(tr("All"), btGreen, tr("Delete"), 0);
}

//***************************************************************************
// Refresh
//***************************************************************************

int cEpgMenuDones::refresh()
{
   int current = Current();
   const char* state = "%";

   cDbStatement* select = order ? menuDb->selectDoneTimerByStateTimeOrder : menuDb->selectDoneTimerByStateTitleOrder;

   Clear();

   switch (journalFilter)
   {
      case jfAll:      state = "%"; SetTitle(tr("Timer Journal"));            break;
      case jfRecorded: state = "R"; SetTitle(tr("Timer Journal - Recorded")); break;
      case jfCreated:  state = "C"; SetTitle(tr("Timer Journal - Created"));  break;
      case jfFailed:   state = "F"; SetTitle(tr("Timer Journal - Failed"));   break;
      case jfDeleted:  state = "D"; SetTitle(tr("Timer Journal - Delete"));   break;
   }

   // fill menu ..

   menuDb->timerDoneDb->clear();
   menuDb->timerDoneDb->setValue("STATE", state);

   for (int f = select->find(); f && menuDb->dbConnected(); f = select->fetch())
   {
      char* buf;
      asprintf(&buf, "%s\t%s\t%s\t%s",
               menuDb->timerDoneDb->getStrValue("STATE"),
               l2pTime(menuDb->timerDoneDb->getIntValue("STARTTIME"), "%d.%m.%y %H:%M").c_str(),
               menuDb->timerDoneDb->getStrValue("TITLE"),
               menuDb->timerDoneDb->getStrValue("SHORTTEXT"));
      Add(new cEpgMenuTextItem(menuDb->timerDoneDb->getIntValue("ID"), buf));
      free(buf);
   }

   select->freeResult();

   // init ..

   SetCurrent(current >= 0 ? Get(current) : First());
   setHelp();
   Display();

   return success;
}

//***************************************************************************
// ProcessKey
//***************************************************************************

eOSState cEpgMenuDones::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
         case k0:
         {
            order = !order;
            refresh();
            break;
         }
         case kRed:
         {
            journalFilter = jfAll;
            refresh();
            return osContinue;

         }
         case kGreen:
         {
            if (++journalFilter >= jfCount)
               journalFilter = jfFirst;

            refresh();
            return osContinue;
         }
         case kYellow:
         {
            cEpgMenuTextItem* item = (cEpgMenuTextItem*)Get(Current());

            if (item && Interface->Confirm(tr("Delete timer from journal?")))
            {
               menuDb->timerDoneDb->deleteWhere("id = %ld", item->getId());
               refresh();
            }

            return osContinue;
         }

         default: break;
      }
   }

   return state;
}
