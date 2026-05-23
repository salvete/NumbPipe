// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#ifndef __NUMB_PRINT_H
#define __NUMB_PRINT_H

#define numb_err(fmt, ...) \
	printk(KERN_ERR "[Numb Pipe] " fmt, ##__VA_ARGS__)
#define numb_info(fmt, ...) \
	printk(KERN_INFO "[Numb Pipe] " fmt, ##__VA_ARGS__)

#endif
