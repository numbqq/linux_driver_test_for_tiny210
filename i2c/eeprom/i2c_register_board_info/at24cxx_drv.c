#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME		"at24cxx"

/* Addresses to scan */
static const unsigned short normal_i2c[] = {0x50, I2C_CLIENT_END };

static const struct i2c_device_id at24cxx_id[] = {
	{DEVICE_NAME, 0},
};

static int major;
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");

static struct cdev at24cxx_cdev;

static struct class *at24cxx_class;
static struct device *at24cxx_class_dev; 

static struct i2c_client *this_client;

//data[0]-address
//data[1]-data to write
//用户空间传来的第一字节为地址，第二字节为要写的数据
static ssize_t at24cxx_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	uint8_t data[2];
	int ret;

	if (2 != count) 
		return -EINVAL;

	if (copy_from_user(data, buf, count))
		return -EFAULT;

	printk("write-> addr: %d, data: %d\n", data[0], data[1]);

	ret = i2c_smbus_write_byte_data(this_client, data[0], data[1]);


	if (!ret)
		return 2;
	else
		return -EIO;
}

//用户空间传来的第一字节为要读的地址
//把独出的数据放在第一字节传到用户空间
static ssize_t at24cxx_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	char addr, data;

	if (1 != size)
		return -EINVAL;

	if (copy_from_user(&addr, buf, size))
		return -EFAULT;

	printk("read-> addr: %d\n", addr);

	data = i2c_smbus_read_byte_data(this_client, addr);
	if (copy_to_user(buf, &data, 1))
		return -EFAULT;

	return 0;
}

static struct file_operations at24cxx_fops = {
	.owner		= THIS_MODULE,
//	.open		= at24cxx_open,
//	.release	= at24cxx_close,
	.write		= at24cxx_write,
	.read		= at24cxx_read
};

static int at24cxx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	dev_t devid;
	int rc;

	printk("at24cxx_probe\n");
	
	this_client = client;

	if (major) {
		devid = MKDEV(major, 0);
		rc = register_chrdev_region(devid, 1, DEVICE_NAME);
	} else {
		rc = alloc_chrdev_region(&devid, 0, 1, DEVICE_NAME);
		major = MAJOR(devid);
	}

	if (rc < 0) {
		printk("%s(%d) failed to register region!error: %d\n", __FILE__, __LINE__, rc);

		return -EBUSY;
	}

	cdev_init(&at24cxx_cdev, &at24cxx_fops);	
	at24cxx_cdev.owner = THIS_MODULE;
	cdev_add(&at24cxx_cdev, devid, 1);

	at24cxx_class = class_create(THIS_MODULE, "nick_eeprom");
	at24cxx_class_dev = device_create(at24cxx_class, NULL, devid, NULL, DEVICE_NAME);

	return 0;
}

static int at24cxx_remove(struct i2c_client *client)
{
	printk("at24cxx_remove\n");

	cdev_del(&at24cxx_cdev);
	unregister_chrdev_region(MKDEV(major,0), 1);
				
	device_unregister(at24cxx_class_dev);
	class_destroy(at24cxx_class);

	return 0;
}

static struct i2c_driver at24cxx_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DEVICE_NAME,
	},
	.probe		= at24cxx_probe,
	.remove		= at24cxx_remove,
	.id_table	= at24cxx_id,
	.address_list	= normal_i2c,
};

static int __init at24cxx_drv_init(void)
{
	printk("at24cxx_drv_init\n");

	return i2c_add_driver(&at24cxx_driver);
}

static void __exit at24cxx_drv_exit(void)
{
	printk("at24cxx_drv_exit\n");
	
	i2c_del_driver(&at24cxx_driver);
}


module_init(at24cxx_drv_init);
module_exit(at24cxx_drv_exit);
MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 EEPROM Driver");
MODULE_LICENSE("GPL");



