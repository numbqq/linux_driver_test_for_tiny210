#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/hdreg.h>


#define DEVICE_NAME			"ramblock"
#define SECTOR_SIZE			(512)
#define RAMBLOCK_SIZE		(1024*1024)	
#define RAMBLOCK_HEADS		(2)		//磁头数
#define RAMBLOCK_SECTORS	(128)	//每个磁道扇区数
#define RAMBLOCK_CYLINDERS	(RAMBLOCK_SIZE/RAMBLOCK_HEADS/RAMBLOCK_SECTORS/SECTOR_SIZE)		

static int major;
static struct gendisk *ramblock_gendisk;
static struct request_queue *ramblock_queue;
static DEFINE_SPINLOCK(ramblock_lock);

static char *ramblock_buffer;

static int ramblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	//容量=磁头数×磁道数（柱面数）×每个磁道扇区数×每扇区字节数（SECTOR_SIZE）
	geo->heads = (unsigned char)RAMBLOCK_HEADS;
	geo->sectors = RAMBLOCK_SECTORS;
	geo->cylinders = RAMBLOCK_CYLINDERS;
	
	return 0;
}

static const struct block_device_operations ramblock_fops =
{
	.owner		= THIS_MODULE,
	.getgeo		= ramblock_getgeo,	//获取磁头数、扇区数和柱面数，为了兼容老的命令，如：fdisk
};

static void do_ramblock_request(struct request_queue *q)
{
	struct request *req;

//	static int w_count = 0, r_count = 0;

	req = blk_fetch_request(q);
	
	while (req) {
		unsigned offset = blk_rq_pos(req) << 9;//扇区转换为字节数偏移
		unsigned len = blk_rq_cur_bytes(req);//当前请求的字节数
		int error = 0;

		if (offset + len > RAMBLOCK_SIZE) {
			printk(DEVICE_NAME ": bad access: block=%llu, count=%u\n", (unsigned long long)blk_rq_pos(req), blk_rq_cur_sectors(req));
			error = -EIO;

			goto done;
		}

		//如果是具体硬件设备，则在此次是要进行硬件读写操作。
		if (READ == rq_data_dir(req)) {
//			printk("do_ramblock_request read %d\n", ++r_count);
			memcpy(req->buffer, ramblock_buffer + offset, len);
		} else {
//			printk("do_ramblock_request write %d\n", ++w_count);
			memcpy(ramblock_buffer + offset, req->buffer, len);
		}
		
	done:
		if (!__blk_end_request_cur(req, error))
			req = blk_fetch_request(q);
	}
}

static int __init ramblock_init(void)
{
	int error;

	printk(DEVICE_NAME ": init!\n");

	major = register_blkdev(0, DEVICE_NAME);//自动分配主设备号，这个注册函数功能已经退化，仅仅是分配主设备号和提供cat /proc/devices信息内容
	if (major < 0) {
		error = -EBUSY;
		printk("%s(%d) failed to register blkdev!error: %d\n", __FILE__, __LINE__, error);
		
		goto err;
	}

	// 1. 分配gendisk
	ramblock_gendisk = alloc_disk(16);//最多可分为15个分区
	if (!ramblock_gendisk) {
		error = -ENOMEM;
		printk("%s(%d) failed to alloc disk!error: %d\n", __FILE__, __LINE__, error);
	
		goto err_unregister_blkdev;
	}

	// 2. 设置
	// 2.1 分配设置请求队列request_queue_t，它提供读写能力
	ramblock_queue = blk_init_queue(do_ramblock_request, &ramblock_lock);
	if (!ramblock_queue) {
		error = -ENOMEM;
		printk("%s(%d) failed init queue!error: %d\n", __FILE__, __LINE__, error);

		goto err_put_disk;
	}
	// 2.2 设置gendisk其他信息，它提供属性，如：容量
	ramblock_gendisk->major = major;
	ramblock_gendisk->first_minor = 0;
	ramblock_gendisk->fops = &ramblock_fops;
	sprintf(ramblock_gendisk->disk_name, DEVICE_NAME);
	ramblock_gendisk->queue = ramblock_queue;
	set_capacity(ramblock_gendisk, RAMBLOCK_SIZE / SECTOR_SIZE);

	// 3. 硬件相关操作
	// 给ramblock分配内存
	ramblock_buffer = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	if (!ramblock_buffer) {
		error = -ENOMEM;
		printk("%s(%d) failed to malloc!error: %d\n", __FILE__, __LINE__, error);

		goto err_cleanup_queue;
	}


	// 4. 注册：add_disk
	add_disk(ramblock_gendisk);

	return 0;

err_cleanup_queue:
	blk_cleanup_queue(ramblock_queue);
err_put_disk:
	put_disk(ramblock_gendisk);
err_unregister_blkdev:
	unregister_blkdev(major, DEVICE_NAME);
err:
	return error;
}

static void __exit ramblock_exit(void)
{
	printk(DEVICE_NAME ": exit!\n");

	unregister_blkdev(major, DEVICE_NAME);
	del_gendisk(ramblock_gendisk);
	put_disk(ramblock_gendisk);
	blk_cleanup_queue(ramblock_queue);
	kfree(ramblock_buffer);
}

module_init(ramblock_init);
module_exit(ramblock_exit);
MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 Ramblock Block Device Driver");
MODULE_LICENSE("GPL");



