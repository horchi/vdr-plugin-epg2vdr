/*
 * epg2vdr.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/menu.h>
#include <vdr/tools.h>

#include "plgconfig.h"

#include "update.h"
#include "menu.h"
#include "handler.h"

#if !defined (APIVERSNUM) || (APIVERSNUM < 20200)
#  error VDR API versions < 2.2.0 are not supported !
#endif

cUpdate* oUpdate = 0;
const char* logPrefix = LOG_PREFIX;

//***************************************************************************
// Static global handler Instance
//***************************************************************************

cEpg2VdrEpgHandler* cEpg2VdrEpgHandler::singleton = cEpg2VdrEpgHandler::getSingleton();

//***************************************************************************
// Menu Edit List Item
//***************************************************************************

class cMenuEditStrListItem : public cMenuEditIntItem
{
   public:

      cMenuEditStrListItem(const char* Name, int* Value, cStringList* List);

   protected:

      virtual void Set();
      cStringList* list;
};

cMenuEditStrListItem::cMenuEditStrListItem(const char* Name, int* Value, cStringList* List)
   : cMenuEditIntItem(Name, Value, 0, List->Size()-1)
{
   list = List;
   Set();
}

void cMenuEditStrListItem::Set()
{
   char* v = (*list)[*value];
   SetValue(v);
}

//***************************************************************************
// Plugin Main Menu
//***************************************************************************

class cEpgPluginMenu : public cOsdMenu
{
   public:

      cEpgPluginMenu(const char* title, cPluginEPG2VDR* aPlugin);
      virtual ~cEpgPluginMenu() { };

      virtual eOSState ProcessKey(eKeys key);

   protected:

      cPluginEPG2VDR* plugin;
};

enum EpgMenuState
{
   emsTimer = os_User,
   emsSearchtimer,
   emsDones,
   emsProgram,
   emsUpdate,
   emsReload
};

cEpgPluginMenu::cEpgPluginMenu(const char* title, cPluginEPG2VDR* aPlugin)
   : cOsdMenu(title)
{
   plugin = aPlugin;

   SetMenuCategory(mcMain);
   SetHasHotkeys(yes);
   Clear();

   if (Epg2VdrConfig.shareInWeb)
   {
      cOsdMenu::Add(new cOsdItem(hk(tr("Timer")), (eOSState)emsTimer));
      cOsdMenu::Add(new cOsdItem(hk(tr("Search Timer")), (eOSState)emsSearchtimer));
      cOsdMenu::Add(new cOsdItem(hk(tr("Timer journal")), (eOSState)emsDones));
   }

   cOsdMenu::Add(new cOsdItem(hk(tr("Program")), (eOSState)emsProgram));
   cOsdMenu::Add(new cOsdItem(hk(tr("Update")), (eOSState)emsUpdate));
   cOsdMenu::Add(new cOsdItem(hk(tr("Reload")), (eOSState)emsReload));

   SetHelp(0, 0, 0, 0);

   Display();
}

eOSState cEpgPluginMenu::ProcessKey(eKeys key)
{
   eOSState state = cOsdMenu::ProcessKey(key);

   switch (state)
   {
      case emsTimer:
      {
         state = AddSubMenu(new cMenuEpgTimers());
         break;
      }

      case emsSearchtimer:
      {
         state = AddSubMenu(new cMenuEpgSearchTimers());
         break;
      }

      case emsDones:
      {
         state = AddSubMenu(new cEpgMenuDones());
         break;
      }

      case emsProgram:
      {
         state = AddSubMenu(new cMenuEpgWhatsOn());
         break;
      }

      case emsUpdate:
      {
         Skins.Message(mtInfo, tr("Update EPG"));
         oUpdate->triggerEpgUpdate();
         return osEnd;
      }

      case emsReload:
      {
         Skins.Message(mtInfo, tr("Reload EPG"));
         oUpdate->triggerEpgUpdate(true);
         return osEnd;
      }

      default: ;
   }

   return state;
}

//***************************************************************************
// Plugin Setup Menu
//***************************************************************************

class cMenuSetupEPG2VDR : public cMenuSetupPage
{
   public:

      cMenuSetupEPG2VDR();
      ~cMenuSetupEPG2VDR() ;

   protected:

      virtual eOSState ProcessKey(eKeys Key);
      virtual void Store();
      virtual void Setup();

   private:

      cEpg2VdrConfig data;
      cMenuDb* menuDb;
      long int webLoginEnabled;
      char** userList;
      int userCount;
      cStringList interfaceList;
      int interfaceIndex;
};

cMenuSetupEPG2VDR::cMenuSetupEPG2VDR()
{
   data = Epg2VdrConfig;
   menuDb = new cMenuDb;
   webLoginEnabled = no;
   userList = 0;
   userCount = 0;
   interfaceIndex = 0;

   Setup();
}

cMenuSetupEPG2VDR::~cMenuSetupEPG2VDR()
{
   if (userList)
   {
      for (int i = 0; i < userCount; i++)
         free(userList[i]);

      delete userList;
   }

   delete menuDb;
}

void cMenuSetupEPG2VDR::Setup()
{
   int current = Current();

   //
   // DVB Event update mode

   static const char* masterModes[4];

   masterModes[0] = tr("auto");
   masterModes[1] = tr("yes");
   masterModes[2] = tr("no");
   masterModes[3] = 0;

   //
   // List of WEB users

   menuDb->initUserList(userList, userCount, webLoginEnabled);

   if (webLoginEnabled && userCount && !isEmpty(Epg2VdrConfig.user))
   {
      // set userIndex to the configured user

      for (int i = 0; i < userCount; i++)
      {
         if (strcmp(userList[i], data.user) == 0)
         {
            data.userIndex = i;
            break;
         }
      }
   }

   //
   // Network Interfaces

   int i = 0;
   char* interfaces;

   interfaces = strdup(getInterfaces());
   interfaceList.Clear();

   for (char* p = strtok(interfaces, " "); p; p = strtok(0, " "))
   {
      interfaceList.Append(strdup(p));

      if (!isEmpty(data.netDevice) && strncmp(p, data.netDevice, strlen(data.netDevice)) == 0)
      {
         tell(0, "Index of '%s' is %d", p, i);
         interfaceIndex = i;
      }

      i++;
   }

   free(interfaces);

   // ...

   Clear();

   Add(new cOsdItem(cString::sprintf("-------------------- %s ----------------------------------", tr("EPG Update"))));
   cList<cOsdItem>::Last()->SetSelectable(false);

   cOsdMenu::Add(new cMenuEditStraItem(tr("Update DVB EPG Database"), (int*)&data.masterMode, cUpdate::mmCount, masterModes));
   Add(new cMenuEditBoolItem(tr("Load Images"), &data.getepgimages));
   Add(new cMenuEditBoolItem(tr("Prohibit Shutdown On Busy 'epgd'"), &data.activeOnEpgd));
   Add(new cMenuEditBoolItem(tr("Schedule Boot For Update"), &data.scheduleBoot));
   Add(new cMenuEditBoolItem(tr("Blacklist not configured Channels"), &data.blacklist));
#if (defined (APIVERSNUM) && (APIVERSNUM >= 20304)) || (WITH_AUX_PATCH)
   Add(new cMenuEditBoolItem(tr("Store extended EPD Data to AUX (e.g. for Skins)"), &data.extendedEpgData2Aux));
#endif

   Add(new cOsdItem(cString::sprintf("--------------------- %s ---------------------------------", tr("Menu"))));
   cList<cOsdItem>::Last()->SetSelectable(false);
   Add(new cMenuEditBoolItem(tr("Show In Main Menu"), &data.mainmenuVisible));
   Add(new cMenuEditBoolItem(tr("Replace Program Menu"), &data.replaceScheduleMenu));
   Add(new cMenuEditBoolItem(tr("Replace Timer Menu"), &data.replaceTimerMenu));
   Add(new cMenuEditBoolItem(tr("XChange Key Ok/Blue"), &data.xchgOkBlue));
   Add(new cMenuEditBoolItem(tr("Show Channels without EPG"), &data.showEmptyChannels));

   Add(new cOsdItem(cString::sprintf("--------------------- %s ---------------------------------", tr("Web"))));
   cList<cOsdItem>::Last()->SetSelectable(false);
   Add(new cMenuEditBoolItem(tr("Share in Web"), &data.shareInWeb));
   Add(new cMenuEditBoolItem(tr("Create Timer Local"), &data.createTimerLocal));
   Add(new cMenuEditBoolItem(tr("Have Common Recordings Folder (NAS)"), &data.useCommonRecFolder));
   Add(new cMenuEditStrListItem(tr("SVDRP Interface"), &interfaceIndex, &interfaceList));
   if (userCount && webLoginEnabled)
      Add(new cMenuEditStraItem(tr("User"), (int*)&data.userIndex, userCount, userList));

   Add(new cOsdItem(cString::sprintf("--------------------- %s ---------------------------------", tr("MySQL"))));
   cList<cOsdItem>::Last()->SetSelectable(false);
   Add(new cMenuEditStrItem(tr("Host"), data.dbHost, sizeof(data.dbHost), tr(FileNameChars)));
   Add(new cMenuEditIntItem(tr("Port"), &data.dbPort, 1, 99999));
   Add(new cMenuEditStrItem(tr("Database Name"), data.dbName, sizeof(data.dbName), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("User"), data.dbUser, sizeof(data.dbUser), tr(FileNameChars)));
   Add(new cMenuEditStrItem(tr("Password"), data.dbPass, sizeof(data.dbPass), tr(FileNameChars)));

   Add(new cOsdItem(cString::sprintf("--------------------- %s ---------------------------------", tr("Technical Stuff"))));
   cList<cOsdItem>::Last()->SetSelectable(false);
   Add(new cMenuEditIntItem(tr("Log level"), &data.loglevel, 0, 4));

   SetCurrent(Get(current));
   Display();
}

eOSState cMenuSetupEPG2VDR::ProcessKey(eKeys Key)
{
   eOSState state = cMenuSetupPage::ProcessKey(Key);

   switch (state)
   {
      case osContinue:
      {
         if (NORMALKEY(Key) == kUp || NORMALKEY(Key) == kDown)
         {
            cOsdItem* item = Get(Current());

            if (item)
               item->ProcessKey(kNone);
         }

         break;
      }

      default: break;
   }

   return state;
}

void cMenuSetupEPG2VDR::Store()
{
   int useCommonRecFolderOptionChanged = no;

   if (Epg2VdrConfig.useCommonRecFolder != data.useCommonRecFolder)
      useCommonRecFolderOptionChanged = yes;

   if (data.hasDbLoginChanged(&Epg2VdrConfig))
      oUpdate->triggerDbReconnect();

   Epg2VdrConfig = data;

   SetupStore("LogLevel", Epg2VdrConfig.loglevel);
   SetupStore("ShowInMainMenu", Epg2VdrConfig.mainmenuVisible);
   SetupStore("Blacklist", Epg2VdrConfig.blacklist);
   SetupStore("DbHost", Epg2VdrConfig.dbHost);
   SetupStore("DbPort", Epg2VdrConfig.dbPort);
   SetupStore("DbName", Epg2VdrConfig.dbName);
   SetupStore("DbUser", Epg2VdrConfig.dbUser);
   SetupStore("DbPass", Epg2VdrConfig.dbPass);
   SetupStore("MasterMode", Epg2VdrConfig.masterMode);
   SetupStore("LoadImages", Epg2VdrConfig.getepgimages);
   SetupStore("ActiveOnEpgd", Epg2VdrConfig.activeOnEpgd);
   SetupStore("ScheduleBoot", Epg2VdrConfig.scheduleBoot);
   SetupStore("ShareInWeb", Epg2VdrConfig.shareInWeb);
   SetupStore("CreateTimerLocal", Epg2VdrConfig.createTimerLocal);
   SetupStore("XChgKeyOkBlue", Epg2VdrConfig.xchgOkBlue);
   SetupStore("ShowEmptyChannels", Epg2VdrConfig.showEmptyChannels);
   SetupStore("UseCommonRecFolder", Epg2VdrConfig.useCommonRecFolder);
   SetupStore("ReplaceScheduleMenu", Epg2VdrConfig.replaceScheduleMenu);
   SetupStore("ReplaceTimerMenu", Epg2VdrConfig.replaceTimerMenu);
   SetupStore("ExtendedEpgData2Aux", Epg2VdrConfig.extendedEpgData2Aux);

   if (userCount && Epg2VdrConfig.userIndex >= 0)
   {
      sstrcpy(Epg2VdrConfig.user, userList[Epg2VdrConfig.userIndex], sizeof(Epg2VdrConfig.user));
      SetupStore("User", Epg2VdrConfig.user);
   }

   char* device = strdup(interfaceList[interfaceIndex]);
   char* ip = strchr(device, ':');

   if (!isEmpty(ip))
   {
      *(ip++) = 0;

      if (strcmp(Epg2VdrConfig.netDevice, device) != 0)
      {
         sstrcpy(Epg2VdrConfig.netDevice, device, sizeof(Epg2VdrConfig.netDevice));

         menuDb->vdrDb->clear();
         menuDb->vdrDb->setValue("UUID", Epg2VdrConfig.uuid);
         menuDb->vdrDb->find();
         menuDb->vdrDb->setValue("IP", ip);
         menuDb->vdrDb->store();
      }
   }

   free(device);

   SetupStore("NetDevice", Epg2VdrConfig.netDevice);

   if (useCommonRecFolderOptionChanged)
      oUpdate->commonRecFolderOptionChanged();
}

//***************************************************************************
// Plugin - EPG2VDR
//***************************************************************************

cPluginEPG2VDR::cPluginEPG2VDR()
{
   pluginInitialized = no;
   oUpdate = 0;
   connection = 0;
   timerDb = 0;
   vdrDb = 0;
   useeventsDb = 0;
   selectTimers = 0;
   selectTimerByEvent = 0;
   selectEventById = 0;
   recordingListDb = 0;
}

cPluginEPG2VDR::~cPluginEPG2VDR()
{
   exitDb();
   delete oUpdate;
}

int cPluginEPG2VDR::initDb()
{
   int status = success;

   exitDb();

   connection = new cDbConnection();

   timerDb = new cDbTable(connection, "timers");
   if (timerDb->open() != success) return fail;

   vdrDb = new cDbTable(connection, "vdrs");
   if (vdrDb->open() != success) return fail;

   useeventsDb = new cDbTable(connection, "useevents");
   if (useeventsDb->open() != success) return fail;

   recordingListDb = new cDbTable(connection, "recordinglist");
   if (recordingListDb->open() != success) return fail;

   // ----------
   // select
   //    t.*,
   //    v.name, v.state
   //    from timers t, vdrs v
   //    where
   //          (t.state in ('P','R') or t.state is null)
   //      and t.vdruuid = v.uuid

   selectTimers = new cDbStatement(timerDb);

   selectTimers->build("select ");
   selectTimers->setBindPrefix("t.");
   selectTimers->bindAllOut();
   selectTimers->setBindPrefix("v.");
   selectTimers->bind(vdrDb, "NAME", cDBS::bndOut, ", ");
   selectTimers->bind(vdrDb, "UUID", cDBS::bndOut, ", ");
   selectTimers->bind(vdrDb, "STATE", cDBS::bndOut, ", ");
   selectTimers->clrBindPrefix();
   selectTimers->build(" from %s t, %s v where t.%s and (t.%s in ('P','R') or t.%s is null)",
                       timerDb->TableName(), vdrDb->TableName(),
                       timerDb->getField("ACTIVE")->getDbName(),
                       timerDb->getField("STATE")->getDbName(),
                       timerDb->getField("STATE")->getDbName());
   selectTimers->build(" and t.%s = '%c'",
                       timerDb->getField("TYPE")->getDbName(), ttRecord);
   selectTimers->build(" and t.%s = v.%s order by t.%s",
                       timerDb->getField("VDRUUID")->getDbName(),
                       vdrDb->getField("UUID")->getDbName(),
                       timerDb->getField("_STARTTIME")->getDbName());

   status += selectTimers->prepare();

   // select id, eventid, channelid, starttime, state, endtime
   //   from timers where
   //     eventid = ? and channelid = ? and vdruuid = ?

   selectTimerByEvent = new cDbStatement(timerDb);

   selectTimerByEvent->build("select ");
   selectTimerByEvent->bindAllOut();
   selectTimerByEvent->build(" from %s where %s and (%s in ('P','R') or %s is null)",
                             timerDb->TableName(),
                             timerDb->getField("ACTIVE")->getDbName(),
                             timerDb->getField("STATE")->getDbName(),
                             timerDb->getField("STATE")->getDbName());
   selectTimerByEvent->bind("EVENTID", cDBS::bndIn | cDBS::bndSet, " and ");

   status += selectTimerByEvent->prepare();

   // select event by useid
   // select * from eventsview
   //      where useid = ?
   //        and updflg in (.....)

   selectEventById = new cDbStatement(useeventsDb);

   selectEventById->build("select ");
   selectEventById->bindAllOut();
   selectEventById->build(" from %s where ", useeventsDb->TableName());
   selectEventById->bind("USEID", cDBS::bndIn | cDBS::bndSet);
   selectEventById->build(" and %s in (%s)",
                          useeventsDb->getField("UPDFLG")->getDbName(),
                          Us::getNeeded());

   status += selectEventById->prepare();

   return status;
}

int cPluginEPG2VDR::exitDb()
{
   if (connection)
   {
      delete selectEventById;      selectEventById = 0;
      delete selectTimers;         selectTimers = 0;
      delete selectTimerByEvent;   selectTimerByEvent = 0;

      delete useeventsDb;          useeventsDb = 0;
      delete timerDb;              timerDb = 0;
      delete vdrDb;                vdrDb = 0;
      delete recordingListDb;      recordingListDb = 0;

      delete connection;           connection = 0;
   }

   return done;
}

void cPluginEPG2VDR::DisplayMessage(const char *s)
{
   tell(0, "%s", s);

   Skins.Message(mtInfo, tr(s));
   sleep(Setup.OSDMessageTime);
}

const char *cPluginEPG2VDR::CommandLineHelp()
{
   return 0;
}

const char **cPluginEPG2VDR::SVDRPHelpPages()
{
   static const char *HelpPages[] =
   {
      "UPDATE\n"
      "    Load new/changed events database.",
      "RELOAD\n"
      "    Reload all events from database to EPG",
      "STATE <state>\n"
      "    For internal usage",
      "TIMERJOB\n"
      "    Check timer jobs",
      "UPDREC\n"
      "    Trigger update of recordings",
      "STOREIFO\n"
      "    Trigger store of recordin info files",
      0
   };

   return HelpPages;
}

cString cPluginEPG2VDR::SVDRPCommand(const char* cmd, const char* Option, int &ReplyCode)
{
   // ------------------------------------
   // update epg from database

   if (strcasecmp(cmd, "UPDATE") == 0)
   {
      oUpdate->triggerEpgUpdate();
      return "EPG2VDR update started.";
   }

   // ------------------------------------
   // reload epg from database

   else if (strcasecmp(cmd, "RELOAD") == 0)
   {
      oUpdate->triggerEpgUpdate(true);
      return "EPG2VDR full reload of events from database.";
   }

   // ------------------------------------
   // update recording table

   else if (strcasecmp(cmd, "UPDREC") == 0)
   {
      oUpdate->triggerRecUpdate();
      return "EPG2VDR update of recordings triggert.";
   }

   // ------------------------------------
   // store recording info files

   else if (strcasecmp(cmd, "STOREIFO") == 0)
   {
      oUpdate->triggerStoreInfoFiles();
      return "EPG2VDR store of info files triggert.";
   }

   // ------------------------------------
   // inform about epgd's state change

   else if (strcasecmp(cmd, "STATE") == 0)
   {
      if (isEmpty(Option))
      {
         ReplyCode = 901;
         return "Error: Missing option";
      }

      oUpdate->epgdStateChange(Option);
      return "EPG2VDR epgd state change accepted";
   }

   // ------------------------------------
   // trigger processing timer job

   else if (strcasecmp(cmd, "TIMERJOB") == 0)
   {
      oUpdate->triggerTimerJobs();
      return "EPG2VDR check of timer jobs triggered";
   }

   // ------------------------------------
   // delete recording

   else if (strcasecmp(cmd, "DELREC") == 0)
   {
      if (isEmpty(Option))
      {
         ReplyCode = 901;
         return "Error: Missing option";
      }

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_RECORDINGS_WRITE;
      cRecordings* recordings = Recordings;
#else
      cRecordings* recordings = &Recordings;
#endif

      if (cRecording* rec = recordings->GetByName(Option))
      {
         if (rec->IsInUse())
         {
            ReplyCode = 554;
            tell(0, "Can't modify, recoring is in use");
            return "Can't delete, recoring is in use";
         }

         if (!rec->Delete())
         {
            ReplyCode = 554;
            tell(0, "Error: Delete of recording '%s' failed", Option);
            return "Error: Delete of recording failed";
         }

         recordings->DelByName(rec->FileName());
#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         recordings->SetModified();
#endif
      }
      else
      {
         ReplyCode = 550;
         tell(0, "Error: Delete failed, can't find recording '%s'", Option);
         return "Error: Delete failed, can't find recording";
      }

      oUpdate->triggerRecUpdate();
      tell(0, "Deleted recording '%s'", Option);
      return "Deleted recording";
   }

   // ------------------------------------
   // play recording

   else if (strcasecmp(cmd, "PLAYREC") == 0)
   {
      if (isEmpty(Option))
      {
         ReplyCode = 901;
         return "Error: Missing option";
      }

      char* opt = strdup(Option);
      char* name = skipspace(opt);
      char* position = skipspace(opt);

      while (*position && !isspace(*position))
         position++;

      if (*position)
      {
         *position = 0;
         position++;
      }

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_RECORDINGS_READ;
      const cRecording* recording = Recordings->GetByName(name);
#else
      const cRecording* recording = Recordings.GetByName(name);
#endif

      if (!recording)
      {
         free(opt);
         ReplyCode = 901;
         return "Recording not found";
      }

      cReplayControl::SetRecording(0);
      cControl::Shutdown();

      if (!isEmpty(position))
      {
         int pos = 0;

         if (strcasecmp(position, "BEGIN") != 0)
            pos = HMSFToIndex(position, recording->FramesPerSecond());

         cResumeFile Resume(recording->FileName(), recording->IsPesRecording());

         if (pos <= 0)
            Resume.Delete();
         else
            Resume.Save(pos);
      }

      cReplayControl::SetRecording(recording->FileName());
      cControl::Launch(new cReplayControl);
      cControl::Attach();
      free(opt);
      tell(0, "Playing recording '%s'", name);
      return "Playing recording";
   }

   // ------------------------------------
   // rename recording

   else if (strcasecmp(cmd, "RENREC") == 0)
   {
      char* alt = 0;
      char* neu = 0;
      char* p;

      if (!isEmpty(Option))
      {
         alt = strdup(Option);
         p = alt;

         if ((p = strchr(p, ' ')))
         {
            *p = 0;
            neu = p+1;
         }
      }

      if (isEmpty(neu) || isEmpty(alt))
      {
         free(alt);
         ReplyCode = 901;
         return "Error: Missing option";
      }

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
      LOCK_RECORDINGS_WRITE;
      cRecordings* recordings = Recordings;
      recordings->SetExplicitModify();
#else
      cRecordings* recordings = &Recordings;
#endif

      if (cRecording* rec = recordings->GetByName(alt))
      {
         if (rec->IsInUse())
         {
            ReplyCode = 554;
            tell(0, "Can't modify, recoring is in use");
            return "Can't modify, recoring is in use";
         }
         if (!rec->ChangeName(neu))
         {
            ReplyCode = 554;
            tell(0, "Error: Modify of recordings '%s' failed", alt);
            free(alt);
            return "Error: Modify of recording failed";
         }

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
         recordings->SetModified();
#endif
         recordings->TouchUpdate();
      }
      else
      {
         ReplyCode = 550;
         tell(0, "Error: Modify failed, can't find recording '%s'", alt);
         free(alt);
         return "Error: Modify failed, can't find recording";
      }

      oUpdate->triggerRecUpdate();
      tell(0, "Modified recording '%s' to '%s'", alt, neu);
      free(alt);
      return "Modified recording";
   }

   return 0;
}

//***************************************************************************
// Service
//***************************************************************************

bool cPluginEPG2VDR::Service(const char* id, void* data)
{
   if (!data)
      return false;

   tell(3, "Service called with '%s', %d/%d", id,
        Epg2VdrConfig.replaceScheduleMenu, Epg2VdrConfig.replaceTimerMenu);

   if (strcmp(id, EPG2VDR_UUID_SERVICE) == 0)
   {
      Epg2vdr_UUID_v1_0* req = (Epg2vdr_UUID_v1_0*)data;

      req->uuid = Epg2VdrConfig.uuid;

      return true;
   }

   else if (strcmp(id, MYSQL_INIT_EXIT) == 0)
   {
      Mysql_Init_Exit_v1_0* ref = (Mysql_Init_Exit_v1_0*)data;

      if (ref->action == mieaInit)
         cDbConnection::init();
      else if (ref->action == mieaExit)
         cDbConnection::exit();
      else
         tell(0, "Warning: Got unexpected action %d in '%s' call",
              ref->action, MYSQL_INIT_EXIT);

      return true;
   }

   if (!pluginInitialized)
   {
      tell(2, "Service called but plugin not ready, retry later");
      return false;
   }

   if (strcmp(id, "MainMenuHooksPatch-v1.0::osSchedule") == 0 && Epg2VdrConfig.replaceScheduleMenu)
   {
      cOsdMenu** menu = (cOsdMenu**)data;

      if (menu)
         *menu = new cMenuEpgWhatsOn();

      return true;
   }

   else if (strcmp(id, "MainMenuHooksPatch-v1.0::osTimers") == 0 && Epg2VdrConfig.replaceTimerMenu && Epg2VdrConfig.shareInWeb)
   {
      cOsdMenu** menu = (cOsdMenu**)data;

      if (menu)
         *menu = new cMenuEpgTimers();

      return true;
   }

   if (strcmp(id, EPG2VDR_TIMER_SERVICE) == 0 || strcmp(id, EPG2VDR_REC_DETAIL_SERVICE) == 0 || strcmp(id, EPG2VDR_TIMER_DETAIL_SERVICE) == 0)
   {
      // Services with direct db access

      cMutexLock lock(&mutexServiceWithDb);

      if (initDb() == success)
      {
         if (strcmp(id, EPG2VDR_TIMER_SERVICE) == 0)
         {
            cEpgTimer_Service_V1* ts = (cEpgTimer_Service_V1*)data;

            if (ts)
               return timerService(ts);
         }
         if (strcmp(id, EPG2VDR_TIMER_DETAIL_SERVICE) == 0)
         {
            cTimer_Detail_V1* d = (cTimer_Detail_V1*)data;

            if (d)
               return hasTimerService(d);
         }
         else if (strcmp(id, EPG2VDR_REC_DETAIL_SERVICE) == 0)
         {
            cEpgRecording_Details_Service_V1* rd = (cEpgRecording_Details_Service_V1*)data;

            if (rd)
               return recordingDetails(rd);
         }

         exitDb();
      }
   }

   return false;
}

//***************************************************************************
// Has Timer Service
//***************************************************************************

int cPluginEPG2VDR::hasTimerService(cTimer_Detail_V1* d)
{
   cMutexLock lock(&mutexTimerService);

   d->hastimer = no;
   d->local = yes;
   d->type = ttRecord;

   timerDb->clear();
   timerDb->setValue("EVENTID", d->eventid);

   if (selectTimerByEvent->find())
   {
      d->hastimer = yes;
      d->local = timerDb->hasValue("VDRUUID", Epg2VdrConfig.uuid);
      d->type = timerDb->getValue("TYPE")->getCharValue();

      tell(3, "timer service found '%s' timer (%ld) for event (%ld) type '%c'",
           d->local ? "local" : "remote", timerDb->getIntValue("ID"), d->eventid, d->type);
   }

   selectTimerByEvent->freeResult();

   return true;
}

//***************************************************************************
// Timer Service
//***************************************************************************

int cPluginEPG2VDR::timerService(cEpgTimer_Service_V1* ts)
{
   cMutexLock lock(&mutexTimerService);
   uint64_t start = cTimeMs::Now();

   timerDb->clear();
   vdrDb->clear();

   ts->epgTimers.clear();

   for (int f = selectTimers->find(); f && connection->check() == success; f = selectTimers->fetch())
   {
      cEpgTimer* epgTimer = newTimerObjectFromRow(timerDb->getRow(), vdrDb->getRow());

      if (Epg2VdrConfig.shareInWeb || epgTimer->isLocal())
         ts->epgTimers.push_back(epgTimer);
      else
         delete epgTimer;
   }

   selectTimers->freeResult();

   tell(1, "Answer '%s' call with %zd timers, duration was (%s)",
        EPG2VDR_TIMER_SERVICE,
        ts->epgTimers.size(),
        ms2Dur(cTimeMs::Now()-start).c_str());

   return true;
}

//***************************************************************************
// Recording Details
//***************************************************************************

#include <vdr/videodir.h>

int cPluginEPG2VDR::recordingDetails(cEpgRecording_Details_Service_V1* rd)
{
   int found = false;

#if defined (APIVERSNUM) && (APIVERSNUM >= 20301)
   const char* videoBasePath = cVideoDirectory::Name();
   md5Buf md5path;
   const cRecording* recording;
   int pathOffset = 0;

   LOCK_RECORDINGS_READ;
   const cRecordings* recordings = Recordings;

   if (!(recording = recordings->GetById(rd->id)))
      return false;

   if (strncmp(recording->FileName(), videoBasePath, strlen(videoBasePath)) == 0)
   {
      pathOffset = strlen(videoBasePath);

      if (*(recording->FileName()+pathOffset) == '/')
         pathOffset++;
   }

   createMd5(recording->FileName()+pathOffset, md5path);

   recordingListDb->clear();

   recordingListDb->setValue("MD5PATH", md5path);
   recordingListDb->setValue("STARTTIME", recording->Start());
   recordingListDb->setValue("OWNER", Epg2VdrConfig.useCommonRecFolder ? "" : Epg2VdrConfig.uuid);

   cXml xml;

   found = recordingListDb->find();

   xml.create("epg2vdr");

   if (found)
      cEventDetails::row2Xml(recordingListDb->getRow(), &xml);

   rd->details = xml.toText();

   recordingListDb->reset();

#endif

   return found;
}

//***************************************************************************
// Initialize
//***************************************************************************

bool cPluginEPG2VDR::Initialize()
{
   return true;
}

//***************************************************************************
// Start
//***************************************************************************

bool cPluginEPG2VDR::Start()
{
   Mysql_Init_Exit_v1_0 req;

   req.action = mieaInit;
   Service(MYSQL_INIT_EXIT, &req);

   oUpdate = new cUpdate(this);

   if (oUpdate->init() == success)
   {
      oUpdate->Start();                 // start plugin thread
      pluginInitialized = yes;
   }
   else
      tell(0, "Initialization failed, start of plugin aborted!");

   return true;
}

cString cPluginEPG2VDR::Active()
{
   if (Epg2VdrConfig.activeOnEpgd && oUpdate->isEpgdBusy())
      return tr("EPG2VDR Waiting on epgd");

//    time_t timeoutAt = time(0) + 10;

//    while (oUpdate->isUpdateActive())
//    {
//       tell(0, "EPG2VDR EPG update running, wating up to 10 seconds ..");

//       if (time(0) > timeoutAt)
//       {
//          tell(0, "EPG2VDR EPG update running, shutdown timed out, aborting");
//          return 0;
//       }

//       usleep(500000);
//    }

   return 0;
}

time_t cPluginEPG2VDR::WakeupTime()
{
   // return custom wakeup time for shutdown script

   if (Epg2VdrConfig.scheduleBoot && oUpdate->getNextEpgdUpdateAt())
      return oUpdate->getNextEpgdUpdateAt();

   return 0;
}

cOsdObject* cPluginEPG2VDR::MainMenuAction()
{
   return new cEpgPluginMenu(MAINMENUENTRY, this);
}

cMenuSetupPage* cPluginEPG2VDR::SetupMenu()
{
   return new cMenuSetupEPG2VDR;
}

bool cPluginEPG2VDR::SetupParse(const char *Name, const char *Value)
{
   // Parse your own setup parameters and store their values.

   if      (!strcasecmp(Name, "LogLevel"))             Epg2VdrConfig.loglevel = atoi(Value);
   else if (!strcasecmp(Name, "ShowInMainMenu"))       Epg2VdrConfig.mainmenuVisible = atoi(Value);
   else if (!strcasecmp(Name, "Blacklist"))            Epg2VdrConfig.blacklist = atoi(Value);
   else if (!strcasecmp(Name, "DbHost"))               sstrcpy(Epg2VdrConfig.dbHost, Value, sizeof(Epg2VdrConfig.dbHost));
   else if (!strcasecmp(Name, "DbPort"))               Epg2VdrConfig.dbPort = atoi(Value);
   else if (!strcasecmp(Name, "DbName"))               sstrcpy(Epg2VdrConfig.dbName, Value, sizeof(Epg2VdrConfig.dbName));
   else if (!strcasecmp(Name, "DbUser"))               sstrcpy(Epg2VdrConfig.dbUser, Value, sizeof(Epg2VdrConfig.dbUser));
   else if (!strcasecmp(Name, "DbPass"))               sstrcpy(Epg2VdrConfig.dbPass, Value, sizeof(Epg2VdrConfig.dbPass));
   else if (!strcasecmp(Name, "MasterMode"))           Epg2VdrConfig.masterMode = atoi(Value);
   else if (!strcasecmp(Name, "LoadImages"))           Epg2VdrConfig.getepgimages = atoi(Value);
   else if (!strcasecmp(Name, "ActiveOnEpgd"))         Epg2VdrConfig.activeOnEpgd = atoi(Value);
   else if (!strcasecmp(Name, "ScheduleBoot"))         Epg2VdrConfig.scheduleBoot = atoi(Value);
   else if (!strcasecmp(Name, "UseCommonRecFolder"))   Epg2VdrConfig.useCommonRecFolder = atoi(Value);
   else if (!strcasecmp(Name, "ShareInWeb"))           Epg2VdrConfig.shareInWeb = atoi(Value);
   else if (!strcasecmp(Name, "CreateTimerLocal"))     Epg2VdrConfig.createTimerLocal = atoi(Value);
   else if (!strcasecmp(Name, "XChgKeyOkBlue"))        Epg2VdrConfig.xchgOkBlue = atoi(Value);
   else if (!strcasecmp(Name, "ShowEmptyChannels"))    Epg2VdrConfig.showEmptyChannels = atoi(Value);
   else if (!strcasecmp(Name, "Uuid"))                 sstrcpy(Epg2VdrConfig.uuid, Value, sizeof(Epg2VdrConfig.uuid));
   else if (!strcasecmp(Name, "NetDevice"))            sstrcpy(Epg2VdrConfig.netDevice, Value, sizeof(Epg2VdrConfig.netDevice));
   else if (!strcasecmp(Name, "ReplaceScheduleMenu"))  Epg2VdrConfig.replaceScheduleMenu = atoi(Value);
   else if (!strcasecmp(Name, "ReplaceTimerMenu"))     Epg2VdrConfig.replaceTimerMenu = atoi(Value);
   else if (!strcasecmp(Name, "User"))                 sstrcpy(Epg2VdrConfig.user, Value, sizeof(Epg2VdrConfig.user));
   else if (!strcasecmp(Name, "ExtendedEpgData2Aux"))  Epg2VdrConfig.extendedEpgData2Aux = atoi(Value);

   else
      return false;

   return true;
}

void cPluginEPG2VDR::Stop()
{
   oUpdate->Stop();

   Mysql_Init_Exit_v1_0 req;

   req.action = mieaExit;
   Service(MYSQL_INIT_EXIT, &req);
   pluginInitialized = no;
}

//***************************************************************************

VDRPLUGINCREATOR(cPluginEPG2VDR)
