# $Header$
#
#    Makefile for glish clients
#
#    Note: Glish requires an ANSI C compiler.

RM = rm -f

CC = gcc

SRC = ..
GLISH_SRC = ../..

INCLUDE = -I$(GLISH_SRC) -I$(GLISH_SRC)/include $(PLUSINC)
CINCLUDE = -I$(GLISH_SRC) -I$(GLISH_SRC)/include

SDS_LIB = $(ISTKLIBS)/libsds.a
GLISH_LIBS = $(GLISH_SRC)/$(ARCH)/libglish.a
LIBS = $(GLISH_LIBS) $(SDS_LIB) -lm

CLIENTS = \
	glishd \
	c_test_client \
	echo_client \
	shell_client \
	tell_glishd \
	test_client \
	timer

INSTALLED_CLIENTS = \
	$(ISTKBIN)/glishd \
	$(ISTKBIN)/c_test_client \
	$(ISTKBIN)/echo_client \
	$(ISTKBIN)/shell_client \
	$(ISTKBIN)/tell_glishd \
	$(ISTKBIN)/test_client \
	$(ISTKBIN)/timer

TAR_FILE = glish-clients.tar

build:
	@if [ -d $(ARCH) ]; then \
		cd $(ARCH); $(MAKE) $(MFLAGS) -f ../Makefile all; \
		else $(MAKE) $(MFLAGS) -f ../Makefile all; fi;

all: $(CLIENTS)

install: $(INSTALLED_CLIENTS)

install-all:
	@if [ ! -d $(ARCH) ]; then mkdir $(ARCH); else exit 0; fi;
	@echo Installing glish/clients ...
	@cd $(ARCH); $(MAKE) $(MFLAGS) -f ../Makefile install

$(GLISH_SRC)/$(ARCH)/libglish.a:
	cd $(GLISH_SRC)/$(ARCH); $(MAKE) $(MFLAGS) -f ../Makefile

glishd.o: $(SRC)/glishd.cc $(GLISH_SRC)/Executable.h \
		$(GLISH_SRC)/include/Glish/Client.h $(GLISH_SRC)/Channel.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/glishd.cc 

glishd: glishd.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o glishd glishd.o $(LIBS)
$(ISTKBIN)/glishd: glishd
	$(RM) $(ISTKBIN)/glishd
	$(CP) glishd $(ISTKBIN)
	strip $(ISTKBIN)/glishd

c_test_client.o: $(SRC)/c_test_client.c $(GLISH_SRC)/glish_client.h
	$(CC) $(CFLAGS) $(CINCLUDE) -c $(SRC)/c_test_client.c

c_test_client: c_test_client.o $(GLISH_LIBS)
	$(CC) $(CFLAGS) $(CINCLUDE) -o c_test_client c_test_client.o $(LIBS)
$(ISTKBIN)/c_test_client: c_test_client
	$(RM) $(ISTKBIN)/c_test_client
	$(CP) c_test_client $(ISTKBIN)
	strip $(ISTKBIN)/c_test_client

echo_client.o: $(SRC)/echo_client.cc $(GLISH_SRC)/include/Glish/Client.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/echo_client.cc 

echo_client: echo_client.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o echo_client echo_client.o $(LIBS)
$(ISTKBIN)/echo_client: echo_client
	$(RM) $(ISTKBIN)/echo_client
	$(CP) echo_client $(ISTKBIN)
	strip $(ISTKBIN)/echo_client

shell_client.o: $(SRC)/shell_client.cc $(GLISH_SRC)/include/Glish/Client.h \
		$(GLISH_SRC)/Channel.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/shell_client.cc 

shell_client: shell_client.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o shell_client shell_client.o $(LIBS)
$(ISTKBIN)/shell_client: shell_client
	$(RM) $(ISTKBIN)/shell_client
	$(CP) shell_client $(ISTKBIN)
	strip $(ISTKBIN)/shell_client

tell_glishd.o: $(SRC)/tell_glishd.cc $(GLISH_SRC)/include/Glish/Client.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -I$(ISTKPLACE)/include -c $(SRC)/tell_glishd.cc 

tell_glishd: tell_glishd.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o tell_glishd tell_glishd.o $(LIBS)
$(ISTKBIN)/tell_glishd: tell_glishd
	$(RM) $(ISTKBIN)/tell_glishd
	$(CP) tell_glishd $(ISTKBIN)
	strip $(ISTKBIN)/tell_glishd

test_client.o: $(SRC)/test_client.cc $(GLISH_SRC)/include/Glish/Client.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/test_client.cc 

test_client: test_client.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o test_client test_client.o $(LIBS)
$(ISTKBIN)/test_client: test_client
	$(RM) $(ISTKBIN)/test_client
	$(CP) test_client $(ISTKBIN)
	strip $(ISTKBIN)/test_client

timer.o: $(SRC)/timer.cc $(GLISH_SRC)/include/Glish/Client.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/timer.cc 

timer: timer.o $(GLISH_LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o timer timer.o $(LIBS)
$(ISTKBIN)/timer: timer
	$(RM) $(ISTKBIN)/timer
	$(CP) timer $(ISTKBIN)
	strip $(ISTKBIN)/timer

tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm -f tar-files

tar-list:
	@ls Makefile *.c *.cc

rcsinfo:
	@echo glish/clients
	@rlog -L -S RCS/*,v

clean:
	@if [ -d $(ARCH) ]; then cd $(ARCH); fi; \
		rm -f $(CLIENTS) *.o errs ../$(TAR_FILE).Z

clean-all:
	@$(MAKE) $(MFLAGS) clean
