// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef localexec_h
#define localexec_h

#include "Executable.h"
#include "Glish/List.h"

class LocalExec;
declare(PList,LocalExec);

class LocalExec : public Executable {
    public:
	LocalExec( const char* arg_executable, const char** argv );
	LocalExec( const char* arg_executable );
	~LocalExec();

	int Active();
	void Ping();

	int PID()	{ return pid; }

	void DoneReceived();

	static void sigchld( );
    protected:
	static PList(LocalExec) *doneList;
	void MakeExecutable( const char** argv );
	int Active( int ignore_deactivate );

	int pid;
	};

#endif	/* localexec_h */
