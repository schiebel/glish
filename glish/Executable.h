// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef executable_h
#define executable_h


// Searches PATH for the given executable; returns a malloc()'d copy
// of the path to the executable, which the caller should delete when
// done with.
char* which_executable( const char* exec_name );
// Given a fully qualified path, this checks to see if the file can
// be executed.
int can_execute( const char* name );

class Executable {
    public:
	Executable( const char* arg_executable );
	virtual ~Executable();

	int ExecError()	{ return exec_error; }

	// true if the executable is still "out there"
	virtual int Active() = 0;
	void Deactivate( ) { deactivated = 1; }
	virtual void Ping() = 0;

	virtual void DoneReceived();

    protected:
	char* executable;
	int exec_error;
	int has_exited;
	int deactivated;
	};

#endif	/* executable_h */
