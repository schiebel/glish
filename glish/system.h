/* $Header$ */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
	/* Change the given fd to non-blocking or blocking I/O. */
	void set_fd_non_blocking( int fd, int non_blocking_flag );

	/* Return the fd of a new TCP socket. */
	int get_tcp_socket();

	/* Return the fd of a new socket for local (same-host) use. */
	int get_local_socket();

	/* Returns a copy of the name of a local socket. */
	char* local_socket_name( int sock );

	/* Attempt to bind to given address.  Returns 0 if address
	 * is in use, 1 if bind successful, and -1 if fatal error
	 * encountered.  Upon success, a listen() is done on the
	 * socket with queue length 5.
	 */
	int bind_socket( int socket, int port );
	int bind_local_socket( int socket );	/* same for local socket */

	/* Accept a connection on the given socket, returning the
	 * new connection socket, or -1 on error.
	 */
	int accept_connection( int connection_socket );

	/* Accepts a local connection on the given socket, returning the
	 * new connection socket, or -1 on error.
	 */
	int accept_local_connection( int connection_socket );

	/* Connects the given local socket to the port on the given remote host.
	 * Returns 0 on failure, 1 on success.
	 */
	int remote_connection( int local_socket, const char* hostname,
				int port );

	/* Connect the given socket to the same-host socket specified by
	 * the given path.
	 */
	int local_connection( int sock, const char* path );

	/* Returns the PID of the next terminated process, if any, or 0
	 * if no more terminated processes are lying around waiting to be
	 * reaped.
	 */
	int reap_terminated_process();

	/* Mark the given file descriptor as close-on-exec. */
	void mark_close_on_exec( int fd );

	/* Do a popen() to read from a shell command, and if input is
	 * non-nil set up its standard input to read the given text.
	 *
	 * This routine currently only supports one active shell command
	 * at a time.
	 */
	FILE* popen_with_input( const char* command, const char* input );

	/* The matching close command. */
	int pclose_with_input( FILE* pipe );

	/* Creates a new named pipe and returns its name, or else nil
	 * on failure.
	 */
	char* make_named_pipe();

	/* Does whatever is needed to maximize the number of file descriptors
	 * available to the process.
	 */
	void maximize_num_fds();

#ifdef __cplusplus
	}
#endif
