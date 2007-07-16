# 
#   Empire - A multi-player, client/server Internet based war game.
#   Copyright (C) 1986-2007, Dave Pare, Jeff Bailey, Thomas Ruschak,
#                            Ken Stevens, Steve McClure
# 
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
# 
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
# 
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 
#   ---
# 
#   See files README, COPYING and CREDITS in the root of the source
#   tree for related information and legal notices.  It is expected
#   that future projects/authors will amend these files as needed.
# 
#   ---
# 
#   Make.mk: 
#  
#   Known contributors to this file:
#      Markus Armbruster, 2005
# 

# This makefile was inspired by `Recursive Make Considered Harmful',
# Peter Miller, 1997.
# http://www.pcug.org.au/~millerp/rmch/recu-make-cons-harm.html

# Recursively expanded variables are occasionally useful, but can be
# slow and tricky.  Do not use them gratuitously.  If you don't
# understand this, always use `:=' rather than `='.

# Default goal
all:

# Delete target on error.  Every Makefile should have this.
.DELETE_ON_ERROR:

# Source files
-include sources.mk
dirs := $(sort $(dir $(src)))
csrc := $(filter %.c, $(src))
tsrc := $(filter %.t, $(src))
man6 := $(filter man/%.6, $(src))
builtins := $(filter src/lib/global/%.config, $(src))

# Info topics and subjects
-include subjects.mk

# Abbreviations
topics := $(patsubst %.t,%,$(notdir $(tsrc)))
info := $(topics) $(subjects) all TOP
subjects.html := $(addprefix info.html/, $(addsuffix .html, $(subjects)))
topics.html := $(addprefix info.html/, $(addsuffix .html, $(topics)))
scripts := $(srcdir)/src/scripts
depcomp := $(SHELL) $(srcdir)/depcomp
tarball := $(SHELL) $(scripts)/tarball
econfig := $(sysconfdir)/empire/econfig
schedule := $(sysconfdir)/empire/schedule
gamedir := $(localstatedir)/empire
builtindir := $(datadir)/empire/builtin
einfodir := $(datadir)/empire/info.nr
ehtmldir := $(datadir)/empire/info.html

# How to substitute Autoconf output variables
# Recursively expanded so that $@ and $< work.
subst.in = sed \
	-e 's?@configure_input\@?$(notdir $@).  Generated from $(notdir $<) by GNUmakefile.?g' \
	-e 's?@builtindir\@?$(builtindir)?g'	\
	-e 's?@econfig\@?$(econfig)?g'		\
	-e 's?@einfodir\@?$(einfodir)?g'	\
	-e 's?@gamedir\@?$(gamedir)?g'		\
	-e 's/@EMPIREHOST\@/$(EMPIREHOST)/g'	\
	-e 's/@EMPIREPORT\@/$(EMPIREPORT)/g'

# Generated files
# See `Cleanliness' below
# sources.mk subjects.mk
# Generated by Autoconf, not distributed:
ac := config.h config.log config.status info.html info.nr lib stamp-h
ac += $(basename $(filter %.in, $(src)))
ac += $(srcdir)/autom4te.cache $(srcdir)/src/client/autom4te.cache
# distributed by dist-source from $(srcdir):
acdist := aclocal.m4 config.h.in configure stamp-h.in
# distributed by dist-client from $(srcdir):
acdistcli := $(addprefix src/client/, aclocal.m4 config.h.in configure)
# Object files:
obj := $(csrc:.c=.o) $(filter %.o, $(ac:.c=.o))
# TODO AIX needs lwpInit.o lwpRestore.o lwpSave.o unless UCONTEXT
# Dependencies:
deps := $(obj:.o=.d)
# Library archives:
libs := $(addprefix lib/, libcommon.a libgen.a libglobal.a)
# Programs:
util := $(addprefix src/util/, $(addsuffix $(EXEEXT), empsched fairland files pconfig))
client := src/client/empire$(EXEEXT)
server := src/server/emp_server$(EXEEXT)
# Info subjects:
tsubj := $(addprefix info/, $(addsuffix .t, $(subjects)))
ttop := info/TOP.t
# Formatted info:
info.nr := $(addprefix info.nr/, $(info))
info.html := $(addprefix info.html/, $(addsuffix .html, $(info)))

# Conditionally generated files:
ifeq ($(empthread),LWP)
empth_obj := src/lib/empthread/lwp.o src/lib/empthread/posix.o
empth_lib := lib/liblwp.a
endif
ifeq ($(empthread),POSIX)
empth_obj := src/lib/empthread/pthread.o src/lib/empthread/posix.o
empth_lib :=
endif
ifeq ($(empthread),Windows)
empth_obj := src/lib/empthread/ntthread.o
empth_lib :=
endif

# Cleanliness
# Each generated file should be in one of the following sets.
# Removed by clean:
clean := $(obj) $(deps) $(libs) $(util) $(client) $(server) $(tsubj)	\
$(ttop) $(info.nr) $(info.html) $(empth_obj) $(empth_lib)
# Removed by distclean:
distclean := $(ac) subjects.mk
# Distributed by dist-source from $(srcdir)
src_distgen := $(acdist)
# Distributed by dist-source from .
bld_distgen := sources.mk
# Distributed by dist-client from $(srcdir)/src/client
cli_distgen := $(acdistcli)

# Compiler flags
CPPFLAGS += -I$(srcdir)/include -I.
ifeq ($(have_gcc),yes)
CFLAGS += -fno-builtin-carg	# conflicts with our carg()
CFLAGS += -fno-common
CFLAGS += -Wall -W -Wno-unused -Wpointer-arith -Wstrict-prototypes	\
-Wmissing-prototypes -Wnested-externs -Wredundant-decls
endif
LDLIBS += -lm
$(client): LDLIBS += $(termlibs)

### Advertized goals

.PHONY: all
all: $(util) $(client) $(server) info

.PHONY: info html
info: $(info.nr)
html: $(info.html)

.PHONY: clean
clean:
	rm -f $(clean)

.PHONY: distclean
distclean: clean
	rm -rf $(distclean)

.PHONY: install
install: all installdirs
	$(INSTALL_PROGRAM) $(util) $(server) $(sbindir)
	$(INSTALL_PROGRAM) $(client) $(bindir)
	$(INSTALL) -m 444 $(addprefix $(srcdir)/, $(builtins)) $(builtindir)
	$(INSTALL_DATA) $(info.nr) $(einfodir)
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(man6)) $(mandir)/man6
	sed -e '1,/^$$/d' -e 's/^/# /g' <$(srcdir)/doc/schedule >$(schedule).dist
	echo >>$(schedule).dist
	echo 'every 10 minutes' >>$(schedule).dist
	[ -e $(schedule) ] || mv $(schedule).dist $(schedule)
	if [ -e $(econfig) ]; then					\
	    if src/util/pconfig $(econfig) >$(econfig).dist; then	\
	        if cmp -s $(econfig) $(econfig).dist; then		\
		    rm $(econfig).dist;					\
		fi;							\
	    else							\
		echo "Your $(econfig) doesn't work";			\
		src/util/pconfig >$(econfig).dist;			\
	    fi;								\
	    if [ -e $(econfig).dist ]; then				\
		echo "Check out $(econfig).dist";			\
	    fi;								\
	else								\
	    src/util/pconfig >$(econfig);				\
	fi

.PHONY: installdirs
installdirs:
	mkdir -p $(sbindir) $(bindir) $(builtindir) $(einfodir) $(mandir)/man6 $(dir $(econfig)) $(gamedir)

.PHONY: install-html
install-html: html
	mkdir -p $(ehtmldir)
	$(INSTALL_DATA) $(info.html) $(ehtmldir)

.PHONY: uninstall
uninstall:
	rm -f $(addprefix $(sbindir)/, $(notdir $(util) $(server)))
	rm -f $(addprefix $(bindir)/, $(notdir $(client)))
	rm -rf $(builtindir) $(einfodir)
	rm -f $(addprefix $(mandir)/man6/, $(notdir $(man6)))
	@[ -e $(gamedir) ] && echo "$(gamedir) not removed, you may wish to remove it manually."
	@[ -e $(econfig) ] && echo "$(econfig) not removed, you may wish to remove it manually."

.PHONY: dist
dist: dist-source dist-client dist-info


### Implicit rules

# Compile with dependencies as side effect, i.e. create %.d in
# addition to %.o.
ifeq ($(how_to_dep),fast)
%.o: %.c
	$(COMPILE.c) -MT $@ -MMD -MP $(OUTPUT_OPTION) $<
endif
ifeq ($(how_to_dep),depcomp)
%.o: %.c
	source='$<' object='$@' depfile='$(@:.o=.d)' $(CCDEPMODE) $(depcomp) \
	$(COMPILE.c) $(OUTPUT_OPTION) $<
endif
# Cancel the rule to compile %.c straight to %, it interferes with
# automatic dependency generation
%: %.c

# Work around MinGW Make's broken built-in link rule:
%$(EXEEXT): %.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@


info.nr/%: info/%.t
	$(NROFF) $(filter %.MAC, $^) $< | $(AWK) -f $(filter %/Blank.awk, $^) >$@
# Pipes in make are a pain.  Catch obvious errors:
	@test -s $@

info.html/%.html: info/%.t
	perl $(filter %.pl, $^) $< >$@


### Explicit rules

# Compilation

$(server): $(filter src/server/% src/lib/as/% src/lib/commands/% src/lib/player/% src/lib/subs/% src/lib/update/%, $(obj)) $(empth_obj) $(libs) $(empth_lib)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(client): $(filter src/client/%, $(obj))
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(util): $(libs)

lib/libcommon.a: $(filter src/lib/common/%, $(obj))
lib/libgen.a: $(filter src/lib/gen/%, $(obj))
lib/libglobal.a: $(filter src/lib/global/%, $(obj))
lib/liblwp.a: $(filter src/lib/lwp/%, $(obj))

$(libs) $(empth_lib):
	$(AR) rc $@ $?
	$(RANLIB) $@

# Info formatting

subjects.mk: $(tsrc) info/findsubj.pl sources.mk
	perl $(srcdir)/info/findsubj.pl
# If sources.mk is out of date, $(tsrc) is.  If it contains files that
# went away, make can't remake subjects.mk.  Tell it to ignore such
# missing files:
$(tsrc):

$(tsubj): info/mksubj.pl
	perl $(srcdir)/info/mksubj.pl $@ $(filter %.t, $^)

$(ttop): $(tsubj)
	perl $(srcdir)/info/mktop.pl $@ $(filter %.t, $^)

info.nr/all: $(filter-out info.nr/all, $(info.nr))
	(cd info.nr && LC_ALL=C ls -C $(info)) >$@
# FIXME shouldn't use ls

info.html/all.html: $(filter-out info.html/all.html, $(info.html)) info/ls2html.pl
	(cd info.html && LC_ALL=C ls -C $(notdir $(info.html))) | expand | perl $(filter %.pl, $^) >$@
# FIXME shouldn't use ls

$(info.nr): info/CRT.MAC info/INFO.MAC info/Blank.awk

$(subjects.html) info.html/TOP.html: info/subj2html.pl
$(topics.html): info/emp2html.pl

info.ps: info/TROFF.MAC info/INFO.MAC $(ttop) $(tsubj) $(tsrc)
	groff $^ >$@

# List of source files

ifeq ($(cvs_controlled),yes)
# Find files and directories under CVS control
sources.mk: $(scripts)/cvsfiles.awk $(addprefix $(srcdir)/, $(addsuffix CVS/Entries, $(dirs)))
	echo 'src := ' `cd $(srcdir) && $(AWK) -f src/scripts/cvsfiles.awk | LC_ALL=C sort` >$@
else
ifneq ($(srcdir),.)
sources.mk: $(srcdir)/sources.mk
	cp -f $^ $@
endif
endif

# Distributing

.PHONY: dist-source
dist-source: $(src_distgen) $(bld_distgen)
	$(tarball) $(TARNAME)-$(VERSION) $(bld_distgen) -C $(srcdir) $(src_distgen) $(src)

.PHONY: dist-client
dist-client: $(cli_distgen)
	$(tarball) $(TARNAME)-client-$(VERSION)				\
	-C $(srcdir)/src/client						\
		$(notdir $(filter src/client/%, $(src))	$(cli_distgen))	\
	-C $(srcdir)/include proto.h					\
	-C $(srcdir)/man empire.6					\
	-C $(srcdir) COPYING INSTALL install-sh

.PHONY: dist-info
dist-info: info html
	$(tarball) $(TARNAME)-info-text-$(VERSION) -C info.nr $(info)
	$(tarball) $(TARNAME)-info-html-$(VERSION) -C info.html $(addsuffix .html, $(info))

ifneq ($(deps),)
-include $(deps)
endif


# Automatic remake of configuration
# See (autoconf)Automatic Remaking.
# This requires sufficiently recent versions of autoconf and automake

$(srcdir)/configure: configure.ac aclocal.m4
	cd $(srcdir) && autoconf

# autoheader might not change config.h.in, so touch a stamp file.
$(srcdir)/config.h.in: stamp-h.in
$(srcdir)/stamp-h.in: configure.ac aclocal.m4
	cd $(srcdir) && autoheader
	touch $@

$(srcdir)/aclocal.m4: $(filter m4/%.m4, $(src))
	cd $(srcdir) && aclocal -I m4

# config.status might not change config.h; use the stamp file.
config.h: stamp-h
stamp-h: config.h.in config.status
	./config.status config.h stamp-h

GNUmakefile: GNUmakefile.in config.status
	./config.status $@

config.status: configure
	./config.status --recheck

src/lib/global/path.c src/client/ipglob.c: %: %.in GNUmakefile Make.mk
	$(subst.in) <$< >$@


# Make files for standalone client distribution

$(srcdir)/src/client/configure: src/client/configure.ac src/client/aclocal.m4
	cd $(dir $@) && autoconf

$(srcdir)/src/client/config.h.in: src/client/configure.ac src/client/aclocal.m4
	cd $(dir $@) && autoheader
	touch $@

$(srcdir)/src/client/aclocal.m4: m4/lib_socket_nsl.m4
	cp -f $< $@
