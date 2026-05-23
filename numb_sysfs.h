#ifndef __NUMB_SYSFS_H
#define __NUMB_SYSFS_H

#include "numb_common.h"
#include <linux/types.h>

int numb_sysfs_init(struct numb_pipe *devs, unsigned int count, dev_t base);
void numb_sysfs_exit(struct numb_pipe *devs, unsigned int count, dev_t base);

#endif