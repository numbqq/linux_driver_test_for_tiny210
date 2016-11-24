KERN_DIR = /home/nick/code/nick_git/linux/linux-3.0.80_for_tiny210/linux-3.0.80

all: module app

module:
	make -C $(KERN_DIR) M=`pwd` modules

app:
	arm-linux-gcc test_tiny210_leds.c -o test_tiny210_leds

clean:
		make -C $(KERN_DIR) M=`pwd` modules clean
			rm -rf modules.order
			rm -rf test_tiny210_leds *.o

obj-m	+= tiny210_leds.o
