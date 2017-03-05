/*
 * svdrpclient.h: SVDRP connection
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __SVDRP_CLIENT_H_
#define __SVDRP_CLIENT_H_

#include <sys/stat.h>
#include <sys/types.h>

#include <stdint.h>   // uint_64_t
#include <string.h>
#include <stdio.h>

#include "lib/common.h"

#ifdef VDR_PLUGIN
#  define __STL_CONFIG_H
#  include <vdr/tools.h>
#else

//***************************************************************************
// 
//***************************************************************************

class cListObject {
private:
  cListObject *prev, *next;
public:
  cListObject(void);
  virtual ~cListObject();
  virtual int Compare(const cListObject &ListObject) const { return 0; }
      ///< Must return 0 if this object is equal to ListObject, a positive value
      ///< if it is "greater", and a negative value if it is "smaller".
  void Append(cListObject *Object);
  void Insert(cListObject *Object);
  void Unlink(void);
  int Index(void) const;
  cListObject *Prev(void) const { return prev; }
  cListObject *Next(void) const { return next; }
};

class cListBase {
protected:
  cListObject *objects, *lastObject;
  cListBase(void);
  int count;
public:
  virtual ~cListBase();
  void Add(cListObject *Object, cListObject *After = NULL);
  void Ins(cListObject *Object, cListObject *Before = NULL);
  void Del(cListObject *Object, bool DeleteObject = true);
  virtual void Move(int From, int To);
  void Move(cListObject *From, cListObject *To);
  virtual void Clear(void);
  cListObject *Get(int Index) const;
  int Count(void) const { return count; }
  void Sort(void);
};

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) const { return (T *)cListBase::Get(Index); }
  T *First(void) const { return (T *)objects; }
  T *Last(void) const { return (T *)lastObject; }
  T *Prev(const T *object) const { return (T *)object->cListObject::Prev(); } // need to call cListObject's members to
  T *Next(const T *object) const { return (T *)object->cListObject::Next(); } // avoid ambiguities in case of a "list of lists"
};

class cFile {
private:
  static bool files[];
  static int maxFiles;
  int f;
public:
  cFile(void);
  ~cFile();
  operator int () { return f; }
  bool Open(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  bool Open(int FileDes);
  void Close();
  bool IsOpen(void) { return f >= 0; }
  bool Ready(bool Wait = true);
  static bool AnyFileReady(int FileDes = -1, int TimeoutMs = 1000);
  static bool FileReady(int FileDes, int TimeoutMs = 1000);
  static bool FileReadyForWriting(int FileDes, int TimeoutMs = 1000);
};

class cTimeMs {
private:
  uint64_t begin;
public:
  cTimeMs(int Ms = 0);
      ///< Creates a timer with ms resolution and an initial timeout of Ms.
      ///< If Ms is negative the timer is not initialized with the current
      ///< time.
  static uint64_t Now(void);
  void Set(int Ms = 0);
  bool TimedOut(void);
  uint64_t Elapsed(void);
  };

#endif // VDR_PLUGIN


//***************************************************************************
// Line
//***************************************************************************

class cLine : public cListObject 
{
   public:

      cLine(const char *s) { line = s ? strdup(s) : 0; };
      virtual ~cLine()     { if (line) free(line); };

      const char* Text()   { return line; }
      int Length()         { return strlen(line); }

   private:

      char* line;
};

//***************************************************************************
// SVDRP Client
//***************************************************************************

class cSvdrpClient 
{
   private:

      char* ip;
      int port;
      cFile file;
      char* buffer;
      int bufSize;

      int connect();
      int readLine(int timeoutMs);

   public:

      cSvdrpClient(const char *aIp, int aPort);
      virtual ~cSvdrpClient();

      int open();
      void close(int sendQuit = yes);
      void abort();

      int send(const char* cmd, int reconnect = true);
      int receive(cList<cLine>* list = 0, int timeoutMs = 20 * 1000);
};

//***************************************************************************
#endif // __SVDRP_CLIENT_H_
