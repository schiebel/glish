// $Header$

#include <stream.h>
#include <osfcn.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>

#include "LocalExec.h"

#ifdef SABER

// Grrrr ....

#include <sys/time.h>
#include <sys/resource.h>

typedef int pid_t;

#undef WIFEXITED
#define WIFEXITED(x)    (((union wait*) &x)->w_stopval != WSTOPPED && \
			 ((union wait*) &x)->w_termsig == 0)

pid_t waitpid( pid_t pid, int *loc, int opts )
	{
	int status = wait4( pid, (union wait*) loc, opts, (struct rusage*) 0 );

	if ( status == 0 )
		return 0;

	return pid;
	}

#endif

#if defined(mips)
#include <unistd.h>
#endif


#ifdef __GNUG__
// This define comes from g++'s <sys/wait.h> (it #undef's the #define later
// in the file); we need it for some flavors of SunOS because (at least in
// SunOS 4.1) there's a #define of __wait to "wait" in /usr/include/sys/wait.h
// and later a definition of the WIFEXITED macro & friends involving
// __wait.  When the macros are subsequently expanded, such as we do in
// this file, they'll turn into references to "wait"; but they rather need
// to turn into references to WaitStatus, since (due to the definition in
// the g++ <sys/wait.h>) "wait" has never been defined - the corresponding
// structure was instead defined as "WaitStatus".
//
// But to further add to the fun, g++ 2.1 has fixed the sys/wait.h header,
// and this hack below breaks that fix.  Fortunately we can check for
// which version of sys/wait.h we got, because the 2.1 version defines
// "__libgxx_sys_wait_h" and the previous ones don't.

#ifndef __libgxx_sys_wait_h
#define wait WaitStatus
#endif
#endif


LocalExec::LocalExec( const char* arg_executable, const char** argv )
    : Executable( arg_executable )
	{
	MakeExecutable( argv );
	}

LocalExec::LocalExec( const char* arg_executable )
    : Executable( arg_executable )
	{
	const char* argv[2];
	argv[0] = arg_executable;
	argv[1] = 0;
	MakeExecutable( argv );
	}


LocalExec::~LocalExec()
	{
	if ( Active() )
		kill( pid, SIGTERM );
	}


void LocalExec::MakeExecutable( const char** argv )
	{
	pid = 0;
	exec_error = 1;
	has_exited = 0;

	if ( access( executable, X_OK ) < 0 )
		return;

	pid = vfork();

	if ( pid == 0 )
		{ // child
		extern char** environ;
		execve( executable, argv, environ );

		cerr << "LocalExec::MakeExecutable: couldn't exec ";
		perror( executable );
		_exit( -1 );
		}

	if ( pid > 0 )
		exec_error = 0;
	}


int LocalExec::Active()
	{
	if ( has_exited || exec_error )
		return 0;

	int status;
	int child_id = (int) waitpid ( (pid_t) pid, &status, WNOHANG );

	if ( child_id == 0 )
		return 1;

	if ( child_id == pid )
		{
		if ( ! WIFEXITED(status) )
			cerr << "LocalExec::Active: strange child status for "
			     << executable << "\n";
		}

	else if ( errno != ECHILD )
		{
		cerr << "LocalExec::Active: problem getting child status for ";
		perror( executable );
		}

	has_exited = 1;
	return 0;
	}


void LocalExec::Ping()
	{
	if ( kill( pid, SIGIO ) < 0 )
		{
		cerr << "LocalExec::Ping: problem pinging executable ";
		perror( executable );
		}
	}
