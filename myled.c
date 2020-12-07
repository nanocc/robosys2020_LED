// SPDX-License-Identifier: GPL-3.0
/*
 *
 * Copyright (c) 2020 Shuhei Horibata and Ryuichi Ueda. All rights reserved.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>

MODULE_AUTHOR("Shuhei Horibata and Ryuichi Ueda");
MODULE_DESCRIPTION("driver for LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
static volatile u32 *gpio_base = NULL;  // アドレスをマッピングするための配列をグローバルで定義

// LEDの点灯，消灯用関数
void led_4(int led, int delay)
{
	gpio_base[led/1000%10*(-3)+10] = 1 << 22;
	gpio_base[led/ 100%10*(-3)+10] = 1 << 23;
	gpio_base[led/  10%10*(-3)+10] = 1 << 24;
	gpio_base[led/   1%10*(-3)+10] = 1 << 25;
	mdelay(delay);
}

// デバイスファイルに書き込みがあったときの挙動
static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos)
{
	char c;
	int n;
	int i;

	// 2進数を保存しておく
	int b[16] = {0, 1, 10, 11, 100, 101, 110, 111, 1000, 1001, 1010, 1011, 1100, 1101, 1110, 1111};

	if(copy_from_user(&c, buf, sizeof(char)))
		return -EFAULT;

        //printk(KERN_INFO "receive %d\n", c);

	// 入力した文字(0,1,2,3,4,5,6,7,8,9,a,b,c,d,e,f)を10進数に変換
	if(c >= '0' && c <= '9')
	{
		n = c - '0';
	}
	else if(c >= 'a' && c <= 'f')
	{
		n = c - 'a' + 10;
	}
	else
	{
		n = -1;
	}

	// 目的の数字まで順にLEDを光らせる
	if(n >= 0)
	{
		for(i=0; i<n+1; i++)
		{
			led_4(b[i], 300+500/n);
		}
	}
	// 全体点滅
	else if(c == 'l')
	{
		for(i=0; i<10; i++)
		{
			led_4(1111, 200);
			led_4(   0, 200);
		}	
	}
	// 螺旋状に点灯
	else if(c == 'p')
	{
		led_4(   0, 100); 
		for(i=0; i<10; i++)
		{
			led_4(1000, 150-i*10);
			led_4( 100, 150-i*10);
			led_4(  10, 150-i*10);
			led_4(   1, 150-i*10);
			led_4(  10, 150-i*10);
			led_4( 100, 150-i*10);
		}
		led_4(   0, 100);
	}

        return 1;
}

static struct file_operations led_fops = {
        .owner = THIS_MODULE,
        .write = led_write
};

// カーネルモジュールの初期化
static int __init init_mod(void)
{
	int i;
	int retval;
	retval =  alloc_chrdev_region(&dev, 0, 1, "myled");
	if(retval < 0)
	{
        	printk(KERN_ERR "alloc_chrdev_region failed.\n");
        	return retval;
    	}
	printk(KERN_INFO "%s is loaded. major:%d\n", __FILE__, MAJOR(dev));

	cdev_init(&cdv, &led_fops);
        retval = cdev_add(&cdv, dev, 1);
        if(retval < 0)
	{
                printk(KERN_ERR "cdev_add failed. major:%d, minor:%d", MAJOR(dev), MINOR(dev));
                return retval;
        }
	
	cls = class_create(THIS_MODULE,"myled");
        if(IS_ERR(cls)){
                printk(KERN_ERR "class_create failed.");
                return PTR_ERR(cls);
        }
	device_create(cls, NULL, dev, NULL, "myled%d", MINOR(dev));

	gpio_base = ioremap_nocache(0xfe200000, 0xA0); // for Pi4

	// GPIOピンを出力にする
	for(i=22; i<26; i++)
	{
		const u32 led = i;
		const u32 index = led/10; // GPFSEL2
		const u32 shift = (led%10)*3; // 15bit
		const u32 mask = ~(0x7 << shift); // 11111111111111000111111111111111
		gpio_base[index] = (gpio_base[index] & mask) | (0x1 << shift); // 001: output flag
	}

	return 0;
}

// 後始末
static void __exit cleanup_mod(void)
{
	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
    	printk(KERN_INFO "%s is unloaded. major:%d\n", __FILE__, MAJOR(dev));
}

// マクロで関数を登録
module_init(init_mod);
module_exit(cleanup_mod);
