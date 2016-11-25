#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

void usage(char *str)
{
	printf("Usage: \n");
	printf("%s <on|off>\n", str);
}

int main(int argc, char **argv)
{
	int fd;
	int val = 1;

	if (2 != argc)
	{
		usage(argv[0]);

		return 0;
	}

	if (0 == strcmp(argv[1], "on"))       
	{
		val = 1;
	}
	else if (0 == strcmp(argv[1], "off"))
	{
		val = 0;
	}
	else
	{
		usage(argv[0]);

		return 0;
	}

	fd = open("/dev/nick_leds", O_RDWR);

	if (fd < 0) 
	{
		printf("can't open!\n");	
	}

	write(fd, &val, 4);

	close(fd);

	return 0;
}
