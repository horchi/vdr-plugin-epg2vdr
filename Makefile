#
# Makefile for a Video Disk Recorder plugin
#

PLUGIN    = epg2vdr
HLIB      = -L./lib -lhorchi
HISTFILE  = "HISTORY.h"

include Make.config

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'define _VERSION ' $(HISTFILE) | awk '{ print $$3 }' | sed -e 's/[";]//g')

LASTHIST    = $(shell grep '^20[0-3][0-9]' $(HISTFILE) | head -1)
LASTCOMMENT = $(subst |,\n,$(shell sed -n '/$(LASTHIST)/,/^ *$$/p' $(HISTFILE) | tr '\n' '|'))
LASTTAG     = $(shell git describe --tags --abbrev=0)
BRANCH      = $(shell git rev-parse --abbrev-ref HEAD)
GIT_REV = $(shell git describe --always 2>/dev/null)

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." pkg-config --variable=$(1) vdr))

LIBDIR   = $(call PKGCFG,libdir)
LOCDIR   = $(call PKGCFG,locdir)
PLGCFG   = $(call PKGCFG,plgcfg)
CONFDEST = $(call PKGCFG,configdir)/plugins/$(PLUGIN)

# autodetect related plugins

ifeq (exists, $(shell test -e ../graphtftng && echo exists))
   WITH_GTFT = 1
endif
ifeq (exists, $(shell test -e ../vdr-plugin-graphtftng && echo exists))
   WITH_GTFT = 1
endif
ifeq (exists, $(shell test -e ../pin && echo exists))
   WITH_PIN = 1
endif
ifeq (exists, $(shell test -e ../vdr-plugin-pin && echo exists))
   WITH_PIN = 1
endif

#

TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   += $(call PKGCFG,cflags)
export CXXFLAGS += $(call PKGCFG,cxxflags)

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

OBJS = $(PLUGIN).o \
       service.o update.o plgconfig.o parameters.o \
       timer.o recording.o recinfofile.o \
       status.o ttools.o svdrpclient.o \
       menu.o menusched.o menutimers.o menudone.o menusearchtimer.o

LIBS += $(HLIB)
LIBS += -lrt -larchive -lcrypto
LIBS += $(shell pkg-config --libs uuid)
LIBS += $(shell pkg-config --libs tinyxml2)
LIBS += $(shell $(SQLCFG) --libs_r)
ifdef USEPYTHON
  CFLAGS += $(shell $(PYTHON)-config --includes)
  LIBS += $(PYTHON_LIBS)
endif
LIBS += $(shell pkg-config --libs jansson)

EPG2VDR_DATA_DIR = "/var/cache/vdr"

ifdef WITH_GTFT
	DEFINES += -DWITH_GTFT
endif

ifdef WITH_PIN
	DEFINES += -DWITH_PIN
endif

ifdef WITH_AUX_PATCH
	DEFINES += -DWITH_AUX_PATCH
endif

ifdef EPG2VDR_DATA_DIR
   DEFINES += -DEPG2VDR_DATA_DIR='$(EPG2VDR_DATA_DIR)'
endif

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES += $(shell $(SQLCFG) --include)

DEFINES += -DEPG2VDR -DLOG_PREFIX='"$(PLUGIN): "' $(USES)

ifdef GIT_REV
   DEFINES += -DGIT_REV='"$(GIT_REV)"'
endif

### The main target:

all: $(SOFILE) i18n

hlib:
	(cd lib && $(MAKE) -s lib)

### Implicit rules:

%.o: %.c
	$(doCompile) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CC) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<vdr@jwendel.de>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): hlib $(OBJS)
	$(CC) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install-lib: $(SOFILE)
	install -D -m644 $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install: install-lib install-i18n install-config

install-config:
	if ! test -d $(DESTDIR)$(CONFDEST); then \
	   mkdir -p $(DESTDIR)$(CONFDEST); \
	   chmod a+rx $(DESTDIR)$(CONFDEST); \
	fi
	install --mode=644 -D ./configs/epg.dat $(DESTDIR)$(CONFDEST)
ifdef VDR_USER
	if test -n $(VDR_USER); then \
		chown $(VDR_USER) $(DESTDIR)$(CONFDEST); \
	   chown $(VDR_USER) $(DESTDIR)$(CONFDEST)/epg.dat; \
	fi
endif

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.core* *~ ./configs/*~
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot $(PODIR)/*~
	@-rm -f $(PACKAGE).tgz
	(cd lib && $(MAKE) clean)

cppchk:
	cppcheck --language=c++ --template="{file}:{line}:{severity}:{message}" --quiet --force *.c *.h

# ------------------------------------------------------
# Git / Versioning / Tagging
# ------------------------------------------------------

vcheck:
	git fetch
	if test "$(LASTTAG)" = "$(VERSION)"; then \
		echo "Warning: tag/version '$(VERSION)' already exists, update HISTORY first. Aborting!"; \
		exit 1; \
	fi

push: vcheck
	echo "tagging git with $(VERSION)"
	git push origin master
	git push github master
	git tag $(VERSION)
	git push origin master --tags
	git push github master --tags

commit: vcheck
	git commit -m "$(LASTCOMMENT)" -a

git: commit push

showv:
	@echo "Git ($(BRANCH)):\\n  Version: $(LASTTAG) (tag)"
	@echo "Local:"
	@echo "  Version: $(VERSION)"
	@echo "  Change:"
	@echo -n "   $(LASTCOMMENT)"

update:
	git pull
	@make clean install
	restart vdr
