/* $Header$ */

#ifndef glish_client_h
#define glish_client_h

#include "Glish/GlishType.h"


/* The Glish C Client library is initialized by giving it the program's
 * argc and argv.  Any client-specific arguments are read and stripped off.
 *
 * Returns true if there is a sequencer connection, false if the client
 * is running stand-alone (in which case client_next_string_event reads events
 * from stdin and client_post_*_event sends events to stdout).
 */
int client_init( int *argc, char **argv );

/* Must be called prior to exit, or else the Glish sequencer will
 * consider the client to have "failed".
 */
void client_terminate();


/* Wait for the next string-valued event to arrive; return its name
 * as the function value and a pointer to its value in "event_value".
 * This pointer is volatile in the sense that its value will be lost
 * upon the next call to client_next_string_event().  (The event name
 * is similarly volatile.)
 *
 * Non-string-valued events cause error messages to be printed and
 * 0 to be returned.
 *
 * If the sequencer connection has been broken then 0 is returned
 * (and the caller should terminate).
 */
char *client_next_string_event( char **event_value );


/* Sends a string-valued event with the given name and value. */
void client_post_string_event( const char *event_name,
				const char *event_value );
void client_post_int_event( const char *event_name, int event_value );
void client_post_double_event( const char *event_name, double event_value );

/* Sends an event with the given name, using a printf-style format
 * and an associated string argument.  For example,
 *
 *	client_post_fmt_event( "error", "couldn't open %s", file_name );
 *
 */
void client_post_fmt_event( const char *event_name, const char *event_fmt,
				const char *fmt_arg );


/* The following provide immediate access to the fd's for events coming
 * from the Glish sequencer (client_read_fd) or for sending events to
 * the sequencer (client_write_fd).
 */
int client_read_fd();
int client_write_fd();


#endif	/* glish_client_h */
