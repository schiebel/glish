/* $Header$ */

#ifndef glish_event_h
#define glish_event_h


/* Glish inter-client protocol version. */
#define GLISH_CLIENT_PROTO_VERSION 2


/* Maximum size of an event name, including the final '\0'. */
#define MAX_EVENT_NAME_SIZE 32


struct event_header
	{
#define STRING_EVENT 0
#define SDS_EVENT 1
#define SDS_OPAQUE_EVENT 2
	u_long code;

	u_long event_length;

	char event_name[MAX_EVENT_NAME_SIZE];
	};

#endif /* glish_event_h */
