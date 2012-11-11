/*
 * Copyright (c) 2012, flygoast <flygoast@126.com>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_http_complex_value_t    filename;
} ngx_http_static_delete_loc_conf_t;

static char *ngx_http_static_delete(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void *ngx_http_static_delete_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_static_delete_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_static_delete_send_response(ngx_http_request_t *r, 
    ngx_str_t *file);
u_char *ngx_http_map_filename_to_path(ngx_http_request_t *r, 
    ngx_str_t *filename, ngx_str_t *path);

static ngx_command_t ngx_http_static_delete_commands[] = {

    { ngx_string("static_delete"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_static_delete,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};

static ngx_http_module_t ngx_http_static_delete_module_ctx = {
    NULL,                                   /* preconfiguration */
    NULL,                                   /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_static_delete_create_loc_conf, /* create location configuration */
    NULL                                    /* merge location configuration */
};

ngx_module_t ngx_http_static_delete_module = {
    NGX_MODULE_V1,
    &ngx_http_static_delete_module_ctx,
    ngx_http_static_delete_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static char ngx_http_static_delete_success_page_top[] = 
"<html>" CRLF
"<head><title>Successful deleted</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>Successfull deleted</h1>" CRLF;

static char ngx_http_static_delete_success_page_tail[] = 
CRLF "</center>" CRLF
"<hr><center>" NGINX_VER "</center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static void *
ngx_http_static_delete_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_static_delete_loc_conf_t *conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_static_delete_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    return conf;
}

static char *
ngx_http_static_delete(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_static_delete_loc_conf_t   *sdlcf = conf;
    ngx_http_compile_complex_value_t     ccv;
    ngx_http_core_loc_conf_t            *clcf;
    ngx_str_t                           *value;

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &sdlcf->filename;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_static_delete_handler;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_static_delete_handler(ngx_http_request_t *r)
{
    u_char                              *last;
    ngx_int_t                            rc;
    ngx_str_t                            filename;
    ngx_str_t                            path;
    size_t                               root;
    ngx_http_static_delete_loc_conf_t   *sdlcf;

    sdlcf = ngx_http_get_module_loc_conf(r, ngx_http_static_delete_module);

    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK && rc != NGX_AGAIN) {
        return rc;
    }

    rc = ngx_http_complex_value(r, &sdlcf->filename, &filename);
    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    if (filename.data[filename.len - 1] == '/') {
        return NGX_HTTP_NOT_FOUND;
    }

    if (ngx_strstr(filename.data, "./")) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (ngx_strstr(filename.data, "../")) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    last = ngx_http_map_filename_to_path(r, &filename, &path);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    path.len = last - path.data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, 
                   "http filename: \"%s\"", path.data);

    if (ngx_delete_file(path.data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno, 
                ngx_delete_file_n " \"%s\" failed", path.data);
        return NGX_HTTP_NOT_FOUND;
    }

    return ngx_http_static_delete_send_response(r, &path);
}

static ngx_int_t
ngx_http_static_delete_send_response(ngx_http_request_t *r, ngx_str_t *path)
{
    ngx_chain_t      out;
    ngx_buf_t       *b;
    ngx_int_t        rc;
    size_t           len;

    len = sizeof(ngx_http_static_delete_success_page_top) - 1
        + sizeof(ngx_http_static_delete_success_page_tail) - 1
        + sizeof("file: ") - 1
        + path->len;

    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *)"text/html";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;
    
    if (r->method == NGX_HTTP_HEAD) {
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->last = ngx_cpymem(b->last, ngx_http_static_delete_success_page_top,
            sizeof(ngx_http_static_delete_success_page_top) - 1);
    b->last = ngx_cpymem(b->last, "file: ", sizeof("file: ") - 1);
    b->last = ngx_cpymem(b->last, path->data, path->len);
    b->last = ngx_cpymem(b->last, ngx_http_static_delete_success_page_tail,
            sizeof(ngx_http_static_delete_success_page_tail) - 1);
    b->last_buf = 1;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

u_char *
ngx_http_map_filename_to_path(ngx_http_request_t *r, ngx_str_t *filename,
    ngx_str_t *path)
{
    u_char                    *last;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->alias) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "At present \"alias\" not supported in static delete "
                      "module");
        return NULL;
    }

    if (clcf->root_lengths == NULL) {
        path->len = clcf->root.len + filename->len + 2;
        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }

        last = ngx_copy(path->data, clcf->root.data, clcf->root.len);
    } else {

        if (ngx_http_script_run(r, path, clcf->root_lengths->elts,
                                filename->len + 2, clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }

        if (ngx_conf_full_name((ngx_cycle_t *)ngx_cycle, path, 0) != NGX_OK) {
            return NULL;
        }

        last = path->data + (path->len - filename->len - 2);
    }

    if (filename->data[0] != '/') {
        *last++ = '/';
    }
    last = ngx_cpystrn(last, filename->data, filename->len + 1);
    return last;
}
