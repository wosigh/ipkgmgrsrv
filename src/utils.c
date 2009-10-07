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
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mount.h>

#include <glib.h>

/*
 * Figure out if a mount point is mounted with (rw) or (ro)
 */
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

/*
 * Remount a mountpoint (rw) or (ro)
 */
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

/*
 * Detect if current device is the Palm Nova-SDK (emulator)
 */
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

/*
 * Check path points to a directory
 */
int is_directory(char *path) {

	struct stat sb;
	int s;

	s= stat(path,&sb);
	if (s) {
		return -1;
	}

	return (sb.st_mode & S_IFMT) == S_IFDIR;

}

/*
 * List files by extension and format output directly into JSON
 */
char *JSON_list_files_in_dir(char *dir, char *file_suffix) {

	int len = 0;
	struct dirent *dp;
	char *tmp = 0, *jsonResponse = 0;

	int len1 = strlen(file_suffix);

	DIR *confdir = opendir(dir);
	while ((dp=readdir(confdir)) != NULL) {
		int len2 = strlen(dp->d_name);
		if (strcmp(dp->d_name+(len2-len1),file_suffix)==0) {
			if (!jsonResponse) {
				len = asprintf(&jsonResponse,"\"%s\"", dp->d_name);
			} else {
				len = asprintf(&tmp,"%s,\"%s\"", jsonResponse, dp->d_name);
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

/*
 * Create or append a feed entry to a feed config file
 */
int add_feed_config(char *feed_config, char *type, char *label, char *url, bool verbose) {
	int ret = -1;
	int len = 0;
	int lock = -1;
	char *configPath = 0;
	len = asprintf(&configPath,"/var/etc/ipkg/%s",feed_config);
	if (configPath) {
		FILE *fp;
		if((fp=fopen(configPath,"r")) != NULL) {
			lock = lockf(fileno(fp), F_LOCK, 0);
			if (lock!=0)
				goto done;
			char l[512];
			while (fscanf(fp, "%*s%s%*s",l) != EOF) {
				if (strcmp(label,l)==0) {
					ret = 1;
					if (verbose)
						g_message("Label \"%s\" found in %s, aborting.", label, configPath);
					goto done;
				}
			}
		}
		if((fp=fopen(configPath,"a+")) != NULL) {
			lock = lockf(fileno(fp), F_LOCK, 0);
			if (lock!=0)
				goto done;
			goto addfeed;
		} else
			goto err;

		addfeed:
		if (verbose)
			g_message("Adding \"%s %s %s\" to %s", type, label, url, configPath);
		if (fprintf(fp,"%s %s %s\n", type, label, url) == strlen(type)+strlen(label)+strlen(url)+3) {
			ret = 0;
			if (verbose)
				g_message("Succeeded.");
		} else {
			if (verbose)
				g_message("Failed.");
		}
		rewind(fp);
		lock = lockf(fileno(fp), F_ULOCK, 0);

		done:
		if (fp)
			fclose(fp);

		err:
		free(configPath);

	}
	return ret;
}

/*
 * Remove a feed config file
 */
int remove_feed_config(char *feed_config, bool verbose) {
	int ret = -1;
	int len = 0;
	char *configPath = 0;
	len = asprintf(&configPath,"/var/etc/ipkg/%s",feed_config);
	if (configPath) {
		if (access(configPath,R_OK)==0) {
			if (verbose)
				g_message("Removing %s",feed_config);
			ret = remove(configPath);
			if (verbose) {
				if (ret==0)
					g_message("Succeeded.");
				else
					g_message("Failed.");
			}
		}
		free(configPath);
	}
	return ret;
}

/*
 * If file ends with .config add .disabled, if file ends with .disabled, remove
 * .disabled.
 */
int toggle_feed_config(char *feed_config, bool verbose) {
	int ret = -1;
	char *config = 0;
	int len = 0;
	char *configPathOld = 0, *configPathNew = 0;
	len = asprintf(&configPathOld,"/var/etc/ipkg/%s",feed_config);
	if (configPathOld) {
		if (access(configPathOld,R_OK)==0) {
			int e = strlen(feed_config)-9;
			if (strcmp(feed_config+e,".disabled")==0) {
				config = calloc(e+1,sizeof(char));
				if (config) {
					strncpy(config,feed_config,e);
				}
			} else {
				len = asprintf(&config,"%s.disabled",feed_config);
			}
			if (config) {
				if (verbose)
					g_message("Toggling %s => %s",feed_config,config);
				len = asprintf(&configPathNew,"/var/etc/ipkg/%s",config);
				if (configPathNew) {
					ret = rename(configPathOld,configPathNew);
					if (verbose) {
						if (ret==0)
							g_message("Succeeded.");
						else
							g_message("Failed.");
					}
					free(configPathNew);
				}
				free(config);
			}
		}
		free(configPathOld);
	}
	return ret;
}
