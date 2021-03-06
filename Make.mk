#
#   Empire - A multi-player, client/server Internet based war game.
#   Copyright (C) 1986-2017, Dave Pare, Jeff Bailey, Thomas Ruschak,
#                 Ken Stevens, Steve McClure, Markus Armbruster
#
#   Empire is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   ---
#
#   See files README, COPYING and CREDITS in the root of the source
#   tree for related information and legal notices.  It is expected
#   that future projects/authors will amend these files as needed.
#
#   ---
#
#   Make.mk: The real Makefile, included by GNUmakefile
#
#   Known contributors to this file:
#      Markus Armbruster, 2005-2017
#

# This makefile was inspired by `Recursive Make Considered Harmful',
# Peter Miller, 1997.
# http://miller.emu.id.au/pmiller/books/rmch/

# Recursively expanded variables are occasionally useful, but can be
# slow and tricky.  Do not use them gratuitously.  If you don't
# understand this, always use `:=' rather than `='.

# Default goal
all:

# Delete target on error.  Every Makefile should have this.
.DELETE_ON_ERROR:

# Source files
ifeq ($(revctrl),git)
src := $(shell cd $(srcdir) && git ls-files | uniq)
version := $(shell cd $(srcdir) && build-aux/git-version-gen /dev/null)
ifeq ($(version),UNKNOWN)
$(warning cannot figure out version number, falling back to git hash)
version := UNKNOWN-$(shell cd $(srcdir) && git-rev-parse --verify --short HEAD || echo "UNKNOWN")
endif
else
include $(srcdir)/sources.mk
version := $(shell cat $(srcdir)/.tarball-version || echo "UNKNOWN")
endif
ifeq ($(version),UNKNOWN)
$(error cannot figure out version number)
endif
dirs := $(sort $(dir $(src)))
csrc := $(filter %.c, $(src))
tsrc := $(filter %.t, $(src))
man6 := $(filter man/%.6, $(src))
builtins := $(filter src/lib/global/%.config, $(src))

# Info subjects
include $(srcdir)/info/subjects.mk

# Abbreviations
topics := $(patsubst %.t,%,$(notdir $(tsrc)))
info := $(topics) $(subjects) all TOP
scripts := $(srcdir)/src/scripts
depcomp := $(SHELL) $(srcdir)/depcomp
tarball := $(SHELL) -e $(scripts)/tarball
econfig := $(sysconfdir)/empire/econfig
schedule := $(sysconfdir)/empire/schedule
gamedir := $(localstatedir)/empire
edatadir := $(datadir)/empire
builtindir := $(edatadir)/builtin
einfodir := $(edatadir)/info.nr
ehtmldir := $(edatadir)/info.html
client/w32 := arpa/inet.h netdb.h netinet/in.h sys/time.h sys/socket.h	\
sys/uio.h unistd.h w32io.c w32sockets.c w32types.h

# Abbreviate make output
# Run make with a V=1 parameter for full output.
ifneq ($(origin V),command line)
V:=
endif
# $(call quiet-command COMMAND,ABBREV) runs COMMAND, but prints only ABBREV.
# Recursively expanded so that variables in COMMAND and ABBREV work.
ifneq ($V$(findstring s,$(MAKEFLAGS)),)
quiet-command = $1
else
quiet-command = @echo $2 && $1
endif

# How to substitute Autoconf output variables
# Recursively expanded so that $@ and $< work.
subst.in = sed \
	-e 's?@configure_input\@?$(notdir $@): Generated from $(notdir $<) by make.?g' \
	-e 's?@builtindir\@?$(builtindir)?g'	\
	-e 's?@econfig\@?$(econfig)?g'		\
	-e 's?@einfodir\@?$(einfodir)?g'	\
	-e 's?@gamedir\@?$(gamedir)?g'		\
	-e 's/@EMPIREHOST\@/$(EMPIREHOST)/g'	\
	-e 's/@EMPIREPORT\@/$(EMPIREPORT)/g'

# Generated files
# See `Cleanliness' below
# Generated makefiles, distributed by dist-source from $(srcdir):
mk := sources.mk
# Generated by Autoconf, not distributed:
ac := config.h config.log config.status info.html info.nr lib stamp-h
ac += $(basename $(filter %.in, $(src)))
ac += $(srcdir)/autom4te.cache $(srcdir)/src/client/autom4te.cache
# distributed by dist-source from $(srcdir):
acdist := aclocal.m4 config.h.in configure stamp-h.in
# Object files:
obj := $(csrc:.c=.o) $(filter %.o, $(ac:.c=.o))
# Dependencies:
deps := $(obj:.o=.d)
# Library archives:
libs := $(addprefix lib/, libcommon.a libgen.a libglobal.a)
# Programs:
util := $(addprefix src/util/, $(addsuffix $(EXEEXT), empdump empsched fairland files pconfig))
client := src/client/empire$(EXEEXT)
server := src/server/emp_server$(EXEEXT)
# Info subjects:
tsubj := $(addprefix info/, $(addsuffix .t, $(subjects)))
# Formatted info:
info.nr := $(addprefix info.nr/, $(info))
info.html := $(addprefix info.html/, $(addsuffix .html, $(info)))
info.all := $(info.nr) $(info.html) info.ps info/stamp-subj
# Tests
# sandbox

# Conditionally generated files:
empth_obj := src/lib/empthread/io.o
empth_lib :=
ifeq ($(empthread),LWP)
empth_obj += src/lib/empthread/lwp.o src/lib/empthread/posix.o
empth_lib += lib/liblwp.a
endif
ifeq ($(empthread),POSIX)
empth_obj += src/lib/empthread/pthread.o src/lib/empthread/posix.o
endif
ifeq ($(empthread),Windows)
empth_obj += src/lib/empthread/ntthread.o
endif

ifeq ($(empthread),Windows)	# really: W32, regardless of thread package
libs += lib/libw32.a
$(client): lib/libw32.a
endif

# Cleanliness
# Each generated file should be in one of the following sets.
# Removed by clean:
clean := $(obj) $(deps) $(libs) $(util) $(client) $(server) $(tsubj)	\
info/toc info/TOP.t $(info.all) $(empth_obj) $(empth_lib) sandbox
# Removed by distclean:
distclean := $(ac)
ifeq ($(revctrl),git)
distclean += $(addprefix $(srcdir)/, $(mk))
endif
# Distributed by dist-source from $(srcdir):
src_distgen := $(acdist) $(mk)

# Compiler flags
CPPFLAGS += -I$(srcdir)/include -I.
ifeq ($(empthread),Windows)	# really: W32, regardless of thread package
CPPFLAGS += -I$(srcdir)/src/lib/w32
endif
$(client): LDLIBS := $(LIBS_client)
$(server): LDLIBS := $(LIBS_server)


### Advertized goals

.PHONY: all
all: $(util) $(client) $(server) info

.PHONY: info html
info: $(info.nr)
html: $(info.html)

.PHONY: clean
clean:
	$(call quiet-command,rm -rf $(clean),CLEAN)

.PHONY: distclean
distclean: clean
	$(call quiet-command,rm -rf $(distclean),DISTCLEAN)

.PHONY: install
install: all installdirs
	$(INSTALL_PROGRAM) $(util) $(server) $(sbindir)
	$(INSTALL_PROGRAM) $(client) $(bindir)
	$(INSTALL) -m 444 $(addprefix $(srcdir)/, $(builtins)) $(builtindir)
	rm -f $(einfodir)/*
	$(INSTALL_DATA) $(info.nr) $(einfodir)
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(man6)) $(mandir)/man6
	sed -e '1,/^$$/d' -e 's/^/# /g' <$(srcdir)/doc/schedule >$(schedule).dist
	echo >>$(schedule).dist
	echo 'every 10 minutes' >>$(schedule).dist
	[ -e $(schedule) ] || mv $(schedule).dist $(schedule)
	if [ -e $(econfig) ]; then					\
	    echo "Attempting to update your econfig";			\
	    if src/util/pconfig $(econfig) >$(econfig).dist; then	\
	        if cmp -s $(econfig) $(econfig).dist; then		\
		    echo "$(econfig) unchanged";			\
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
	rm -f $(ehtmldir)/*
	$(INSTALL_DATA) $(info.html) $(ehtmldir)

.PHONY: uninstall
uninstall:
	rm -f $(addprefix $(sbindir)/, $(notdir $(util) $(server)))
	rm -f $(addprefix $(bindir)/, $(notdir $(client)))
	rm -rf $(builtindir) $(einfodir) $(ehtmldir)
	rmdir $(edatadir)
	rm -f $(addprefix $(mandir)/man6/, $(notdir $(man6)))
	@echo "$(dir $(econfig)) and $(gamedir) not removed, you may wish to remove it manually."

.PHONY: dist
dist: dist-source dist-client dist-info

.PHONY: check check-accept _check
check check-accept: _check
check:        export EMPIRE_CHECK_ACCEPT :=
check-accept: export EMPIRE_CHECK_ACCEPT := y
_check: all
	@echo "Warning: test suite is immature and needs work." >&2
	$(srcdir)/tests/files-test $(srcdir)
	$(srcdir)/tests/fairland-test $(srcdir)
	$(srcdir)/tests/info-test $(srcdir)
ifeq ($(empthread),LWP)
	$(srcdir)/tests/smoke-test $(srcdir)
	$(srcdir)/tests/actofgod-test $(srcdir)
	$(srcdir)/tests/build-test $(srcdir)
	$(srcdir)/tests/navi-march-test $(srcdir)
	$(srcdir)/tests/fire-test $(srcdir)
	$(srcdir)/tests/torpedo-test $(srcdir)
	$(srcdir)/tests/bridgefall-test $(srcdir)
	$(srcdir)/tests/retreat-test $(srcdir)
	$(srcdir)/tests/update-test $(srcdir)
	$(srcdir)/tests/version-test $(srcdir)
else
	@echo "$(srcdir)/tests/smoke-test SKIPPED"
	@echo "$(srcdir)/tests/actofgod-test SKIPPED"
	@echo "$(srcdir)/tests/build-test SKIPPED"
	@echo "$(srcdir)/tests/navi-march-test SKIPPED"
	@echo "$(srcdir)/tests/fire-test SKIPPED"
	@echo "$(srcdir)/tests/torpedo-test SKIPPED"
	@echo "$(srcdir)/tests/bridgefall-test SKIPPED"
	@echo "$(srcdir)/tests/retreat-test SKIPPED"
	@echo "$(srcdir)/tests/update-test SKIPPED"
	@echo "$(srcdir)/tests/version-test SKIPPED"
endif
	$(srcdir)/tests/empdump-test $(srcdir)


### Implicit rules

# Compile with dependencies as side effect, i.e. create %.d in
# addition to %.o.
ifeq ($(how_to_dep),fast)
%.o: %.c
	$(call quiet-command,$(COMPILE.c) -MT $@ -MMD -MP -MF $(@:.o=.d) \
	$(OUTPUT_OPTION) $< || { rm -f $(@:.o=.d) $@; false; },CC $@)
# Why the rm?  If gcc's preprocessor chokes, it leaves an empty
# dependency file behind, and doesn't touch the object file.  If an
# old object file exists, and is newer than the .c file, make will
# then consider the object file up-to-date.
endif
ifeq ($(how_to_dep),depcomp)
%.o: %.c
	$(call quiet-command,source='$<' object='$@' depfile='$(@:.o=.d)' \
	$(CCDEPMODE) $(depcomp) $(COMPILE.c) $(OUTPUT_OPTION) $<,CC $@)
endif
# Cancel the rule to compile %.c straight to %, it interferes with
# automatic dependency generation
%: %.c

%$(EXEEXT): %.o
	$(call quiet-command,$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@,LINK $@)


info.nr/%: info/%.t
	$(call quiet-command,$(NROFF) $(filter %.MAC, $^) $< | $(AWK) -f $(filter %/Blank.awk, $^) >$@ && test -s $@,NROFF $@)
# Pipes in make are a pain.  The "test -s" catches obvious errors.

info.html/%.html: info/%.t
	$(call quiet-command,perl $(srcdir)/info/emp2html.pl $(info) <$< >$@,GEN $@)


### Explicit rules

# Compilation

$(server): $(filter src/server/% src/lib/commands/% src/lib/player/% src/lib/subs/% src/lib/update/%, $(obj)) $(empth_obj) $(empth_lib) $(libs)
	$(call quiet-command,$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@,LINK $@)

$(client): $(filter src/client/%, $(obj)) src/lib/global/version.o src/lib/gen/fnameat.o
	$(call quiet-command,$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@,LINK $@)

$(util): $(libs)

lib/libcommon.a: $(filter src/lib/common/%, $(obj))
lib/libgen.a: $(filter src/lib/gen/%, $(obj))
lib/libglobal.a: $(filter src/lib/global/%, $(obj))
lib/liblwp.a: $(filter src/lib/lwp/%, $(obj))
lib/libw32.a: $(filter src/lib/w32/%, $(obj))

$(libs) $(empth_lib):
	$(call quiet-command,$(AR) rc $@ $?,AR $@)
	$(RANLIB) $@

src/lib/global/version.o: CPPFLAGS += -DVERSION='"$(version)"'
src/lib/global/version.o: $(src)

ifneq ($(revctrl),git)
$(srcdir)/.tarball-version: $(src)
	v=`sed -e 's/-dirty$$//' <$@`; echo "$$v-dirty" >$@
# Force Make to start over after updating .tarball-version, so that
# $(version) gets the new value
$(srcdir)/.dirty-stamp: $(srcdir)/.tarball-version
	>$@
include $(srcdir)/.dirty-stamp
endif

# Info formatting

# mksubj.pl reads $(tsrc) and writes $(tsubj).  A naive rule
#     $(tsubj): $(tsrc)
#           COMMAND
# runs COMMAND once for each target.  That's because multiple targets
# in an explicit rule is just a shorthand for one rule per target,
# each with the same prerequisites and commands.  Use a stamp file.
$(tsubj) info/toc: info/stamp-subj ;
info/stamp-subj: info/mksubj.pl $(tsrc)
	$(call quiet-command,perl $(srcdir)/info/mksubj.pl $(subjects) $(filter %.t, $^),GEN '$(tsubj) info/toc')
	>$@

info/TOP.t: info/mktop.pl info/subjects.mk
	$(call quiet-command,perl $(srcdir)/info/mktop.pl $@ $(subjects),GEN $@)

info.nr/all: $(filter-out info.nr/all, $(info.nr))
	>$@
	(cd info.nr && LC_ALL=C ls -C $(info)) >>$@

info.html/all.html: info.nr/all info/ls2html.pl
	expand $< | perl $(srcdir)/info/ls2html.pl >$@

$(info.nr): info/CRT.MAC info/INFO.MAC info/Blank.awk

$(info.html): info/emp2html.pl

info.ps: info/TROFF.MAC info/INFO.MAC info/TOP.t $(tsubj) $(tsrc)
	groff $^ >$@

# Distributing

.PHONY: dist-source
dist-source: $(addprefix $(srcdir)/, $(src_distgen))
	$(tarball) -x $(srcdir)/src/scripts/gen-tarball-version $(TARNAME) $(version) -C $(srcdir) $(src_distgen) $(src)

ifeq ($(revctrl),git)
.PHONY: $(srcdir)/sources.mk
$(srcdir)/sources.mk:
	$(call quiet-command,echo "src := $(src)" >$@,GEN $@)
endif

.PHONY: dist-client
dist-client:
	$(tarball) -x $(srcdir)/src/scripts/gen-client-configure	\
	$(TARNAME)-client $(version)					\
	-C $(srcdir)/src/client						\
		$(notdir $(filter src/client/%, $(src)))		\
	-C $(srcdir)/include fnameat.h proto.h version.h		\
	-C $(srcdir)/src/lib/global version.c				\
	-C $(srcdir)/src/lib/gen fnameat.c				\
	-C $(srcdir)/src/lib $(addprefix w32/, $(client/w32))		\
	-C $(srcdir)/man empire.6					\
	-C $(srcdir)/build-aux install-sh				\
	-C $(srcdir) COPYING INSTALL					\
		m4/ax_lib_socket_nsl.m4 m4/my_lib_readline.m4		\
		m4/my_terminfo.m4 m4/my_windows_api.m4

.PHONY: dist-info
dist-info: info html
	$(tarball) $(TARNAME)-info-text $(version) -C info.nr $(info)
	$(tarball) $(TARNAME)-info-html $(version) -C info.html $(addsuffix .html, $(info))

# Dependencies

ifneq ($(deps),)
-include $(deps)
endif


# Automatic remake of configuration
# See (autoconf)Automatic Remaking.
# This requires sufficiently recent versions of autoconf and automake

$(srcdir)/configure: configure.ac aclocal.m4
	cd $(srcdir) && autoconf

# autoheader might not change config.h.in, so touch a stamp file.
$(srcdir)/config.h.in: stamp-h.in ;
$(srcdir)/stamp-h.in: configure.ac aclocal.m4
	cd $(srcdir) && autoheader
	touch $@

$(srcdir)/aclocal.m4: $(filter m4/%.m4, $(src))
	cd $(srcdir) && aclocal -I m4

# config.status might not change config.h; use the stamp file.
config.h: stamp-h ;
stamp-h: config.h.in config.status
	./config.status config.h stamp-h

GNUmakefile: GNUmakefile.in config.status
	./config.status $@

config.status: configure
	./config.status --recheck

src/lib/global/path.c src/client/ipglob.c: %: %.in GNUmakefile Make.mk
	$(call quiet-command,$(subst.in) <$< >$@,GEN $@)
