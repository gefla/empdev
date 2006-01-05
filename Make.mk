# 
#   Empire - A multi-player, client/server Internet based war game.
#   Copyright (C) 1986-2006, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
#   See the "LEGAL", "LICENSE", "CREDITS" and "README" files for all the
#   related information and legal notices. It is expected that any future
#   projects/authors will amend these files as needed.
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
man1 := $(filter man/%.1, $(src))
man6 := $(filter man/%.6, $(src))

# Info topics and subjects
-include subjects.mk
topics := $(patsubst %.t,%,$(notdir $(tsrc)))
info := $(topics) $(subjects) all TOP

# Generated files
mk := sources.mk subjects.mk
ac := autom4te.cache config.h config.status config.log stamp-h	\
$(basename $(filter %.in, $(src)))
obj := $(csrc:.c=.o) $(filter %.o, $(ac:.c=.o))
# TODO AIX needs lwpInit.o lwpRestore.o lwpSave.o unless UCONTEXT
deps := $(obj:.o=.d)
libs := $(addprefix lib/, libcommon.a libgen.a libglobal.a)
util := $(addprefix src/util/, fairland files pconfig)
client := src/client/empire
server := src/server/emp_server
progs := $(util) $(client) $(server)
tsubj := $(addprefix info/, $(addsuffix .t, $(subjects)))
ttop := info/TOP.t
info.nr := $(addprefix info.nr/, $(info))
subjects.html := $(addprefix info.html/, $(addsuffix .html, $(subjects)))
topics.html := $(addprefix info.html/, $(addsuffix .html, $(topics)))
info.html := $(addprefix info.html/, $(addsuffix .html, $(info)))

ifeq ($(empthread),LWP)
empth_obj := src/lib/empthread/lwp.o
empth_lib := lib/liblwp.a
endif
ifeq ($(empthread),POSIX)
empth_obj := src/lib/empthread/pthread.o
empth_lib :=
endif
ifeq ($(empthread),Windows)
empth_obj := src/lib/empthread/ntthread.o
empth_lib :=
endif

# Abbreviations
scripts = $(srcdir)/src/scripts
depcomp = $(SHELL) $(srcdir)/depcomp
clean := $(obj) $(deps) $(libs) $(progs) $(empth_lib) $(tsubj)	\
$(info.nr) $(info.html)
distclean := $(ac) info/stamp $(ttop)
econfig := $(sysconfdir)/empire/econfig
edatadir := $(localstatedir)/empire
einfodir := $(datadir)/empire/info.nr
ehtmldir := $(datadir)/empire/info.html
# Recursively expanded so that $@ and $< work.
subst.in = sed \
	-e 's?@configure_input\@?$(notdir $@).  Generated from $(notdir $<) by GNUmakefile.?g' \
	-e 's?@econfig\@?$(econfig)?g' \
	-e 's?@edatadir\@?$(edatadir)?g' \
	-e 's?@einfodir\@?$(einfodir)?g' \
	-e 's/@EMPIREHOST\@/$(EMPIREHOST)/g' \
	-e 's/@EMPIREPORT\@/$(EMPIREPORT)/g'

# Compiler flags
CPPFLAGS += -I$(srcdir)/include -I.
ifeq ($(have_gcc),yes)
CFLAGS += -fno-common
CFLAGS += -Wall -W -Wno-unused -Wpointer-arith -Wstrict-prototypes	\
-Wmissing-prototypes -Wnested-externs -Wredundant-decls
endif
LDLIBS += -lm

### Advertized goals

.PHONY: all
all: $(progs) info

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
	$(INSTALL_DATA) $(info.nr) $(einfodir)
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(man1)) $(mandir)/man1
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(man6)) $(mandir)/man6
	if test -e $(econfig); then					\
	    if src/util/pconfig $(econfig) >$(econfig).new; then	\
	        if cmp -s $(econfig) $(econfig).new; then		\
		    rm $(econfig).new;					\
		else							\
		    echo "Check out $(econfig).new";			\
		fi							\
	    else							\
		echo "Your $(econfig) doesn't work";			\
	    fi								\
	else								\
	    src/util/pconfig >$(econfig);				\
	fi

.PHONY: installdirs
installdirs:
	mkdir -p $(bindir) $(sbindir) $(edatadir) $(einfodir) $(mandir)/man1 $(mandir)/man6 $(dir $(econfig))

.PHONY: install-html
install-html: html | $(ehtmldir)
	$(INSTALL_DATA) $(info.html) $(ehtmldir)

.PHONY: uninstall
uninstall:
	false # FIXME

.PHONY: dist
dist:
	false # FIXME


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

info.nr/%: info/%.t
	$(NROFF) -I $(srcdir)/info $(filter %/CRT.MAC, $^) $< | $(AWK) -f $(filter %/Blank.awk, $^) >$@
# FIXME AT&T nroff doesn't grok -I

info.html/%.html: info/%.t
	perl $(filter %.pl, $^) $< >$@


### Explicit rules

$(server): $(filter src/server/% src/lib/as/% src/lib/commands/% src/lib/player/% src/lib/subs/% src/lib/update/%, $(obj)) $(empth_obj) $(libs) $(empth_lib)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(client): $(filter src/client/%, $(obj)) $(termlibs)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(util): $(libs)

lib/libcommon.a: $(filter src/lib/common/%, $(obj))
lib/libgen.a: $(filter src/lib/gen/%, $(obj))
lib/libglobal.a: $(filter src/lib/global/%, $(obj))
lib/liblwp.a: $(filter src/lib/lwp/%, $(obj))

$(libs) $(empth_lib): | lib
	$(AR) rc $@ $?
	$(RANLIB) $@

subjects.mk: $(tsrc) info/findsubj.pl sources.mk
	perl $(srcdir)/info/findsubj.pl

$(tsubj): info/mksubj.pl
	perl $(srcdir)/info/mksubj.pl $@ $(filter %.t, $^)

$(ttop): $(tsubj)
	perl $(srcdir)/info/mktop.pl $@ $(filter %.t, $^)

info.nr/all: $(filter-out info.nr/all, $(info.nr))
	(cd info.nr && ls -CF) >$@
# FIXME should use $^ and not ls

info.html/all.html: $(filter-out info.html/all.html, $(info.html)) info/ls2html.pl
	(cd info.html && ls -CF *.html) | expand | perl $(filter %.pl, $^) >$@
# FIXME should use $^ and not ls

$(info.nr): info/CRT.MAC info/INFO.MAC info/Blank.awk | info.nr

$(subjects.html): info/subj2html.pl | info.html
$(topics.html): info/emp2html.pl | info.html

info.nr info.html lib:
	mkdir -p $@

ifeq ($(cvs_controlled),yes)
# Find files and directories under CVS control
sources.mk: $(scripts)/cvsfiles.awk $(addprefix $(srcdir)/, $(addsuffix CVS/Entries, $(dirs)))
	echo 'src := ' `cd $(srcdir) && $(AWK) -f src/scripts/cvsfiles.awk` >$@
endif

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
	>$@

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

src/lib/global/path.c src/client/ipglob.c: %: %.in GNUmakefile
	$(subst.in) <$< >$@
