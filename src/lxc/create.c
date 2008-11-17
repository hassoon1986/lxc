/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include <lxc/lxc.h>

static int dir_filter(const struct dirent *dirent)
{
	if (!strcmp(dirent->d_name, ".") ||
            !strcmp(dirent->d_name, ".."))
                return 0;
        return 1;
}

static int is_empty_directory(const char *dirname)
{
	struct dirent **namelist;
	int n;

	n = scandir(dirname, &namelist, dir_filter, alphasort);
	if (n < 0)
		lxc_log_syserror("failed to scan %s directory", dirname);
	return n == 0;
}

static int create_lxc_directory(const char *dirname)
{
	char path[MAXPATHLEN];

	if (mkdir(LXCPATH, 0755) && errno != EEXIST)
		return -errno;

	sprintf(path, LXCPATH "/%s", dirname);

	if (mkdir(path, 0755))
		return -errno;

	return 0;
}

static int remove_lxc_directory(const char *dirname)
{
	char path[MAXPATHLEN];

	sprintf(path, LXCPATH "/%s", dirname);

	if (rmdir(path)) {
		lxc_log_syserror("failed to remove %s directory", path);
		return -1;
	}

	if (is_empty_directory(LXCPATH)) {
		if (rmdir(LXCPATH)) {
			lxc_log_syserror("failed to remove %s directory", LXCPATH);
			return -1;
		}
	}

	return 0;
}

int lxc_create(const char *name, struct lxc_conf *conf)
{
	int lock, err;

	err = create_lxc_directory(name);
	if (err < 0)
		return err == -EEXIST ?
			-LXC_ERROR_ALREADY_EXISTS:LXC_ERROR_INTERNAL;
	
	lock = lxc_get_lock(name);
	if (lock < 0)
		return -LXC_ERROR_LOCK;

	err = LXC_ERROR_INTERNAL;
	if (lxc_mkstate(name)) {
		lxc_log_error("failed to create the state file for %s", name);
		goto err;
	}

	if (lxc_setstate(name, STOPPED)) {
		lxc_log_error("failed to set state for %s", name);
		goto err_state;
	}

	if (lxc_configure(name, conf)) {
		lxc_log_error("failed to set configuration for %s", name);
		goto err_state;
	}

	err = 0;
out:
	lxc_put_lock(lock);
	return err;

err_state:
	lxc_unconfigure(name);

	if (lxc_rmstate(name))
		lxc_log_error("failed to remove state file for %s", name);
err:
	if (remove_lxc_directory(name))
		lxc_log_error("failed to cleanup lxc directory for %s", name);
	goto out;
}
