#	$NetBSD: bsd.dep.mk,v 1.11 1994/12/13 11:14:56 mycroft Exp $

.if exists(/usr/local/share/bin/mkdeps.sh)
MK_DEP?=mkdeps.sh -N
.endif
MK_DEP?=mkdep

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: beforedepend .depend _SUBDIRUSE afterdepend
.if defined(SRCS)
.depend: ${SRCS}
	rm -f .depend
	files="${.ALLSRC:M*.s} ${.ALLSRC:M*.S}"; \
	if [ "$$files" != " " ]; then \
	  ${MK_DEP} -a ${MKDEP} ${CFLAGS:M-[ID]*} ${CPPFLAGS} ${AINC} $$files; \
	fi
	files="${.ALLSRC:M*.c}"; \
	if [ "$$files" != "" ]; then \
	  ${MK_DEP} -a ${MKDEP} ${CFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
	files="${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}"; \
	if [ "$$files" != "  " ]; then \
	  ${MK_DEP} -a ${MKDEP} ${CXXFLAGS:M-[ID]*} ${CPPFLAGS} $$files; \
	fi
.else
.depend:
.endif
.if !target(beforedepend)
beforedepend:
.endif
.if !target(afterdepend)
afterdepend:
.endif
.endif

.if !target(tags)
.if defined(SRCS)
tags: ${SRCS} _SUBDIRUSE
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.else
tags:
.endif
.endif

.if defined(SRCS)
cleandir: cleandepend
cleandepend:
	rm -f .depend ${.CURDIR}/tags
.endif
