# $Header$

#
# Makefile for glish clients.

TAR_FILE = glish-clients.tar

all:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS all

install:
	@echo Installing glish clients ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS install

build: all

install-all:
	@echo Installing glish clients ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	if test "$(PFX)" = "1"; then				\
		cd $$ARCH; $(MAKE) $$FLGS prefix-install;	\
	else	cd $$ARCH; $(MAKE) $$FLGS install; fi;

glishd:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS glishd

echo_client:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS echo_client

echo_mclient:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS echo_mclient

shell_client:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS shell_client

tell_glishd:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS tell_glishd

test_client:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS test_client

timer:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS timer

multiplexer:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS multiplexer

tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm -f tar-files

tar-list:
	@ls Makefile.in *.cc

rcsinfo:
	@echo glish/clients
	@rlog -L -S RCS/*,v

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

