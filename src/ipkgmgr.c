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
#include <unistd.h>
#include <string.h>

#include "ipkgmgr.h"

typedef enum {
	ipkg_update,
	ipkg_info,
	ipkg_install,
	ipkg_remove,
	ipkg_purge,
	ipkg_postinst,
	ipkg_prerm,
} ipkgcmd_t;

static char *ipkgmgr_argv[] = {
		"ipkgmgrsrv",
		"-V",
		"1",
		"-o",
		"/var",
		0
};

static int ipkgmgr_argc = 5;

args_t args;

int count = 0;

int doOfflineAction(char *package, char *extension) {
	int ret = 0, len = 0;
	char *filePath = 0;
	len = asprintf(&filePath, "/var/usr/lib/ipkg/info/%s%s",package,extension);
	if (filePath) {
		if (access(filePath,R_OK|X_OK)==0)
			ret = execv(filePath,NULL);
		free(filePath);
	}
	return ret;
}

void ipkgmgr_process_callback_request(char *category, char *data) {
	LSError lserror;
	LSErrorInit(&lserror);

	int len = strlen(data);

	char *tmp = data;
	if (tmp[len-1] == '\n')
		tmp[len-1] = '\0';

	char *jsonResponse = 0;
	len = asprintf(&jsonResponse, "{[\"%s\"]}",tmp);

	if(jsonResponse) {
		LSSubscriptionReply(lserviceHandle, category, jsonResponse, &lserror);
		free(jsonResponse);
	}

	LSErrorFree(&lserror);
}

int ipkgmgr_ipkg_message_callback(ipkg_conf_t *conf, message_level_t level, char *msg) {
	if (level<=1)
		ipkgmgr_process_callback_request("/callbacks/message", msg);
	return 0;
}

int ipkgmrg_ipkg_status_callback(char *name, int istatus, char *desc, void *userdata) {
	count++;
	ipkgmgr_process_callback_request("/callbacks/status", desc);
	return 0;
}

bool ipkgmgr_reply(LSHandle* lshandle, LSMessage *message, ipkgcmd_t ipkgcmd) {

	LSError lserror;
	LSErrorInit(&lserror);

	char *package = 0;
	if (ipkgcmd==ipkg_info||ipkgcmd==ipkg_install||ipkgcmd==ipkg_remove||
			ipkgcmd==ipkg_purge||ipkgcmd==ipkg_postinst||ipkgcmd==ipkg_prerm) {
		json_t *object = LSMessageGetPayloadJSON(message);
		if (object) {
			json_get_string(object, "package", &package);
		}
		if (ipkgcmd!=ipkg_info && !package) {
			LSMessageReply(lserviceHandle, message, "{\"returnValue\": false, \"errorText\": \"Parameter \"package\" required and not found\"}", &lserror);
			goto finnish;
		}

	}

	int len = 0, val = 0;
	char *jsonResponse = 0;

	switch (ipkgcmd) {
	case ipkg_update: val = ipkg_lists_update(&args); break;
	case ipkg_info: val = ipkg_packages_info(&args, package,ipkgmrg_ipkg_status_callback,NULL); break;
	case ipkg_install: {
		val = ipkg_packages_install(&args, package);
		if (val==0) {
			val = doOfflineAction(package,".postinst");
			if (val==-1)
				ipkg_packages_remove(&args, package, FALSE);
		}
		break;
	}
	case ipkg_remove: {
		val = doOfflineAction(package,".prerm");
		if (val==-1)
			ipkg_packages_remove(&args, package, FALSE);
		else
			val = ipkg_packages_remove(&args, package, FALSE);
		break;
	}
	case ipkg_purge: {
		val = doOfflineAction(package,".prerm");
		if (val==-1)
			ipkg_packages_remove(&args, package, TRUE);
		else
			val = ipkg_packages_remove(&args, package, TRUE);
		break;
	}
	case ipkg_postinst: val = doOfflineAction(package,".postinst"); break;
	case ipkg_prerm: val = doOfflineAction(package,".prerm"); break;
	}

	len = asprintf(&jsonResponse,"{\"returnValue\":%d}",val);

	if (jsonResponse) {
		LSMessageReply(lserviceHandle, message, jsonResponse, &lserror);
		free(jsonResponse);
	} else
		LSMessageReply(lserviceHandle, message, "{\"returnValue\": false, \"errorText\": \"Generic error\"}", &lserror);

	finnish:

	LSErrorFree(&lserror);

	return TRUE;

}

bool ipkgmgr_process_request(LSHandle* lshandle, LSMessage *message, ipkgcmd_t ipkgcmd, SubscriptionRequirement subscription_required) {

	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	if (!LSMessageIsSubscription(message)) {
		if (subscription_required)
			retVal = LSMessageReply(lshandle, message, "{\"returnValue\": false, \"errorText\": \"Subscription required\"}", &lserror);
		else
			retVal = ipkgmgr_reply(lshandle, message, ipkgcmd);
	} else {
		bool subscribed;
		retVal = LSSubscriptionProcess(lshandle, message, &subscribed, &lserror);
		if (!retVal) {
			g_message("Subscription process failed.");
			LSErrorPrint(&lserror, stderr);
			retVal = LSMessageReply(lshandle, message,
					"{\"returnValue\": false, \"errorText\": \"Subscription error\"}", &lserror);
			if (!retVal) {
				LSErrorPrint(&lserror, stderr);
			}
		}
	}

	LSErrorFree(&lserror);
	return TRUE;
}

static bool ipkgmgr_callback_message(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,-1,SUBSCRIPTION_REQUIRED);
}

static bool ipkgmgr_callback_status(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,-1,SUBSCRIPTION_REQUIRED);
}

LSMethod ipkgmgr_callback_methods[] = {
		{"message",ipkgmgr_callback_message},
		{"status",ipkgmgr_callback_status},
		{0,0}
};

static bool ipkgmgr_command_update(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_update,SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_command_info(LSHandle* lshandle, LSMessage *message, void *ctx) {
	count = 0;
	int ret = ipkgmgr_process_request(lshandle,message,ipkg_info,SUBSCRIPTION_NOTREQUIRED);
	if (verbose)
		g_message("Items:%d",count);
	return ret;
}

static bool ipkgmgr_command_install(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_install,SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_command_remove(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_remove,SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_command_purge(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_purge,SUBSCRIPTION_NOTREQUIRED);
}

LSMethod ipkgmgr_command_methods[] = {
		{"update",ipkgmgr_command_update},
		{"info",ipkgmgr_command_info},
		{"install",ipkgmgr_command_install},
		{"remove",ipkgmgr_command_remove},
		{"purge",ipkgmgr_command_purge},
		{0,0}
};

static bool ipkgmgr_offline_action_postinst(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_postinst,SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_offline_action_prerm(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle,message,ipkg_prerm,SUBSCRIPTION_NOTREQUIRED);
}

LSMethod ipkgmgr_offline_action_methods[] = {
		{"doPostInst",ipkgmgr_offline_action_postinst},
		{"doPreRm",ipkgmgr_offline_action_prerm},
		{0,0}
};

bool ipkgmgr_init() {

	bool retVal = TRUE;

	LSError lserror;
	LSErrorInit(&lserror);

	ipkg_cb_message = ipkgmgr_ipkg_message_callback;

	args_init(&args);
	args_parse(&args, ipkgmgr_argc, ipkgmgr_argv);

	if (verbose) {
		g_message("Registering category: /callbacks");
	}
	retVal = LSRegisterCategory(lserviceHandle, "/callbacks", ipkgmgr_callback_methods, 0, NULL, &lserror);
	if (!retVal) {
		if (verbose)
			g_message("Failed.");
		goto error;
	} else {
		if (verbose)
			g_message("Succeeded.");
	}

	if (verbose) {
		g_message("Registering category: /commands");
	}
	retVal = LSRegisterCategory(lserviceHandle, "/commands", ipkgmgr_command_methods, 0, NULL, &lserror);
	if (!retVal) {
		if (verbose)
			g_message("Failed.");
		goto error;
	} else {
		if (verbose)
			g_message("Succeeded.");
	}

	if (verbose) {
			g_message("Registering category: /offline_actions");
		}
		retVal = LSRegisterCategory(lserviceHandle, "/offline_actions", ipkgmgr_command_methods, 0, NULL, &lserror);
		if (!retVal) {
			if (verbose)
				g_message("Failed.");
			goto error;
		} else {
			if (verbose)
				g_message("Succeeded.");
		}

	error: if (LSErrorIsSet(&lserror)) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return retVal;

}

bool ipkgmgr_deinit() {
	args_deinit(&args);
	return TRUE;
}
