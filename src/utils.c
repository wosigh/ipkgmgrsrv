/*==============================================================================
 Copyright (C) 2009 Ryan Hope <rmh3093@gmail.com>
 Copyright (C) 2009 WebOS Internals <http://www.webos-internals.org/>

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

char *JSON_list_files_in_dir(char *dir, char *file_suffix) {

	int len = 0;
	struct dirent *dp;
	char *tmp = 0, *jsonResponse = 0;

	int len1 = strlen(file_suffix);

	DIR *confdir = opendir(dir);
	while ((dp=readdir(confdir)) != NULL) {
		int len2 = strlen(dp->d_name);
		if (strcmp(dp->d_name+(len2-len1),file_suffix)) {
			if (!jsonResponse) {
				len = asprintf(&jsonResponse,"\"%s\"", dp->d_name);
			} else {
				len = asprintf(&tmp,"\"%s\",\"%s\"", jsonResponse, dp->d_name);
				free(jsonResponse);
				jsonResponse = strdup(tmp);
				free(tmp);
			}
		}
	}
	closedir(confdir);

	if (jsonResponse) {
		len = asprintf(&tmp,"[%s]",jsonResponse);
		free(jsonResponse);
		return tmp;
	} else
		return NULL;

}
