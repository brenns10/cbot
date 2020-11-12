#ifndef CBOT_CURL_H
#define CBOT_CURL_H

#include "cbot/cbot.h"
#include <curl/curl.h>

/**
 * @brief Use this instead of curl_easy_perform in order to make a request.
 *
 * Blocks the current lwt until the request is completed. The actual execution,
 * of course, will be asynchronous -- other lwts, like the main IRC thread, will
 * continue working when they have work.
 *
 * This function makes use of CURLOPT_PRIVATE, which means that you cannot use
 * this yourself.
 *
 * @param bot Bot which is being used (all requests for the same bot are handled
 *   on the same curl multi instance)
 * @param handle Easy handle for the request.
 * @returns The return code which curl_easy_perform would have returned
 */
CURLcode cbot_curl_perform(struct cbot *bot, CURL *handle);

#endif
