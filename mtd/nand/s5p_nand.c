#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/err.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <plat/regs-nand.h>
#include <plat/nand.h>

#include <asm/io.h>
#include <asm/sizes.h>
#include <mach/hardware.h>
#include <mach/gpio.h>

 
#define S5P_NAND_BASE		0xB0E00000

/* Nand flash definition values */
#define S5P_NAND_TYPE_UNKNOWN	0x0
#define S5P_NAND_TYPE_SLC	0x1
#define S5P_NAND_TYPE_MLC	0x2

struct s5p_nand_regs {
	unsigned long nfconf;
	unsigned long nfcont;
	unsigned long nfcmmd;
	unsigned long nfaddr;
	unsigned long nfdata;
	unsigned long nfmeccd0;
	unsigned long nfmeccd1;
	unsigned long nfseccd;
	unsigned long nfsblk;
	unsigned long nfeblk;
	unsigned long nfstat;
	unsigned long nfeccerr0;
	unsigned long nfeccerr1;
	unsigned long nfmecc0;
	unsigned long nfmecc1;
	unsigned long nfsecc;
	unsigned long nfmlcbitpt;
};

static int nand_type = S5P_NAND_TYPE_SLC;
static struct mtd_info	*s5p_mtd = NULL;
static struct nand_chip *s5p_nand = NULL;
static struct s5p_nand_regs *s5p_nand_regs;

struct mtd_partition s5p_partition_info[] = {
	{
		.name		= "bootloader",
		.offset		= (0*SZ_1K),          /* for bootloader */
		.size		= (1*SZ_1M),
		.mask_flags	= MTD_CAP_NANDFLASH,
	},
	{
		.name		= "params",
		.offset		= MTDPART_OFS_APPEND,
		.size		= (128*SZ_1K),
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= (5*SZ_1M),
	},
	{
		.name		= "rootfs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= (256*SZ_1M),
	},
	{
		.name		= "system",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	}

};

static void s5p_nand_hwcontrol(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_NCE) {
			if (dat != NAND_CMD_NONE) {
				//select
				s5p_nand_regs->nfcont &= ~(1 << 1);
			}
		} else {
			//deselect
			s5p_nand_regs->nfcont |= (1 << 1);
		}
	}

	if (dat != NAND_CMD_NONE) {
		if (ctrl & NAND_CLE)
			s5p_nand_regs->nfcmmd = dat;
		else if (ctrl & NAND_ALE)
			s5p_nand_regs->nfaddr = dat;
	}
}	


static int s5p_nand_device_ready(struct mtd_info *mtd)
{
	return (s5p_nand_regs->nfstat & (1 << 0));
}

static int s5p_nand_scan_bbt(struct mtd_info *mtd)
{
	return 0;
}

static void s5p_nand_init_later(struct mtd_info *mtd)
{
	u_long nfconf;

	nfconf = s5p_nand_regs->nfconf;

	if (nand_type == S5P_NAND_TYPE_SLC) {
		if (mtd->writesize == 512) {
			nfconf |= (1 << 2);
		} else {
			nfconf &= ~(1 << 2);
		}
	} else {
		nfconf |= (3 << 23) | (1 << 3);
		if (mtd->writesize == 2048) {
			nfconf |= (1 << 2);
		} else {
			nfconf &= ~(1 << 2);
		}
	}

	s5p_nand_regs->nfconf = nfconf;
}


static int __init s5p_nand_init(void)
{
	int err = 0;
	struct clk *clk;

	printk("s5p_nand_init!\n");

	// 1.分配mtd_info和nand_chip结构体
	s5p_mtd = kzalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!s5p_mtd) {
		err = -ENOMEM;
		printk("%s(%d) filed to malloc!\n", __FILE__, __LINE__);

		return err;
	}

	s5p_nand = (struct nand_chip *)(&s5p_mtd[1]);

	//重映射nand寄存器
	s5p_nand_regs = ioremap(S5P_NAND_BASE, sizeof(struct s5p_nand_regs));
	if (!s5p_nand_regs) {
		err = -EINVAL;
		printk("%s(%d) failed to remap io: 0x%08x\n", __FILE__, __LINE__, S5P_NAND_BASE);

		goto err_free_s5p_mtd;
	}

	// 2. 设置nand_chip结构体
	//设置nand_chip是给nand_scan用的
	//它应该提供:选中,发命令,发地址,发数据,读数据,判断状态的功能
	s5p_nand->IO_ADDR_R = &s5p_nand_regs->nfdata;
	s5p_nand->IO_ADDR_W = &s5p_nand_regs->nfdata;
	s5p_nand->cmd_ctrl = s5p_nand_hwcontrol;
	s5p_nand->dev_ready = s5p_nand_device_ready;
	s5p_nand->scan_bbt = s5p_nand_scan_bbt;
	s5p_nand->options = 0;
	s5p_nand->badblockbits = 8;
	s5p_nand->ecc.mode = NAND_ECC_SOFT;

	// 3. 硬件相关设置，根据nandflash手册设置
	//是能nand控制器时钟
	clk = clk_get(NULL, "nand");
	if (IS_ERR(clk)) {
		err = -ENOENT;
		printk("%s(%d) failed to get nand clk!\n", __FILE__, __LINE__);
	
		goto err_iounmap;
	}

	clk_enable(clk);

#define TACLS    1
#define TWRPH0   2
#define TWRPH1   1
	//K9K8G08U0B
	//HCLK=200MHz ==> 5ns
	//Duration = HCLK x TACLS >  (tCLS - tWP) = (12ns - 12ns) = 0 ==> TACLS > 0
	//Duration = HCLK x ( TWRPH0 + 1 ) > tWP = 12ns ==> TWRPH0 + 1 > 2.4 ==> TWRPH0 > 1.4
	//Duration = HCLK x ( TWRPH1 + 1 ) > tCLH = 5ns ==> TWRPH1 > 0
	s5p_nand_regs->nfconf = (TACLS << 12) | (TWRPH0 << 8) | (TWRPH1 << 4);

	//取消片选，是能控制器
	s5p_nand_regs->nfcont = (1 << 1) | (1 << 0);

	// 4. 使用nand_scan
	s5p_mtd->owner = THIS_MODULE;
	s5p_mtd->priv = s5p_nand;

	//识别nand flash，构造mtd_info
	if (nand_scan(s5p_mtd, 1)) {
		err = -ENXIO;
		printk("%s(%d) failed to scan nand!\n", __FILE__, __LINE__);

		goto err_disable_nand_clk;
	}

	//根据nandflash类型设置控制器flash页大小
	s5p_nand_init_later(s5p_mtd);

	//注册mtd设备
	mtd_device_register(s5p_mtd, s5p_partition_info, ARRAY_SIZE(s5p_partition_info));

	return 0;
	
err_disable_nand_clk:
	clk_disable(clk);
err_iounmap:
	iounmap(s5p_nand_regs);
err_free_s5p_mtd:
	kfree(s5p_mtd);

	return err;
}


static void __exit s5p_nand_exit(void)
{
	struct clk *clk;

	printk("s5p_nand_exit!\n");

	mtd_device_unregister(s5p_mtd);
	clk = clk_get(NULL, "nand");
	clk_disable(clk);
	iounmap(s5p_nand_regs);
	kfree(s5p_mtd);
}

module_init(s5p_nand_init);
module_exit(s5p_nand_exit);
MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 Nand Flash MTD Driver");
MODULE_LICENSE("GPL");



