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

#ifndef UTILS_H_
#define UTILS_H_

#include "ipkgmgrsrv.h"

bool get_mountpoint_writability(char *mountpoint);
bool set_mountpoint_writability(char *mountpoint, bool writable);
bool is_emulator();
int is_directory(char *path);
char *JSON_list_files_in_dir(char *dir, char *file_suffix);

#endif /* UTILS_H_ */
