# $Header$

#
# Makefile for npd C libraries.


TAR_FILE = npd.tar

all:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libs

install:
	@echo Installing npd ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS install

build: all

install-all:
	@echo Installing npd ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	if test "$(PFX)" = "1"; then				\
		cd $$ARCH; $(MAKE) $$FLGS prefix-install;	\
	else	cd $$ARCH; $(MAKE) $$FLGS install; fi;

libnpd.a:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libnpd.a

genkey:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS genkey

tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm -f tar-files

tar-list:
	@echo npd >&2
	@ls Makefile.in *.c include/Npd/*.h

rcsinfo:
	@echo sds
	@rlog -L RCS/*,v | sed -n -e 's/Working file://p' -e 's/locked by//p'
	@echo sds/include/Sds
	@cd include/Sds;rlog -L RCS/*,v | sed -n -e 's/Working file://p' -e 's/locked by//p'

clean:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS clean

clean-all: clean

distclean:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS distclean

distclean-all: distclean

