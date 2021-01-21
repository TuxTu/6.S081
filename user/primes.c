#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX 35

// generating numbers
void source(unsigned int n) {
	for(unsigned int i = 2;i <= n;i++) {
		write(1, &i, sizeof(i));
	}
}

void redirect(int k, int p[]) {
	close(k);
	dup(p[k]);
	close(p[0]);
	close(p[1]);
}

void sieve(unsigned int first_num) {
	unsigned int temp_n;

	while(read(0, &temp_n, sizeof(temp_n))) {
		if (temp_n % first_num != 0) {
			write(1, &temp_n, sizeof(temp_n));
		}
	}
}

void child() {
	uint first_num;
	int p[2];

	pipe(p);

	if(read(0, &first_num, sizeof(first_num))) {
		if(fork()) {
			redirect(0, p);
			child();
		} else {
			printf("prime %d\n", first_num);
			redirect(1, p);
			sieve(first_num);
		}
	}
}

int
main(int argc, int *argv[]) {
	int p[2];

	pipe(p);

	if(fork()) {
		redirect(0, p);
		child();
	} else {
		redirect(1, p);
		source(MAX);
	}
	exit(0);
}
