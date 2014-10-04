/*
 * Logging before filter into upstream
 *
 * To compile in nginx:
 *   $ ./configure --add-module=/your/module/path/ngx_http_prelog_module
 *   $ make
 *
 * Configuration:
 *
 *  Syntax: prelog on | off;
 *  Default: off;
 *  Context: http, server, location
 *
 *  Enable prelog will logging before into NGX_CONTENT_PHASE.
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_flag_t enable;
} ngx_http_prelog_loc_conf_t;


static void* ngx_http_prelog_create_loc_conf(ngx_conf_t* f);
static char* ngx_http_prelog_merge_loc_conf(ngx_conf_t* cf, void* parent,
        void* child);
static ngx_int_t ngx_http_prelog_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_prelog_handler(ngx_http_request_t* r);


static ngx_command_t ngx_http_prelog_commands[] = {
    { ngx_string("prelog"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_prelog_loc_conf_t, enable),
      NULL },
    ngx_null_command
};


static ngx_http_module_t ngx_http_prelog_module_ctx = {
    NULL,
    ngx_http_prelog_init,         /* postconfiguration */
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_prelog_create_loc_conf,
    ngx_http_prelog_merge_loc_conf,
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

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_prelog_handler;

    return NGX_OK;
}

static ngx_int_t
ngx_http_prelog_handler(ngx_http_request_t* r){
    ngx_http_prelog_loc_conf_t *prelog_loc_conf;
    prelog_loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_prelog_module);

    // if internal redirect, not prelog
    if (!prelog_loc_conf->enable || r->internal == 1){
        return NGX_DECLINED;
    }

    ngx_http_log_handler(r);

    return NGX_DECLINED;
}

static void*
ngx_http_prelog_create_loc_conf(ngx_conf_t* cf){
    ngx_http_prelog_loc_conf_t* conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_prelog_loc_conf_t));
    if (conf == NULL){
        return NGX_CONF_ERROR;
    }
    conf->enable = NGX_CONF_UNSET;

    return conf;
}


static char*
ngx_http_prelog_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child){
    ngx_http_prelog_loc_conf_t* prev = parent;
    ngx_http_prelog_loc_conf_t* conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    return NGX_CONF_OK;
}
