// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef localexec_h
#define localexec_h

#include "Executable.h"

class LocalExec;

class LocalExec : public Executable {
    public:
	LocalExec( const char* arg_executable, const char** argv );
	LocalExec( const char* arg_executable );
	~LocalExec();

	int Active();
	void Ping();

	int pid();

    protected:
	// calling this implies that the child
	// has exited and has been waited on.
	void SetStatus( int s );

	void MakeExecutable( const char** argv );
	int Active( int ignore_deactivate );

	ExecMinder lexpid;

	int pid_;
	int status;
	};

#endif	/* localexec_h */
