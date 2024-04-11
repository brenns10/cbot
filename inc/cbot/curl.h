#ifndef CBOT_CURL_H
#define CBOT_CURL_H

#include "cbot/cbot.h"
#include "sc-collections.h"
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

/**
 * @brief Use this to configure your CURL handle to write response to a charbuf
 *
 * It's just a simple way to get the whole response data as a single string.
 *
 * @param easy Handle
 * @param buf Buffer to write response to
 */
void cbot_curl_charbuf_response(CURL *easy, struct sc_charbuf *buf);

#endif
