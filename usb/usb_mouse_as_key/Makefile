KERN_DIR = /home/nick/code/nick_git/linux/linux-3.0.80_for_tiny210/linux-3.0.80

all: module

module:
	make -C $(KERN_DIR) M=`pwd` modules


clean:
		make -C $(KERN_DIR) M=`pwd` modules clean
			rm -rf modules.order

obj-m	+= usbmouse_as_key.o
