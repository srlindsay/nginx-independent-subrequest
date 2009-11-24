#ifndef _NGX_INDEP_SUBREQ_H
#define _NGX_INDEP_SUBREQ_H

#include <ngx_http.h>

typedef void (*ngx_indep_subreq_fetch_callback_pt) (ngx_buf_t *in, ngx_http_request_t *subreq, ngx_int_t rc, void *data);

typedef struct {
	ngx_int_t (*create_request)(ngx_http_request_t *r, void *data);
	ngx_int_t (*process_header)(ngx_http_request_t *r, void *data);
	ngx_int_t (*reinit_request)(ngx_http_request_t *r, void *data);
	void (*abort_request)(ngx_http_request_t *r, void *data);
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
 * fake request builds a dummy request suitable for use by 
 * ngx_indep_subreq_init_upstream.
 */
ngx_http_request_t *ngx_indep_subreq_fake_request(void);

/**
 * Force a request to proxy to the specified request.  When the request 
 * completes (in the upstream->finalize request callback), your callback
 * will be called.
 *
 * Currently, the upstream is set to buffer the entire response in memory,
 * which could be a serious problem for large upstream responses.
 */
ngx_int_t ngx_indep_subreq_init_upstream(
		ngx_http_request_t *r, 
		ngx_url_t *u,
		ngx_indep_subreq_fetch_callback_pt callback,
		void *callback_data,
		ngx_indep_subreq_upstream_callbacks_t *upstream_extensions);

/**
 * This function does not work properly yet.  The end goal is to have one
 * function to call to kick off a proxy request without having to set up the
 * fake request yourself.  Currently, however, this requires some pieces of
 * an existing request (configuration for some of the underlying pieces, for
 * instance).
 */
ngx_int_t ngx_indep_subreq_fetch(
		ngx_http_request_t *r,
		ngx_pool_t *pool, 
		ngx_url_t *url, 
		ngx_indep_subreq_fetch_callback_pt callback, 
		void *callback_data, 
		ngx_indep_subreq_upstream_callbacks_t *upstream_extensions);

#endif


