#	from: @(#)Makefile	5.2 (Berkeley) 12/28/90
#	$Id$

PROG=	bmake
#CC=gcc -pipe -g
MACHINE?=sunos
MACHINE_ARCH?=sun4
#.ifndef BSDOBJDIR
#OBJMACHINE=yes
#.endif

all: config.h ${PROG}

CFLAGS+= -DHAVE_CONFIG_H -I. -I${.CURDIR} -DMACHINE=\"${MACHINE}\" -DMACHINE_ARCH=\"${MACHINE_ARCH}\"
.if ${MACHINE} == "sun386"
CFLAGS+= -DPORTAR
.else
CFLAGS+= -DPOSIX
.endif

SRCS=	arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c util.c
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
SRCS+=	getenv.c 
.if ${MACHINE} != sunos
SRCS+= sigcompat.c
.endif
.PATH:	${.CURDIR}/lst.lib

BINDIR=	/usr/local/bin

.if make(install)

SUBDIR+= PSD.doc
.endif

.include "prog.mk"

.include "../Makefile.autoconf.inc"
