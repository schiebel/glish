/* $Id$ 
** Copyright (c) 1993 The Regents of the University of California.
** Copyright (c) 1997 Associated Universities Inc.
*/
#ifndef glish_event_h
#define glish_event_h

/* Glish inter-client protocol version. */
#define GLISH_CLIENT_PROTO_VERSION 4

/* Bits in the flags that are transmitted */
/* with each event.                       */
#define GLISH_HAS_ATTRIBUTE	0x1
#define GLISH_REQUEST_EVENT	0x2
#define GLISH_REPLY_EVENT 	0x4
#define GLISH_STRING_EVENT 	0x8

#endif /* glish_event_h */
