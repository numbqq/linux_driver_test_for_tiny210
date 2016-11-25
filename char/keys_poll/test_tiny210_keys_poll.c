#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>

char key_values[8] = {0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88};

struct pollfd fds[1];

int main(int argc, char **argv)
{
	int fd;
	int i;
	int ret;

	fd = open("/dev/nick_keys", O_RDWR);

	if (fd < 0) 
	{
		printf("can't open!\n");

		return 0;
	}

	fds[0].fd = fd;
	fds[0].events = POLLIN;

	while (1)
	{
		ret = poll(fds, 1, 5000);
		
		if (0 == ret)
		{
			printf("timeout!\n");
		}
		else
		{
			read(fd, key_values, sizeof(key_values));
			for (i=0; i<8; i++)
			{   
				    printf("APP: KEY%d: %s(0x%02x)\n", i + 1, key_values[i] & 0x80 ? "UP  " : "DOWN", key_values[i]);
			}
			printf("\n\n");
		}
	}

	close(fd);

	return 0;
}
