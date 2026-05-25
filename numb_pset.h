// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#ifndef __NUMB_PSET_H
#define __NUMB_PSET_H

#include <linux/types.h>

int numb_add_p(pid_t pid);
void numb_remove_p(pid_t pid);
bool numb_contains_p(pid_t pid);
bool numb_pset_test(void);
void numb_pset_exit(void);

#endif
