/* $Id$
**
**  npd  --  network probe daemon
*/
#ifndef npd_h
#define npd_h

#ifdef __cplusplus
extern "C" {
#endif

	int authenticate_client( int sock );
	int authenticate_to_server( int sock );

	/* Returns 0 upon failure */
	int create_keyfile();
	void init_log(const char *program_name);

	long random_long();

#ifdef __cplusplus
	}
#endif

#endif
