#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct input_dev *uk_dev;
static char *usb_buf;
static dma_addr_t usb_buf_phys;
static struct urb *uk_urb;
static int len;

static void usbmouse_as_key_callback(struct urb *urb)
{
	static int pre_val = 0;
	
	//usb鼠标数据含义
	//data[0]:	bit[0]-左键，1-按下，0-松开
	//			bit[1]-右键，1-按下，0-松开
	//			bit[2]-中键，1-按下，0-松开

	//左键状态改变	
	if ((pre_val & (1 << 0)) != (usb_buf[0] & (1 << 0))) {
		input_report_key(uk_dev, KEY_L, (usb_buf[0] & (1 << 0)) ? 1 : 0);
		input_sync(uk_dev);
	}

	//右键状态改变
	if ((pre_val & (1 << 1)) != (usb_buf[0] & (1 << 1))) {
		input_report_key(uk_dev, KEY_S, (usb_buf[0] & (1 << 1)) ? 1 : 0);
		input_sync(uk_dev);
	}
	
	//中键状态改变
	if ((pre_val & (1 << 2)) != (usb_buf[0] & (1 << 2))) {
		input_report_key(uk_dev, KEY_ENTER, (usb_buf[0] & (1 << 2)) ? 1 : 0);
		input_sync(uk_dev);
	}

	pre_val = usb_buf[0];

	//***注意此处一定要使用标志GFP_ATOMIC，否则会产生BUG: sleeping function called from invalid context at mm/dmapool.c:309
	//的错误。
	usb_submit_urb(uk_urb, GFP_ATOMIC);
}

static int usb_mouse_as_key_open(struct input_dev *dev)
{
	//使用三要素	
	if (usb_submit_urb(uk_urb, GFP_KERNEL))
		return -EIO;

	return 0;
}

static void usb_mouse_as_key_close(struct input_dev *dev)
{
	usb_kill_urb(uk_urb);
}

static int usb_mouse_as_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);	
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	int error;
	int pipe;

	interface = intf->cur_altsetting;

	endpoint = &interface->endpoint[0].desc;

	// a.分配一个input_dev
	uk_dev = input_allocate_device();

	if (!uk_dev) {
		printk("failed to allocate input dev!\n");

		return -ENOMEM;
	}

	// b. 设置
	// b.1 设置能产生哪类事件	
	set_bit(EV_KEY, uk_dev->evbit);//按键事件
	set_bit(EV_REP, uk_dev->evbit);//重复事件

	// b.2 设置能产生哪些事件
	set_bit(KEY_L, uk_dev->keybit);
	set_bit(KEY_S, uk_dev->keybit);
	set_bit(KEY_ENTER, uk_dev->keybit);

	// c. 硬件相关操作
	// 数据传输三要素：源， 目的，长度
	// 源：usb设备的某个端点
	pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);

	//长度：端点最大数据长度
	len = endpoint->wMaxPacketSize;

	//目的：
	usb_buf = usb_alloc_coherent(dev, len, GFP_ATOMIC, &usb_buf_phys);
	if (!usb_buf) {
		error = -ENOMEM;

		printk("failed to alloc usb buffer!\n");

		goto free_input_dev;
	}

	//使用三要素
	//分配urb（usb request block）
	uk_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!uk_urb) {
		error = -ENOMEM;

		printk("failed to alloc urb!\n");

		goto free_usb_buffer;
	}

	uk_dev->open = usb_mouse_as_key_open;
	uk_dev->close = usb_mouse_as_key_close;

	usb_fill_int_urb(uk_urb, dev, pipe, usb_buf, len, usbmouse_as_key_callback, NULL, endpoint->bInterval);
	uk_urb->transfer_dma = usb_buf_phys;
	uk_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	
	// d. 注册input dev
	error = input_register_device(uk_dev);
	if (error) {
		printk("Unable to register input device, error: %d\n", error);
		
		goto free_usb_urb;
	}
	
	return 0;
	
free_usb_urb:
	usb_free_urb(uk_urb);
free_usb_buffer:
	usb_free_coherent(dev, len, usb_buf, usb_buf_phys);
free_input_dev:
	input_free_device(uk_dev);

	return error;
}

static void usb_mouse_as_key_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);	

	usb_kill_urb(uk_urb);
	usb_free_urb(uk_urb);
	usb_free_coherent(dev, len, usb_buf, usb_buf_phys);
	input_unregister_device(uk_dev);
	input_free_device(uk_dev);
}


static struct usb_device_id usb_mouse_as_key_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }	/* Terminating entry */
};

static struct usb_driver usb_mouse_as_key_driver = {
	.name		= "usb_mouse_as_key",
	.probe		= usb_mouse_as_key_probe,
	.disconnect	= usb_mouse_as_key_disconnect,
	.id_table	= usb_mouse_as_key_id_table,
};

static int __init usbmouse_as_key_init(void)
{
	int retval = usb_register(&usb_mouse_as_key_driver);
	if (retval == 0)
		printk(KERN_INFO "usbmouse as key init!\n");
		
	return retval;	
}

static void __exit usbmouse_as_key_exit(void)
{
	usb_deregister(&usb_mouse_as_key_driver);
}

module_init(usbmouse_as_key_init);
module_exit(usbmouse_as_key_exit);
MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 USB HID Driver");
MODULE_LICENSE("GPL");

