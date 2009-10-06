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

#include <stdio.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

struct DownloadFile {
	const char *filename;
	const char *destination;
	FILE *stream;
};

static size_t fileWriter(void *buffer, size_t size, size_t nmemb, void *stream) {

	struct DownloadFile *out=(struct DownloadFile *)stream;
	char *output_file = 0;
	asprintf(&output_file,"%s/%s",out->destination,out->filename);
	if(output_file && out && !out->stream) {
		out->stream=fopen(output_file, "wb");
		if(!out->stream) {
			free(output_file);
			return -1;
		}
		free(output_file);
	}
	return fwrite(buffer, size, nmemb, out->stream);
}


int curl_download(char *url, char *destination, int timeout) {

	CURL *curl;
	CURLcode res;

	struct DownloadFile file={
			strrchr(url,'/'),
			destination,
			NULL
	};

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriter);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}

	if(file.stream)
		fclose(file.stream);

	curl_global_cleanup();

	return 0;
}
