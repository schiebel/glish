# $Header$

# Note: Glish requires flex version 2.4.6 or greater.  Flex is available
# from ftp.ee.lbl.gov.

TAR_FILE = glish.tar

all:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS all

install:
	@echo Installing glish ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS install;			\
	cd clients; $(MAKE) $$FLGS PFX= install-all

install-all:
	@echo Installing glish ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	if test "$(PFX)" = "1"; then				\
		cd $$ARCH; $(MAKE) $$FLGS prefix-install;	\
	else	cd $$ARCH; $(MAKE) $$FLGS install; fi;		\
	cd clients; $(MAKE) $$FLGS PFX=$(PFX) install-all

build: all

glish:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS glish

libglish.a:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libglish.a

libinterp.a:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libinterp.a

tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm tar-files

tar-list:
	@echo glish >&2
	@ls Makefile.in Makefile config.h.in \
		COPYING glish.init mkinit.sh *.c *.cc *.h *.l *.y \
		*.sed doc/*.ps demos/*.g demos/*.cc demos/*.c \
		demos/README* include/Glish/*.h | grep -v '^config.h$$'
	@cd clients; $(MAKE) $(MFLAGS) tar-list | sed 's,^,clients/,'

rcsinfo:
	@echo glish
	@rlog -L -S RCS/*,v
	@echo glish/include/Glish
	@cd include/Glish; rlog -L -S RCS/*,v
	@cd clients; $(MAKE) $(MFLAGS) rcsinfo

clean:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS clean

clean-all: clean
	@cd clients; $(MAKE) $(MFLAGS) clean-all

distclean: clean
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS distclean

distclean-all: distclean
	@cd clients; $(MAKE) $(MFLAGS) distclean-all

