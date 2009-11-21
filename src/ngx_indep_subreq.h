#ifndef _NGX_INDEP_SUBREQ_H
#define _NGX_INDEP_SUBREQ_H

#include <ngx_http.h>

typedef void (*ngx_indep_subreq_fetch_callback_pt) (ngx_http_request_t *subreq, ngx_int_t rc, void *data);

typedef struct {
	ngx_http_upstream_conf_t 	uc;
	ngx_str_t 					schema;
} ngx_indep_subreq_conf_t;

typedef struct {
	ngx_url_t upstream_url;
	ngx_indep_subreq_fetch_callback_pt callback;
	void *callback_data;

	int main_req_gone:1;
} ngx_indep_subreq_ctx_t;

ngx_int_t ngx_indep_subreq_fetch(ngx_pool_t *pool, ngx_url_t *url, ngx_indep_subreq_fetch_callback_pt callback, void *callback_data);

#endif


