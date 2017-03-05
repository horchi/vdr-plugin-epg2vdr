 /*
 * svdrpclient.c
 *
 * See the README file for copyright information
 *
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "lib/common.h"

#include "svdrpclient.h"

#ifndef VDR_PLUGIN

ssize_t safe_read(int filedes, void *buffer, size_t size)
{
  for (;;)
  {
     ssize_t p = read(filedes, buffer, size);

     if (p < 0 && errno == EINTR)
     {
        tell(0, "EINTR while reading from file handle %d - retrying", filedes);
        continue;
     }
     return p;
  }
}

ssize_t safe_write(int filedes, const void *buffer, size_t size)
{
  ssize_t p = 0;
  ssize_t written = size;
  const unsigned char *ptr = (const unsigned char *)buffer;

  while (size > 0)
  {
     p = write(filedes, ptr, size);

     if (p < 0)
     {
        if (errno == EINTR)
        {
           tell(0, "EINTR while writing to file handle %d - retrying", filedes);
           continue;
        }

        break;
     }
     ptr  += p;
     size -= p;
  }

  return p < 0 ? p : written;
}

// --- cListObject -----------------------------------------------------------

cListObject::cListObject(void)
{
  prev = next = NULL;
}

cListObject::~cListObject()
{
}

void cListObject::Append(cListObject *Object)
{
  next = Object;
  Object->prev = this;
}

void cListObject::Insert(cListObject *Object)
{
  prev = Object;
  Object->next = this;
}

void cListObject::Unlink(void)
{
  if (next)
     next->prev = prev;
  if (prev)
     prev->next = next;
  next = prev = NULL;
}

int cListObject::Index(void) const
{
  cListObject *p = prev;
  int i = 0;

  while (p) {
        i++;
        p = p->prev;
        }
  return i;
}

// --- cListBase -------------------------------------------------------------

cListBase::cListBase(void)
{
  objects = lastObject = NULL;
  count = 0;
}

cListBase::~cListBase()
{
  Clear();
}

void cListBase::Add(cListObject *Object, cListObject *After)
{
  if (After && After != lastObject) {
     After->Next()->Insert(Object);
     After->Append(Object);
     }
  else {
     if (lastObject)
        lastObject->Append(Object);
     else
        objects = Object;
     lastObject = Object;
     }
  count++;
}

void cListBase::Ins(cListObject *Object, cListObject *Before)
{
  if (Before && Before != objects) {
     Before->Prev()->Append(Object);
     Before->Insert(Object);
     }
  else {
     if (objects)
        objects->Insert(Object);
     else
        lastObject = Object;
     objects = Object;
     }
  count++;
}

void cListBase::Del(cListObject *Object, bool DeleteObject)
{
  if (Object == objects)
     objects = Object->Next();
  if (Object == lastObject)
     lastObject = Object->Prev();
  Object->Unlink();
  if (DeleteObject)
     delete Object;
  count--;
}

void cListBase::Move(int From, int To)
{
  Move(Get(From), Get(To));
}

void cListBase::Move(cListObject *From, cListObject *To)
{
  if (From && To && From != To) {
     if (From->Index() < To->Index())
        To = To->Next();
     if (From == objects)
        objects = From->Next();
     if (From == lastObject)
        lastObject = From->Prev();
     From->Unlink();
     if (To) {
        if (To->Prev())
           To->Prev()->Append(From);
        From->Append(To);
        }
     else {
        lastObject->Append(From);
        lastObject = From;
        }
     if (!From->Prev())
        objects = From;
     }
}

void cListBase::Clear(void)
{
  while (objects) {
        cListObject *object = objects->Next();
        delete objects;
        objects = object;
        }
  objects = lastObject = NULL;
  count = 0;
}

cListObject *cListBase::Get(int Index) const
{
  if (Index < 0)
     return NULL;
  cListObject *object = objects;
  while (object && Index-- > 0)
        object = object->Next();
  return object;
}

static int CompareListObjects(const void *a, const void *b)
{
  const cListObject *la = *(const cListObject **)a;
  const cListObject *lb = *(const cListObject **)b;
  return la->Compare(*lb);
}

void cListBase::Sort(void)
{
  int n = Count();
  cListObject** a;
  cListObject* object = objects;
  int i = 0;

  a = (cListObject**)malloc(n * sizeof(cListObject*));

  while (object && i < n)
  {
     a[0] = 0;
     a[i++] = object;
     object = object->Next();
  }

  qsort(a, n, sizeof(cListObject*), CompareListObjects);
  objects = lastObject = NULL;

  for (i = 0; i < n; i++)
  {
     a[i]->Unlink();
     count--;
     Add(a[i]);
  }

  free(a);
}

// --- cTimeMs ---------------------------------------------------------------

cTimeMs::cTimeMs(int Ms)
{
  if (Ms >= 0)
     Set(Ms);
  else
     begin = 0;
}

uint64_t cTimeMs::Now(void)
{
#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
#define MIN_RESOLUTION 5 // ms
  static bool initialized = false;
  static bool monotonic = false;
  struct timespec tp;
  if (!initialized) {
     // check if monotonic timer is available and provides enough accurate resolution:
     if (clock_getres(CLOCK_MONOTONIC, &tp) == 0) {
        // long Resolution = tp.tv_nsec;
        // require a minimum resolution:
        if (tp.tv_sec == 0 && tp.tv_nsec <= MIN_RESOLUTION * 1000000) {
           if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0) {
              // tell(0, "cTimeMs: using monotonic clock (resolution is %ld ns)", Resolution);
              monotonic = true;
              }
           else
              tell(0, "cTimeMs: clock_gettime(CLOCK_MONOTONIC) failed");
           }
        else
           tell(0, "cTimeMs: not using monotonic clock - resolution is too bad (%ld s %ld ns)", tp.tv_sec, tp.tv_nsec);
        }
     else
        tell(0, "cTimeMs: clock_getres(CLOCK_MONOTONIC) failed");
     initialized = true;
     }
  if (monotonic) {
     if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
        return (uint64_t(tp.tv_sec)) * 1000 + tp.tv_nsec / 1000000;
     tell(0, "cTimeMs: clock_gettime(CLOCK_MONOTONIC) failed");
     monotonic = false;
     // fall back to gettimeofday()
     }
#else
#  warning Posix monotonic clock not available
#endif
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return (uint64_t(t.tv_sec)) * 1000 + t.tv_usec / 1000;
  return 0;
}

void cTimeMs::Set(int Ms)
{
  begin = Now() + Ms;
}

bool cTimeMs::TimedOut(void)
{
  return Now() >= begin;
}

uint64_t cTimeMs::Elapsed(void)
{
  return Now() - begin;
}


// --- cFile -----------------------------------------------------------------

bool cFile::files[FD_SETSIZE] = { false };
int cFile::maxFiles = 0;

cFile::cFile(void)
{
  f = -1;
}

cFile::~cFile()
{
  Close();
}

bool cFile::Open(const char *FileName, int Flags, mode_t Mode)
{
  if (!IsOpen())
     return Open(open(FileName, Flags, Mode));
  tell(0, "ERROR: attempt to re-open %s", FileName);
  return false;
}

bool cFile::Open(int FileDes)
{
  if (FileDes >= 0) {
     if (!IsOpen()) {
        f = FileDes;
        if (f >= 0) {
           if (f < FD_SETSIZE) {
              if (f >= maxFiles)
                 maxFiles = f + 1;
              if (!files[f])
                 files[f] = true;
              else
                 tell(0, "ERROR: file descriptor %d already in files[]", f);
              return true;
              }
           else
              tell(0, "ERROR: file descriptor %d is larger than FD_SETSIZE (%d)", f, FD_SETSIZE);
           }
        }
     else
        tell(0, "ERROR: attempt to re-open file descriptor %d", FileDes);
     }
  return false;
}

void cFile::Close(void)
{
  if (f >= 0) {
     close(f);
     files[f] = false;
     f = -1;
     }
}

bool cFile::Ready(bool Wait)
{
  return f >= 0 && AnyFileReady(f, Wait ? 1000 : 0);
}

bool cFile::AnyFileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  FD_ZERO(&set);
  for (int i = 0; i < maxFiles; i++) {
      if (files[i])
         FD_SET(i, &set);
      }
  if (0 <= FileDes && FileDes < FD_SETSIZE && !files[FileDes])
     FD_SET(FileDes, &set); // in case we come in with an arbitrary descriptor
  if (TimeoutMs == 0)
     TimeoutMs = 10; // load gets too heavy with 0
  struct timeval timeout;
  timeout.tv_sec  = TimeoutMs / 1000;
  timeout.tv_usec = (TimeoutMs % 1000) * 1000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && (FileDes < 0 || FD_ISSET(FileDes, &set));
}

bool cFile::FileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs >= 0) {
     if (TimeoutMs < 100)
        TimeoutMs = 100;
     timeout.tv_sec  = TimeoutMs / 1000;
     timeout.tv_usec = (TimeoutMs % 1000) * 1000;
     }
  return select(FD_SETSIZE, &set, NULL, NULL, (TimeoutMs >= 0) ? &timeout : NULL) > 0 && FD_ISSET(FileDes, &set);
}

bool cFile::FileReadyForWriting(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs < 100)
     TimeoutMs = 100;
  timeout.tv_sec  = 0;
  timeout.tv_usec = TimeoutMs * 1000;
  return select(FD_SETSIZE, NULL, &set, NULL, &timeout) > 0 && FD_ISSET(FileDes, &set);
}

#endif // VDR_PLUGIN

//***************************************************************************
// SVDRP Client
//***************************************************************************

const char* eoc = "\n";   // End Of Command

cSvdrpClient::cSvdrpClient(const char* aIp, int aPort)
{
   port = aPort;
   ip = aIp ? ::strdup(aIp) : 0;
   bufSize = BUFSIZ;
   buffer = (char*)malloc(bufSize);
}

cSvdrpClient::~cSvdrpClient(void)
{
   close();

   free(ip);
   free(buffer);
}

//***************************************************************************
// Connect
//***************************************************************************

int cSvdrpClient::connect()
{
   if (!ip)
   {
      tell(0, "SVDRPCL: No server IP specified");
      return -1;
   }

   struct hostent* hostInfo;
   struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(port);
   in_addr_t remoteAddr;

   if (!isdigit(ip[1]))
   {
      // map hostname to ip

      if (hostInfo = ::gethostbyname(ip))
         memcpy((char*)&remoteAddr, hostInfo->h_addr, hostInfo->h_length);

      else if ((remoteAddr = inet_addr(ip)) == INADDR_NONE)
         return -1;

      memcpy(&server_addr.sin_addr, &remoteAddr, sizeof(struct in_addr));
   }
   else if (!::inet_aton(ip, &server_addr.sin_addr))
   {
      tell(0, "SVDRPCL: Invalid server IP '%s'", ip);
      return -1;
   }

   int sock = ::socket(PF_INET, SOCK_STREAM, 0);

   if (sock < 0)
   {
      tell(0, "SVDRPCL: Error creating socket for connection to %s: %s", ip, strerror(errno));
      return -1;
   }

   // set nonblocking

   int flags = ::fcntl(sock, F_GETFL, 0);

   if (flags < 0 || ::fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
   {
      tell(0, "SVDRPCL: Unable to use nonblocking I/O for %s: %s", ip, strerror(errno));
      return -1;
   }

   if (::connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
   {
      if (errno != EINPROGRESS)
      {
         tell(0, "SVDRPCL: connect to %s:%hu failed: %s", ip, port, strerror(errno));
         return -1;
      }

      int result;
      fd_set fds;
      struct timeval tv;
      cTimeMs starttime;
      int timeout = 10 * 1000;

      do {
         FD_ZERO(&fds);
         FD_SET(sock, &fds);
         tv.tv_usec = (timeout % 1000) * 1000;
         tv.tv_sec = timeout / 1000;
         result = ::select(sock + 1, 0, &fds, 0, &tv);

      } while (result == -1 && errno == EINTR
               && (timeout = 10 * 1000 - starttime.Elapsed()) > 100);

      if (!result)            // timeout
      {
         result = -1;
         errno = ETIMEDOUT;
      }
      else if (result == 1)    // check socket for errors
      {
         int error;
         socklen_t size = sizeof(error);
         result = ::getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &size);
         if (result == 0 && error != 0)
         {
            result = -1;
            errno = error;
         }
      }

      if (result != 0)
      {
         tell(0, "SVDRPCL: Error connecting to %s:%hu: %s", ip, port, strerror(errno));
         ::close(sock);

         return -1;
      }
   }

   return sock;
}

int cSvdrpClient::open()
{
   if (file.IsOpen())
      return 0;

   int fd = connect();

   if (fd < 0)
      return -1;

   if (!file.Open(fd))
   {
      ::close(fd);
      return -1;
   }

   // check for greeting

   cList<cLine> greeting;

   if (receive(&greeting) != 220)
   {
      tell(0, "SVDRPCL: did not receive greeting from %s. Closing...", ip);
      abort();

      return -1;
   }

   const char* msg = 0;

   if (greeting.First() && greeting.First()->Text())
      msg = greeting.First()->Text();

   tell(2, "SVDRPCL: connected to %s:%hu '%s'", ip, port, msg);

   return 0;
}

void cSvdrpClient::close(int sendQuit)
{
   if (!file.IsOpen())
      return;

   if (sendQuit)
   {
      if (send("QUIT", false))
         receive();
   }

   file.Close();
}

void cSvdrpClient::abort(void)
{
   file.Close();
}

int cSvdrpClient::send(const char* cmd, int reconnect)
{
   if (!cmd)
      return false;

   if (reconnect && !file.IsOpen())
      open();

   if (!file.IsOpen())
   {
      tell(0, "SVDRPCL: unable to send command to %s. Socket is closed", ip);
      return false;
   }

   int len = ::strlen(cmd);

   if (safe_write(file, cmd, len) < 0 || safe_write(file, eoc, strlen(eoc)) < 0)
   {
      tell(0, "SVDRPCL: error while writing to %s: %s", ip, strerror(errno));
      abort();

      return false;
   }

   return true;
}

int cSvdrpClient::receive(cList<cLine>* list, int timeoutMs)
{
   // #TODO iconv ?!

   while (readLine(timeoutMs))
   {
      char* tail;
      long int code = ::strtol(buffer, &tail, 10);

      if (tail - buffer == 3
          && code >= 100
          && code <= 999
          && (*tail == ' ' || *tail == '-'))
      {
         const char* s = buffer + 4;

         if (code >= 451 && code <= 699)
            tell(0, "Error: Got (%ld) '%s' form ", code, s);

         if (list)
            list->Add(new cLine(s));

         if (*tail == ' ')
            return code;
      }
      else
      {
         tell(0, "SVDRPCL: Unexpected reply from %s '%s'", ip, buffer);
         close();
         break;
      }
   }

   if (list)
      list->Clear();

   return 0;
}

int cSvdrpClient::readLine(int timeoutMs)
{
   if (!file.IsOpen())
      return false;

   int tail = 0;

   while (cFile::FileReady(file, timeoutMs))
   {
      unsigned char c;
      int r = safe_read(file, &c, 1);

      if (r > 0)
      {
         if (c == *eoc || c == 0x00)
         {
            // line complete, make sure the string is terminated

            buffer[tail] = 0;
            return true;
         }
         else if ((c <= 0x1F || c == 0x7F) && c != 0x09)
         {
            // ignore control characters
         }
         else
         {
            if (tail >= bufSize - 1)
            {
               bufSize += BUFSIZ;
               buffer = srealloc(buffer, bufSize);

               if (!buffer)
               {
                  tell(0, "SVDRPCL: unable to increase buffer size to %d byte", bufSize);
                  close();

                  return false;
               }
            }

            buffer[tail++] = c;
         }
      }
      else
      {
         tell(0, "SVDRPCL: lost connection '%s'", ip);
         buffer[0] = 0;
         abort();

         return false;
      }
   }

   tell(0, "SVDRPCL: timeout waiting server reply '%s'", ip);
   buffer[0] = 0;
   abort();

   return false;
}
