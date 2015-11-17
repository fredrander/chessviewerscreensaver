#ifndef __popen2_h__
#define __popen2_h__


#include <stdbool.h>
#include <sys/types.h>


struct popen2
{
	pid_t child_pid;
	int from_child;
	int to_child;
};

bool popen2(const char *cmdline, struct popen2 *childinfo);
void popen2_close( struct popen2 *childinfo );

#endif /* __popen2_h__ */
