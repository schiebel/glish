// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.

#include "Glish/glish.h"
RCSID("@(#) $Id$")
#include "system.h"

#include <stream.h>
#include <osfcn.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef HAVE_SIGLIB_H
#include <sigLib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif

#include "LocalExec.h"

#if defined(SIGCHLD)
#define GLISH_SIGCHLD SIGCHLD
#elif defined(SIGCLD)
#define GLISH_SIGCHLD SIGCLD
#endif

PList(LocalExec) *LocalExec::doneList = 0;

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
	if ( doneList )
		{
		signal_handler old = install_signal_handler( GLISH_SIGCHLD, (signal_handler) SIG_DFL );
		doneList->remove(this);
		install_signal_handler( GLISH_SIGCHLD, (signal_handler) old );
		}
	}

void LocalExec::MakeExecutable( const char** argv )
	{
	pid = 0;
	exec_error = 1;
	has_exited = 0;

	if ( access( executable, X_OK ) < 0 )
		return;

	/*
	** Ignore ^C coming from parent because ^C is used in the
	** interpreter interface, and it gets delievered to all children.
	*/
	signal_handler old_sigint = install_signal_handler( SIGINT, (signal_handler) SIG_IGN );

	pid = (int) vfork();

	if ( pid == 0 )
		{ // child
		extern char** environ;
#ifndef POSIX
		execve( executable, (char **)argv, environ );
#else
		execve( executable, (char *const*)argv, environ );
#endif

		cerr << "LocalExec::MakeExecutable: couldn't exec ";
		perror( executable );
		_exit( -1 );
		}


	if ( pid > 0 )
		exec_error = 0;

	/*
	** Restore ^C
	*/
	install_signal_handler( SIGINT, (signal_handler) old_sigint );

	}


int LocalExec::Active()
	{
	if ( has_exited || exec_error || deactivated )
		return 0;

	int status;
	int child_id = wait_for_pid( pid, &status, WNOHANG );

	if ( child_id == 0 )
		return 1;

	if ( child_id == pid )
		{
		if ( (status & 0xff) != 0 )
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


void LocalExec::DoneReceived()
	{
#if defined(GLISH_SIGCHLD)
	if ( ! doneList ) doneList = new PList(LocalExec);
	install_signal_handler( GLISH_SIGCHLD, (signal_handler) SIG_DFL );
	doneList->append(this);
	install_signal_handler( GLISH_SIGCHLD, (signal_handler) sigchld );
#endif
	}

void LocalExec::sigchld()
	{
#if defined(GLISH_SIGCHLD)
	for (int i=doneList->length()-1; i >= 0; --i)
		if ( ! (*doneList)[i]->Active() )
			doneList->remove_nth(i);

	if ( doneList->length() )
		install_signal_handler( GLISH_SIGCHLD, (signal_handler) sigchld );
	else
		install_signal_handler( GLISH_SIGCHLD, (signal_handler) SIG_DFL );
#endif
	}
