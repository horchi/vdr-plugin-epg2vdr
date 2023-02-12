/*
 * -----------------------------------
 * epg2vdr Plugin - Revision History
 * -----------------------------------
 *
 */

#define _VERSION     "1.2.13"
#define VERSION_DATE "12.02.2023"

#define DB_API 8

#ifdef GIT_REV
#  define VERSION _VERSION "-GIT" GIT_REV
#else
#  define VERSION _VERSION
#endif

/*
 * ------------------------------------

2023-02-12: version 1.2.13 (horchi)
   - change: Using syslog severity (Tkx to kfb77)

2023-01-22: version 1.2.12 (horchi)
   - change: More dynamic detection of python version

2023-01-22: version 1.2.11 (horchi)
 - change: Lib SSL porting

2023-01-22: version 1.2.10 (horchi)
 - change: DBAPI migration

2022-10-22: version 1.2.9 (horchi)
 - bugfix: Fixed crash on initial start without uuid

2022-07-12: version 1.2.8 (horchi)
 - change: Added EPG path of MegaV0lt to fix special chars in EPG

2022-04-29: version 1.2.7 (horchi)
 - change: Moded initialize of EPG handler to Initialize() (reported by Alexander Grothe)

2022-01-12: version 1.2.6 (horchi)
 - change: Further porting to vdr 2.6 (fixed detection of timer stop)

2022-01-04: version 1.2.5 (horchi)
 - change: Makefile change due to more distribution compatibility

2022-01-03: version 1.2.4 (horchi)
   - change: Removed dependency to mysql-compat package

2022-01-02: version 1.2.3 (horchi)
   - change: Removed timer start margin for switch timers

2022-01-01: version 1.2.2 (horchi)
   - change: Undo of 1.2.1 (can't detect the situation for this log message at plugin side)

2022-01-01: version 1.2.1 (horchi)
   - added: Error message for broken EPG handler sequence

2021-12-31: version 1.2.0 (horchi)
   - change: VDR 2.6 porting

2020-09-22: version 1.1.118 (horchi)
   - bugfix: Fixed problem with update of timer table

2020-08-18: version 1.1.117 (horchi)
   - added: compatibility to VDRs peering feature

2020-08-17: version 1.1.116 (horchi, patch by kfb77)
   - change: Improved match of series

2020-08-14: version 1.1.115 (horchi)
   - bugfix: Fixed missing images after restart - again

2020-08-13: version 1.1.114 (horchi)
   - bugfix: Fixed missing images after restart

2020-03-23: version 1.1.113 (horchi)
   - change: Minor change of log message

2020-03-13: version 1.1.112 (horchi)
   - bugfix: Fixed timer deactivation by webif again

2020-02-29: version 1.1.111 (horchi)
   - bugfix: Fixed time calculation

2020-02-24: version 1.1.110 (horchi)
   - bugfix: Fixed timer deactivation by webif

2020-02-17: version 1.1.109 (horchi)
   - bugfix: Fixed conflicting timer start times

2020-02-16: version 1.1.108 (horchi)
   - bugfix: 'Fix of fix' of time handling for timers without event :(

2020-02-14: version 1.1.107 (horchi,seahawk1986)
   - bugfix: Fixed cleanup of image links
   - change: Improved cleanup spead for images and links
   - bugfix: Fixed time handling for timers without event

2019-12-23: version 1.1.106 (horchi)
   - change: Perform timer job also if eppgd busy

2019-12-17: version 1.1.105 (horchi)
   - added: Fixed compile with g++ 10 (provided by marco@vdr-developer.org)

2019-12-15: version 1.1.104 (horchi)
   - added: Added _endtime to table timers

2019-12-02: version 1.1.103 (horchi)
   - bugfix: fixed python patch

2019-11-28: version 1.1.102 (horchi)
   - added: Support of python 3.8 (by Alexander Grothe)

2019-10-30 version 1.1.101 (horchi)
   - change: Added evaluation of PKG_CONFIG_PATH environment variable (thaks to kfb77@vdr-portal)

2019-10-29 version 1.1.100 (horchi)
   - bugfix: Added patch to fix potentially crash at end of recording (thaks to kfb77@vdr-portal)

2019-10-28 version 1.1.99 (horchi)
   - added: Option to keep menu on channel switch open

2018-10-15 version 1.1.98 (horchi)
   - bugfix: Fixed init of dummy separator items for EPG menu

2018-09-26 version 1.1.97 (horchi)
   - change: Improved detection of recording end

2018-04-11 version 1.1.96 (horchi)
   - change: Changed menu item for better catagory choice by skin plugin (by Tomas Saxer)

2018-03-11 version 1.1.95 (horchi)
   - bugfix: Fixed uninitialzed variable

2018-03-11 version 1.1.94 (horchi)
   - change: Porting to dbapi 7 changes of epgd

2018-03-09 version 1.1.93 (horchi)
   - added: Menu for matching 'jobs' (timersdone) of search timer results, with delete option
   - added: some german translations
   - added: optional (configurable) osd notification on timer change

2018-03-07 version 1.1.92 (horchi)
   - added: create of local switch timer even when a 'recording' timer exists

2018-02-26 version 1.1.91 (horchi)
   - change: backward compatibility to vdr 2.2.0 - another step

2018-02-23 version 1.1.90 (horchi)
   - change: improved timer cleanup

2018-02-23 version 1.1.89 (horchi)
   - change: Improved switch timer 'timing'

018-02-22 version 1.1.88 (horchi)
   - added: Optional switch timer notification message

2018-02-20 version 1.1.87 (horchi)
   - change: No forerunnings for switch timers

2018-02-17 version 1.1.86 (horchi)
   - bugfix: Fix for 1.1.85

2018-02-17 version 1.1.85 (horchi)
   - added: Timer type and location to timer detail service

2018-02-13 version 1.1.84 (horchi)
   - added: Timer 'type' to timers menu

2018-02-10 version 1.1.83 (horchi)
   - bugfix: Fixed delete of Switch timer
   - added: Fill recording images table with images of existing recordings

2018-02-09 version 1.1.82 (horchi)
   - added: Switch timer

2018-01-29 version 1.1.81 (horchi)
   - bugfix: Fixed possible crash on recording update without longdescription
   - change: More readable error message on DBAPI mismatch (thx to Lars!)

2018-01-27 version 1.1.80 (horchi)
   - added: Service to check if event has a timer

2018-01-24 version 1.1.79 (horchi)
   - change: minor changes, fixes and code cleanup

2017-12-22 version 1.1.78 (horchi)
   - change: update of recording description handling

2017-12-22 version 1.1.77 (horchi)
   - change: backward compatibility to vdr 2.2.0 - another step

2017-12-21 version 1.1.76 (horchi)
   - change: backward compatibility to vdr 2.2.0 - second try
   - change: g++ 7 porting

2017-12-21 version 1.1.75 (horchi)
   - change: backward compatibility to vdr 2.2.0

2017-12-19 version 1.1.74 (horchi)
   - added: recording detail query to service interface

2017-06-23 version 1.1.73 (horchi)
   - bugfix: Fixed compile with VDR 2.2.0
   - bugfix: Fixed problem with unknown channels

2017-06-22 version 1.1.72 (horchi)
   - bugfix: Fixed crash on end of recording

2017-06-22 version 1.1.71 (horchi)
   - bugfix: Fixed crash on recording

2017-06-22 version 1.1.70 (horchi)
   - change: Fixed possible crash with unknown channels on wrong configuration

2017-06-22 version 1.1.69 (horchi)
   - change: More rework of lock handling

2017-06-11: version 1.1.68 (horchi)
   - change: Added lock macros for easier handling the vdr versions

2017-06-11: version 1.1.67 (horchi)
   - change: Porting to VDR 2.3.7

2017-06-10: version 1.1.66 (horchi)
   - Bugfix: Fixed vdr 2.2.0 compile problem (thx to Alexander Grothe)

2017-06-09: version 1.1.65 (horchi)
   - Bugfix: Fixed another lock sequence

2017-06-09: version 1.1.64 (horchi)
   - Bugfix: Fixed lock sequence

2017-06-08 version 1.1.63 (horchi)
   - change: Improved aux field
   - change: Suppress inactive timers on service interface

2017-06-02 version 1.1.62 (horchi)
   - change: Minor sorting change

2017-05-22 version 1.1.61 (horchi)
   - bugfix: Fixed aux handling

2017-05-07 version 1.1.60 (horchi)
   - bugfix: Fixed possible crash on channel lock

2017-05-05 version 1.1.59 (horchi)
   - bugfix: Fixed crash on missing channels

2017-05-03 version 1.1.58 (horchi)
   - change: Fill timersdone table even for timers created on OSD

2017-05-02 version 1.1.57 (horchi)
   - bugfix: Fixed possible problem with epg for new channels since vdr 2.3.x

2017-03-24 version 1.1.56 (horchi)
   - bugfix: Fixed problem with service interface (frequent db reconnects)

2017-03-24 version 1.1.55 (horchi)
   - change: Fixed default make option

2017-03-23 version 1.1.54 (horchi)
   - added: New event plugin interface

2017-03-22 version 1.1.53 (horchi)
   - change: Removed old patches for vdr < 2.2.0
   - added: Patch to extend cEvent with aux field like cTimer
   - change: Moved user defines from Makefile to Make.config

2017-03-21 version 1.1.52 (horchi)
   - bugfix: crash in EPG menu

2017-03-20: version 1.1.51 (horchi)
   - change: Removed compiler warnings when using clang
   - added:  Added clang++ to Make.config (as optional compiler)
   - change: Fixed APIVERSION check for VDR < 2.2.0 (thx to nobanzai)

2017-03-19: version 1.1.50 (horchi)
   - bugfix: Fixed AMC address lookup

2017-03-19: version 1.1.49 (horchi)
   - bugfix: Fixed possible crash in extended skins interface

2017-03-16: version 1.1.48 (horchi)
   - added: Further improvement of extended skins interface

2017-03-14: version 1.1.47 (horchi)
   - added: Started extended event interface for skins

2017-03-06: version 1.1.46 (horchi)
   - added: Added column for future features to vdrs table

2017-03-06: version 1.1.45 (horchi)
   - change: Fixed order of push/tag in Makefile

2017-03-06: version 1.1.44 (horchi)
   - change: Improved Makefile (Thx to magicamun)

2017-03-01: version 1.1.43 (horchi)
   - bugfix: Fixed crash in meues without database connection

2017-02-27: version 1.1.41 (horchi)
   - bugfix: Fixed lookup of repeating events

2017-02-27: version 1.1.40 (horchi)
   - change: minor changes (logging, ...)

2017-02-24: version 1.1.39 (horchi)
   - change: modified null field handling for timersdonedb

2017-02-14: version 1.1.38 (horchi)
   - bugfix: fixed compile bug

2017-02-14: version 1.1.37 (horchi)
   - bugfix: fixed detection if recording is complete or not

2017-02-14: version 1.1.36 (horchi)
   - added: command menu for schedules (key 0)

2017-02-14: version 1.1.35 (horchi)
   - change: modified timer delete/reject handling

2017-02-10: version 1.1.34 (horchi)
   - bugfix: Fixed possible crash at timer update

2017-02-09: version 1.1.33 (horchi)
   - added: Added epg handler patch for VDR 2.3.2
            -> enhancement of epg handler interface to fix a threading
               problem which can occur on systems with more than one tuner

2017-02-08: version 1.1.32 (horchi)
   - bugfix: first program menu item now also selectable

2017-02-08: version 1.1.31 (horchi)
   - added: Added patch for VDR >= 2.3.1
            Important, without this patch it will not work!

2017-02-06: version 1.1.30 (horchi)
   - bugfix: fix of '1.1.28' next try ;)

2017-02-06: version 1.1.29 (horchi)
   - bugfix: fixed compile issue with gcc 6.2.0

2017-02-06: version 1.1.28 (horchi)
   - bugfix: fixed sql error in epg search menu

2017-02-01: version 1.1.27 (horchi)
   - bugfix: fixed timer naming problem

2017-01-19: version 1.1.26 (horchi)
   - bugfix: Fixed buf with cyclic db reconnect

2017-01-19: version 1.1.25 (horchi)
   - added: support of namingmode 'template' by epgd/epghttpd

2017-01-16: version 1.1.24 (horchi)
   - change: Finished porting to VDR 2.3.2

2017-01-08: version 1.1.23 (horchi)
   - bugfix: fixed schedules lock problem for vdr < 2.3.x

2017-01-08: version 1.1.22 (horchi)
   - change: fixed problem with 2.3.1 / 2.3.2

2017-01-07: version 1.1.21 (horchi)
   - change: now compiles with vdr 2.3.1 / 2.3.2

2017-01-03: version 1.1.20 (horchi)
   - bugfix: fixed reinstate of events in epg handler

2016-11-30: version 1.1.19 (horchi)
   - change: merging of lib

2016-11-30: version 1.1.18 (horchi)
   - added: Delete / Modify of timers on event changes

2016-11-30: version 1.1.17 (horchi)
   - change: improved format strings for 32 bit systems

2016-11-02: version 1.1.16 (horchi)
   - change: improved cleanup handling for recordings

2016-11-02: version 1.1.15 (horchi)
   - added: epg-detail menu toggle even for schedules

2016-11-01: version 1.1.14 (horchi)
   - added: added title for green and yellow button in epg-detail menu

2016-11-01: version 1.1.13 (horchi)
   - added: channel toggle in Event-Info menu (green/yellow)

2016-11-01: version 1.1.12 (horchi)
   - added: started implementaion of schedule toggle in Event-Info menu

2016-10-30: version 1.1.11 (horchi)
   - added: mark epgseasrch timers (set source to 'epgs')

2016-10-20: version 1.1.10 (horchi)
   - added: update of db field timers._starttime

2016-10-18: version 1.1.9 (horchi)
   - bugfix: fixed missing close of socket handle

2016-10-17: version 1.1.8 (horchi)
   - added: search repetition of event

2016-10-16: version 1.1.7 (horchi)
   - change: improved recording search using LV distance < 50%

2016-08-26: version 1.1.6 (horchi)
   - added: support of long eventids for tvsp (merged dev into master)

2016-07-19: version 1.1.5 (rechner)
   - change: increased event id to unsigned int64

2016-07-15: version 1.1.4 (horchi)
   - change: started increase of event id to unsigned int64

2016-07-06: version 1.1.3 (horchi)
   - bugfix: fixed channel switch in schedule menu

2016-07-06: version 1.1.2 (horchi)
   - change: minor change on menu refresh

2016-07-05: version 1.1.1 (horchi)
   - bugfix: fixed refreah of schedules menu on timer create

2016-07-04: version 1.1.0 (horchi)
   - change: merged http branch into master

2016-06-10: version 1.0.49 (horchi)
   - change: fixed unitialized variable (thx to Joerg)

2016-06-01: version 1.0.48 (horchi)
   - change: adjustet some log levels in epg handler (more silent)

2016-06-01: version 1.0.47 (horchi)
   - change: remove 'alter table'
   - change: added new dictionary version

2016-05-25: version 1.0.46 (horchi)
   - change: optimized scrap recording trigger

2016-05-24: version 1.0.45 (horchi)
   - change: fixed sequence of epg load and perform of timer jobs at VDR start

2016-05-23: version 1.0.44 (horchi)
   - bugfix: fixed crash at timer-service on empty epg

2016-05-23: version 1.0.43 (horchi)
   - bugfix: fixed crash on setup-menu close

2016-05-23: version 1.0.42 (horchi)
   - added:  support of 'highlighted' flag for user times

2016-05-20: version 1.0.41 (horchi)
   - added:  new column for textual rating and commentator
   - change: removed unused info column

2016-05-18: version 1.0.40 (horchi)
   - bugfix: Fixed toggle of timer state with [red]
   - bugfix: Fixed change of timer start/end timer

2016-05-17: version 1.0.39 (horchi)
   - added: service to notify timer changes

2016-05-13: version 1.0.38 (horchi)
   - added: optimized timer lookup for schedules menu

2016-05-12: version 1.0.37 (horchi)
   - added: store MAC address to vdrs table

2016-05-11: version 1.0.36 (horchi)
   - added: State query for service interface

2016-05-11: version 1.0.35 (horchi)
   - change: honor 'Share in Web' config for service 'EPG2VDR_TIMER_SERVICE'
   - change: set of NAMINGMODE and AUTOTIMEINSSP in timers table

2016-05-09: version 1.0.34 (horchi)
   - fixed: added sql string-escape for python return

2016-05-04: version 1.0.33 (horchi)
   - bugfix: fixed cursor pos in timermenu after refresh

2016-05-03: version 1.0.32 (horchi)
   - change: column with for searchtimer result menu

2016-05-03: version 1.0.31 (horchi)
   - added: parallel processing für timer service

2016-05-02: version 1.0.30 (horchi)
   - change: finished service interface for timers

2016-05-02: version 1.0.29 (horchi)
   - change: redesign of service interface for timers

2016-04-23: version 1.0.28 (horchi)
   - bugfix: added missing table init

2016-04-23: version 1.0.27 (horchi)
   - change: added totoal count for service interface (ForEachTimer)

2016-04-22: version 1.0.26 (horchi)
   - bugfix: fixed set of scrnew on info.epg2vdr changes

2016-04-22: version 1.0.25 (horchi)
   - added: reconnect to database server on change of connection settigns

2016-04-22: version 1.0.24 (horchi)
   - bugfix: fixed crash on toggle NAS option

2016-04-22: version 1.0.23 (horchi)
   - bugfix: fixed compile without GTFT patch

2016-04-21: version 1.0.22 (horchi)
   - added: channel switch for channels without epg

2016-04-20: version 1.0.21 (horchi)
   - bugfix: set plugin menu category to mcMain

2016-04-20: version 1.0.20 (horchi)
   - change: set plugin menu category to mcMain
   - added:  ForEachTimer to service interface
   - bugfix: fixed active/deactive distribution

2016-04-19: version 1.0.19 (horchi)
   - bugfix: fixed timer menu refresh

2016-04-19: version 1.0.18 (horchi)
   - bugfix: fixed active/deactive handling in timers menu

2016-04-18: version 1.0.17 (horchi)
   - added: added hack to force skindesigner to use 'favorite menu view' for user defined searches

2016-04-18: version 1.0.16 (horchi)
   - added: more german translations (thx to utility)

016-04-18: version 1.0.15 (horchi)
   - bugfix: fixed fix init of trigger flag :o ;)

2016-04-18: version 1.0.14 (horchi)
   - bugfix: fixed init of trigger flag (thx to mini73)

2016-04-18: version 1.0.13 (horchi)
   - added: temporary shortcut to searchtimer dialog in epg menu via key 3

2016-04-18: version 1.0.12 (horchi)
   - added: option to show channels without epg in schedules menu

2016-04-17: version 1.0.11 (horchi)
   - change: removed needless dependency

2016-04-15: version 1.0.10 (horchi)
   - bugfix: fixed column width of search result menu

2016-04-15: version 1.0.9 (horchi)
   - bugfix: fixed column width of search timer menu

2016-04-15: version 1.0.8 (horchi)
   - bugfix: fixed channel switch in details menu

2016-04-15: version 1.0.7 (horchi)
   - added: setup option to x-change kBlue and kOk

2016-04-14: version 1.0.6 (horchi)
   - bugfix: fixed crash on large quicktimes value

2016-04-14: version 1.0.5 (horchi)
   - change: increased parameter size to 500 character

2016-04-14: version 1.0.4 (horchi)
   - added: Added user defined favorite search to epg menu

2016-04-14: version 1.0.3 (horchi)
   - change: Fixed log message typo

2016-04-14: version 1.0.2 (horchi)
   - change: Update of lib-horchi
   - change: Morde debug log messages

2016-04-14: version 1.0.1 (horchi)
   - bugfix: Fixed crash in timers-done menu

2016-04-05: version 1.0.0 (horchi)
   - change: First BETA release
   - change: Added config option 'create timer local'
   - change: Use WEB settings of 'default VDR' at timer create

2016-03-30: version 0.3.49 (horchi)
   - bugfix: fixed default network device,
             using first of device list except 'lo' as default

2016-03-30: version 0.3.48 (horchi)
   - bugfix: fixed buffer error on large aux values

2016-03-26: version 0.3.47 (horchi)
   - bugfix: fixed char compare of db API

2016-03-22: version 0.3.46 (horchi)
   - bugfix: fixed format of epg.dat

2016-03-22: version 0.3.45 (horchi)
   - changed: removed reason from timers table

2016-03-21: version 0.3.44 (horchi)
   - changed: added default value für timer state

2016-03-18: version 0.3.43
  - added: added make update to Makefile

2016-03-17: version 0.3.42
  - change: minor speed improvement for timer menu

2016-03-17: version 0.3.41
  - change: igoring already tooked 'any'VDR' timer in timer menu

2016-03-16: version 0.3.40
  - added: added log message on forced epg reload
  - change: reset 'reason' on successfull retried timers

2016-03-16: version 0.3.39
  - added: force load of events for specific channel on missing event at timer creation

2016-03-15: version 0.3.38
  - bugfix: fixed timer job for not vdr bound timers again

2016-03-15: version 0.3.37
  - bugfix: fixed timer job for not vdr bound timers

2016-03-10: version 0.3.36
  - bugfix: fixed usage of db connection in wrong thread

2016-03-05: version 0.3.35
  - bugfix: fixed segfault on parameter read

2016-03-04: version 0.3.34
  - bugfix: fixed bug with parameter handling
  - added:  trigger update of video space on recording change

2016-03-03: version 0.3.33
  - added:  config option for network device

2016-03-03: version 0.3.32
  - change: Now and Next of epg menu now configured in webif
            (Use @Now and @Next as Time like 'Jetzt=@Now')

2016-02-29: version 0.3.31
  - added: configuration for start page of schedule menu
  - change: internal redesign of schedule menu implementation

2016-02-26: version 0.3.30
  - added: more translations

2016-02-26: version 0.3.29
  - bugfix: fixed compile problem with new translations (thx to Ole)

2016-02-26: version 0.3.28
  - change: added german translations (thx to Ole)

2016-02-26: version 0.3.27
  - change: trigger scaping of changed recordings
  - change: fixed menu display

2016-02-24: version 0.3.26
   - added: added missing files :o

2016-02-24: version 0.3.25
   - added: epg2vdr skin interface for timer

2016-02-18: version 0.3.24
   - added: delete option for dones to search-result menu

2016-02-18: version 0.3.23
   - added: show count of already done timers in search-result menu

2016-02-18: version 0.3.22
   - added: test-search for searchtimer

2016-02-16: version 0.3.21
   - bugfix: fixed potetioal crash on empty user list

2016-02-16: version 0.3.20
   - change: chnaged state for moved/deleted timers

2016-02-16: version 0.3.19
   - bugfix: fixed timer move

2016-02-15: version 0.3.18
   - added: check for recoring running at 'timer move'

2016-02-15: version 0.3.17
   - change: hold menu position in epg menu at iterate over times
   - change: added 'Timer still running' confirmation on timer delete

2016-02-15: version 0.3.16
   - change: minor change of timer on 'move'

2016-02-11: version 0.3.15
   - added: code and log message review

2016-02-10: version 0.3.14
   - added: Reorganice recordingslist table at chage of useCommonRecFolder setting

2016-02-10: version 0.3.13
   - added: Seach of recordings in programm menu with key '2'

2016-02-09: version 0.3.12
   - bugfix: fixed problem timer handling

2016-02-09: version 0.3.11
   - bugfix: fixed problem with timer state handling

2016-02-08: version 0.3.10
   - change: ported new db lib
   - change: improved updates of timersdone table

2016-02-07: version 0.3.9
   - change: added missing trigger for timer update

2016-02-05: version 0.3.8
   - added: delete and activate/deactivate searchtimers

2016-02-05: version 0.3.7
   - added: centralized some code
   - added: implemented delete button for done timers

2016-02-05: version 0.3.6
   - added: searchtimer menu

2016-02-05: version 0.3.5
   - added: take timer requests even without vdruuid
   - change: some code cleanup

2016-02-05: version 0.3.4
   - added: Display OSD message after manual triggered successful reload/update of EPG

2016-02-04: version 0.3.3
   - bugfix: fixed compile issue

2016-02-04: version 0.3.2
   - change: removed obsolete table from epg.dat

2016-02-04: version 0.3.1
   - change: redesign of timerjobs, remove table timerdistribution

2016-02-03: version 0.3.0
   - change: desupport VDR versions before 2.2.0
   - change: code cleanup

2016-02-02: version 0.2.8
   - bugfix: fixed compile error

2016-02-02: version 0.2.7
   - bugfix: fixed display of filename (cutted at : sign)

2016-02-02: version 0.2.6
   - change: minor code cleanup

2016-02-01: version 0.2.5
   - bugfix: fixed crash un undefined channel

2016-02-01: version 0.2.4
   - bugfix: minor fix of config display

2016-02-01: version 0.2.3
   - added: improved display of timer status in case of error

2016-01-30: version 0.2.2
   - added: timer status and action to timer edit menu
   - change: show only future events at repeat query

2016-01-30: version 0.2.1
   - bugfix: minor fix

2016-01-30: version 0.2.0
   - added: Replacement of VDRs Timer and Program menus, now directly support remote timer

2016-01-21: version 0.1.18
   - bugfix: fixed possible crash at shutdown

2016-01-18: version 0.1.17
   - change: initialize epg handler still earlier
   - bugfix: fixed timer delete (via menu)
   - bugfix: increased source field of table timerdistribution (to fit uuid)

2016-01-18: version 0.1.16
   - bugfix: fixed timer move (via menu)

2016-01-18: version 0.1.15
   - bugfix: fixed problem with double epg entries on merge = 0

2016-01-15: version 0.1.14
   - change: updated dictionary

2015-11-02: version 0.1.13
   - change: recording path (now without base path)
   - added:  service to register mysql_lib_init

2015-11-02: version 0.1.13
   - change: recording path (now without base path)
   - added:  service to register mysql_lib_init

2015-06-25: version 0.1.12
   - change: start implementation of a timer menu

2015-02-02: version 0.1.11
   - change: merges lib from epgd

2014-12-28: version 0.1.10
   - bugfix: fixed possible crash at exit
   - change: deleting old timers from timers table

2014-05-13: version 0.1.9
   - bugfix: fixed mutex problem in epg handler

2014-03-22: version 0.1.8
   - bugfix: fixed name lookup for mapdb

2014-01-03: version 0.1.7
   - bugfix: fixed epg handler

2014-01-03: version 0.1.6
   - change: adapted epgdata plugin to modified header
   - change: added fields producer, other and camera
   - change: removed fields origtitle and team

2013-02-05: version 0.1.5
   - change: removed activity check, instead wait up to 20 seconds to give the mail loop time to finish

2013-12-31: version 0.1.4
   - change: increased field guest to 1000

2013-12-04: version 0.1.3
   - bugfix: fixed insert handling with multimerge
   - change: speedup reload with multimerge

2013-11-20: version 0.1.2
   - change: increased shorttext and compshorttext to 300 chars, topic and guest to 500 chars
             therefore you have to alter your tables

2013-10-28: version 0.1.1
   - bugfix: fixed core on missing database

2013-10-23: version 0.1.0
   - change:  first release with epg merge
   - added:   'LoadImages', 'Shutdown On Busy', 'Schedule Boot For Update'
              and 'Prohibit Shutdown On Busy epgd' to plugin setup menu
   - removed: "UpdateTime" Option

2013-09-27: version 0.0.7b
   - change: first alpha version with epg merge
   - added:  optional schedule of wakeup
   - added:  optional "Prohibit Shutdown On Busy 'epgd'"
   - change: improved picture load and link handling

2013-09-23: version 0.0.7a
   - change: started devel branch for epg merge

2013-09-16: version 0.0.6
   - bugfix: fixed install of locale files
   - bugfix: fixed handler bug (thread conflict), reported by 3po
   - change: improved handler speed
   - change: enhanced connection check reported by OleS
   - bugfix: problem with blacklist, reportet by theChief

2013-09-05: version 0.0.5
   - change: pause update only if epgd is busy with events (not while epgd is loading images)

2013-09-04: version 0.0.4
   - added: removed cyclic update og EPG from database
            -> instead auto trigger update after epgd finished download
   - added: get changes by DVB of "vdr:000" channels every 5 minutes
   - added: pause epg handler while epgd is busy
   - added: pause update while epgd is busy

2013-08-31: version 0.0.3
   - change: improved speed of connection check
             (speed of 0.0.1 restored)

2013-08-30: version 0.0.2
   - bugfix: improved database reconnect
   - added:  check DBAPI on startup
   - bugfix: empty shorttext now NULL instead of ""

2013-08-28: version 0.0.1
   - first official release

2012-12-19: version 0.0.1-rc1
   - initiale version

 * ------------------------------------
 */
