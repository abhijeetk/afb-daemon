/*
 * Copyright 2016 IoT.bzh
 * Author: José Bollo <jose.bollo@iot.bzh>
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


struct afb_req_itf {
	const char *(*get_cookie)(void *data, const char *name);
	const char *(*get_argument)(void *data, const char *name);
#if 0
	int (*set_cookie)(void *data, const char *name, const char *value);
#endif
};

struct afb_req {
	struct afb_req_itf *itf;
	void *data;
};

inline const char *afb_get_cookie(struct afb_req req, const char *name)
{
	return req.itf->get_cookie(req.data, name);
}

inline const char *afb_get_argument(struct afb_req req, const char *name)
{
	return req.itf->get_argument(req.data, name);
}



