// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026, Hongzhen Luo
 */

#include "numb_print.h"
#include "numb_pset.h"
#include <linux/xarray.h>

static DEFINE_XARRAY(numb_pset);
static char dummy;

int numb_add_p(pid_t pid)
{
	return xa_err(xa_store(&numb_pset, pid, &dummy,
			GFP_KERNEL));
}

void numb_remove_p(pid_t pid)
{
	xa_erase(&numb_pset, pid);
}

bool numb_contains_p(pid_t pid)
{
	return xa_load(&numb_pset, pid) == &dummy;
}

void numb_pset_exit(void)
{
    xa_destroy(&numb_pset);
}

bool numb_pset_test(void)
{
	unsigned long index;
	void *entry;
	int i;
	bool pass = true;

	if (numb_contains_p(1)) {
		numb_err("numb_pset_test: FAIL - set should be empty initially\n");
		pass = false;
		goto out;
	}

	if (numb_add_p(1) != 0) {
		numb_err("numb_pset_test: FAIL - add PID 1\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(1)) {
		numb_err("numb_pset_test: FAIL - PID 1 not found after add\n");
		pass = false;
		goto out;
	}

	if (numb_add_p(1) != 0) {
		numb_err("numb_pset_test: FAIL - duplicate add PID 1\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(1)) {
		numb_err("numb_pset_test: FAIL - PID 1 lost after duplicate add\n");
		pass = false;
		goto out;
	}

	if (numb_add_p(100) != 0) {
		numb_err("numb_pset_test: FAIL - add PID 100\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(1) || !numb_contains_p(100)) {
		numb_err("numb_pset_test: FAIL - both PIDs should exist\n");
		pass = false;
		goto out;
	}

	numb_remove_p(1);
	if (numb_contains_p(1)) {
		numb_err("numb_pset_test: FAIL - PID 1 should have been removed\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(100)) {
		numb_err("numb_pset_test: FAIL - PID 100 should still exist\n");
		pass = false;
		goto out;
	}

	numb_remove_p(9999);
	if (numb_contains_p(9999)) {
		numb_err("numb_pset_test: FAIL - non-existent PID should not appear\n");
		pass = false;
		goto out;
	}

	if (numb_add_p(0) != 0) {
		numb_err("numb_pset_test: FAIL - add PID 0\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(0)) {
		numb_err("numb_pset_test: FAIL - PID 0 not found\n");
		pass = false;
		goto out;
	}
	if (numb_add_p(32767) != 0) {
		numb_err("numb_pset_test: FAIL - add large PID\n");
		pass = false;
		goto out;
	}
	if (!numb_contains_p(32767)) {
		numb_err("numb_pset_test: FAIL - large PID not found\n");
		pass = false;
		goto out;
	}
	numb_remove_p(0);
	numb_remove_p(1);
	numb_remove_p(100);
	numb_remove_p(32767);

	#define BULK_START 10
	#define BULK_NUM   500
	for (i = BULK_START; i < BULK_START + BULK_NUM; i++) {
		if (numb_add_p(i) != 0) {
			numb_err("numb_pset_test: FAIL - bulk add at %d\n", i);
			pass = false;
			goto out;
		}
	}
	for (i = BULK_START; i < BULK_START + BULK_NUM; i++) {
		if (!numb_contains_p(i)) {
			numb_err("numb_pset_test: FAIL - bulk PID %d not found\n", i);
			pass = false;
			goto out;
		}
	}

	for (i = BULK_START; i < BULK_START + BULK_NUM; i += 2)
		numb_remove_p(i);
	for (i = BULK_START; i < BULK_START + BULK_NUM; i++) {
		if (i % 2 == 0 && numb_contains_p(i)) {
			numb_err("numb_pset_test: FAIL - even PID %d should be removed\n", i);
			pass = false;
			goto out;
		}
		if (i % 2 != 0 && !numb_contains_p(i)) {
			numb_err("numb_pset_test: FAIL - odd PID %d should remain\n", i);
			pass = false;
			goto out;
		}
	}

out:
	xa_for_each(&numb_pset, index, entry) {
		xa_erase(&numb_pset, index);
	}

	if (pass)
		numb_info("numb_pset_test: all tests passed\n");
	else
		numb_err("numb_pset_test: some tests failed\n");

	return pass;
}
