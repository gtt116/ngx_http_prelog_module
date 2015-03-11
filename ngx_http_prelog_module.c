/*
 * Copyright (C) Tiantian Gao
 * Copyright (C) NetEase, Inc.
 *
 *
 * Logging in the end of ACCESS_PHASE.
 *
 * Configuration:
 *
 *  Syntax: prelog path;
 *          prelog path [format];
 *          prelog off;
 *  Default: off;
 *  Context: http, server, location
 *
 * Sometimes upstream server may block, so nginx can't wait for response to
 * logging. This module will do logging before dive into upstream. Note that it
 * only logging before any internal redirect.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_prelog_module.h"


extern ngx_module_t             ngx_http_log_module;



typedef struct {
    ngx_open_file_t            *file;
    ngx_http_log_fmt_t         *format;
    ngx_uint_t                  enable; /* default: 0 */
    time_t                      error_log_time;
} ngx_http_prelog_loc_conf_t;


static char * ngx_http_prelog_set_log(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf);

static void* ngx_http_prelog_create_loc_conf(ngx_conf_t* f);
static char* ngx_http_prelog_merge_loc_conf(ngx_conf_t* cf, void* parent,
        void* child);
static ngx_int_t ngx_http_prelog_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_prelog_handler(ngx_http_request_t* r);


static ngx_command_t ngx_http_prelog_commands[] = {
    { ngx_string("prelog"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_http_prelog_set_log,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};


static ngx_http_module_t ngx_http_prelog_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_prelog_init,                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_prelog_create_loc_conf,        /* create location configuration */
    ngx_http_prelog_merge_loc_conf,         /* merge location configuration */
};


ngx_module_t ngx_http_prelog_module = {
    NGX_MODULE_V1,
    &ngx_http_prelog_module_ctx,
    ngx_http_prelog_commands,
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


static ngx_int_t
ngx_http_prelog_init(ngx_conf_t *cf){
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_prelog_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_prelog_handler(ngx_http_request_t* r){

    ngx_http_prelog_loc_conf_t *prelog_loc_conf;
    size_t                      len;
    ssize_t                     ret;
    u_char                      *line, *p;
    ngx_http_log_op_t           *op;
    ngx_uint_t                  i;
    time_t                      now;
    ngx_err_t                   err;
    u_char                      *name;

    prelog_loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_prelog_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http prelog handler");

    if (prelog_loc_conf->enable == 0){
        return NGX_DECLINED;
    }

    if (r->internal == 1){
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "http prelog hit internal redirect, pass");
        return NGX_DECLINED;
    }

    len = 0;
    op = prelog_loc_conf->format->ops->elts;
    for (i = 0; i < prelog_loc_conf->format->ops->nelts; i++) {
        if (op[i].len == 0) {
            len += op[i].getlen(r, op[i].data);

        } else {
            len += op[i].len;
        }
    }
    len += NGX_LINEFEED_SIZE;

    line = ngx_pnalloc(r->pool, len);
    if (line == NULL) {
        return NGX_ERROR;
    }
    p = line;

    for (i = 0; i < prelog_loc_conf->format->ops->nelts; i++) {
        p = op[i].run(r, p, &op[i]);
    }

    ngx_linefeed(p);
    ret = ngx_write_fd(prelog_loc_conf->file->fd, line, p - line);

    name = prelog_loc_conf->file->name.data;
    now = ngx_time();

    if (ret == (ssize_t)len){
        goto ok;
    }

    if (ret == -1) {
        err = ngx_errno;

        if (now - prelog_loc_conf->error_log_time > 59) {
            ngx_log_error(NGX_LOG_ALERT, r->connection->log, err,
                          ngx_write_fd_n " to \"%s\" failed", name);

            prelog_loc_conf->error_log_time = now;
        }
        goto ok;
    }

    if (now - prelog_loc_conf->error_log_time > 59) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      ngx_write_fd_n " to \"%s\" was incomplete: %z of %uz",
                      name, ret, len);

        prelog_loc_conf->error_log_time = now;
    }

ok:
    return NGX_DECLINED;
}


static void *
ngx_http_prelog_create_loc_conf(ngx_conf_t* cf){
    ngx_http_prelog_loc_conf_t* conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_prelog_loc_conf_t));
    if (conf == NULL){
        return NGX_CONF_ERROR;
    }
    conf->enable = NGX_CONF_UNSET;
    conf->file = NGX_CONF_UNSET_PTR;
    conf->format= NGX_CONF_UNSET_PTR;

    return conf;
}


static char *
ngx_http_prelog_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child){
    ngx_http_prelog_loc_conf_t* prev = parent;
    ngx_http_prelog_loc_conf_t* conf = child;

    ngx_conf_merge_uint_value(conf->enable, prev->enable, 0);

    if (conf->file == NGX_CONF_UNSET_PTR){
        conf->file = prev->file;
        conf->format = prev->format;
    }
    conf->error_log_time = 0;

    return NGX_CONF_OK;
}

static char *
ngx_http_prelog_set_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {

    ngx_uint_t                  i;
    ngx_str_t                  *value, name;
    ngx_http_log_main_conf_t   *lmcf;
    ngx_http_log_fmt_t         *fmt;

    ngx_http_prelog_loc_conf_t *preloglc = conf;
    value = cf->args->elts;

    /* prelog off; */
    if (ngx_strcmp(value[1].data, "off") == 1) {
        preloglc->enable = 0;
        if (cf->args->nelts == 2) {
            return NGX_CONF_OK;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[2]);
        return NGX_CONF_ERROR;
    } else {
        preloglc->enable = 1;
    }

    /* prelog path; */

    preloglc->file = ngx_conf_open_file(cf->cycle, &value[1]);
    if (preloglc->file == NULL) {
        return NGX_CONF_ERROR;
    }

    /* prelog path format; */
    if (cf->args->nelts == 3) {
        name = value[2];
    } else {
        ngx_str_set(&name, "combined");
    }

    lmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_log_module);
    fmt = lmcf->formats.elts;
    for (i = 0; i < lmcf->formats.nelts; i++) {
        if (fmt[i].name.len == name.len
            && ngx_strcasecmp(fmt[i].name.data, name.data) == 0)
        {
            preloglc->format = &fmt[i];
            return NGX_CONF_OK;
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "unknown log format \"%V\"", &name);
    return NGX_CONF_ERROR;
}
