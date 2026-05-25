#ifndef __NUMB_COMMON_H
#define __NUMB_COMMON_H

#include <linux/cdev.h>

struct numb_pipe {
	struct cdev cdev;
	void *buf;
	size_t len;
	size_t head, tail;
	struct mutex lock;
};

#endif