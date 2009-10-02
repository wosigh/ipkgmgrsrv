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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "ipkgmgrsrv.h"
#include "ipkgmgr.h"

const char *dbusAddress = "us.ryanhope.ipkgmgrsrv";

static struct option long_options[] =   {
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"version", no_argument, 0, 'V'},
		{0, 0, 0, 0}
};

void print_version() {
	printf("IPKG Manager Service %s\n", VERSION);
}

void print_help(char *argv[]) {

	printf(
			"Usage: %s [OPTION]...\n\n"
			"Miscellaneous:\n"
			"  -h, --help\t\tprint help information and exit\n"
			"  -v, --verbose\t\tturn on verbose output\n"
			"  -V, --version\t\tprint version information and exit\n",
			argv[0]
	);

}

int getopts(int argc, char *argv[]) {

	int c, retVal = 0;

	while (1) {
		int option_index = 0;
		c = getopt_long (argc, argv, "vVh", long_options, &option_index);
		if (c == -1) break;
		switch (c) {
		case 'v': verbose = 1; break;
		case 'V': print_version(); retVal = 1; break;
		case 'h': print_help(argv); retVal = 1; break;
		case '?': print_help(argv); retVal = 1; break;
		default: abort();
		}
	}
	return retVal;

}

int main(int argc, char *argv[]) {

	verbose = FALSE;

	if (getopts(argc,argv)==1)
		return 1;

	bool retVal = TRUE;

	LSError lserror;
	LSErrorInit(&lserror);

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	if (loop == NULL)
		goto error;

	if (verbose) {
		g_message("IPKG Manager Service %s", VERSION);
		g_message("Registering service: %s ... ", dbusAddress);
	}

	retVal = LSRegisterPubPriv(dbusAddress, &lserviceHandle, TRUE, &lserror);
	if (!retVal) {
		if (verbose)
			g_message("Failed.");
		goto error;
	} else {
		if (verbose)
			g_message("Succeeded.");
	}

	if (!retVal)
			goto error;

	retVal = ipkgmgr_init();

	if (verbose)
		g_message("Attaching to GmainLoop ... ");

	retVal = LSGmainAttach(lserviceHandle, loop, &lserror);
	if (!retVal) {
		if (verbose)
			g_message("Failed.");
		goto error;
	} else {
		if (verbose)
			g_message("Succeeded.");
		g_main_loop_run(loop);
	}

	error: if (LSErrorIsSet(&lserror)) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	retVal = ipkgmgr_deinit();

	if (lserviceHandle) {
		retVal = LSUnregister(lserviceHandle, &lserror);
		if (!retVal) {
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}

	if (loop)
		g_main_loop_unref(loop);

	return retVal;

}
