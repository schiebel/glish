#	$Id$
#	skip missing directories...

#	$NetBSD: bsd.subdir.mk,v 1.11 1996/04/04 02:05:06 jtc Exp $
#	@(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91

.if exists(${.CURDIR}/Makefile.inc)
.include "Makefile.inc"
.endif
.if !target(.MAIN)
.MAIN: all
.endif

.ifdef SUBDIR_MUST_EXIST
CHK_EXISTS=:
.else
CHK_EXISTS=test -d ${.CURDIR}/$${_newdir_} || { echo "Skipping ===> $${_nextdir_}"; continue; }
.endif

_SUBDIRUSE: .USE
.if defined(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(set -e; if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			_newdir_="$${entry}.${MACHINE}"; \
		else \
			_newdir_="$${entry}"; \
		fi; \
		if test X"${_THISDIR_}" = X""; then \
			_nextdir_="$${_newdir_}"; \
		else \
			_nextdir_="$${_THISDIR_}/$${_newdir_}"; \
		fi; \
		${CHK_EXISTS} ; \
			echo "===> $${_nextdir_}"; \
			cd ${.CURDIR}/$${_newdir_}; \
			${MAKE} _THISDIR_="$${_nextdir_}" \
			    ${.TARGET:S/realinstall/install/:S/.depend/depend/}) || exit 1; \
	done

${SUBDIR}::
	@set -e; if test -d ${.CURDIR}/${.TARGET}.${MACHINE}; then \
		_newdir_=${.TARGET}.${MACHINE}; \
	else \
		_newdir_=${.TARGET}; \
	fi; \
	echo "===> $${_newdir_}"; \
	cd ${.CURDIR}/$${_newdir_}; \
	${MAKE} _THISDIR_="$${_newdir_}" all
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: maninstall
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif

.if !target(all)
all: _SUBDIRUSE
.endif

.if !target(clean)
clean: _SUBDIRUSE
.endif

.if !target(cleandir)
cleandir: _SUBDIRUSE
.endif

.if !target(includes)
includes: _SUBDIRUSE
.endif

.if !target(depend)
depend: _SUBDIRUSE
.endif

.if !target(lint)
lint: _SUBDIRUSE
.endif

.if !target(obj)
obj: _SUBDIRUSE
.endif

.if !target(tags)
tags: _SUBDIRUSE
.endif

.if !target(etags)
.if defined(SRCS)
etags: ${SRCS} _SUBDIRUSE
	-cd ${.CURDIR}; etags `echo ${.ALLSRC:N*.h} | sed 's;${.CURDIR}/;;'`
.elif defined(SUBDIR)
etags:
	-rm -f TAGS
	-cd ${.CURDIR}; find ${SUBDIR} \( -name '*.[chy]' -o -name '*.pc' -o -name '*.cc' \) -print | grep -v /obj | xargs etags -a
.else
etags: _SUBDIRUSE
.endif
.endif

.include <own.mk>
