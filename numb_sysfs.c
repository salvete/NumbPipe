// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include "numb_sysfs.h"

static struct class *numb_class;

static ssize_t head_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct numb_pipe *pipe = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%zu\n", pipe->head);
}

static ssize_t tail_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct numb_pipe *pipe = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%zu\n", pipe->tail);
}

static DEVICE_ATTR_RO(head);
static DEVICE_ATTR_RO(tail);

static struct attribute *numb_pipe_attrs[] = {
	&dev_attr_head.attr,
	&dev_attr_tail.attr,
	NULL,
};
ATTRIBUTE_GROUPS(numb_pipe);

int numb_sysfs_init(struct numb_pipe *devs, unsigned int count, dev_t base)
{
	int i, ret;
	struct device *dev;

	if (count == 0)
		return -EINVAL;

	numb_class = class_create("numb_pipe");
	if (IS_ERR(numb_class))
		return PTR_ERR(numb_class);

	for (i = 0; i < count; i++) {
		dev = device_create_with_groups(numb_class, NULL, base + i,
				&devs[i], numb_pipe_groups, "numb_pipe%d", i);
		if (IS_ERR(dev)) {
			ret = PTR_ERR(dev);
			goto err_remove;
		}
	}
	return 0;

err_remove:
	for (i--; i >= 0; i--)
		device_destroy(numb_class, base + i);
	class_destroy(numb_class);
	return ret;
}

void numb_sysfs_exit(struct numb_pipe *devs, unsigned int count, dev_t base)
{
	int i;

	if (!numb_class)
		return;

	for (i = 0; i < count; i++)
		device_destroy(numb_class, base + i);
	class_destroy(numb_class);
	numb_class = NULL;
}