KERN_DIR = /home/nick/code/nick_git/linux/linux-3.0.80_for_tiny210/linux-3.0.80

all: module app

module:
	make -C $(KERN_DIR) M=`pwd` modules

app:
	arm-linux-gcc test_at24cxx.c -o test_at24cxx

clean:
		make -C $(KERN_DIR) M=`pwd` modules clean
			rm -rf modules.order *.o test_at24cxx

obj-m	+= at24cxx_drv.o
