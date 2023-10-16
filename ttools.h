/*
 * ttools.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#pragma once

#include <vdr/timers.h>

#include "service.h"

//***************************************************************************
// Timer Tools
//***************************************************************************

int contentOfTag(const char* tag, const char* xml, char* buf, int size);
int contentOfTag(const cTimer* timer, const char* tag, char* buf, int size);
int contentOfTag(const cTimer* timer, const char* tag, int& value);
int getTimerIdOf(const cTimer* timer);
int getTimerIdOf(const char* aux);
void removeTag(char* xml, const char* tag);
int insertTag(char* xml, const char* parent, const char* tag, int value);
int insertTag(char* xml, const char* parent, const char* tag, const char* value);
int setTagTo(cTimer* timer, const char* tag, int value);
int setTagTo(cTimer* timer, const char* tag, const char* value);
int setTimerId(cTimer* timer, int tid);
cTimer* getTimerById(cTimers* timers, int timerid);
cTimer* getTimerByEvent(cTimers* timers, const cEvent* event);

cDbRow* newTimerRowFromEvent(const cEvent* event);
cDbRow* newRowFromTimer(const cTimer* t);
int updateRowByTimer(cDbRow* timerDb, const cTimer* t);

cEpgTimer* newTimerObjectFromRow(cDbRow* timerRow, cDbRow* vdrRow);
int updateTimerObjectFromRow(cTimer* timer, cDbRow* timerRow, const cEvent* event);
