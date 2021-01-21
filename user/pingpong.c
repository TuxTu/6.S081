#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
	int p1[2], p2[2];
	char ch;

	pipe(p1);
	pipe(p2);

	if(fork() != 0) {
		write(p1[1], "1", 1);
		read(p2[0], &ch, 1);
		fprintf(1, "%d: received pong\n", getpid());
	} else {
		read(p1[0], &ch, 1);
		fprintf(1, "%d: received ping\n", getpid());
		write(p2[1], &ch, 1);
		exit(0);
	}

	exit(0);
}
