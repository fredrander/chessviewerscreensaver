#include "popen2.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


bool popen2(const char *cmdline, struct popen2 *childinfo)
{
	int pipe_stdin[2];
	int pipe_stdout[2];

	if (pipe(pipe_stdin)) {
		return false;
	}
	if (pipe(pipe_stdout)) {
		return false;
	}

	pid_t p = fork();
	if (p < 0) {
		return false; /* Fork failed */
	}
	if (p == 0) { /* child */
		close(pipe_stdin[1]);
		dup2(pipe_stdin[0], 0);
		close(pipe_stdout[0]);
		dup2(pipe_stdout[1], 1);
		execl("/bin/sh", "sh", "-c", cmdline, 0);
		perror("execl");
		exit(99);
	}
	childinfo->child_pid = p;
	childinfo->to_child = pipe_stdin[1];
	childinfo->from_child = pipe_stdout[0];
	return true;
}

void popen2_close( struct popen2 *childinfo )
{
	close( childinfo->to_child );
	close( childinfo->from_child );
	childinfo->child_pid = -1;
}
