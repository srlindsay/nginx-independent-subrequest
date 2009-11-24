/*
 * The basis for this code was taken from Piotr Sikora's ngx-supervisord
 * module.  Since make_fake_request is almost entirely unchanged from
 * his original version, it retains the original license, reproduced
 * below:
 *
 * Copyright (c) 2009, FRiCKLE Piotr Sikora <info@frickle.com>
 * All rights reserved.
 * 
 * This project was fully funded by megiteam.pl.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FRiCKLE PIOTR SIKORA AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL FRiCKLE PIOTR
 * SIKORA OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ngx_indep_subreq.h"
#include <nginx.h>


static ngx_chain_t *
ngx_indep_subreq_send_chain_override(ngx_connection_t *c, ngx_chain_t *in,  off_t limit)
{
	return NULL;
}

ngx_http_request_t*
ngx_indep_subreq_fake_request(void)
{
    ngx_connection_t              *c;
    ngx_http_request_t            *r;
    ngx_log_t                     *log;
    ngx_http_log_ctx_t            *ctx;
	ngx_pool_t                    *req_pool;

	req_pool = ngx_create_pool(8192, ngx_cycle->log);
	if (!req_pool) {
		return NULL;
	}

    /* fake incoming connection */
    c = ngx_pcalloc(req_pool, sizeof(ngx_connection_t));
    if (c == NULL) {
        goto failed_none;
    }

    c->pool = ngx_create_pool(1024, ngx_cycle->log);
    if (c->pool == NULL) {
        goto failed_none;
    }

    log = ngx_pcalloc(c->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        goto failed_conn;
    }

    ctx = ngx_pcalloc(c->pool, sizeof(ngx_http_log_ctx_t));
    if (ctx == NULL) {
        goto failed_conn;
    }

    /* fake incoming request */
    r = ngx_pcalloc(c->pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        goto failed_conn;
    }

    r->pool = req_pool;
    if (r->pool == NULL) {
        goto failed_conn;
    }

    ctx->connection = c;
    ctx->request = r;
    ctx->current_request = r;

    log->action = "initializing fake request";
    log->data = ctx;
    log->file = ngx_cycle->new_log.file;
	/*
    log->log_level = NGX_LOG_DEBUG_CONNECTION
                   | NGX_LOG_DEBUG_ALL;
				   */
	log->log_level = NGX_LOG_NOTICE;

    c->log = log;
    c->log_error = NGX_ERROR_INFO;
    c->pool->log = log;
    r->pool->log = log;

    c->fd = -1;
    c->data = r;

	c->send_chain = ngx_indep_subreq_send_chain_override;

    r->main = r;
    r->connection = c;

#if (nginx_version >= 8011)
    r->count = 1;
#endif

    /* used by ngx_http_upstream_init */
    c->read = ngx_pcalloc(c->pool, sizeof(ngx_event_t));
    if (c->read == NULL) {
        goto failed_conn;
    }
	c->read->log = log;

    c->write = ngx_pcalloc(c->pool, sizeof(ngx_event_t));
    if (c->write == NULL) {
        goto failed_conn;
    }
	c->write->log = log;

    c->write->active = 1;

    /* used by ngx_http_log_request */
    r->main_conf = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->main_conf == NULL) {
        goto failed_req;
    }

    r->main_conf[ngx_http_core_module.ctx_index] =
        ngx_pcalloc(r->pool, sizeof(ngx_http_core_main_conf_t));
    if (r->main_conf[ngx_http_core_module.ctx_index] == NULL) {
        goto failed_req;
    }

    /* used by ngx_http_copy_filter */
    r->loc_conf = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->loc_conf == NULL) {
        goto failed_req;
    }

    r->loc_conf[ngx_http_core_module.ctx_index] =
        ngx_pcalloc(r->pool, sizeof(ngx_http_core_loc_conf_t));
    if (r->loc_conf[ngx_http_core_module.ctx_index] == NULL) {
        goto failed_req;
    }

    /* used by ngx_http_output_filter */
    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        goto failed_req;
    }

    return r;

failed_req:
    ngx_destroy_pool(r->pool);

failed_conn:
    ngx_destroy_pool(c->pool);

failed_none:
    ngx_destroy_pool(req_pool);

    return NULL;
}



