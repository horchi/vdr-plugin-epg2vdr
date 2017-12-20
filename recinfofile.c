/*
 * recinfofile.c: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "update.h"

//***************************************************************************
// Class Event Details
//***************************************************************************

const char* cEventDetails::fields[] =
{
   "ACTOR",
   "AUDIO",
   "CATEGORY",
   "COUNTRY",
   "DIRECTOR",
   "FLAGS",
   "GENRE",
   "MUSIC",
   "PRODUCER",
   "SCREENPLAY",
   "SHORTREVIEW",
   "TIPP",
   "TOPIC",
   "YEAR",
   "RATING",
   "NUMRATING",
   "MODERATOR",
   "OTHER",
   "GUEST",
   "CAMERA",

   "SCRSERIESID",
   "SCRSERIESEPISODE",
   "SCRMOVIEID",

   // just in recordinglist, not in events or useevents row

   "CHANNELNAME",
   "SCRINFOMOVIEID",
   "SCRINFOSERIESID",
   "SCRINFOEPISODEID",

   0
};

//***************************************************************************
// Set Value
//***************************************************************************

void cEventDetails::setValue(const char* name, const char* value)
{
   std::map<std::string,std::string>::iterator it;

   it = values.find(name);

   if (it == values.end() || it->first != value)
   {
      changes++;
      values[name] = value;
   }
}

void cEventDetails::setValue(const char* name, int value)
{
   setValue(name, num2Str(value).c_str());
}

//***************************************************************************
// Update By Row
//***************************************************************************

int cEventDetails::updateByRow(cDbRow* row)
{
   std::map<std::string,std::string>::iterator it;

   for (int i = 0; fields[i]; i++)
   {
      if (!row->getTableDef()->getField(fields[i], /*silent*/ yes))
         continue;  // skip silent!

      cDbValue* value = row->getValue(fields[i]);

      if (!value)
      {
         tell(2, "Warning: Field '%s' not found", fields[i]);  // only for debug
         continue;
      }

      if (!value->isNull())
      {
         std::string v;

         if (value->getField()->isString())
            v = row->getStrValue(fields[i]);
         else if (value->getField()->isInt())
            v = num2Str(row->getIntValue(fields[i]));
         else
         {
            tell(0, "Info: Field '%s' unhandled for info.epg2vdr", fields[i]);
            continue;
         }

         it = values.find(fields[i]);

         if (it == values.end() || it->second != v)
         {
            changes++;
            values[fields[i]] = v;
         }
      }
   }

   return success;
}

//***************************************************************************
// Row To Xml
//***************************************************************************

int cEventDetails::row2Xml(cDbRow* row, cXml* xml)
{
   for (int i = 0; fields[i]; i++)
   {
      cDbValue* value = row->getValue(fields[i]);

      if (!value || value->isEmpty())
         continue;

      if (value->getField()->hasFormat(cDBS::ffAscii) || value->getField()->hasFormat(cDBS::ffText) || value->getField()->hasFormat(cDBS::ffMText))
         xml->appendElement(fields[i], value->getStrValue());
      else
         xml->appendElement(fields[i], value->getIntValue());
   }

   return success;
}

//***************************************************************************
// Update To Row
//***************************************************************************

int cEventDetails::updateToRow(cDbRow* row)
{
   std::map<std::string, std::string>::iterator it;

   for (it = values.begin(); it != values.end(); it++)
   {
      if (!it->first.length())
         continue;

      cDbValue* value = row->getValue(it->first.c_str());

      if (!value)
      {
         tell(0, "Warning: Field '%s' not found", it->first.c_str());
         continue;
      }

      if (value->getField()->isString())
         value->setValue(it->second.c_str());
      else if (value->getField()->isInt())
         value->setValue(atoi(it->second.c_str()));
      else
      {
         tell(0, "Info: Field '%s' unhandled for info.epg2vdr", it->first.c_str());
         continue;
      }
   }

   return success;
}

//***************************************************************************
// Store To Fs
//***************************************************************************

int cEventDetails::storeToFs(const char* path)
{
   FILE* f;
   char* fileName = 0;
   std::map<std::string,std::string>::iterator it;

   asprintf(&fileName, "%s/info.epg2vdr", path);

   if (!(f = fopen(fileName, "w")))
   {
      tell(0, "Error opening file '%s' failed, %s", fileName, strerror(errno));
      free(fileName);

      return fail;
   }

   tell(0, "Storing event details to '%s'", fileName);

   // store fields

   for (it = values.begin(); it != values.end(); it++)
      fprintf(f, "%s: %s\n", it->first.c_str(), it->second.c_str());

   free(fileName);
   fclose(f);

   return success;
}

//***************************************************************************
// Load From Fs
//***************************************************************************

int cEventDetails::loadFromFs(const char* path)
{
   FILE* f;
   char* fileName = 0;
   std::map<std::string,std::string>::iterator it;

   values.clear();
   changes = 0;

   asprintf(&fileName, "%s/info.epg2vdr", path);

   if (!fileExists(fileName))
   {
      free(fileName);
      return done;
   }

   if (!(f = fopen(fileName, "r")))
   {
      tell(0, "Error opening file '%s' failed, %s", fileName, strerror(errno));
      free(fileName);

      return fail;
   }

   tell(3, "Loading event details from '%s'", fileName);

   // load fields

   char* p;
   char* s;
   cReadLine readLine;

   while ((s = readLine.Read(f)))
   {
      if (!(p = strchr(s, ':')))
      {
         tell(0, " ");
         continue;
      }

      *(p++) = 0;
      p = skipspace(rTrim(p));

      if (!isEmpty(p))
         values[s] = p;
   }

   free(fileName);
   fclose(f);

   return success;
}
