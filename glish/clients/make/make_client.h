#ifndef bmake_h_
#define bmake_h_

#ifdef __cplusplus
extern "C" {
#endif
	/**
	 **  initialize bmake library
	 **/
	int bMake_Init( int argc, char **argv);
	/** 
	 **  finalize bmake library
	 **/
	int bMake_Finish( );
	/**
	 **  perform make
	 **/
	int bMake( );
	/**
	 **  define a variable
	 **/
	void bMake_Define( const char *var, const char *val );
	/**
	 **  define a makefile target
	 **/
	void bMake_TargetDef( const char *tag, const char **cmd, int cmd_len, const char **depend, int depend_len);
	/**
	 **  define a suffix rule
	 **/
	void bMake_SuffixDef( const char *tag, const char **cmd, int cmd_len );
	/**
         **  set root targets
	 **/
	void bMake_SetMain( const char **tgt, int len );
	/**
	 **  check to see if root targets have been
	 **  established
	 **/
	int  bMake_HasMain( );
	/**  set the function which is called to
	 **  perform each make action
	 **/
	void bMake_SetHandler( void (*)(char*) );
#ifdef __cplusplus
	}
#endif

#endif
