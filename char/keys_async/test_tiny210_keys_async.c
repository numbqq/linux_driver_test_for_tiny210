#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

char key_values[8] = {0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88};

int fd;

void key_pressed_handler(int signum)
{
	int i;

//	printf("SIG: %d\n", signum);

	read(fd, key_values, 8);
	for (i=0; i<8; i++)
	{
		printf("APP: KEY%d: %s(0x%02x)\n", i + 1, key_values[i] & 0x80 ? "UP  " : "DOWN", key_values[i]);
	}

	printf("\n\n");
}

int main(int argc, char **argv)
{
	int i;
	int ret;
	int flags;

	fd = open("/dev/nick_keys", O_RDWR);

	if (fd < 0) 
	{
		printf("can't open!\n");

		return 0;
	}

	signal(SIGIO, key_pressed_handler);

	fcntl(fd, F_SETOWN, getpid());
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | FASYNC);


	while (1)
	{
		sleep(12);
	}

	close(fd);

	return 0;
}
