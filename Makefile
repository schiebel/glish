# $Header$

CWD=.

all:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;		 			\
	$(MAKE) $$FLGS $@_r

install:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS $@_r
	
clean:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS $@_r

distclean:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS $@_r

all_r: 
	@echo "Building Glish for $(ARCH)"
	@$(MAKE) build.sds
	@$(MAKE) build.editline
	@$(MAKE) build.npd
	@$(MAKE) build.tk
	@$(MAKE) build.glish

install_r: 
	@echo "Installing Glish for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) install.sds
	@$(MAKE) ARCH=$(ARCH) install.editline
	@$(MAKE) ARCH=$(ARCH) install.npd
	@$(MAKE) ARCH=$(ARCH) install.tk
	@$(MAKE) ARCH=$(ARCH) install.glish

clean_r:
	@echo "Cleaning Glish for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) clean.sds
	@$(MAKE) ARCH=$(ARCH) clean.editline
	@$(MAKE) ARCH=$(ARCH) clean.npd
	@$(MAKE) ARCH=$(ARCH) clean.tk
	@$(MAKE) ARCH=$(ARCH) clean.glish

distclean_r:
	@echo "Removing Everything for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) distclean.sds
	@$(MAKE) ARCH=$(ARCH) distclean.editline
	@$(MAKE) ARCH=$(ARCH) distclean.npd
	@$(MAKE) ARCH=$(ARCH) distclean.tk
	@$(MAKE) ARCH=$(ARCH) distclean.glish
	@rm -rf $(ARCH)

##
## Building SDS
##
build.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="";  fi;			 		\
	cd sds; $(MAKE) $$FLGS

install.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd sds; $(MAKE) $$FLGS install

clean.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd sds; $(MAKE) $$FLGS clean

distclean.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd sds; $(MAKE) $$FLGS distclean

##
## Building Editline
##
build.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd editline; $(MAKE) $$FLGS

install.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd editline; $(MAKE) $$FLGS install

clean.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd editline; $(MAKE) $$FLGS clean

distclean.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd editline; $(MAKE) $$FLGS distclean

##
## Building Glish
##
build.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd glish; $(MAKE) $$FLGS

install.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd glish; $(MAKE) $$FLGS install

clean.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd glish; $(MAKE) $$FLGS clean

distclean.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	cd glish; $(MAKE) $$FLGS distclean

##
## Building NPD
##
build.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

build.npd_r: 
	@if test -z "$(BUILD_NPD)"; then 			\
		FLGS="$(MFLAGS) BUILD_NPD=`head -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

build.npd_r_r: $(BUILD_NPD)

BUILD_NPD: 
	@cd npd; $(MAKE) $(MFLAGS) ARCH=$(ARCH)

install.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi; 					\
	$(MAKE) $$FLGS $@_r

install.npd_r: 
	@if test -z "$(BUILD_NPD)"; then 			\
		FLGS="$(MFLAGS) BUILD_NPD=`head -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

install.npd_r_r: install-$(BUILD_NPD)

install-BUILD_NPD:
	@cd npd; $(MAKE) install

clean.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

clean.npd_r: 
	@if test -z "$(BUILD_NPD)"; then 			\
		FLGS="$(MFLAGS) BUILD_NPD=`head -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;	\
	$(MAKE) $$FLGS $@_r

clean.npd_r_r: clean-$(BUILD_NPD)

clean-BUILD_NPD: 
	@cd npd; $(MAKE) clean

distclean.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

distclean.npd_r: 
	@if test -z "$(BUILD_NPD)"; then 			\
		FLGS="$(MFLAGS) BUILD_NPD=`head -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

distclean.npd_r_r: distclean-$(BUILD_NPD)

distclean-BUILD_NPD: 
	@cd npd; $(MAKE) distclean

##
## Building TK
##
build.tk: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

build.tk_r: 
	@if test -z "$(BUILD_TK)"; then 				\
		FLGS="$(MFLAGS) BUILD_TK=`tail -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;						\
	$(MAKE) $$FLGS $@_r

build.tk_r_r: $(BUILD_TK)

BUILD_TK: 
	@cd rivet; $(MAKE)

install.tk: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

install.tk_r: 
	@if test -z "$(BUILD_TK)"; then 			\
		FLGS="$(MFLAGS) BUILD_TK=`tail -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

install.tk_r_r: install-$(BUILD_TK)

install-BUILD_TK: 
	@cd rivet; $(MAKE) install

clean.tk: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="";	fi; 		\
	$(MAKE) $$FLGS $@_r

clean.tk_r: 
	@if test -z "$(BUILD_TK)"; then 			\
		FLGS="$(MFLAGS) BUILD_TK=`tail -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

clean.tk_r_r: clean-$(BUILD_TK)

clean-BUILD_TK: 
	@cd tk; $(MAKE) clean

distclean.tk: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS=""; fi;			 		\
	$(MAKE) $$FLGS $@_r

distclean.tk_r: 
	@if test -z "$(BUILD_TK)"; then 			\
		FLGS="$(MFLAGS) BUILD_TK=`tail -1 $(CWD)/$(ARCH)/glish.options`"; \
	else 	FLGS=""; fi;					\
	$(MAKE) $$FLGS $@_r

distclean.tk_r_r: distclean-$(BUILD_TK)

distclean-BUILD_TK: 
	@cd rivet; $(MAKE) distclean

##
## Dummy targets
##
install-:

clean-:

distclean-:

