/* $Id$
**
**  npd  --  network probe daemon
*/
#ifndef npd_h
#define npd_h

#ifdef __cplusplus
extern "C" {
#endif

	char **authenticate_client( int sock );
	int authenticate_to_server( int sock );
	int get_userid( const char *name );
	int get_user_group( const char *name );
	const char *get_group_name( int gid );

	void set_key_directory( const char * );
	const char *get_key_directory( );

	/* Returns 0 upon failure */
	int create_keyfile();
	void init_log(const char *program_name);

	long random_long();

#ifdef __cplusplus
	}
#endif

#endif
