#	$Id$
# skip the :Q for now as we don't support it (yet) on non-BSD systems
#	$NetBSD: bsd.lib.mk,v 1.83 1997/02/17 19:24:47 cgd Exp $
#	@(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.include <own.mk>				# for 'NOPIC' definition

.if exists(${.CURDIR}/shlib_version)
SHLIB_MAJOR != . ${.CURDIR}/shlib_version ; echo $$major
SHLIB_MINOR != . ${.CURDIR}/shlib_version ; echo $$minor
.endif

.MAIN: all

# prefer .S to a .c, add .po, remove stuff not used in the BSD libraries.
# .so used for PIC object files.  .ln used for lint output files.
.SUFFIXES:
.SUFFIXES: .out .o .po .so .S .s .c .cc .C .f .y .l .ln .m4

CFLAGS+=	${COPTS}

# we only use this if they ask for it
USE_LIBTOOL?=no

# sys.mk can override these
CC_PG?=-pg
CC_PIC?=-DPIC

LD_X?=-X
LD_x?=-x
LD_r?=-r

.if (${MACHINE} == "sunos")
LD_shared=-assert pure-text
.elif (${MACHINE} == "solaris")
LD_shared=-h lib${LIB}.so.${SHLIB_MAJOR} -G
.elif (${MACHINE} == "hp-ux")
LD_shared=-b
LD_so=sl
DLLIB=
# HPsUX lorder does not grok anything but .o
LD_sobjs=`${LORDER} ${OBJS} | ${TSORT} | sed 's,\.o,.so,'`
LD_pobjs=`${LORDER} ${OBJS} | ${TSORT} | sed 's,\.o,.po,'`
.elif (${MACHINE} == "osf1")
LD_shared= -msym -shared -expect_unresolved '*'
LD_solib= -all lib${LIB}_pic.a
DLLIB=
# lorder does not grok anything but .o
LD_sobjs=`${LORDER} ${OBJS} | ${TSORT} | sed 's,\.o,.so,'`
LD_pobjs=`${LORDER} ${OBJS} | ${TSORT} | sed 's,\.o,.po,'`
AR_cq= -cqs
.elif (${MACHINE} == "freebsd")
LD_solib= lib${LIB}_pic.a
.elif (${MACHINE} == "linux")
# but which one? does it matter?
# Red-hat has tsort but not lorder?
# but we can use lorder from NetBSD :-)
LD_shared=-shared -h lib${LIB}.so.${SHLIB_MAJOR}
LD_solib= --whole-archive lib${LIB}_pic.a
.endif
LORDER?=lorder
TSORT?=tsort
LIBTOOL?=libtool
LD_shared?=-Bshareable -Bforcearchive
LD_so?=so.${SHLIB_MAJOR}.${SHLIB_MINOR}
LD_sobjs?=`${LORDER} ${SOBJS} | ${TSORT}`
LD_pobjs?=`${LORDER} ${POBJS} | ${TSORT}`
LD_solib?=${LD_sobjs}
AR_cq?=cq
.if exists(/netbsd)
DLLIB?=-ldl
.endif

.if defined(USE_LIBTOOL) && (${USE_LIBTOOL} == "yes")
# because libtool is so facist about naming the object files,
# we cannot (yet) build profiled libs
NOPROFILE=no
_LIBS=lib${LIB}.a
.if exists(${.CURDIR}/shlib_version)
SHLIB_AGE != . ${.CURDIR}/shlib_version ; echo $$age
.endif
.else
# for the normal .a we do not want to strip symbols
.c.o:
	${COMPILE.c} ${.IMPSRC}
#	@echo ${COMPILE.c:Q} ${.IMPSRC}
#	@${COMPILE.c} ${.IMPSRC}  -o ${.TARGET}.o
#	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
#	@rm -f ${.TARGET}.o


# for the normal .a we do not want to strip symbols
.cc.o .C.o:
	${COMPILE.cc} ${.IMPSRC}
#	@echo ${COMPILE.cc:Q} ${.IMPSRC}
#	@${COMPILE.cc} ${.IMPSRC} -o ${.TARGET}.o
#	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
#	@rm -f ${.TARGET}.o

.S.o .s.o:
	@echo ${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC}
	@${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} 
#	@${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}.o
#	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
#	@rm -f ${.TARGET}.o

.if (${LD_X} == "")
.c.po:
	${COMPILE.c} ${CC_PG} ${.IMPSRC} -o ${.TARGET}

.cc.po .C.po:
	${COMPILE.cc} -pg ${.IMPSRC} -o ${.TARGET}

.S.so .s.so:
	${COMPILE.S} ${PICFLAG} ${CC_PIC} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}
.else
.c.po:
	@echo ${COMPILE.c} ${CC_PG} ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.c} ${CC_PG} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_X} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.po .C.po:
	@echo ${COMPILE.cc} -pg ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.cc} -pg ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_X} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.so .s.so:
	@echo ${COMPILE.S} ${PICFLAG} ${CC_PIC} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.S} ${PICFLAG} ${CC_PIC} ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
.endif

.if (${LD_x} == "")
.c.so:
	${COMPILE.c} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}

.cc.so .C.so:
	${COMPILE.cc} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}

.S.po .s.po:
	${COMPILE.S} -DGPROF -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}
.else

.c.so:
	@echo ${COMPILE.c} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.c} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.cc.so .C.so:
	@echo ${COMPILE.cc} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.cc} ${PICFLAG} ${CC_PIC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_x} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.S.po .s.po:
	@echo ${COMPILE.S} -DGPROF -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}
	@${COMPILE.S} -DGPROF -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} -o ${.TARGET}.o
	@${LD} ${LD_X} ${LD_r} ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.endif
.endif

.c.ln:
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[IDU]*} -i ${.IMPSRC}

.if defined(USE_LIBTOOL) && (${USE_LIBTOOL} != "yes")

.if !defined(PICFLAG)
PICFLAG=-fpic
.endif

.if !defined(NOARCHIVE)
_LIBS=lib${LIB}.a
.else
_LIBS=
.endif

.if !defined(NOPROFILE)
_LIBS+=lib${LIB}_p.a
.endif

.if !defined(NOPIC)
_LIBS+=lib${LIB}_pic.a
.if defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
_LIBS+=lib${LIB}.${LD_so}
.endif
.endif
.endif

.if !defined(NOLINT)
_LIBS+=llib-l${LIB}.ln
.endif

all: ${_LIBS} _SUBDIRUSE

OBJS+=	${SRCS:N*.h:R:S/$/.o/g}

.if defined(USE_LIBTOOL) && (${USE_LIBTOOL} == "yes")
.ifdef NOPIC
LT_STATIC=-static
.else
LT_STATIC=
.endif
SHLIB_AGE?=0

# .lo's are created as a side effect
.s.o .S.o .c.o:
	${LIBTOOL} --mode=compile ${CC} ${LT_STATIC} ${CFLAGS} ${CPPFLAGS} -c ${.IMPSRC}

# can't really do profiled libs with libtool - its too facist about
# naming the output...
lib${LIB}.a:: ${OBJS}
	@rm -f ${.TARGET}
	${LIBTOOL} --mode=link ${CC} ${LT_STATIC} -o ${.TARGET:.a=.la} ${OBJS:.o=.lo} -rpath ${prefix}/lib -version-info ${SHLIB_MAJOR}:${SHLIB_MINOR}:${SHLIB_AGE}
	@ln .libs/${.TARGET} .

lib${LIB}.${LD_so}:: lib${LIB}.a
	@[ -s ${.TARGET}.${SHLIB_AGE} ] || { ln -s .libs/lib${LIB}.${LD_so}* . 2>/dev/null; : }
	@[ -s ${.TARGET} ] || ln -s ${.TARGET}.${SHLIB_AGE} ${.TARGET}
.else

lib${LIB}.a:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} ${AR_cq} lib${LIB}.a `${LORDER} ${OBJS} | ${TSORT}`
	${RANLIB} lib${LIB}.a

POBJS+=	${OBJS:.o=.po}
lib${LIB}_p.a:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} ${AR_cq} lib${LIB}_p.a ${LD_pobjs}
	${RANLIB} lib${LIB}_p.a

SOBJS+=	${OBJS:.o=.so}
lib${LIB}_pic.a:: ${SOBJS}
	@echo building shared object ${LIB} library
	@rm -f lib${LIB}_pic.a
	@${AR} ${AR_cq} lib${LIB}_pic.a ${LD_sobjs}
	${RANLIB} lib${LIB}_pic.a

# bound to be non-portable...
lib${LIB}.${LD_so}: lib${LIB}_pic.a ${DPADD}
	@echo building shared ${LIB} library \(version ${SHLIB_MAJOR}.${SHLIB_MINOR}\)
	@rm -f lib${LIB}.${LD_so}
.if exists(/netbsd)
.if (${MACHINE_ARCH} == "alpha")
	$(LD) -shared -o lib${LIB}.${LD_so} \
	    -soname lib${LIB}.so.${SHLIB_MAJOR} /usr/lib/crtbeginS.o \
	    --whole-archive lib${LIB}_pic.a --no-whole-archive ${SHLIB_LDADD} \
	    /usr/lib/crtendS.o
.else
	$(LD) ${LD_x} ${LD_shared} \
	    -o lib${LIB}.${LD_so} lib${LIB}_pic.a ${SHLIB_LDADD}
.endif
.else
	# hope this works for you...
	$(LD) -o lib${LIB}.${LD_so} ${LD_shared} ${LD_solib} ${DLLIB} ${SHLIB_LDADD}
.endif
.endif
.if defined(NEED_SOLINKS)
	rm -f lib${LIB}.so; ln -s lib${LIB}.${LD_so} lib${LIB}.so
.endif

LOBJS+=	${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
LLIBS?=	-lc
llib-l${LIB}.ln: ${LOBJS}
	@echo building llib-l${LIB}.ln
	@rm -f llib-l${LIB}.ln
	@${LINT} -C${LIB} ${LOBJS} ${LLIBS}

.if !target(clean)
cleanlib:
	rm -f a.out [Ee]rrs mklog core *.core ${CLEANFILES}
	rm -f lib${LIB}.a ${OBJS}
	rm -f lib${LIB}_p.a ${POBJS}
	rm -f lib${LIB}_pic.a lib${LIB}.so.*.* ${SOBJS}
	rm -f llib-l${LIB}.ln ${LOBJS}
.if defined(NEED_SOLINKS)
	rm -f lib${LIB}.so*
.endif

clean: _SUBDIRUSE cleanlib
cleandir: _SUBDIRUSE cleanlib
.else
cleandir: _SUBDIRUSE clean
.endif

.if defined(SRCS)
afterdepend: .depend
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o[ ]*:/\1.o \1.po \1.so \1.ln:/' \
	      < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall:
.if !defined(NOARCHIVE)
#	ranlib lib${LIB}.a
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m 600 lib${LIB}.a \
	    ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}.a
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/lib${LIB}.a
.endif
.if !defined(NOPROFILE)
#	ranlib lib${LIB}_p.a
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m 600 \
	    lib${LIB}_p.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
.endif
.if !defined(NOPIC)
#	ranlib lib${LIB}_pic.a
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m 600 \
	    lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_pic.a
	chmod ${LIBMODE} ${DESTDIR}${LIBDIR}/lib${LIB}_pic.a
.endif
.if !defined(NOPIC) && defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    lib${LIB}.${LD_so} ${DESTDIR}${LIBDIR}
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE} == "solaris") || (${MACHINE} == "linux") || defined(NEED_SOLINKS)
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}
	ln -s lib${LIB}.${LD_so} \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so
	ln -s lib${LIB}.${LD_so} \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.so
.endif
.endif
.if !defined(NOLINT)
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    llib-l${LIB}.ln ${DESTDIR}${LINTLIBDIR}
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif


# during building we usually need/want to install libs somewhere central
# note that we do NOT ch{own,grp} as that would likely fail at this point.
# otherwise it is the same as realinstall
.libinstall:	${_LIBS}
.if !defined(NOARCHIVE)
	${INSTALL} ${COPY} -m 644 lib${LIB}.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}.a
.endif
.if !defined(NOPROFILE)
	${INSTALL} ${COPY} -m 644 lib${LIB}_p.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
.endif
.if !defined(NOPIC)
	${INSTALL} ${COPY} -m 644 lib${LIB}_pic.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_pic.a
.endif
.if !defined(NOPIC) && defined(SHLIB_MAJOR) && defined(SHLIB_MINOR)
	${INSTALL} ${COPY} -m ${LIBMODE} lib${LIB}.${LD_so} ${DESTDIR}${LIBDIR}
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE} == "solaris") || (${MACHINE} == "linux") || defined(NEED_SOLINKS)
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}
	ln -s lib${LIB}.${LD_so} \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so
	ln -s lib${LIB}.${LD_so} \
	    ${DESTDIR}${LIBDIR}/lib${LIB}.so
.endif
.endif
	@touch ${.TARGET}

install: maninstall _SUBDIRUSE
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !defined(NOMAN)
.include <man.mk>
.endif

.if !defined(NONLS)
.include <nls.mk>
.endif

.include <obj.mk>
.include <dep.mk>
.include <subdir.mk>
