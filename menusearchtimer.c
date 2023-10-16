/*
 * menusearchtimer.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/interface.h>

#include "lib/searchtimer.h"
#include "menu.h"
#include "lib/vdrlocks.h"

//***************************************************************************
// class cEpgMenuDonesOf
//***************************************************************************

class cEpgMenuDonesOf : public cOsdMenu
{
   public:

      cEpgMenuDonesOf(cMenuDb* mdb, const cDbRow* aRow);
      virtual ~cEpgMenuDonesOf();
      virtual eOSState ProcessKey(eKeys Key);

   protected:

      int refresh();

      const cDbRow* useEventRow;
      cMenuDb* menuDb;
};

//***************************************************************************
// Object
//***************************************************************************

cEpgMenuDonesOf::cEpgMenuDonesOf(cMenuDb* mdb, const cDbRow* aRow)
   : cOsdMenu(tr("Timer Journal"), 2, 20, 35, 40)
{
   menuDb = mdb;
   useEventRow = aRow;
   refresh();
}

cEpgMenuDonesOf::~cEpgMenuDonesOf()
{
}

//***************************************************************************
// Refresh
//***************************************************************************

int cEpgMenuDonesOf::refresh()
{
   cDbStatement* selectDones {};

   Clear();

   menuDb->useeventsDb->clear();
   menuDb->useeventsDb->getRow()->copyValues(useEventRow, cDbService::ftPrimary);

   if (!menuDb->useeventsDb->find())
      return done;

   // tell(0, "EVENT found: '%s/%s/%ld' [%s / %s]",
   //      menuDb->useeventsDb->getStrValue("CNTSOURCE"),
   //      menuDb->useeventsDb->getStrValue("CHANNELID"),
   //      menuDb->useeventsDb->getBigintValue("CNTEVENTID"),
   //      menuDb->useeventsDb->getStrValue("EPISODECOMPPARTNAME"),
   //      menuDb->useeventsDb->getStrValue("COMPSHORTTEXT"));

   // collect dones ...

   if (menuDb->search->prepareDoneSelect(menuDb->useeventsDb->getRow(),
                                         menuDb->searchtimerDb->getIntValue("REPEATFIELDS"),
                                         selectDones) == success && selectDones)
   {
      for (int f = selectDones->find(); f; f = selectDones->fetch())
      {
         char* buf;

         menuDb->search->getTimersDoneDb()->find();

         asprintf(&buf, "%s\t%s\t%s\t%s",
                  menuDb->search->getTimersDoneDb()->getStrValue("STATE"),
                  l2pTime(menuDb->search->getTimersDoneDb()->getIntValue("STARTTIME"), "%d.%m.%y %H:%M").c_str(),
                  menuDb->search->getTimersDoneDb()->getStrValue("TITLE"),
                  menuDb->search->getTimersDoneDb()->getStrValue("SHORTTEXT"));

         tell(0, "%s", buf);
         Add(new cEpgMenuTextItem(menuDb->search->getTimersDoneDb()->getIntValue("ID"), buf));
         free(buf);
      }

      selectDones->freeResult();
   }

   menuDb->useeventsDb->reset();

   SetHelp(0, 0, tr("Delete"), 0);
   Display();

   return success;
}

//***************************************************************************
// ProcessKey
//***************************************************************************

eOSState cEpgMenuDonesOf::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
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

//***************************************************************************
//***************************************************************************
// Class cEpgMenuSearchTimerEdit
//***************************************************************************

class cEpgMenuSearchTimerEdit : public cOsdMenu
{
   public:

      cEpgMenuSearchTimerEdit(cMenuDb* db, long id, bool New = no);
      virtual ~cEpgMenuSearchTimerEdit();
      virtual eOSState ProcessKey(eKeys Key);

   protected:

      cMenuDb* menuDb;
};

cEpgMenuSearchTimerEdit::cEpgMenuSearchTimerEdit(cMenuDb* db, long id, bool New)
   : cOsdMenu(tr("Edit Search Timer"), 12)
{
   menuDb = db;
   menuDb->searchtimerDb->clear();
   menuDb->searchtimerDb->setValue("ID", id);

   if (!menuDb->searchtimerDb->find())
      return;

   cDbTableDef* def = menuDb->searchtimerDb->getTableDef();

   for (int i = 0; i < def->fieldCount(); i++)
   {
      cDbValue* value = menuDb->searchtimerDb->getValue(def->getField(i)->getName());
      const char* name {}; //  = def->getField(i)->getDescription();

      if (!value || def->getField(i)->hasType(cDBS::ftMeta))
         continue;

      if (isEmpty(name))
         name = def->getField(i)->getName();

      if (def->getField(i)->hasFormat(cDBS::ffAscii))
         Add(new cMenuEditItem(tr(name))); // , (char*)value->getStrValueRef(), def->getField(i)->getSize()));
      else if (def->getField(i)->hasFormat(cDBS::ffInt))
         Add(new cMenuEditIntItem(tr(name), (int*)value->getIntValueRef()));
   }

   Display();
}

cEpgMenuSearchTimerEdit::~cEpgMenuSearchTimerEdit()
{
}

eOSState cEpgMenuSearchTimerEdit::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   return state;
}

//***************************************************************************
// Class cEpgMenuSearchTimerItem
//***************************************************************************

class cEpgMenuSearchTimerItem : public cOsdItem
{
   public:

      cEpgMenuSearchTimerItem(long aId, int aActive, const char* text)
      {
         id = aId;
         active = aActive;
         SetText(text);
      }

      ~cEpgMenuSearchTimerItem() {}

      long getId()        { return id; }
      int isActive()      { return active; }

   protected:

      long id;
      int active;
};

//***************************************************************************
//***************************************************************************
// Class cEpgMenuSearchResult
//***************************************************************************

class cEpgMenuSearchResult : public cOsdMenu
{
   public:

      cEpgMenuSearchResult(cMenuDb* db, long searchTimerId);
      virtual ~cEpgMenuSearchResult();
      virtual eOSState ProcessKey(eKeys Key);

   protected:

      int refresh(long searchTimerId);

      cMenuDb* menuDb;
};

cEpgMenuSearchResult::cEpgMenuSearchResult(cMenuDb* db, long searchTimerId)
   : cOsdMenu(tr("Search Result"), 17, CHNAMWIDTH, 3, 30)
{
   menuDb = db;
   refresh(searchTimerId);
}

cEpgMenuSearchResult::~cEpgMenuSearchResult()
{
}

//***************************************************************************
// Refresh
//***************************************************************************

int cEpgMenuSearchResult::refresh(long searchTimerId)
{
   cDbStatement* select = 0;

   menuDb->searchtimerDb->setValue("ID", searchTimerId);

   if (!menuDb->searchtimerDb->find())
      return done;

   if (!(select = menuDb->search->prepareSearchStatement(menuDb->searchtimerDb->getRow(), menuDb->useeventsDb)))
      return fail;

   GET_CHANNELS_READ(channels);

   menuDb->useeventsDb->clear();

   for (int res = select->find(); res; res = select->fetch())
   {
      int cnt = 0;
      cDbStatement* selectDones = 0;

      menuDb->useeventsDb->find();  // get all fields ..

      if (!menuDb->search->matchCriterias(menuDb->searchtimerDb->getRow(), menuDb->useeventsDb->getRow()))
         continue;

      // dones ...

      if (menuDb->search->prepareDoneSelect(menuDb->useeventsDb->getRow(),
                                            menuDb->searchtimerDb->getIntValue("REPEATFIELDS"),
                                            selectDones) == success && selectDones)
      {
         for (int f = selectDones->find(); f; f = selectDones->fetch())
            cnt++;

         selectDones->freeResult();
      }

      //

      cDbRow* useEventRow = new cDbRow("useevents");
      useEventRow->copyValues(menuDb->useeventsDb->getRow(), cDbService::ftPrimary);

      const char* strChannelId = menuDb->useeventsDb->getStrValue("CHANNELID");
      const cChannel* channel = channels->GetByChannelID(tChannelID::FromString(strChannelId));

      Add(new cEpgMenuTextItem(useEventRow,
                               cString::sprintf("%s\t%s\t%s\t%s\t%s",
                                                l2pTime(menuDb->useeventsDb->getIntValue("STARTTIME")).c_str(),
                                                channel->Name(),
                                                cnt ? num2Str(cnt).c_str() : "",
                                                menuDb->useeventsDb->getStrValue("TITLE"),
                                                menuDb->useeventsDb->getStrValue("SHORTTEXT"))));
   }

   select->freeResult();
   menuDb->searchtimerDb->reset();

   SetHelp(0, tr("Show Journal"), 0, 0);
   Display();

   return success;
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cEpgMenuSearchResult::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key)
      {
         case kGreen:
         {
            cEpgMenuTextItem* item = (cEpgMenuTextItem*)Get(Current());
            return AddSubMenu(new cEpgMenuDonesOf(menuDb, item->getRow()));
         }

         default: break;
      }
   }

   return state;
}

//***************************************************************************
// Class cMenuEpgSearchTimers
//***************************************************************************

cMenuEpgSearchTimers::cMenuEpgSearchTimers()
   : cOsdMenu(tr("Search Timers"), 2, 6)
{
   menuDb = new cMenuDb;
   refresh();
}

cMenuEpgSearchTimers::~cMenuEpgSearchTimers()
{
   delete menuDb;
}

int cMenuEpgSearchTimers::refresh()
{
   int current = Current();

   menuDb->searchtimerDb->clear();

   Clear();

   for (int f = menuDb->selectSearchTimers->find(); f && menuDb->dbConnected(); f = menuDb->selectSearchTimers->fetch())
   {
      char* buf;
      asprintf(&buf, "%c\t%ld\t%s",
               menuDb->searchtimerDb->getIntValue("ACTIVE") ? '>' : ' ',
               menuDb->searchtimerDb->getIntValue("HITS"),
               menuDb->searchtimerDb->getStrValue("EXPRESSION"));
      Add(new cEpgMenuSearchTimerItem(menuDb->searchtimerDb->getIntValue("ID"),
                                      menuDb->searchtimerDb->getIntValue("ACTIVE"), buf));
      free(buf);
   }

   menuDb->selectSearchTimers->freeResult();

   Sort();
   SetCurrent(current >= 0 ? Get(current) : First());

   setHelpKeys();
   Display();

   return done;
}

void cMenuEpgSearchTimers::setHelpKeys()
{
   cEpgMenuSearchTimerItem* item = (cEpgMenuSearchTimerItem*)Get(Current());

   if (item)
      SetHelp(item->isActive() ? tr("Deactivate") : tr("Activate"),
              tr("Check"),
              tr("Delete"),
              0);
   else
      SetHelp(0, 0, 0, 0);

   return ;
}

//***************************************************************************
// Process Key
//***************************************************************************

eOSState cMenuEpgSearchTimers::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);
   cEpgMenuSearchTimerItem* item = (cEpgMenuSearchTimerItem*)Get(Current());

   if (state == osUnknown)
   {
      switch (Key)
      {
         case kOk:      // edit search timer
         {
            if (HasSubMenu() || Count() == 0)
               return osContinue;

            return osContinue; // #TODO
            return AddSubMenu(new cEpgMenuSearchTimerEdit(menuDb, item->getId()));
         }

         case kRed:     // ativate/deactivate search timer
         {
            menuDb->searchtimerDb->setValue("ID", item->getId());

            if (menuDb->searchtimerDb->find())
            {
               menuDb->searchtimerDb->setValue("ACTIVE", !menuDb->searchtimerDb->getIntValue("ACTIVE"));
               menuDb->searchtimerDb->update();
               refresh();
            }

            return osContinue;
         }

         case kGreen:    // test search
         {
            return AddSubMenu(new cEpgMenuSearchResult(menuDb, item->getId()));
         }

         case kYellow:   // delete search timer
         {
            if (item && Interface->Confirm(tr("Delete search timer?")))
            {
               menuDb->searchtimerDb->setValue("ID", item->getId());

               if (menuDb->searchtimerDb->find())
               {
                  menuDb->searchtimerDb->setCharValue("STATE", 'D');
                  menuDb->searchtimerDb->update();
                  refresh();
               }
            }

            return osContinue;
         }

         default: break;
      }
   }

   if (!HasSubMenu() && Key != kNone)
      setHelpKeys();

   return state;
}
