#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/signal.h>

#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>


struct key_desc{
	int pin;
	int key_value;
	char *name;
};

static struct key_desc keys[8] = {
	{S5PV210_GPH2(0),	KEY_L,			"KEY1"},
	{S5PV210_GPH2(1),	KEY_S,			"KEY2"},
	{S5PV210_GPH2(2),	KEY_LEFTSHIFT,	"KEY3"},
	{S5PV210_GPH2(3),	KEY_ENTER,		"KEY4"},
	{S5PV210_GPH3(0),	KEY_LEFTCTRL,	"KEY5"},
	{S5PV210_GPH3(1),	KEY_C,			"KEY6"},
	{S5PV210_GPH3(2),	KEY_ESC,		"KEY7"},
	{S5PV210_GPH3(3),	KEY_BACKSPACE,	"KEY8"},
};


static struct input_dev *keys_dev;
static struct timer_list keys_timer;
static struct key_desc *key;

static void tiny210_keys_timer_handler(unsigned long data)
{
	if (!key)
		return;

	if (gpio_get_value(key->pin)) {
		input_event(keys_dev, EV_KEY, key->key_value, 0);
		input_sync(keys_dev);
	} else {
		input_event(keys_dev, EV_KEY, key->key_value, 1);
		input_sync(keys_dev);
	}		       
}

static irqreturn_t tiny210_keys_isr(int irq, void *dev_id)
{
	key = (struct key_desc *)dev_id;

	mod_timer(&keys_timer, jiffies + HZ / 100);//定时时间10ms

	return IRQ_HANDLED;
}

static int __init tiny210_keys_init(void)
{
	int error;
	int i, irq;

	//1.分配input_dev结构体
	keys_dev = input_allocate_device();
	if (!keys_dev) {
		printk("failed to alloc!\n");

		return -ENOMEM;
	}

	//2.设置
	//设置事件类型为按键类型
	set_bit(EV_KEY, keys_dev->evbit);
	set_bit(EV_REP, keys_dev->evbit);//支持重复连按

	//设置具体按键事件，分别对应l,s,Left Shift,Enter,Left Ctrl, c, ESC, Backspace
	set_bit(KEY_L, keys_dev->keybit);
	set_bit(KEY_S, keys_dev->keybit);
	set_bit(KEY_LEFTSHIFT, keys_dev->keybit);
	set_bit(KEY_ENTER, keys_dev->keybit);
	set_bit(KEY_LEFTCTRL, keys_dev->keybit);
	set_bit(KEY_C, keys_dev->keybit);
	set_bit(KEY_ESC, keys_dev->keybit);
	set_bit(KEY_BACKSPACE, keys_dev->keybit);

	//3.注册
	error = input_register_device(keys_dev);
	if (error) {
		printk("Unable to register input device, error: %d\n", error);

		goto free_input_device;
	}
	
	//4.硬件相关代码，如注册中断等
	for (i=0; i<ARRAY_SIZE(keys); i++) {
		if (!keys[i].pin)
			continue;

		irq = gpio_to_irq(keys[i].pin);
		error = request_irq(irq, tiny210_keys_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, keys[i].name, (void *)&keys[i]);
		if (error) {
			    printk("%s(%d) failed to request IRQ: %d,error: %d.\n", __FILE__, __LINE__, irq, error);

				    break;
		}   
	}

	//释放申请的IRQ
	if (error) {
		i--;
		for (; i>=0; i--) {
			if (!keys[i].pin)
				continue;

			irq = gpio_to_irq(keys[i].pin);
			disable_irq(irq);
			free_irq(irq, (void *)&keys[i]);
		}

		error = -EBUSY;

		goto free_input_device;
	}

	//初始化定时器
	init_timer(&keys_timer);
	keys_timer.function = tiny210_keys_timer_handler;
	add_timer(&keys_timer);

	return 0;

free_input_device:
	input_free_device(keys_dev);

	return error;
}

static void __exit tiny210_keys_exit(void)
{
	int i, irq;

	input_unregister_device(keys_dev);
	input_free_device(keys_dev);
	del_timer(&keys_timer);

	for (i=0; i<ARRAY_SIZE(keys); i++) {
		if (!keys[i].pin)
			continue;

		irq = gpio_to_irq(keys[i].pin);
		disable_irq(irq);
		free_irq(irq, (void *)&keys[i]);
	}
}


module_init(tiny210_keys_init);
module_exit(tiny210_keys_exit);

MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 KEYs Input Driver");
MODULE_LICENSE("GPL");

