#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "system.h"

main()
	{
	int fd_pipe[2], pid, fd = 0;
	char buf[4096];

	if ( stream_pipe( fd_pipe ) < 0 )
		{
		perror("pipe");
		exit(1);
		}

	pid = fork();
	if ( pid < 0 )
		{
		perror("fork");
		exit(1);
		}

	
	if ( pid == 0 )
		{
		close( fd_pipe[1] );
		sleep(1);

		fd = recv_fd( fd_pipe[0] );
		if ( fd < 0 )
			{
			perror("recv_fd");
			exit(1);
			}
		while ( read( fd, buf, sizeof(buf) ) )
			printf("%s", buf);
		exit(0);
		}

	close( fd_pipe[0] );
	fd = open("/etc/passwd",O_RDONLY);
	send_fd( fd_pipe[1], fd );
	sleep(2);
	exit(0);
	}
