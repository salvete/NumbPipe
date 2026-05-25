// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#include "numb_common.h"
#include "numb_print.h"
#include "numb_sysfs.h"
#include "numb_pset.h"
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/log2.h>

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

static struct numb_pipe *devs;

static ssize_t numb_read_buf(struct numb_pipe *pipe, char __user *user_buf,
			  size_t size, loff_t *offset)
{
	size_t right, read;

	if (size > pipe->head - pipe->tail)
		size = pipe->head - pipe->tail;

	/*
	 * Case (1): (tail before head)
	 * ├─────────────────────────────┤
	 *      ▲                ▲       
	 *      │                │       
	 *     tail             head     
	 * 
	 * Case (2): (head before tail)
	 * ├─────────────────────────────┤
	 *      ▲                ▲       
	 *      │                │       
	 *     head             tail     
	 * 
	 * Always process:
	 * 1. [tail, buffer end]   (from tail to the right end)
	 * 2. [buffer start, head] (from left start up to head)
	 */
	right = min_t(size_t, (pipe->tail & pipe->len) + pipe->len, pipe->head);
	/* Limit the first segment to the requested read size. */
	right = min_t(size_t, right, pipe->tail + size);
	if (copy_to_user(user_buf, pipe->buf + (pipe->tail % pipe->len),
			 right - pipe->tail))
	{
		numb_err("Faild to read segment@[%zu, %zu](size: %zu)\n",
			 pipe->tail, right, pipe->len);
		return -EIO;
	}

	read = right - pipe->tail;
	*offset += read;
	size -= read;
	pipe->tail += read;

	/* We are done. */
	if (!size)
		goto done;

	if (copy_to_user(user_buf + read, pipe->buf, size)) {
		numb_err("Faild to read segment@[%zu, %zu](size: %zu)\n",
			 pipe->tail, pipe->tail + size, pipe->len);
		return -EIO;
	}
	read += size;
	*offset += size;
	pipe->tail += size;
done:
	return (ssize_t)read;
}

static int numb_write_buf(struct numb_pipe *pipe, const char __user *user_buf,
			  size_t size, loff_t *offset)
{
	size_t right, write;

	if (size > pipe->tail + pipe->len - pipe->head)
		size = pipe->tail + pipe->len - pipe->head;

	right = min_t(size_t, (pipe->head & pipe->len) + pipe->len,
			pipe->tail + pipe->len);
	right = min_t(size_t, right, pipe->head + size);
	if (copy_from_user(pipe->buf + (pipe->head % pipe->len), user_buf,
			   right - pipe->head))
	{
		numb_err("Failed to write segment@[%zu, %zu](size: %zu)\n",
			 pipe->head, right, pipe->len);
		return -EIO;
	}

	write = right - pipe->head;
	*offset += write;
	size -= write;
	pipe->head += write;

	if (!size)
		goto done;

	if (copy_from_user(pipe->buf, user_buf + write, size)) {
		numb_err("Failed to write segment@[%zu, %zu](size: %zu)\n",
			 pipe->head, pipe->head + size, pipe->len);
		return -EIO;
	}
	write += size;
	*offset += size;
	pipe->head += size;
done:
	return (ssize_t)write;
}

static int numb_open(struct inode *inode, struct file *file)
{
	struct numb_pipe *pipe;

	pipe = container_of(inode->i_cdev, struct numb_pipe, cdev);
	file->private_data = pipe;
	return 0;
}

static int numb_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static ssize_t numb_read(struct file *file, char __user *user_buf, size_t size,
			 loff_t *offset)
{
	struct numb_pipe *pipe = file->private_data;
	size_t to_read;
	ssize_t err = -EAGAIN;

	mutex_lock(&pipe->lock);
	to_read = min_t(size_t, pipe->head - pipe->tail, size);
	if (to_read)
		err = numb_read_buf(pipe, user_buf, to_read, offset);
	mutex_unlock(&pipe->lock);

	return err;
}

static ssize_t numb_write(struct file *file, const char __user *user_buf,
			  size_t size, loff_t *offset)
{
	struct numb_pipe *pipe = file->private_data;
	size_t to_write;
	ssize_t err = -EAGAIN;

	mutex_lock(&pipe->lock);
	to_write = min_t(size_t, pipe->tail + pipe->len - pipe->head,
			 size);
	if (to_write)
		err = numb_write_buf(pipe, user_buf, size, offset);
	mutex_unlock(&pipe->lock);

	return err;
}

const struct file_operations numb_fops = {
	.open		= numb_open,
	.release	= numb_release,
	.read		= numb_read,
	.write		= numb_write,
};

static int __init numb_pipe_init(void)
{
	int i, err;

	if (minor > 255) {
		numb_err("Invalid minor count: %u, max: 255.", minor);
		return -EINVAL;
	}

	if (pipe_size > MAX_PIPE_SIZE || !is_power_of_2(pipe_size)) {
		numb_err("Invalid pipe size or is not power of 2: %u, max: %u",
			 pipe_size, MAX_PIPE_SIZE);
		return -EINVAL;
	}

	if (!numb_pset_test()) {
		numb_err("Failed to pass the pset tests.");
		return -EINVAL;
	}

	devs = kcalloc(minor, sizeof(struct numb_pipe), GFP_KERNEL);
	if (!devs)
		return -ENOMEM;

	for (i = 0; i < minor; i++) {
		devs[i].head = devs[i].tail = 0;
		devs[i].buf = kmalloc(pipe_size, GFP_KERNEL);
		devs[i].len = pipe_size;
		if (!devs[i].buf) {
			err = -ENOMEM;
			goto err_devs;
		}
		mutex_init(&devs[i].lock);
	}

	err = register_chrdev_region(MKDEV(major, 0), minor, "numb_pipe");
	if (err) {
		numb_err("Failed to register char dev region.");
		goto err_devs;
	}

	err = numb_sysfs_init(devs, minor, MKDEV(major, 0));
	if (err) {
		numb_err("Failed to init sysfs.");
		goto err_unregister;
	}

	for (i = 0; i < minor; i++) {
		cdev_init(&devs[i].cdev, &numb_fops);
		err = cdev_add(&devs[i].cdev, MKDEV(major, i), 1);
		if (err) {
			numb_err("Failed to add cdev for minor %d.", i);
			for (; i >= 0; i--)
				cdev_del(&devs[i].cdev);
			numb_sysfs_exit(devs, minor, MKDEV(major, 0));
			goto err_unregister;
		}
	}

	return 0;
err_unregister:
	unregister_chrdev_region(MKDEV(major, 0), minor);
err_devs:
	for (i = 0; i < minor; i++)
		kfree(devs[i].buf);
	return err;
}

static void __exit numb_pipe_exit(void)
{
	int i;

	for (i = 0; i < minor; i++)
		cdev_del(&devs[i].cdev);

	numb_sysfs_exit(devs, minor, MKDEV(major, 0));

	unregister_chrdev_region(MKDEV(major, 0), minor);

	for (i = 0; i < minor; i++)
		kfree(devs[i].buf);
	kfree(devs);

	numb_pset_exit();
}

module_init(numb_pipe_init);
module_exit(numb_pipe_exit);

MODULE_DESCRIPTION("Numb Pipe");
MODULE_AUTHOR("Hongzhen Luo");
MODULE_LICENSE("GPL");
