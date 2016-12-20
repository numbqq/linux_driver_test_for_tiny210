#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>

#define DEVICE_NAME		"at24cxx"

static struct i2c_client *client;

static struct i2c_board_info i2c_board_info = {
		I2C_BOARD_INFO(DEVICE_NAME, 0x50),
};

static int __init at24cxx_dev_init(void)
{
	struct i2c_adapter *i2c_adap;
	int err = 0;

	printk("at24cxx_dev_init\n");

	i2c_adap = i2c_get_adapter(0);
	
	client = i2c_new_device(i2c_adap, &i2c_board_info);
	if (!client) {
		printk("%s(%d) failed to new i2c device!\n", __FILE__, __LINE__);

		err = -ENODEV;
	}

	i2c_put_adapter(i2c_adap);

	return err;
}

static void __exit at24cxx_dev_exit(void)
{
	printk("at24cxx_dev_exit\n");

	i2c_unregister_device(client);
}


module_init(at24cxx_dev_init);
module_exit(at24cxx_dev_exit);
MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 EEPROM Driver");
MODULE_LICENSE("GPL");



