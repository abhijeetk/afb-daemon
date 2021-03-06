/* 
 * Copyright (C) 2015, 2016 "IoT.bzh"
 * Author "Fulup Ar Foll"
 * Author José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

#include <afb/afb-req-itf.h>
#include "afb-context.h"
#include "afb-hreq.h"
#include "afb-apis.h"
#include "session.h"
#include "afb-websock.h"

int afb_hswitch_apis(struct afb_hreq *hreq, void *data)
{
	const char *api, *verb;
	size_t lenapi, lenverb;

	api = &hreq->tail[strspn(hreq->tail, "/")];
	lenapi = strcspn(api, "/");
	verb = &api[lenapi];
	verb = &verb[strspn(verb, "/")];
	lenverb = strcspn(verb, "/");

	if (!(*api && *verb && lenapi && lenverb))
		return 0;

	if (afb_hreq_init_context(hreq) < 0)
		afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
	else
		afb_apis_call(afb_hreq_to_req(hreq), &hreq->context, api, lenapi, verb, lenverb);
	return 1;
}

int afb_hswitch_one_page_api_redirect(struct afb_hreq *hreq, void *data)
{
	size_t plen;
	char *url;

	if (hreq->lentail >= 2 && hreq->tail[1] == '#')
		return 0;
	/*
	 * Here we have for example:
	 *    url  = "/pre/dir/page"   lenurl = 13
	 *    tail =     "/dir/page"   lentail = 9
	 *
	 * We will produce "/pre/#!dir/page"
	 *
	 * Let compute plen that include the / at end (for "/pre/")
	 */
	plen = hreq->lenurl - hreq->lentail + 1;
	url = alloca(hreq->lenurl + 3);
	memcpy(url, hreq->url, plen);
	url[plen++] = '#';
	url[plen++] = '!';
	memcpy(&url[plen], &hreq->tail[1], hreq->lentail);
	return afb_hreq_redirect_to(hreq, url, 1);
}

int afb_hswitch_websocket_switch(struct afb_hreq *hreq, void *data)
{
	if (hreq->lentail != 0)
		return 0;

	if (afb_hreq_init_context(hreq) < 0) {
		afb_hreq_reply_error(hreq, MHD_HTTP_INTERNAL_SERVER_ERROR);
		return 1;
	}

	if (!hreq->context.validated) {
		afb_hreq_reply_error(hreq, MHD_HTTP_UNAUTHORIZED);
		return 1;
	}

	return afb_websock_check_upgrade(hreq);
}


