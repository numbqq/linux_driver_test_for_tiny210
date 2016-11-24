/* linux/drivers/char/tiny210_leds.c


   Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>,
*/

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/mutex.h>
#include <linux/nsc_gpio.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/device.h>

#define DEVNAME "tiny210-leds"

MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 LEDs Driver");
MODULE_LICENSE("GPL");

static int major;		/* default to dynamic major */
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");


static struct class *tiny210_leds_class;
static struct class_device *tiny210_leds_class_dev; 

//GPJ2_0/1/2/3为LED引脚

#define GPJ2CON		((volatile unsigned long *)0xE0200280)
#define GPJ2DAT		((volatile unsigned long *)0xE0200284)

//对应虚拟地址
static volatile unsigned long *gpj2con_va, *gpj2dat_va;

static int tiny210_leds_open(struct inode *inode, struct file *file)
{
	unsigned m = iminor(inode);

	//配置为输出
	*gpj2con_va &= ~((0xf << 0) | (0xf << 4) | (0xf << 8) | (0xf << 12));
	*gpj2con_va |= (1 << 0) | (1 << 4) | (1 << 8) | (1 << 12);

	printk("tiny210-leds: open!\n");
	
	return nonseekable_open(inode, file);
}

static ssize_t tiny210_leds_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	int val;

	printk("tiny210-leds: write!\n");

	copy_from_user(&val, buf, count);

	if (val){
		//on
		*gpj2dat_va &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)); 
	} else {
		//off
		*gpj2dat_va |= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
	}

	return 0;
}

static const struct file_operations tiny210_leds_fops = {
	.owner	= THIS_MODULE,
	.open	= tiny210_leds_open,
	.write	= tiny210_leds_write,
};

static struct cdev tiny210_leds_cdev;

static int __init tiny210_leds_init(void)
{
	int rc;
	dev_t devid;

	printk("tiny210-leds: init!\n");

	if (major) {
		devid = MKDEV(major, 0);
		rc = register_chrdev_region(devid, 1, DEVNAME);
	} else {
		rc = alloc_chrdev_region(&devid, 0, 1, DEVNAME);
		major = MAJOR(devid);
	}

	if (rc < 0) {
		printk("register-chrdev failed: %d\n", rc);
		return -EBUSY;
	}
	if (!major) {
		major = rc;
		printk("got dynamic major %d\n", major);
	}


	/* ignore minor errs, and succeed */
	cdev_init(&tiny210_leds_cdev, &tiny210_leds_fops);
	tiny210_leds_cdev.owner = THIS_MODULE;
	cdev_add(&tiny210_leds_cdev, devid, 1);

	tiny210_leds_class = class_create(THIS_MODULE, "tiny210_leds");
	tiny210_leds_class_dev = device_create(tiny210_leds_class, NULL, devid, NULL, "nick_leds");

	//转化为虚拟地址
	gpj2con_va = (volatile unsigned long *)ioremap(GPJ2CON, 4);
	gpj2dat_va = (volatile unsigned long *)ioremap(GPJ2DAT, 4);

	return 0;
}

static void __exit tiny210_leds_cleanup(void)
{
	printk("tiny210-leds: cleanup!\n");

	cdev_del(&tiny210_leds_cdev);
	unregister_chrdev_region(MKDEV(major,0), 1);
	
	device_unregister(tiny210_leds_class_dev);
	class_destroy(tiny210_leds_class);

	iounmap(gpj2con_va);
	iounmap(gpj2dat_va);
}

module_init(tiny210_leds_init);
module_exit(tiny210_leds_cleanup);
