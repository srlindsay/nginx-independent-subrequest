#ifndef _NGX_INDEP_SUBREQ_H
#define _NGX_INDEP_SUBREQ_H

#include <ngx_http.h>

typedef void (*ngx_indep_subreq_fetch_callback_pt) (ngx_http_request_t *subreq, ngx_int_t rc, void *data);

typedef struct {
	ngx_int_t (*create_request)(ngx_http_request_t *r);
	ngx_int_t (*process_header)(ngx_http_request_t *r);
	ngx_int_t (*reinit_request)(ngx_http_request_t *r);
	void (*abort_request)(ngx_http_request_t *r);
} ngx_indep_subreq_upstream_callbacks_t;

typedef struct {
	ngx_http_upstream_conf_t 	uc;
	ngx_str_t 					schema;
} ngx_indep_subreq_conf_t;

typedef struct {
	ngx_url_t upstream_url;
	ngx_indep_subreq_fetch_callback_pt callback;
	void *callback_data;

	ngx_indep_subreq_upstream_callbacks_t upstream_extensions;

	int main_req_gone:1;
} ngx_indep_subreq_ctx_t;

/**
 * upstream_extensions can be NULL if you don't need to customize anything 
 */
ngx_int_t ngx_indep_subreq_fetch(
		ngx_pool_t *pool, 
		ngx_url_t *url, 
		ngx_indep_subreq_fetch_callback_pt callback, 
		void *callback_data, 
		ngx_indep_subreq_upstream_callbacks_t *upstream_extensions);

#endif


