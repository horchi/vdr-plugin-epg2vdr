/*
 * menusearchtimer.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/interface.h>

#include "lib/searchtimer.h"
#include "menu.h"

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
      const char* name = def->getField(i)->getDescription();

      if (!value || def->getField(i)->hasType(cDBS::ftMeta))
         continue;

      if (isEmpty(name))
         name = def->getField(i)->getName();

      if (def->getField(i)->hasFormat(cDBS::ffAscii))
         Add(new cMenuEditStrItem(tr(name), (char*)value->getStrValueRef(), def->getField(i)->getSize()));

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

      ~cEpgMenuSearchTimerItem() { }

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

      cEpgMenuSearchResult(cMenuDb* db, long id);
      virtual ~cEpgMenuSearchResult();
      virtual eOSState ProcessKey(eKeys Key);

   protected:

      int refresh(long id);

      cMenuDb* menuDb;
};

cEpgMenuSearchResult::cEpgMenuSearchResult(cMenuDb* db, long id)
   : cOsdMenu(tr("Edit Search Timer"), 17, CHNAMWIDTH, 3, 30)
{
   menuDb = db;
   refresh(id);
}

cEpgMenuSearchResult::~cEpgMenuSearchResult()
{
}

//***************************************************************************
// Refresh
//***************************************************************************

int cEpgMenuSearchResult::refresh(long id)
{
   cDbStatement* select = 0;

   menuDb->searchtimerDb->setValue("ID", id);

   if (!menuDb->searchtimerDb->find())
      return done;

   if (!(select = menuDb->search->prepareSearchStatement(menuDb->searchtimerDb->getRow(), menuDb->useeventsDb)))
      return fail;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   LOCK_CHANNELS_READ;
   const cChannels* channels = Channels;
   // cChannelsLock channelsLock(false);
   // const cChannels* channels = channelsLock.Channels();
#else
   cChannels* channels = &Channels;
#endif

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
                                            selectDones) == success
          && selectDones)
      {
         // first only count - #TODO show them in a sub-menu oder let delete by kYellow?

         for (int f = selectDones->find(); f; f = selectDones->fetch())
            cnt++;

         selectDones->freeResult();
      }

      //

      const char* strChannelId = menuDb->useeventsDb->getStrValue("CHANNELID");
      const cChannel* channel = channels->GetByChannelID(tChannelID::FromString(strChannelId));

      Add(new cEpgMenuTextItem(menuDb->useeventsDb->getIntValue("USEID"),
                               cString::sprintf("%s\t%s\t%s\t%s\t%s",
                                                l2pTime(menuDb->useeventsDb->getIntValue("STARTTIME")).c_str(),
                                                channel->Name(),
                                                cnt ? num2Str(cnt).c_str() : "",
                                                menuDb->useeventsDb->getStrValue("TITLE"),
                                                menuDb->useeventsDb->getStrValue("SHORTTEXT"))));
  }

   select->freeResult();
   menuDb->searchtimerDb->reset();

   SetHelp(0, 0, "Delete dones", 0);
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
         case kYellow:
         {
            cDbStatement* selectDones = 0;
            cEpgMenuTextItem* item = (cEpgMenuTextItem*)Get(Current());

            if (item && Interface->Confirm(tr("Remove all done entries of this event?")))
            {
               menuDb->useeventsDb->clear();
               menuDb->useeventsDb->setValue("USEID", item->getId());

               if (menuDb->useeventsDb->find())
               {
                  if (menuDb->search->prepareDoneSelect(menuDb->useeventsDb->getRow(),
                                                        menuDb->searchtimerDb->getIntValue("REPEATFIELDS"),
                                                        selectDones) == success
                      && selectDones)
                  {
                     for (int f = selectDones->find(); f; f = selectDones->fetch())
                        selectDones->getTable()->deleteWhere("id = %ld", selectDones->getTable()->getIntValue("ID"));

                     selectDones->freeResult();
                  }
               }
            }
         }

         default: break;
      }
   }

   return state;
}

//***************************************************************************
// Class cMenuEpgSearchTimers
//***************************************************************************
//***************************************************************************
// Object
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
              tr("Test"),
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
         case kOk:
         {
            if (HasSubMenu() || Count() == 0)
               return osContinue;

            return osContinue; // #TODO
            return AddSubMenu(new cEpgMenuSearchTimerEdit(menuDb, item->getId()));
         }

         case kRed:
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

         case kGreen:
         {
            return AddSubMenu(new cEpgMenuSearchResult(menuDb, item->getId()));
         }

         case kYellow:
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
