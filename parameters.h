/*
 * parameters.h
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#include "lib/db.h"

//***************************************************************************
// Parameters
//***************************************************************************

class cParameters
{
   public:

      enum Type
      {
         ptNum,
         ptTime,     // unix time
         ptBool,
         ptAscii
      };

      struct Parameter
      {
         const char* owner;
         const char* name;
         int type;
         const char* def;
         const char* regexp;
         int readonly;
         int visible;
      };

      cParameters();

      int initDb(cDbConnection* connection);
      int exitDb();

      int getParameter(const char* owner, const char* name, char* value = 0);
      int getParameter(const char* owner, const char* name, long int& value);
      int setParameter(const char* owner, const char* name, const char* value);
      int setParameter(const char* owner, const char* name, long int value);
      
   protected:

      cDbTable* parametersDb;
      cDbStatement* selectParameters;

      static Parameter parameters[];
      static Parameter* getDefinition(const char* owner, const char* name);
};

#endif // __PARAMETERS_H
