
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <locale.h>
#include <zlib.h>
#include <nginx_emp_server.h>

/*
static int gzip_uncompress(char *bufin, int lenin, char *bufout, int lenout)
{
        z_stream d_stream;
        int result;

        memset(bufout, '\0', lenout);
        d_stream.zalloc = NULL;
        d_stream.zfree  = NULL;
        d_stream.opaque = NULL;

        result = inflateInit2(&d_stream, MAX_WBITS + 16);
        d_stream.next_in   = (Byte*)bufin;
        d_stream.avail_in  = lenin;
        d_stream.next_out  = (Byte*)bufout;
        d_stream.avail_out = lenout;

        inflate(&d_stream, Z_SYNC_FLUSH);
        inflateEnd(&d_stream);
        return result;
}
*/


static ngx_int_t ngx_http_write_filter_init(ngx_conf_t *cf);


static ngx_http_module_t  ngx_http_write_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_write_filter_init,            /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL,                                  /* merge location configuration */
};


ngx_module_t  ngx_http_write_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_write_filter_module_ctx,     /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_int_t
ngx_http_write_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    off_t                      size, sent, nsent, limit;
    ngx_uint_t                 last, flush;
    ngx_msec_t                 delay;
    ngx_chain_t               *cl, *ln, **ll, *chain;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;

    if (c->error) {
        return NGX_ERROR;
    }

    size = 0;
    flush = 0;
    last = 0;
    ll = &r->out;

    /* find the size, the flush point and the last link of the saved chain */

    for (cl = r->out; cl; cl = cl->next) {
        ll = &cl->next;

        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "write old buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %z",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);

#if 1
        if (ngx_buf_size(cl->buf) == 0 && !ngx_buf_special(cl->buf)) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "zero size buf in writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          cl->buf->temporary,
                          cl->buf->recycled,
                          cl->buf->in_file,
                          cl->buf->start,
                          cl->buf->pos,
                          cl->buf->last,
                          cl->buf->file,
                          cl->buf->file_pos,
                          cl->buf->file_last);

            ngx_debug_point();
            return NGX_ERROR;
        }
#endif

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    /* add the new chain to the existent one */

    for (ln = in; ln; ln = ln->next) {
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = ln->buf;
        *ll = cl;
        ll = &cl->next;
		//if(r->headers_out.content_type.data)
			//printf("response content type is %s\n", r->headers_out.content_type.data);
		if(r->headers_out.content_type.data && r->app_id && strlen(r->app_id) != 0 &&
			r->verify_code && strlen((const char*)r->verify_code) != 0 &&
			( !ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"text", 4) 
			|| !ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"application/xml", 15) 
			|| !ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"application/json", 16) )
			&& !cl->buf->in_file && c->log->action && strcmp( c->log->action, "sending to client") == 0 && ngx_buf_size(cl->buf) > 2) {
			int buf_size = ngx_buf_size(cl->buf);
			int body_grow_step = ngx_emp_server_body_grow_step();
			int body_max_multiple = ngx_emp_server_body_max_multiple();
			int new_mod = (r->connection->body_out_byte + buf_size + 1) / (1024 * body_grow_step);
			int old_mod = r->connection->body_out_byte / (1024 * body_grow_step);
			if(new_mod < body_max_multiple ){
				if(r->connection->body_out == NULL) {
					r->connection->body_out = ngx_create_temp_buf(r->connection->pool, 1024 * body_grow_step );
					ngx_memzero(r->connection->body_out->start, 1024 * body_grow_step);
				}
				if( new_mod > old_mod ) {
					ngx_buf_t *temp_buf = r->connection->body_out;
					r->connection->body_out = ngx_create_temp_buf(r->connection->pool, 1024 * body_grow_step * (new_mod + 1));
					ngx_memzero(r->connection->body_out->start, 1024 * body_grow_step * (new_mod + 1));
					ngx_memcpy(r->connection->body_out->last, temp_buf->pos, (size_t) r->connection->body_out_byte);
					r->connection->body_out->last += (size_t) r->connection->body_out_byte;
					printf("byte send now is %d need to enlarge \n", new_mod);
					ngx_pfree(r->connection->pool, temp_buf->pos);
					ngx_pfree(r->connection->pool, temp_buf);
				}
				
				if (r->headers_out.content_encoding 
				  	&& r->headers_out.content_encoding->value.len
				  	&& !ngx_strcasecmp(r->headers_out.content_encoding->value.data, (u_char *)"gzip"))
				{
					r->connection->is_body_gzip = 1;
				}
				ngx_memcpy(r->connection->body_out->last, cl->buf->pos, (size_t) buf_size);
		       	r->connection->body_out->last += (size_t) buf_size;
				r->connection->body_out_byte += buf_size;
				printf("byte send now is %zd \n", r->connection->body_out_byte);
			} else {
				printf("byte send now is %zd overflow\n", r->connection->body_out_byte + buf_size );
			}
			
		}
        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "write new buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %z",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);

#if 1
        if (ngx_buf_size(cl->buf) == 0 && !ngx_buf_special(cl->buf)) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "zero size buf in writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          cl->buf->temporary,
                          cl->buf->recycled,
                          cl->buf->in_file,
                          cl->buf->start,
                          cl->buf->pos,
                          cl->buf->last,
                          cl->buf->file,
                          cl->buf->file_pos,
                          cl->buf->file_last);

            ngx_debug_point();
            return NGX_ERROR;
        }
#endif

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    *ll = NULL;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter: l:%d f:%d s:%O", last, flush, size);
	for (cl = r->out; cl; cl = cl->next) {
		if(strcmp( c->log->action, "sending to client") == 0 && ngx_buf_size(cl->buf) > 10) {
			//printf("action is %d \n", cl->buf->last - cl->buf->pos);
		 	ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "my write new buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %z",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
		 /*
		 FILE *fp;			
		 char filename[64] = {0};			
		 sprintf(filename, "%ld%d", r->start_sec, r->start_msec);			
		 fp=fopen(filename,"at");
		 u_char *p;			
		 p = cl->buf->pos;			
		 while(p != cl->buf->last) {				
		 	p++;				
			if(*p != CR && *p != LF && *p != '\0'){					
				fputc(*p,fp);
			}			
		 }
		 fclose(fp);
		 */
		}
		
	}
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /*
     * avoid the output if there are no last buf, no flush point,
     * there are the incoming bufs and the size of all bufs
     * is smaller than "postpone_output" directive
     */

    if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
        return NGX_OK;
    }

    if (c->write->delayed) {
        c->buffered |= NGX_HTTP_WRITE_BUFFERED;
        return NGX_AGAIN;
    }

    if (size == 0 && !(c->buffered & NGX_LOWLEVEL_BUFFERED)) {
        if (last) {
            r->out = NULL;
            c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

            return NGX_OK;
        }

        if (flush) {
            do {
                r->out = r->out->next;
            } while (r->out);

            c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

            return NGX_OK;
        }

        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "the http output chain is empty");

        ngx_debug_point();

        return NGX_ERROR;
    }

    if (r->limit_rate) {
        limit = r->limit_rate * (ngx_time() - r->start_sec + 1)
                - (c->sent - clcf->limit_rate_after);

        if (limit <= 0) {
            c->write->delayed = 1;
            ngx_add_timer(c->write,
                          (ngx_msec_t) (- limit * 1000 / r->limit_rate + 1));

            c->buffered |= NGX_HTTP_WRITE_BUFFERED;

            return NGX_AGAIN;
        }

        if (clcf->sendfile_max_chunk
            && (off_t) clcf->sendfile_max_chunk < limit)
        {
            limit = clcf->sendfile_max_chunk;
        }

    } else {
        limit = clcf->sendfile_max_chunk;
    }

    sent = c->sent;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter limit %O", limit);

    chain = c->send_chain(c, r->out, limit, 0);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter %p", chain);

    if (chain == NGX_CHAIN_ERROR) {
        c->error = 1;
        return NGX_ERROR;
    }

    if (r->limit_rate) {

        nsent = c->sent;

        if (clcf->limit_rate_after) {

            sent -= clcf->limit_rate_after;
            if (sent < 0) {
                sent = 0;
            }

            nsent -= clcf->limit_rate_after;
            if (nsent < 0) {
                nsent = 0;
            }
        }

        delay = (ngx_msec_t) ((nsent - sent) * 1000 / r->limit_rate);

        if (delay > 0) {
            limit = 0;
            c->write->delayed = 1;
            ngx_add_timer(c->write, delay);
        }
    }

    if (limit
        && c->write->ready
        && c->sent - sent >= limit - (off_t) (2 * ngx_pagesize))
    {
        c->write->delayed = 1;
        ngx_add_timer(c->write, 1);
    }

    for (cl = r->out; cl && cl != chain; /* void */) {
        ln = cl;
        cl = cl->next;
        ngx_free_chain(r->pool, ln);
    }

    r->out = chain;

    if (chain) {
        c->buffered |= NGX_HTTP_WRITE_BUFFERED;
        return NGX_AGAIN;
    }

    c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

    if ((c->buffered & NGX_LOWLEVEL_BUFFERED) && r->postponed == NULL) {
        return NGX_AGAIN;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_write_filter_init(ngx_conf_t *cf)
{
    ngx_http_top_body_filter = ngx_http_write_filter;

    return NGX_OK;
}
