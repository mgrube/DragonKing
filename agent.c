#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(){

	int a;
	printf("Is this thing on?\n");
	//a = open("Dickbutt666", O_RDONLY);
	a = close(66432);
	int devfd = open("/dev/dkCommand", O_WRONLY);
	write(devfd, "test", 4);
	printf("a is %d\n", a);
	return 0;
}
