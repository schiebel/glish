# $Header$

#
# Makefile for editline C libraries.


TAR_FILE = editline.tar

all:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libs

install:
	@echo Installing editline ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS install

build: all

install-all:
	@echo Installing editline ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	if test "$(PFX)" = "1"; then				\
		cd $$ARCH; $(MAKE) $$FLGS prefix-install;	\
	else	cd $$ARCH; $(MAKE) $$FLGS install; fi;

libedit.a: $(OBJS)
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../config/architecture`;			\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS="$(MFLAGS) ARCH=$(ARCH)"; fi;		\
	cd $$ARCH; $(MAKE) $$FLGS libedit.a


tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm -f tar-files

tar-list:
	@echo editline >&2
	@ls README Makefile.in *.c editline.3 include/*.h

rcsinfo:
	@echo editline
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

