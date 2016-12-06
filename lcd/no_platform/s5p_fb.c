#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <plat/fb.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>

//gpio
#define GPF0CON			(0xE0200120)
#define GPF1CON			(0xE0200140)
#define GPF2CON			(0xE0200160)
#define GPF3CON			(0xE0200180)

#define GPD0CON			(0xE02000A0)
#define GPD0DAT			(0xE02000A4)
#define GPD0PUD			(0xE02000A8)

//lcd
#define VIDCON0			(0xF8000000)
#define VIDCON1			(0xF8000004)
#define VIDTCON2		(0xF8000018)
#define WINCON0 		(0xF8000020)
#define WINCON2 		(0xF8000028)
#define SHADOWCON 		(0xF8000034)
#define VIDOSD0A 		(0xF8000040)
#define VIDOSD0B 		(0xF8000044)
#define VIDOSD0C 		(0xF8000048)

#define VIDW00ADD0B0 	(0xF80000A0)
#define VIDW00ADD1B0 	(0xF80000D0)

#define VIDTCON0 		(0xF8000010)
#define VIDTCON1 		(0xF8000014)


//clock regs
#define DISPLAY_CONTROL		(0xe0107008)


////根据lcd datasheet设置
#define HSPW 			(0)
#define HBPD			(46 -1)
#define HFPD 			(210 -1)
#define VSPW			(0)
#define VBPD 			(23 - 1)
#define VFPD 			(22 - 1)

#define ROW				(480)
#define COL				(800)
#define HOZVAL			(COL - 1)
#define LINEVAL			(ROW - 1)

#define LeftTopX     0
#define LeftTopY     0
#define RightBotX   799
#define RightBotY   479

#define MHZ (1000*1000)
#define PRINT_MHZ(m) 			((m) / MHZ), ((m / 1000) % 1000)


static struct fb_info *tiny210_fbinfo;

//gpio regs
static volatile unsigned long *gpf0con;
static volatile unsigned long *gpf1con;
static volatile unsigned long *gpf2con;
static volatile unsigned long *gpf3con;
static volatile unsigned long *gpd0con;
static volatile unsigned long *gpd0dat;
static volatile unsigned long *gpd0pud;

//lcd regs
static volatile unsigned long *vidcon0;
static volatile unsigned long *vidcon1;
static volatile unsigned long *vidtcon2;
static volatile unsigned long *wincon0;
static volatile unsigned long *wincon2;
static volatile unsigned long *shadowcon;
static volatile unsigned long *vidosd0a;
static volatile unsigned long *vidosd0b;
static volatile unsigned long *vidosd0c;
static volatile unsigned long *vidw00add0b0;
static volatile unsigned long *vidw00add1b0;
static volatile unsigned long *vidtcon0;
static volatile unsigned long *vidtcon1;


//clock regs
static volatile unsigned long *display_control;

static u32 pseudo_pal[16];

static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int s5p_fb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info)
{
	unsigned int val;
	
	if (regno > 16)
		return 1;

	/* 用red,green,blue三原色构造出val */
	val  = chan_to_field(red,	&info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,	&info->var.blue);
	
	//((u32 *)(info->pseudo_palette))[regno] = val;
	pseudo_pal[regno] = val;
	return 0;
}

static struct fb_ops s5p_fb_ops = {
	.owner			= THIS_MODULE,
	.fb_setcolreg	= s5p_fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static void tiny210_remap_regs(void)
{
	//remap gpio regs
	gpf0con = ioremap(GPF0CON, 4);
	gpf1con = ioremap(GPF1CON, 4);
	gpf2con = ioremap(GPF2CON, 4);
	gpf3con = ioremap(GPF3CON, 4);

	gpd0con = ioremap(GPD0CON, 4);
	gpd0dat = ioremap(GPD0DAT, 4);
	gpd0pud = ioremap(GPD0PUD, 4);

	//remap lcd regs
	vidcon0 = ioremap(VIDCON0, 4);
	vidcon1 = ioremap(VIDCON1, 4);
	vidtcon2 = ioremap(VIDTCON2, 4);
	wincon0 = ioremap(WINCON0, 4);
	wincon2 = ioremap(WINCON2, 4);
	shadowcon = ioremap(SHADOWCON, 4);
	vidosd0a = ioremap(VIDOSD0A, 4);
	vidosd0b = ioremap(VIDOSD0B, 4);
	vidosd0c = ioremap(VIDOSD0C, 4);
	vidw00add0b0 = ioremap(VIDW00ADD0B0, 4);
	vidw00add1b0 = ioremap(VIDW00ADD1B0, 4);
	vidtcon0 = ioremap(VIDTCON0, 4);
	vidtcon1 = ioremap(VIDTCON1, 4);

	//remap clock regs
	display_control = ioremap(DISPLAY_CONTROL, 4);
}

static void tiny210_unmap_regs(void)
{
	//unmap gpio regs
	iounmap(gpf0con);
	iounmap(gpf1con);
	iounmap(gpf2con);
	iounmap(gpf3con);

	iounmap(gpd0con);
	iounmap(gpd0dat);

	//unmap lcd regs
	iounmap(vidcon0);
	iounmap(vidcon1);
	iounmap(vidtcon2);
	iounmap(wincon0);
	iounmap(wincon2);
	iounmap(shadowcon);
	iounmap(vidosd0a);
	iounmap(vidosd0b);
	iounmap(vidosd0c);
	iounmap(vidw00add0b0);
	iounmap(vidw00add1b0);
	iounmap(vidtcon0);
	iounmap(vidtcon1);

	//unmap clock regs	
	iounmap(display_control);
}


static int __init tiny210_lcdfb_init(void)
{
	int err;
	struct clk	*tiny210_clk;

	printk("tiny210 lcd init!\n");

	// 1、分配一个fb_info结构
	tiny210_fbinfo = framebuffer_alloc(0, NULL);

	if (!tiny210_fbinfo) {
		printk("failed to alloc framebuffer!\n");

		return -ENOMEM;
	}
	
	// 2、设置
	// 2.1 设置固定参数
	strcpy(tiny210_fbinfo->fix.id, "tiny210_lcd");
	tiny210_fbinfo->fix.smem_len = COL * ROW * 4;//显存大小bytes，24bpp
	tiny210_fbinfo->fix.type = FB_TYPE_PACKED_PIXELS;
	tiny210_fbinfo->fix.visual = FB_VISUAL_TRUECOLOR;//TFT屏幕为真彩色
	tiny210_fbinfo->fix.line_length = COL * 4;//24bpp
	
	//tiny210_fbinfo->fix.accel = FB_ACCEL_NONE;
	

	// 2.2 设置可变参数
	tiny210_fbinfo->var.xres = COL;
	tiny210_fbinfo->var.yres = ROW;
	tiny210_fbinfo->var.xres_virtual = COL;
	tiny210_fbinfo->var.yres_virtual = ROW;
	tiny210_fbinfo->var.xoffset = 0;
	tiny210_fbinfo->var.yoffset = 0;
	tiny210_fbinfo->var.bits_per_pixel = 32;

	// RGB:888
	tiny210_fbinfo->var.red.length 		= 8;
	tiny210_fbinfo->var.red.offset 		= 16;
	tiny210_fbinfo->var.green.length 	= 8;
	tiny210_fbinfo->var.green.offset 	= 8;
	tiny210_fbinfo->var.blue.length 	= 8;
	tiny210_fbinfo->var.blue.offset 	= 0;
	
	tiny210_fbinfo->var.activate = FB_ACTIVATE_NOW;
//	tiny210_fbinfo->var.vmode = FB_VMODE_NONINTERLACED;
	
	// 2.3 设置操作函数
	tiny210_fbinfo->fbops = &s5p_fb_ops;

	// 2.4 其他的设置
//	tiny210_fbinfo->flags = FBINFO_FLAG_DEFAULT;
	tiny210_fbinfo->pseudo_palette = pseudo_pal;
	tiny210_fbinfo->screen_size = COL * ROW * 4;//显存大小

	// 3、硬件相关的操作
	// 3.1 配置GPIO用于lcd
	tiny210_remap_regs();

	//配置引脚用于lcd
	*gpf0con = 0x22222222;
	*gpf1con = 0x22222222;
	*gpf2con = 0x22222222;
	*gpf3con = 0x22222222;

	//配置背光管脚
	*gpd0con &= (0xf << 4);
	*gpd0con |= (1 << 4);
//	*gpd0dat |= (1<<1);

	// 3.2 使能时钟
	//这里一定要打开lcd时钟，因为系统在初始化时默认是把lcd始终关闭的！
	tiny210_clk = clk_get(NULL, "lcd");
	if (!tiny210_clk || IS_ERR(tiny210_clk)) {
		printk(KERN_INFO "failed to get lcd clock source\n");
	}
	//使能时钟，实际上就是设置寄存器CLK_GATE_IP1 bit0为1
	clk_enable(tiny210_clk);
	printk("Tiny210 LCD clock got enabled :: %ld.%03ld Mhz\n", PRINT_MHZ(clk_get_rate(tiny210_clk)));

	// 3.3 根据手册设置lcd控制器，比如VCLOCK频率，HSPW等参数
	//10: RGB=FIMD I80=FIMD ITU=FIMD
	*display_control = (2 << 0);
	
	// bit[26~28]:使用RGB接口
	// bit[18]:RGB 并行
	// bit[2]:选择时钟源为HCLK_DSYS=166MHz
	*vidcon0 &= ~((3 << 26) | (1 << 18) | (1 << 2));

	// bit[1]:使能lcd控制器
	// bit[0]:当前帧结束后使能lcd控制器
	*vidcon0 |= ((1 << 1));

	// bit[6]:选择需要分频
	// bit[6~13]:分频系数为15，即VCLK = 166M/(4+1) = 33.2M，根据LCD datasheet DCLK Frequency设置
	*vidcon0 |= ((4 << 6) | (1 << 4));

	// S700-AT070TN92.pdf(p13) 时序图：VSYNC和HSYNC都是低脉冲
	// s5pv210芯片手册(p1207) 时序图：VSYNC和HSYNC都是高脉冲有效，所以需要反转
	*vidcon1 |= ((1 << 5) | (1 << 6));
	
	// 设置时序
	*vidtcon0 = ((VBPD << 16) | (VFPD << 8) | (VSPW << 0));
	*vidtcon1 = ((HBPD << 16) | (HFPD << 8) | (HSPW << 0));
	// 设置长宽
	*vidtcon2 = ((LINEVAL << 11) | (HOZVAL << 0));

	// 设置window0
	// bit[0]:使能
	// bit[2~5]:24bpp
	//*wincon0 |= (1 << 0);
	*wincon0 &= ~(0xf << 2);
	*wincon0 |= ((0xB << 2) | (1 << 15));

	
	// 设置window0的上下左右
	*vidosd0a = ((LeftTopX << 11) | (LeftTopY << 0));
	*vidosd0b = ((RightBotX << 11) | (RightBotY << 0));
	*vidosd0c = ((LINEVAL + 1) * (HOZVAL + 1));

	// 3.3 分配显存(framebuffer)，并把地址告诉lcd控制器
	tiny210_fbinfo->screen_base = dma_alloc_writecombine(NULL, PAGE_ALIGN(tiny210_fbinfo->fix.smem_len), 
															(dma_addr_t *)&tiny210_fbinfo->fix.smem_start, GFP_KERNEL);
	if (tiny210_fbinfo->screen_base) {
		/* prevent initial garbage on screen */
		printk("map_video_memory: clear %p:%08x\n",
			tiny210_fbinfo->screen_base, tiny210_fbinfo->fix.smem_len);
		memset(tiny210_fbinfo->screen_base, 0x00, tiny210_fbinfo->fix.smem_len);

		printk("map_video_memory: dma=%08lx cpu=%p size=%08x\n",
			tiny210_fbinfo->fix.smem_start, tiny210_fbinfo->screen_base, tiny210_fbinfo->fix.smem_len);
	} else {
		printk(KERN_ERR "unable to allocate screen memory\n");
		
		err =  -ENOMEM;
		goto unmap_regs;
	}

	// 设置fb的地址
	*vidw00add0b0 = tiny210_fbinfo->fix.smem_start;
	*vidw00add1b0 = tiny210_fbinfo->fix.smem_start + tiny210_fbinfo->fix.smem_len;

	//使能lcd控制器
	*vidcon0 |= (1 << 0);
	*wincon0 |= (1 << 0);
	// 使能channel 0传输数据
	*shadowcon = 0x1;
	//打开背光
	*gpd0dat |= (1 << 1);

	// 4、注册
	err = register_framebuffer(tiny210_fbinfo);
	if (err < 0) {
		pr_err("unable to register framebuffer\n");

		err = -EINVAL;
		goto free_dma_buffer;
	}

	return 0;

free_dma_buffer:
	dma_free_writecombine(NULL, tiny210_fbinfo->fix.smem_len, tiny210_fbinfo->screen_base, tiny210_fbinfo->fix.smem_start);
	//关闭lcd控制器
	*vidcon0 &= ~(1 << 0);
	*wincon0 &= ~(1 << 0);
	// 关闭channel 0传输数据
	*shadowcon &= ~0x1;
	//关闭背光
	*gpd0dat &= ~(1 << 1);
unmap_regs:
	tiny210_unmap_regs();
	framebuffer_release(tiny210_fbinfo);

	return err;
}

static void __exit tiny210_lcdfb_exit(void)
{
	struct clk *tiny210_clk;
	
	unregister_framebuffer(tiny210_fbinfo);
	dma_free_writecombine(NULL, tiny210_fbinfo->fix.smem_len, tiny210_fbinfo->screen_base, tiny210_fbinfo->fix.smem_start);
	
	//关闭时钟
	tiny210_clk = clk_get(NULL, "lcd");
	if (!tiny210_clk || IS_ERR(tiny210_clk)) {
		printk(KERN_INFO "failed to get lcd clock source\n");
	}
	clk_enable(tiny210_clk);

	//关闭lcd控制器
	*vidcon0 &= ~(1 << 0);
	*wincon0 &= ~(1 << 0);
	// 关闭channel 0传输数据
	*shadowcon &= ~0x1;
	//关闭背光
	*gpd0dat &= ~(1 << 1);
	tiny210_unmap_regs();
	framebuffer_release(tiny210_fbinfo);
}

module_init(tiny210_lcdfb_init);
module_exit(tiny210_lcdfb_exit);

MODULE_AUTHOR("Nick <hbjsxieqi@163.com>");
MODULE_DESCRIPTION("TINY210 LCD Driver");
MODULE_LICENSE("GPL");


