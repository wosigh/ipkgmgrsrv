/*==============================================================================
 Copyright (C) 2009  Ryan Hope <rmh3093@gmail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
==============================================================================*/

#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/mount.h>

bool get_mountpoint_writability(char *mountpoint) {

	FILE *fp;

	if((fp=fopen("/proc/mounts", "r")) != NULL) {

		char dev[PATH_MAX];
		char mp[PATH_MAX];
		char opts[512];
		char *delim = ",";
		char *opt = 0;

		while (fscanf(fp, "%s%s%*s%s%*d%*d", dev, mp, opts) != EOF) {
			if (strcmp(dev,"rootfs")==0) continue;
			if (strcmp(mp,mountpoint)!=0) continue;
			opt = strtok(opts,delim);
			while (true) {
				if (!opt)
					break;
				else if (strcmp(opt,"rw")==0) {
					fclose(fp);
					return true;
				}
				opt = strtok(NULL,delim);
			}

		}

		fclose(fp);

	}

	return false;

}

bool set_mountpoint_writability(char *mountpoint, bool writable) {

	int ret = 0;

	if (writable)
		ret = mount(NULL,mountpoint,NULL,MS_REMOUNT,NULL);
	else
		ret = mount(NULL,mountpoint,NULL,MS_REMOUNT|MS_RDONLY,NULL);

	if (ret==0)
		return true;
	else
		return false;

}

bool is_emulator() {

	bool ret = false;

	FILE *fp;

	if((fp=fopen("/etc/palm-build-info", "r")) != NULL) {

		char *i = 0;
		char line[512];
		char *delim = "=";
		char *token = 0;

		while (!feof(fp)) {
			i = fgets(line, 512, fp);
			token = strtok(line,delim);
			if (strcmp(token,"BUILDNAME"))
				token = strtok(NULL,delim);
			else
				continue;
			if (strcmp(token,"Nova-SDK"))
				ret = true;
			break;
		}

		fclose(fp);

	}

	return ret;

}

int is_directory(char *path) {

	struct stat sb;
	int s;

	s= stat(path,&sb);
	if (s) {
		return -1;
	}

	return (sb.st_mode & S_IFMT) == S_IFDIR;

}

char **list_files_in_dir(char *dir) {

	struct dirent *dp;
	DIR *confdir = opendir(dir);

	int count = 0;
	while ((dp=readdir(confdir)) != NULL) {
		if (dp->d_name[0] != '.')
			count++;
	}
	rewinddir(confdir);

	char **list = calloc(count+1, sizeof(char*));

	count = 0;
	while ((dp=readdir(confdir)) != NULL) {
		if (dp->d_name[0] != '.') {
			list[count] = strdup(dp->d_name);
			count++;
		}
	}
	list[count] = NULL;

	closedir(confdir);

	return list;
}
