// $Id$
// Copyright (c) 1993 The Regents of the University of California.
// Copyright (c) 1997 Associated Universities Inc.
#ifndef remoteexec_h
#define remoteexec_h

#include "Executable.h"

class Channel;

class RemoteExec : public Executable {
    public:
	RemoteExec( Channel* daemon_channel,
		    const char* arg_executable, const char** argv );
	~RemoteExec();

	int Active();
	void Ping();

    protected:
	char* id;
	Channel* daemon_channel;
	};

#endif	/* remoteexec_h */
