/*=============================================================================
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
 =============================================================================*/

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include "ipkgmgr.h"
#include "utils.h"

typedef enum {
	ipkg_update,
	ipkg_info,
	ipkg_install,
	ipkg_remove,
	ipkg_listfeeds,
	ipkg_addfeed,
	ipkg_removefeed,
	ipkg_togglefeed,
} ipkgcmd_t;

static char *ipkgmgr_argv[] = { "ipkgmgrsrv", "-V", "1", "-o", "/var", 0 };

static int ipkgmgr_argc = 5;

args_t args;

int count = 0;

int doOfflineAction(char *package, char *extension) {
	int ret = 0, len = 0;
	char *filePath = 0;
	char * const envp[] = { "IPKG_OFFLINE_ROOT=/var", "PKG_ROOT=/", NULL };
	len
			= asprintf(&filePath, "/var/usr/lib/ipkg/info/%s%s", package,
					extension);
	if (filePath) {
		if (access(filePath, R_OK | X_OK) == 0)
			if (vfork() == 0)
				ret = execve(filePath, NULL, envp);
		free(filePath);
	}
	return ret;
}

void ipkgmgr_process_callback_request(char *category, char *data) {

	LSError lserror;
	LSErrorInit(&lserror);

	int len = strlen(data);

	char *tmp = data;
	if (tmp[len - 1] == '\n')
		tmp[len - 1] = '\0';

	char *jsonResponse = 0;
	len = asprintf(&jsonResponse, "{[\"%s\"]}", tmp);

	if (jsonResponse) {
		LSSubscriptionRespond(serviceHandle, category, jsonResponse, &lserror);
		free(jsonResponse);
	}

	LSErrorFree(&lserror);

}

int ipkgmgr_ipkg_message_callback(ipkg_conf_t *conf, message_level_t level,
		char *msg) {
	if (level <= 1)
		ipkgmgr_process_callback_request("/callbacks/message", msg);
	return 0;
}

int ipkgmrg_ipkg_info_callback(char *name, int istatus, char *desc,
		void *userdata) {

	count++;

	LSMessage *message = (LSMessage *) userdata;

	LSError lserror;
	LSErrorInit(&lserror);

	char *d1 = ":";
	char *d2 = "\n";

	char *tmp = 0;
	char *jsonResponse = 0;

	char *key = strtok(desc, d1);
	char *value = strtok(NULL, d2) + 1;
	int len = asprintf(&jsonResponse, "\"%s\":\"%s\"", key, value);

	while ((key = strtok(NULL, d1)) != NULL && (value = strtok(NULL, d2) + 1)
			!=NULL) {
		if (value[0] == '{')
			len = asprintf(&tmp, "%s,\"%s\":%s", jsonResponse, key, value);
		else
			len = asprintf(&tmp, "%s,\"%s\":\"%s\"", jsonResponse, key, value);
		free(jsonResponse);
		jsonResponse = strdup(tmp);
		free(tmp);
	}

	len = asprintf(&tmp, "{%s}", jsonResponse);
	free(jsonResponse);

	LSMessageReply(pub_serviceHandle, message, tmp, &lserror);
	free(tmp);

	LSErrorFree(&lserror);

	return 0;

}

bool ipkgmgr_response_relay_handler(LSHandle *lshandle, LSMessage *reply,
		void *ctx) {

	LSError lserror;
	LSErrorInit(&lserror);

	LSMessage *message = (LSMessage *) ctx;
	json_t *object = LSMessageGetPayloadJSON(reply);

	bool returnValue = false;
	json_get_bool(object, "returnValue", returnValue);

	if (returnValue)
		LSMessageReply(pub_serviceHandle, message, "{\"returnValue\":0}",
				&lserror);
	else
		LSMessageReply(pub_serviceHandle, message, "{\"returnValue\":-1}",
				&lserror);

	LSMessageUnref(message);

	LSErrorFree(&lserror);

	return TRUE;

}

bool ipkgmgr_reply(LSHandle* lshandle, LSMessage *message, ipkgcmd_t ipkgcmd) {

	LSError lserror;
	LSErrorInit(&lserror);

	json_t *object = LSMessageGetPayloadJSON(message);

	char *package = 0;
	if (ipkgcmd == ipkg_info || ipkgcmd == ipkg_install || ipkgcmd
			== ipkg_remove) {
		if (object) {
			json_get_string(object, "package", &package);
			if (ipkgcmd != ipkg_info && !package) {
				LSMessageReply(
						pub_serviceHandle,
						message,
						"{\"returnValue\":-1,\"errorText\":\"Parameter \"package\" required and not found\"}",
						&lserror);
				goto finnish;
			}
		}
	}

	char *type = 0, *label = 0, *url = 0;
	if (ipkgcmd == ipkg_addfeed) {
		if (object) {
			json_get_string(object, "type", &type);
			if (!type) {
				LSMessageReply(
						pub_serviceHandle,
						message,
						"{\"returnValue\":-1,\"errorText\":\"Parameter \"type\" required and not found\"}",
						&lserror);
				goto finnish;
			}
			json_get_string(object, "label", &label);
			if (!label) {
				LSMessageReply(
						pub_serviceHandle,
						message,
						"{\"returnValue\":-1,\"errorText\":\"Parameter \"label\" required and not found\"}",
						&lserror);
				goto finnish;
			}
			json_get_string(object, "url", &url);
			if (!url) {
				LSMessageReply(
						pub_serviceHandle,
						message,
						"{\"returnValue\":-1,\"errorText\":\"Parameter \"url\" required and not found\"}",
						&lserror);
				goto finnish;
			}
		}
		if (!type || !label || !url) {
			LSMessageReply(
					pub_serviceHandle,
					message,
					"{\"returnValue\":-1,\"errorText\":\"Generic parameter error\"}",
					&lserror);
			goto finnish;
		}
	}

	char *feed_config = 0;
	if (ipkgcmd == ipkg_removefeed || ipkgcmd == ipkg_addfeed || ipkgcmd
			== ipkg_togglefeed) {
		if (object) {
			json_get_string(object, "feed_config", &feed_config);
			if (!feed_config) {
				LSMessageReply(
						pub_serviceHandle,
						message,
						"{\"returnValue\":-1,\"errorText\":\"Parameter \"feed_config\" required and not found\"}",
						&lserror);
				goto finnish;
			}
		}
	}

	int len = 0, val = -1;
	char *enabled_feeds = 0, *disabled_feeds = 0;
	char *jsonResponse = 0;

	bool rootfs_writable = get_mountpoint_writability("/");
	if (!is_emulator() && !rootfs_writable)
		set_mountpoint_writability("/", true);

	switch (ipkgcmd) {
	case ipkg_update:
		val = ipkg_lists_update(&args);
		break;
	case ipkg_info:
		val = ipkg_packages_info(&args, package, ipkgmrg_ipkg_info_callback,
				message);
		break;
	case ipkg_install: {
		val = ipkg_packages_install(&args, package);
		if (val == 0) {
			val = doOfflineAction(package, ".postinst");
			if (val == -1)
				ipkg_packages_remove(&args, package, FALSE);
		}
		break;
	}
	case ipkg_remove: {
		bool purge = false;
		json_t *object = LSMessageGetPayloadJSON(message);
		if (object)
			json_get_bool(object, "purge", purge);
		val = doOfflineAction(package, ".prerm");
		if (val == -1)
			ipkg_packages_remove(&args, package, FALSE);
		else
			val = ipkg_packages_remove(&args, package, FALSE);
		break;
	}
	case ipkg_listfeeds: {
		enabled_feeds = JSON_list_files_in_dir("/var/etc/ipkg", ".conf");
		disabled_feeds = JSON_list_files_in_dir("/var/etc/ipkg",
				".conf.disabled");
		break;
	}
	case ipkg_addfeed:
		val = add_feed_config(feed_config, type, label, url, verbose);
		break;
	case ipkg_removefeed:
		val = remove_feed_config(feed_config, verbose);
		break;
	case ipkg_togglefeed:
		val = toggle_feed_config(feed_config, verbose);
		break;
	}

	if (!is_emulator() && !rootfs_writable)
		set_mountpoint_writability("/", false);

	/*
	 * This is ugly! This should probably be converted to some sort of callback
	 * like the other functions. So, so ugly.
	 */
	if (ipkgcmd == ipkg_listfeeds) {
		if (enabled_feeds && disabled_feeds) {
			len
					= asprintf(
							&jsonResponse,
							"{\"returnValue\":0,\"enabledFeeds\":%s,\"disabledFeeds\":%s}",
							enabled_feeds, disabled_feeds);
			free(enabled_feeds);
			free(disabled_feeds);
		} else if (enabled_feeds && !disabled_feeds) {
			len = asprintf(&jsonResponse,
					"{\"returnValue\":0,\"enabledFeeds\":%s}", enabled_feeds);
			free(enabled_feeds);
		} else if (!enabled_feeds && disabled_feeds) {
			len = asprintf(&jsonResponse,
					"{\"returnValue\":0,\"disabledFeeds\":%s}", disabled_feeds);
			free(disabled_feeds);
		} else
			len = asprintf(&jsonResponse, "{\"returnValue\":-1}");
	} else
		len = asprintf(&jsonResponse, "{\"returnValue\":%d}", val);

	if (jsonResponse) {
		LSMessageReply(pub_serviceHandle, message, jsonResponse, &lserror);
		free(jsonResponse);
	} else
		LSMessageReply(pub_serviceHandle, message,
				"{\"returnValue\":-1,\"errorText\":\"Generic error\"}",
				&lserror);

	finnish:

	LSErrorFree(&lserror);

	return TRUE;

}

bool ipkgmgr_process_request(LSHandle* lshandle, LSMessage *message,
		ipkgcmd_t ipkgcmd, SubscriptionRequirement subscription_required) {

	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);

	if (!LSMessageIsSubscription(message)) {
		if (subscription_required)
			retVal
					= LSMessageReply(
							lshandle,
							message,
							"{\"returnValue\":-1,\"errorText\":\"Subscription required\"}",
							&lserror);
		else
			retVal = ipkgmgr_reply(lshandle, message, ipkgcmd);
	} else {
		bool subscribed;
		retVal
				= LSSubscriptionProcess(lshandle, message, &subscribed,
						&lserror);
		if (!retVal) {
			g_message("Subscription process failed.");
			LSErrorPrint(&lserror, stderr);
			retVal
					= LSMessageReply(
							lshandle,
							message,
							"{\"returnValue\":-1,\"errorText\":\"Subscription error\"}",
							&lserror);
			if (!retVal) {
				LSErrorPrint(&lserror, stderr);
			}
		}
	}

	LSErrorFree(&lserror);
	return TRUE;
}

static bool ipkgmgr_callback_message(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, -1, SUBSCRIPTION_REQUIRED);
}

LSMethod ipkgmgr_callback_methods[] = {
		{ "message", ipkgmgr_callback_message }, { 0, 0 } };

static bool ipkgmgr_command_update(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_update,
			SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_command_info(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	count = 0;
	int ret = ipkgmgr_process_request(lshandle, message, ipkg_info,
			SUBSCRIPTION_NOTREQUIRED);
	if (verbose)
		g_message("Items:%d",count);
	return ret;
}

static bool ipkgmgr_command_install(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_install,
			SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_command_remove(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_remove,
			SUBSCRIPTION_NOTREQUIRED);
}

LSMethod ipkgmgr_command_methods[] = { { "update", ipkgmgr_command_update }, {
		"info", ipkgmgr_command_info }, { "install", ipkgmgr_command_install },
		{ "remove", ipkgmgr_command_remove }, { 0, 0 } };

static bool ipkgmgr_feed_list(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_listfeeds,
			SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_feed_add(LSHandle* lshandle, LSMessage *message, void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_addfeed,
			SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_feed_remove(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_removefeed,
			SUBSCRIPTION_NOTREQUIRED);
}

static bool ipkgmgr_feed_toggle(LSHandle* lshandle, LSMessage *message,
		void *ctx) {
	return ipkgmgr_process_request(lshandle, message, ipkg_togglefeed,
			SUBSCRIPTION_NOTREQUIRED);
}

LSMethod ipkgmgr_feed_methods[] = { { "list", ipkgmgr_feed_list }, { "add",
		ipkgmgr_feed_add }, { "remove", ipkgmgr_feed_remove }, { "toggle",
		ipkgmgr_feed_toggle }, { 0, 0 } };

static bool ipkgmgr_misc_rescan_luna(LSHandle* lshandle, LSMessage *message,
		void *ctx) {

	LSError lserror;
	LSErrorInit(&lserror);

	bool retVal = false;

	LSMessageRef(message);
	retVal = LSCall(priv_serviceHandle,
			"luna://com.palm.applicationManager/rescan", "{}",
			ipkgmgr_response_relay_handler, message, NULL, &lserror);

	if (!retVal)
		LSErrorPrint(&lserror, stderr);
	LSErrorFree(&lserror);

	return TRUE;

}

static bool ipkgmgr_misc_kill_luna(LSHandle* lshandle, LSMessage *message,
		void *ctx) {

	LSError lserror;
	LSErrorInit(&lserror);

	char *jsonResponse = 0;
	int len = 0;
	len = asprintf(&jsonResponse, "{\"returnValue\":%d}", system(
			"killall -HUP LunaSysMgr"));

	if (jsonResponse) {
		if (!LSMessageReply(pub_serviceHandle, message, jsonResponse, &lserror))
			LSErrorPrint(&lserror, stderr);
		free(jsonResponse);
	}

	LSErrorFree(&lserror);

	return TRUE;

}

static bool ipkgmgr_misc_kill_java(LSHandle* lshandle, LSMessage *message,
		void *ctx) {

	LSError lserror;
	LSErrorInit(&lserror);

	char *jsonResponse = 0;
	int len = 0;
	len = asprintf(&jsonResponse, "{\"returnValue\":%d}", system(
			"killall -9 java"));

	if (jsonResponse) {
		if (!LSMessageReply(pub_serviceHandle, message, jsonResponse, &lserror))
			LSErrorPrint(&lserror, stderr);
		free(jsonResponse);
	}

	LSErrorFree(&lserror);

	return TRUE;

}

static bool ipkgmgr_misc_machine_reboot(LSHandle* lshandle, LSMessage *message,
		void *ctx) {

	LSError lserror;
	LSErrorInit(&lserror);

	bool retVal = false;
	int len = 0;
	char *reason = 0, *jsonResponse = 0;
	json_t *object = LSMessageGetPayloadJSON(message);
	json_get_string(object, "reason", &reason);

	if (!reason)
		reason = "User initiated / IPKG Manager Service";

	len = asprintf(&jsonResponse, "{\"reason\":\"%s\"}", reason);
	if (jsonResponse) {
		LSMessageRef(message);
		retVal = LSCall(priv_serviceHandle,
				"luna://com.palm.power/shutdown/machineReboot", jsonResponse,
				ipkgmgr_response_relay_handler, message, NULL, &lserror);
		free(jsonResponse);
	} else {
		retVal = LSMessageReply(pub_serviceHandle, message,
				"{\"returnValue\":-1,\"errorText\":\"Generic error\"}",
				&lserror);
	}

	if (!retVal)
		LSErrorPrint(&lserror, stderr);
	LSErrorFree(&lserror);

	return TRUE;

}

LSMethod ipkgmgr_misc_methods[] = {
		{ "rescan_luna", ipkgmgr_misc_rescan_luna }, { "kill_luna",
				ipkgmgr_misc_kill_luna },
		{ "kill_java", ipkgmgr_misc_kill_java }, { "machine_reboot",
				ipkgmgr_misc_machine_reboot }, { 0, 0 } };

/*
 * Register a struct of LSMethod's under a particular category.
 */
bool ipkgmgr_register_category(char *category, LSMethod *methods) {

	bool retVal = TRUE;

	LSError lserror;
	LSErrorInit(&lserror);

	if (verbose)
		g_message("Registering category: %s", category);
	retVal = LSPalmServiceRegisterCategory(serviceHandle, category, methods,
			NULL, NULL, NULL, &lserror);
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

/*
 * Setup libipkg usage and register each group of LSMethod's in the
 * correct catalog.
 */
bool ipkgmgr_init() {

	ipkg_cb_message = ipkgmgr_ipkg_message_callback;

	args_init(&args);
	args_parse(&args, ipkgmgr_argc, ipkgmgr_argv);

	if (!ipkgmgr_register_category("/callbacks", ipkgmgr_callback_methods))
		goto fail;
	/* Standard ipkg commands: update/info/install/remove */
	if (!ipkgmgr_register_category("/commands", ipkgmgr_command_methods))
		goto fail;
	/* Feed manipulations: list/add/remove/toggle */
	if (!ipkgmgr_register_category("/feeds", ipkgmgr_feed_methods))
		goto fail;
	/* Misc actions: luna_rescan/kill_luna/kill_java/rebootMachine */
	if (!ipkgmgr_register_category("/misc", ipkgmgr_misc_methods))
		goto fail;

	return true;

	fail: return false;

}

bool ipkgmgr_deinit() {
	args_deinit(&args);
	return TRUE;
}
