KERN_DIR = /home/nick/code/nick_git/linux/linux-3.0.80_for_tiny210/linux-3.0.80

all: module app

module:
	make -C $(KERN_DIR) M=`pwd` modules

app:
	cd ./fb_test;make;cd ..

clean:
		make -C $(KERN_DIR) M=`pwd` modules clean
			rm -rf modules.order
			rm -rf ./fb_test/*.o ./fb_test/fb_test

obj-m	+= s5p_fb.o
