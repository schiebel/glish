# $Header$
#
# Makefile for Glish interpreter.

#  ##########################################################################
#  ### Environment changed in glish/Makefile: CC, LFLAGS                  ###
#  ### Note: You must have the newest version of flex (2.3.7 or greater)  ###
#  ### to complete this make. The tarfile is available via anonymous ftp  ###
#  ### from ftp.ee.lbl.gov                                                ###
#  ##########################################################################

CC = gcc
LFLAGS = -Ist

SRC = ..

H = $(SRC)/include/Glish

INCLUDE = -I$(SRC) -I$(SRC)/include -I../../include $(PLUSINC)

GLISH_OBJS = \
	Agent.o \
	BinOpExpr.o \
	BuiltIn.o \
	Client.o \
	Dict.o \
	Event.o \
	Executable.o \
	Expr.o \
	Frame.o \
	Func.o \
	List.o \
	LocalExec.o \
	Object.o \
	Queue.o \
	RemoteExec.o \
	Reporter.o \
	Select.o \
	Sequencer.o \
	Socket.o \
	Stmt.o \
	Task.o \
	Value.o \
	glish_client.o \
	parse.o \
	scan.o \
	strdup.o \
	system.o

KEY_HEADERS = \
	$(SRC)/Expr.h $(SRC)/Func.h $(H)/GlishType.h \
	$(H)/Object.h $(SRC)/Sequencer.h $(H)/Value.h \
	$(H)/glish.h

LIBS = $(ISTKPLACE)/lib/$(ARCH)/libsds.a

TAR_FILE = glish.tar

build:
	@if [ -d $(ARCH) ]; then \
		cd $(ARCH); $(MAKE) $(MFLAGS) -f ../Makefile all; \
		else $(MAKE) $(MFLAGS) -f ../Makefile all; fi;

all: comments libglish.a glish 

install: comments $(ISTKLIBS)/libglish.a $(ISTKBIN)/glish
	rm -f $(ISTKPLACE)/lib/glish.init
	$(CP) ../glish.init $(ISTKPLACE)/lib
	rm -f $(ISTKPLACE)/include/Glish/*.h
	$(CP) ../include/Glish/*.h $(ISTKPLACE)/include/Glish

install-all:
	@if [ ! -d $(ARCH) ]; then mkdir $(ARCH); else exit 0; fi;
	@echo Installing glish ...
	@cd $(ARCH); $(MAKE) $(MFLAGS) -f ../Makefile install
	@cd clients; $(MAKE) $(MFLAGS) install-all

comments::

glish: main.o libglish.a $(LIBS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -o glish main.o libglish.a $(LIBS) -lm

libglish.a: $(GLISH_OBJS)
	rm -f libglish.a
	$(AR) r libglish.a $(GLISH_OBJS)
	$(RANLIB) libglish.a

$(ISTKBIN)/glish: glish
	rm -f $(ISTKBIN)/glish
	$(CP) glish $(ISTKBIN)
	strip $(ISTKBIN)/glish

$(ISTKLIBS)/libglish.a: libglish.a
	rm -f $(ISTKLIBS)/libglish.a
	$(CP) libglish.a $(ISTKLIBS)
	ranlib $(ISTKLIBS)/libglish.a

Agent.o: $(SRC)/Agent.cc $(KEY_HEADERS) $(SRC)/Stmt.h $(SRC)/Reporter.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Agent.cc
BinOpExpr.o: $(SRC)/BinOpExpr.cc $(SRC)/BinOpExpr.h $(KEY_HEADERS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/BinOpExpr.cc
BuiltIn.o: $(SRC)/BuiltIn.cc $(SRC)/BuiltIn.h $(KEY_HEADERS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/BuiltIn.cc
Client.o: $(SRC)/Client.cc $(H)/Client.h $(H)/Value.h $(H)/Object.h \
		$(SRC)/Reporter.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Client.cc
Dict.o: $(SRC)/Dict.cc $(H)/Dict.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Dict.cc
Event.o: $(SRC)/Event.cc $(KEY_HEADERS) $(SRC)/Event.h $(SRC)/Reporter.h \
		$(SRC)/Task.h $(SRC)/Agent.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Event.cc
Executable.o: $(SRC)/Executable.cc $(SRC)/Executable.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Executable.cc
Expr.o: $(SRC)/Expr.cc $(KEY_HEADERS) $(SRC)/Reporter.h $(SRC)/Task.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Expr.cc
Frame.o: $(SRC)/Frame.cc $(SRC)/Frame.h $(H)/Object.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Frame.cc
Func.o: $(SRC)/Func.cc $(SRC)/Func.h $(SRC)/Stmt.h $(KEY_HEADERS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Func.cc
List.o: $(SRC)/List.cc $(H)/List.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/List.cc
LocalExec.o: $(SRC)/LocalExec.cc $(SRC)/LocalExec.h $(SRC)/Executable.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/LocalExec.cc
Object.o: $(SRC)/Object.cc $(H)/Object.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Object.cc
Queue.o: $(SRC)/Queue.cc $(SRC)/Queue.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Queue.cc
RemoteExec.o: $(SRC)/RemoteExec.cc $(SRC)/RemoteExec.h $(SRC)/Executable.h \
		$(SRC)/Channel.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/RemoteExec.cc
Reporter.o: $(SRC)/Reporter.cc $(SRC)/Reporter.h $(H)/Object.h $(SRC)/input.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Reporter.cc
Select.o: $(SRC)/Select.cc $(SRC)/Select.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Select.cc
Sequencer.o: $(SRC)/Sequencer.cc $(SRC)/Sequencer.h $(KEY_HEADERS) \
    $(SRC)/Task.h $(SRC)/Stmt.h $(SRC)/Channel.h $(SRC)/Queue.h \
    $(SRC)/Frame.h $(SRC)/ports.h $(SRC)/version.h
	$(CPLUS) '-DGLISH_DIR="$(ISTKPLACE)/lib"' $(CFLAGS) $(INCLUDE) \
		-c $(SRC)/Sequencer.cc
Socket.o: $(SRC)/Socket.cc $(SRC)/Socket.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Socket.cc
Stmt.o: $(SRC)/Stmt.cc $(SRC)/Stmt.h $(KEY_HEADERS)
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Stmt.cc
Task.o:	$(SRC)/Task.cc $(SRC)/Task.h $(KEY_HEADERS) $(SRC)/Channel.h \
    $(SRC)/BuiltIn.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Task.cc
Value.o: $(SRC)/Value.cc $(H)/Value.h $(SRC)/Expr.h $(SRC)/BinOpExpr.h \
			 $(H)/Object.h $(H)/glish.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/Value.cc
main.o: $(SRC)/main.cc $(SRC)/Sequencer.h
	$(CPLUS) $(CFLAGS) $(INCLUDE) -c $(SRC)/main.cc

y.tab.h parse.o: $(SRC)/parse.y

parse.cc: $(SRC)/parse.y
	$(YACC) -d $(SRC)/parse.y
	mv -f y.tab.c parse.cc

parse.o: parse.cc $(KEY_HEADERS) $(SRC)/Stmt.h $(SRC)/BinOpExpr.h
	@sed -f $(SRC)/fix_yyparse.sed < parse.cc > parse.cc.fixed
	@mv parse.cc.fixed parse.cc
	$(CPLUS) $(CFLAGS) $(INCLUDE) -I. -DYYDEBUG -c parse.cc

scan.o: $(SRC)/scan.l y.tab.h $(KEY_HEADERS)
	@rm -f scan.cc
	$(LEX) $(LFLAGS) $(SRC)/scan.l > scan.cc
	-chmod g+w scan.cc
	$(CPLUS) $(CFLAGS) $(INCLUDE) -I. -c scan.cc

strdup.o: $(SRC)/strdup.cc
	$(CPLUS) $(CFLAGS) -c $(SRC)/strdup.cc

glish_client.o: $(SRC)/glish_client.c $(SRC)/glish_client.h
	$(CC) $(CFLAGS) -I../include -c $(SRC)/glish_client.c
system.o: $(SRC)/system.c $(SRC)/system.h
	$(CC) $(CFLAGS) -c $(SRC)/system.c

tar:
	@rm -f $(TAR_FILE).Z tar-files
	@$(MAKE) $(MFLAGS) tar-list >tar-files
	tar cf $(TAR_FILE) -I tar-files
	compress $(TAR_FILE)
	@rm tar-files

tar-list:
	@ls Makefile COPYING Changes glish.init *.c *.cc *.h *.l *.y \
		*.sed doc/*.ps demos/*.g demos/*.cc demos/*.c \
		demos/README* include/Glish/*.h
	@cd clients; $(MAKE) $(MFLAGS) tar-list | sed 's,^,clients/,'

rcsinfo:
	@echo glish
	@rlog -L -S RCS/*,v
	@echo glish/include/Glish
	@cd include/Glish; rlog -L -S RCS/*,v
	@cd clients; $(MAKE) $(MFLAGS) rcsinfo

clean:
	@if [ -d $(ARCH) ]; then cd $(ARCH); fi; \
		rm -f $(GLISH_OBJS) libglish.a main.o glish errs parse.cc \
			    y.tab.h scan.cc ../$(TAR_FILE).Z

clean-all:
	@$(MAKE) $(MFLAGS) clean
	@cd clients; $(MAKE) $(MFLAGS) clean-all
