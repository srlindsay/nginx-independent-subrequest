#include <ngx_http.h>

#include "ngx_indep_subreq.h"

#define NGX_INDEP_SUBREQ_CONNECT_TIMEOUT 5000
#define NGX_INDEP_SUBREQ_SEND_TIMEOUT 5000
#define NGX_INDEP_SUBREQ_READ_TIMEOUT 1000 * 120
#define NGX_INDEP_SUBREQ_TIMEOUT 1000 * 90

static ngx_int_t ngx_indep_subreq_postconf (ngx_conf_t *cf);
static ngx_indep_subreq_conf_t *global_conf;

static ngx_http_module_t ngx_indep_subreq_ctx = {
    NULL,                              		/* preconfiguration */
    ngx_indep_subreq_postconf,  			/* postconfiguration */
    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */
    NULL,                                	/* create server configuration */
    NULL,                                	/* merge server configuration */
    NULL,                                   /* create location configration */
    NULL,                                   /* merge location configration */
};

ngx_module_t ngx_indep_subreq = {
    NGX_MODULE_V1,
    &ngx_indep_subreq_ctx,          			/* module context */
    NULL,                        			/* module directives */
    NGX_HTTP_MODULE,                       	/* module type */
    NULL,                                  	/* init master */
    NULL,                                  	/* init module */
    NULL,                                  	/* init process */
    NULL,                                  	/* init thread */
    NULL,                                  	/* exit thread */
    NULL,                                  	/* exit process */
    NULL,                                  	/* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_indep_subreq_get_peer(ngx_peer_connection_t *pc, void *data)
{
	ngx_http_request_t *r = data;
	ngx_indep_subreq_ctx_t *ctx;

	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);
	ngx_url_t *u = &ctx->upstream_url;

	pc->sockaddr = u->addrs->sockaddr;
	pc->socklen  = u->addrs->socklen;
	pc->name     = &u->addrs->name;
	pc->log = r->connection->log;

	return NGX_OK;
}

static void
ngx_indep_subreq_free_peer(ngx_peer_connection_t *pc, void *data, ngx_uint_t state)
{
    pc->tries = 0;
	return;
}

static ngx_int_t
ngx_indep_subreq_peer_init(ngx_http_request_t *r,
		ngx_http_upstream_srv_conf_t *us)
{
	r->upstream->peer.free = ngx_indep_subreq_free_peer;
	r->upstream->peer.get  = ngx_indep_subreq_get_peer;

	r->upstream->peer.tries = 1;
	r->upstream->peer.data  = r;

	return NGX_OK;
}

static ngx_int_t
ngx_indep_subreq_create_request(ngx_http_request_t *r)
{
	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);
	if (ctx->upstream_extensions.create_request) {
		return ctx->upstream_extensions.create_request(r, ctx->callback_data);
	}

	return NGX_OK;
}

static ngx_int_t
ngx_indep_subreq_process_header(ngx_http_request_t *r)
{
	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);
	if (ctx->upstream_extensions.process_header) {
		return ctx->upstream_extensions.process_header(r, ctx->callback_data);
	}
	return NGX_OK;
}

static ngx_int_t
ngx_indep_subreq_reinit_request(ngx_http_request_t *r)
{
	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);
	if (ctx->upstream_extensions.reinit_request) {
		return ctx->upstream_extensions.reinit_request(r, ctx->callback_data);
	}
	return NGX_OK;
}

static void
ngx_indep_subreq_abort_request(ngx_http_request_t *r)
{
	fprintf(stderr, "[%s] -- fuck, why am I here?\n", __FUNCTION__);
	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);
	if (ctx->upstream_extensions.abort_request) {
		return ctx->upstream_extensions.abort_request(r, ctx->callback_data);
	}
	return;
}

static void
ngx_indep_subreq_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_http_get_module_ctx(r, ngx_indep_subreq);

	if (ctx->main_req_gone) {
		return;
	}

	if (ctx->callback) {
		ctx->callback(&r->upstream->buffer, r, rc, ctx->callback_data);
	}

	return;
}

ngx_int_t
ngx_indep_subreq_init_upstream(
		ngx_http_request_t *r, 
		ngx_url_t *url,
		ngx_indep_subreq_fetch_callback_pt callback,
		void *callback_data,
		ngx_indep_subreq_upstream_callbacks_t *upstream_extensions)
{
	ngx_http_upstream_t *u;
	ngx_indep_subreq_conf_t 	*iscc;

	iscc = global_conf;
	if (!iscc) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* allocate and initialize the upstream data structures */
	u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
	if (u == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	r->upstream = u;

	u->schema = iscc->schema;
	u->conf = &iscc->uc;

	/* setting up this log is what makes adding the read event in the
	 * request's connection->log necessary
	 */
	u->peer.log = r->connection->log;
	u->peer.log_error = NGX_ERROR_ERR;

	/* output tags are used for debugging, perhaps? */
	u->output.tag = (ngx_buf_tag_t) &ngx_indep_subreq;

	u->create_request = ngx_indep_subreq_create_request;
	u->reinit_request = ngx_indep_subreq_reinit_request;
	u->process_header = ngx_indep_subreq_process_header;
	u->abort_request = ngx_indep_subreq_abort_request;
	u->finalize_request = ngx_indep_subreq_finalize_request;

	u->buffering = 1; /*buffer everything in memory? might not be necessary */

	/* input pipe that abstracts the read events */
	u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
	if (u->pipe == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	/* we don't need to do anything special with the input as it arrives, so
	 * we're using the pipe_copy filter.  It's worth mentioning that this
	 * doesn't actually copy the data, it just creates shadow buffers of the 
	 * data read in.  
	 */
	u->pipe->input_filter = ngx_event_pipe_copy_input_filter;

	ngx_indep_subreq_ctx_t *ctx;
	ctx = ngx_pcalloc(r->pool, sizeof(ngx_indep_subreq_ctx_t));
	if (!ctx) {
		return NGX_ERROR;
	}
	ngx_http_set_ctx(r, ctx, ngx_indep_subreq);

	ctx->upstream_url = *url;
	ctx->callback = callback;
	ctx->callback_data = callback_data;
	if (upstream_extensions) {
		ctx->upstream_extensions = *upstream_extensions;
	}

	ngx_http_upstream_init(r);
	return NGX_OK;
}

ngx_int_t
ngx_indep_subreq_fetch (
		ngx_http_request_t *r,
		ngx_pool_t *pool, 
		ngx_url_t *url, 
		ngx_indep_subreq_fetch_callback_pt callback,
		void *callback_data,
		ngx_indep_subreq_upstream_callbacks_t *upstream_extensions)
{

	ngx_http_request_t 		*subreq;

	subreq = ngx_indep_subreq_fake_request();

	/* this is total junk -- I need to set up the configuration somehow
	 * so the various modules don't segfault.  However, I don't know how
	 * to properly do that, so, in the interim, I'll just steal the config
	 * from another request.  Lame.
	 */
	subreq->loc_conf = r->loc_conf;
	subreq->main_conf = r->main_conf;
	ngx_list_t *h = ngx_list_create(subreq->pool, 20, 48);
	subreq->headers_out.headers = *h;

	subreq->variables = r->variables;

	return ngx_indep_subreq_init_upstream(subreq, url,
			callback, callback_data, upstream_extensions);
}

static ngx_int_t ngx_indep_subreq_postconf (ngx_conf_t *cf)
{
	ngx_indep_subreq_conf_t 		*iscc;
	ngx_http_upstream_conf_t 		*uc;
    ngx_int_t 						rc;

	global_conf = ngx_pcalloc(cf->pool, sizeof(ngx_indep_subreq_conf_t));
	iscc = global_conf;

	/* schema doesn't really seem to affect anything, but makes for nice
	 * log output
	 */
	iscc->schema.len = sizeof("unknown://") - 1;
	iscc->schema.data = (u_char *) "unknown://";

	iscc->uc.upstream = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_srv_conf_t));

	uc = &iscc->uc;
	/* this is the important part: the ngx_indep_subreq_peer_init function sets up the 
	 * get_peer() function which figures out where to proxy based on the
	 * sessionkey*/
	uc->upstream->peer.init = ngx_indep_subreq_peer_init; 

	uc->connect_timeout = NGX_INDEP_SUBREQ_CONNECT_TIMEOUT;
	uc->send_timeout = NGX_INDEP_SUBREQ_SEND_TIMEOUT;
	uc->read_timeout = NGX_INDEP_SUBREQ_READ_TIMEOUT;
	uc->timeout = NGX_INDEP_SUBREQ_TIMEOUT;

	/* buffer sizes taken from the proxy module */
	uc->buffering = 1;
	uc->buffer_size = ngx_pagesize;
	uc->bufs.num = 8;
	uc->bufs.size = ngx_pagesize;
	uc->busy_buffers_size = 2 * uc->buffer_size;

	/* initialize the hide headers hash -- this is used by the upstream
	 * module and it will segfault if the hash isn't initialized
	 */
	ngx_hash_init_t 	hash;
	hash.max_size = 8;
	hash.bucket_size = 64;
	hash.name = "ngx_indep_subreq_headers_hash";
	hash.hash = &uc->hide_headers_hash;
	hash.pool = cf->pool;
	hash.key = ngx_hash_key_lc;
	rc = ngx_hash_init(&hash, NULL, 0);
	if (rc != NGX_OK) {
		return NGX_ERROR;
	}

	return NGX_OK;
}

