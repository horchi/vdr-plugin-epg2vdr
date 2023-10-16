/*
 * parameters.c
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "plgconfig.h"
#include "parameters.h"

//***************************************************************************
// Parameters
//***************************************************************************

cParameters::Parameter cParameters::parameters[] =
{
   // owner,     name,                       type,      default,                    regexp,           readonly,   visible

#ifndef VDR_PLUGIN
   // --------------------------------------------
   // epgd / epghttpd

   { "epgd",     "manualTimer2Done",         ptBool,    "1",                        "^[01]$",               no,    yes },
   { "epgd",     "timerJobFailedHistory",    ptNum,     "10",                       "^[0-9]{1,3}$",         no,    yes },

   { "epgd",     "logoUpperCase",            ptBool,    "0",                        "^[01]$",               no,    yes },
   { "epgd",     "logoById",                 ptBool,    "0",                        "^[01]$",               no,    yes },
   { "epgd",     "logoSuffix",               ptAscii,   "png",                      "^.{1,5}$",             no,    yes },

   { "epgd",     "mailScript",               ptAscii,   BINDEST"/epgd-sendmail",    "^.{0,150}$",           no,    yes },

   { "epgd",     "mergeStart",               ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   no  },
   { "epgd",     "lastMergeAt",              ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   no  },
   { "epgd",     "lastScrRefUpdate",         ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   no  },
   { "epgd",     "lastEpisodeRun",           ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   no  },
   { "epgd",     "lastEpisodeFullRun",       ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   no  },

   { "epgd",     "maxEventTime",             ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   yes },
   { "epgd",     "minEventTime",             ptTime,    "0",                        "^[0-9]{1,20}$",        yes,   yes },

   { "epgd",     "md5",                      ptAscii,   "",                         "^.{0,150}$",           yes,   no  },

#endif

   // --------------------------------------------
   // epg2vdr / scraper2vdr

   { "uuid",     "lastEventsUpdateAt",       ptNum,     NULL,                       "^[0-9]{1,20}$",        yes,   yes },

   // --------------------------------------------
   // webif

   { "webif",    "needLogin",                ptBool,    "0",                        "^[01]$",               no,    yes },

   // --------------------------------------------
   // user

   { "@",        "chFormat",                 ptAscii,   "",                         "^.{0,150}$",           no,    yes },
   { "@",        "defaultVDRuuid",           ptAscii,   "",                         "^.{0,150}$",           no,    yes },
   { "@",        "startPage",                ptAscii,   "menu_magazine",            "^.{0,150}$",           no,    yes },
   { "@",        "timerDefaultVDRuuid",      ptAscii,   "",                         "^.{0,150}$",           no,    yes },
   { "@",        "quickTimes",               ptAscii,   "Jetzt=@Now~Nächste=@Next~PrimeTime=20:20~EarlyNight=!22:20~MidNight=!00:00~LateNight=!02:00~Tipp=@TagesTipps~Action=@Action",
                                                                                    "^(~?[^=]+=!?(([0-1]?[0-9]|2[0-4]):[0-5]?[0-9]|@Now|@Next|@[A-Za-z0-9]*))*$",
                                                                                                            no,    yes },
   { "@",        "startWithSched",           ptBool,    "0",                        "^[01]$",               no,    yes },
   { "@",        "searchAdv",                ptBool,    "1",                        "^[01]$",               no,    yes },
   { "@",        "namingModeSerie",          ptNum,     "1",                        "^[0-5]$",              no,    yes },
   { "@",        "namingModeSearchSerie",    ptNum,     "1",                        "^[0-5]$",              no,    yes },
   { "@",        "namingModeMovie",          ptNum,     "1",                        "^[0-5]$",              no,    yes },
   { "@",        "namingModeSearchMovie",    ptNum,     "1",                        "^[0-5]$",              no,    yes },
   { "@",        "pickerFirstDay",           ptNum,     "1",                        "^[0-6]$",              no,    yes },
   { "@",        "sendTCC",                  ptBool,    "1",                        "^[01]$",               no,    yes },
   { "@",        "constabelLoginPath",       ptAscii,   "",                         "^.{0,150}$",           no,    yes },
   { "@",        "mailReceiver",             ptAscii,   "",                         "^.{0,150}$",           no,    yes },
   { "@",        "osdTimerNotify",           ptBool,    "0",                        "^[01]$",               no,    yes },

   { 0, 0, 0, 0, 0, 0, 0 }
};

//***************************************************************************
// Get Definition
//***************************************************************************

cParameters::Parameter* cParameters::getDefinition(const char* owner, const char* name)
{
   if (isEmpty(owner) || isEmpty(name))
      return 0;

   // some specials for dynamic owner or name

   if (owner[0] == '@')
      owner = "@";

   if (strcmp(owner, "epgd") == 0 && strstr(name, ".md5"))
      name = "md5";

   // lookup

   for (int i = 0; parameters[i].name != 0; i++)
   {
      if (strcmp(parameters[i].owner, owner) == 0 && strcasecmp(parameters[i].name, name) == 0)
         return &parameters[i];
   }

   tell(0, "Warning: Requested parameter '%s/%s' not known, ignoring", owner, name);

   return 0;
}

//***************************************************************************
// Object / Init / Exit
//***************************************************************************

cParameters::cParameters()
{
}

int cParameters::initDb(cDbConnection* connection)
{
   int status = success;

   parametersDb = new cDbTable(connection, "parameters");
   if (parametersDb->open() != success) return fail;

   // ----------
   // select * from parameters
   //   where owner = ?

   selectParameters = new cDbStatement(parametersDb);

   selectParameters->build("select ");
   selectParameters->bindAllOut();
   selectParameters->build(" from %s", parametersDb->TableName());

   status += selectParameters->prepare();

   return status;
}

int cParameters::exitDb()
{
   delete parametersDb;      parametersDb = 0;
   delete selectParameters;  selectParameters = 0;

   return done;
}

//***************************************************************************
// Get String Parameter
//***************************************************************************

int cParameters::getParameter(const char* owner, const char* name, char* value)
{
   int found;
   Parameter* definition = getDefinition(owner, name);

   if (value)
      *value = 0;

   if (!definition)
      return no;

   if (strcasecmp(owner, "uuid") == 0)
      owner = Epg2VdrConfig.uuid;

   parametersDb->clear();
   parametersDb->setValue("OWNER", owner);
   parametersDb->setValue("NAME", name);

   if ((found = parametersDb->find()))
   {
      if (value)
         sprintf(value, "%s", parametersDb->getStrValue("Value"));
   }
   else if (value && definition->def)
   {
      sprintf(value, "%s", definition->def);
      found = yes;
      setParameter(owner, name, value);
   }

   parametersDb->reset();

   return found;
}

//***************************************************************************
// Get Integer Parameter
//***************************************************************************

int cParameters::getParameter(const char* owner, const char* name, long int& value)
{
   char txt[100]; *txt = 0;
   int found;

   found = getParameter(owner, name, txt);

   if (!isEmpty(txt))
      value = atol(txt);
   else
      value = 0;

   return found;
}

//***************************************************************************
// Set String Parameter
//***************************************************************************

int cParameters::setParameter(const char* owner, const char* name, const char* value)
{
   Parameter* definition = getDefinition(owner, name);

   if (!definition)
      return fail;

   if (!value)
      return fail;

   if (strcasecmp(owner, "uuid") == 0)
      owner = Epg2VdrConfig.uuid;

   tell(2, "Storing '%s' for '%s' with value '%s'", name, owner, value);

   // validate parameter

   if (definition->regexp)
   {
      if (rep(value, definition->regexp) != success)
      {
         tell(0, "Ignoring '%s' for parameter '%s/%s' don't match expression '%s'",
              value, owner, name, definition->regexp);

         return fail;
      }
   }

   parametersDb->clear();
   parametersDb->setValue("OWNER", owner);
   parametersDb->setValue("NAME", name);
   parametersDb->setValue("VALUE", value);

   return parametersDb->store();
}

int cParameters::setParameter(const char* owner, const char* name, long int value)
{
   char txt[16];

   snprintf(txt, sizeof(txt), "%ld", value);

   return setParameter(owner, name, txt);
}
