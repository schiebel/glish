## $Id$
##
##
## Makefile for glish clients.
##

TAR_FILE = glish-clients.tar

STATIC_DIR=$$ARCH/static
DYNAMIC_DIR=$$ARCH/dynamic

SHELL = /bin/sh
SH = $(SHELL)

all:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../../../config/architecture`;		\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS=""; fi;					\
	(cd $(DYNAMIC_DIR); $(MAKE) $$FLGS all);		\
	(cd $(STATIC_DIR); $(MAKE) $$FLGS all);			\
	if test -d pgplot; then					\
		(cd pgplot; $(MAKE) $$FLGS all); fi;

install:
	@echo Installing glish clients ...
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../../../config/architecture`;		\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS=""; fi;					\
	(cd $(DYNAMIC_DIR); $(MAKE) $$FLGS install);		\
	(cd $(STATIC_DIR); $(MAKE) $$FLGS install);		\
	if test -d pgplot; then					\
		(cd pgplot; $(MAKE) $$FLGS install); fi;

build: all


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
		ARCH=`../../../config/architecture`;		\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS=""; fi;					\
	(cd $(DYNAMIC_DIR); $(MAKE) $$FLGS clean);		\
	(cd $(STATIC_DIR); $(MAKE) $$FLGS clean);		\
	if test -d pgplot; then					\
		(cd pgplot; $(MAKE) $$FLGS clean); fi;

distclean:
	@if test -z "$(ARCH)"; then 				\
		ARCH=`../../../config/architecture`;		\
		FLGS="$(MFLAGS) ARCH=$$ARCH";			\
	else	FLGS=""; fi;					\
	(cd $(DYNAMIC_DIR); $(MAKE) $$FLGS distclean);		\
	(cd $(STATIC_DIR); $(MAKE) $$FLGS distclean);		\
	if test -d pgplot; then					\
		(cd pgplot; $(MAKE) $$FLGS distclean); fi;
