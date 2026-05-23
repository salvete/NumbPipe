// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#include "numb_print.h"
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>

#define DEFAULT_PIPE_SIZE	(4 * 1024)
#define MAX_PIPE_SIZE	(4 * 1024 * 1024)
static unsigned int __read_mostly pipe_size = DEFAULT_PIPE_SIZE;
module_param(pipe_size, uint, 0644);
MODULE_PARM_DESC(pipe_size,
	"The size of the internal pipe buffer (defult: 4KB, max: 4MB).");

static unsigned int __read_mostly major = 199;
module_param(major, uint, 0644);
MODULE_PARM_DESC(major, "Major device number (default: 199)");

static unsigned int __read_mostly minor = 1;
module_param(minor, uint, 0644);
MODULE_PARM_DESC(minor, "Minor device count (default: 1)");

struct numb_pipe {
	struct cdev cdev;
	void *buf;
	int head, tail;
};

static struct numb_pipe *devs;

const struct file_operations numb_fops = {

};

static int __init numb_pipe_init(void)
{
	int i, err;

	if (minor > 255) {
		numb_err("Invalid minor count: %u, max: 255.", minor);
		return -EINVAL;
	}

	if (pipe_size > MAX_PIPE_SIZE) {
		numb_err("Invalid pipe size: %u, max: %u", pipe_size,
			  MAX_PIPE_SIZE);
		return -EINVAL;
	}

	devs = kmalloc_array(minor, sizeof(struct numb_pipe), GFP_KERNEL);
	if (!devs)
		return -ENOMEM;

	for (i = 0; i < minor; i++) {
		devs[i].head = devs[i].tail = 0;
		devs[i].buf = kmalloc(pipe_size, GFP_KERNEL);
		if (!devs[i].buf) {
			err = -ENOMEM;
			goto err_devs;
		}
	}

	err = register_chrdev_region(MKDEV(major, 0), minor, "numb_pipe");
	if (err) {
		numb_err("Failed to register char dev region.");
		goto err_devs;
	}

	for (i = 0; i < minor; i++) {
		cdev_init(&devs[i].cdev, &numb_fops);
		cdev_add(&devs[i].cdev, MKDEV(major, i), 1);
	}

	return 0;
err_devs:
	for (; i > 0; i--)
		kfree(devs[i].buf);
	return err;
}

static void __exit numb_pipe_exit(void)
{
	int i;

	for (i = 0; i < minor; i++) {
		kfree(devs[i].buf);
		cdev_del(&devs[i].cdev);
	}
	kfree(devs);
	unregister_chrdev_region(MKDEV(major, 0), minor);
}

module_init(numb_pipe_init);
module_exit(numb_pipe_exit);

MODULE_DESCRIPTION("Numb Pipe");
MODULE_AUTHOR("Hongzhen Luo");
MODULE_LICENSE("GPL");
