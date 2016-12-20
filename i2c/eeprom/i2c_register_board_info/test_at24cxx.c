#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void usage(char *str)
{
	printf("%s w addr data\n", str);
	printf("%s r addr\n", str);
}

int main(int argc, char **argv)
{
	char buffer[2];
	int fd;

	if (argc != 3 && argc != 4) {
		usage(argv[0]);
	
		return -1;
	}	

	fd = open("/dev/at24cxx", O_RDWR);
	if (fd < 0) {
		printf("failed to open /dev/at24cxx!\n");

		return -1;
	}

	if (0 == strcmp("w", argv[1]) && 4 == argc) {
		buffer[0] = strtoul(argv[2], NULL, 0);
		buffer[1] = strtoul(argv[3], NULL, 0);
		if (2 != write(fd, buffer, 2))
			printf("write error: addr: %d, data: %d\n", buffer[0], buffer[1]);
	} else if (0 == strcmp("r", argv[1]) && 3 == argc) {
		buffer[0] = strtoul(argv[2], NULL, 0);
		read(fd, buffer, 1);
		printf("read data: %c, %d, 0x%02x\n", buffer[0], buffer[0], buffer[0]);
	} else {
		usage(argv[0]);
		
		close(fd);

		return -1;
	}

	close(fd);

	return 0;
}

