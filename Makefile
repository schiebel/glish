# $Header$


all:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX= $@_r

install:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX=1 all_r
	
clean:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX= $@_r

distclean:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX= $@_r

all_r: 
	@echo "Building Glish for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) build.sds
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) build.editline
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) build.npd
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) build.tk
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) build.glish

clean_r:
	@echo "Cleaning Glish for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) clean.sds
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) clean.editline
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) clean.npd
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) clean.tk
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) clean.glish

distclean_r:
	@echo "Removing Everything for $(ARCH)"
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) distclean.sds
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) distclean.editline
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) distclean.npd
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) distclean.tk
	@$(MAKE) ARCH=$(ARCH) PFX=$(PFX) distclean.glish
	@rm -rf $(ARCH)

##
## Building SDS
##
build.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd sds; $(MAKE) $$FLGS PFX=$(PFX) install-all

clean.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd sds; $(MAKE) $$FLGS PFX=$(PFX) clean-all

distclean.sds:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd sds; $(MAKE) $$FLGS PFX=$(PFX) distclean-all

##
## Building Editline
##
build.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd editline; $(MAKE) $$FLGS PFX=$(PFX) install-all

clean.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd editline; $(MAKE) $$FLGS PFX=$(PFX) clean-all

distclean.editline:
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd editline; $(MAKE) $$FLGS PFX=$(PFX) distclean-all

##
## Building Glish
##
build.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd glish; $(MAKE) $$FLGS PFX=$(PFX) install-all

clean.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd glish; $(MAKE) $$FLGS PFX=$(PFX) clean-all

distclean.glish: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	cd glish; $(MAKE) $$FLGS PFX=$(PFX) distclean-all

##
## Building NPD
##
build.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

build.npd_r: 
	@if test -z "$(GLISH_NPD)"; then 			\
		FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=`head -1 $(ARCH)/glish.options`"; \
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=$(GLISH_NPD)"; fi;	\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

build.npd_r_r: $(GLISH_NPD)

GLISH_NPD: 
	@cd npd; $(MAKE) $(MFLAGS) ARCH=$(ARCH) PFX=$(PFX) install-all

clean.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

clean.npd_r: 
	@if test -z "$(GLISH_NPD)"; then 			\
		FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=`head -1 $(ARCH)/glish.options`"; \
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=$(GLISH_NPD)"; fi;	\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

clean.npd_r_r: clean-$(GLISH_NPD)

clean-GLISH_NPD: 
	@cd npd; $(MAKE) $(MFLAGS) ARCH=$(ARCH) PFX=$(PFX) clean-all

distclean.npd: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

distclean.npd_r: 
	@if test -z "$(GLISH_NPD)"; then 			\
		FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=`head -1 $(ARCH)/glish.options`"; \
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_NPD=$(GLISH_NPD)"; fi;	\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

distclean.npd_r_r: distclean-$(GLISH_NPD)

distclean-GLISH_NPD: 
	@cd npd; $(MAKE) $(MFLAGS) ARCH=$(ARCH) PFX=$(PFX) distclean-all

##
## Building TK
##
build.tk: 
	@if test -z "$(ARCH)"; then 				\
		FLGS="$(MFLAGS) ARCH=`config/architecture`";	\
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH)";	fi; 		\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

build.tk_r: 
	@if test -z "$(GLISH_TK)"; then 				\
		FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_TK=`tail -1 $(ARCH)/glish.options`"; \
	else 	FLGS="$(MFLAGS) ARCH=$(ARCH) GLISH_TK=$(GLISH_TK)"; fi;	\
	$(MAKE) $$FLGS PFX=$(PFX) $@_r

build.tk_r_r: $(GLISH_TK)

GLISH_TK: 
	@if test -z "$(PFX)"; then			\
		config/mkhier include/Rivet ;		\
	fi
	@cd rivet; $(MAKE) $(MFLAGS) ARCH=$(ARCH) PFX=$(PFX) install-all

clean.tk:

distclean.tk:

##
## Dummy targets
##
clean-:

distclean-:

