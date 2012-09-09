#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx_emp_server.h>


static ngx_uint_t     ngx_emp_server_max_module;
static char *ngx_log_servers_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_servers_server(ngx_conf_t *cf, ngx_command_t *cmd,    void *conf);
static void *ngx_emp_server_create_conf(ngx_cycle_t *cycle);
static char *ngx_emp_server_init_conf(ngx_cycle_t *cycle, void *conf);


static ngx_command_t  ngx_emp_server_commands[] = {

    { ngx_string("log_servers"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE1,
      ngx_log_servers_block,
      0,
      0,
      NULL },

    { ngx_string("server"),
      NGX_EMP_SERVER_CONF|NGX_CONF_1MORE,
      ngx_log_servers_server,
      0,
      0,
      NULL },

    ngx_null_command
};


static ngx_core_module_t  ngx_emp_server_module_ctx = {
    ngx_string("emp_server"),
    ngx_emp_server_create_conf,
    ngx_emp_server_init_conf
};


ngx_module_t  ngx_emp_server_module = {
    NGX_MODULE_V1,
    &ngx_emp_server_module_ctx,                /* module context */
    ngx_emp_server_commands,                   /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *
ngx_log_servers_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                 *rv;
    void               ***ctx;
    ngx_uint_t            i;
    ngx_conf_t            pcf;
    ngx_emp_server_module   *m;

    /* count the number of the event modules and set up their indices */

    ngx_emp_server_max_module = 0;
    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EMP_SERVER_MODULE) {
            continue;
        }

        ngx_modules[i]->ctx_index = ngx_emp_server_max_module++;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(void *));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *ctx = ngx_pcalloc(cf->pool, ngx_emp_server_max_module * sizeof(void *));
    if (*ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *(void **) conf = ctx;

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EMP_SERVER_MODULE) {
            continue;
        }

        m = ngx_modules[i]->ctx;

        if (m->create_conf) {
            (*ctx)[ngx_modules[i]->ctx_index] = m->create_conf(cf->cycle);
            if ((*ctx)[ngx_modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    pcf = *cf;
    cf->ctx = ctx;
    cf->module_type = NGX_EMP_SERVER_MODULE;
    cf->cmd_type = NGX_EMP_SERVER_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv != NGX_CONF_OK)
        return rv;

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EMP_SERVER_MODULE) {
            continue;
        }

        m = ngx_modules[i]->ctx;

        if (m->init_conf) {
            rv = m->init_conf(cf->cycle, (*ctx)[ngx_modules[i]->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_log_servers_server(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{

	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;
    ngx_url_t                    u;
    ngx_addr_t  *addr;

    if (escf->servers == NULL) {
        escf->servers = ngx_array_create(cf->pool, 4,
                                         sizeof(ngx_addr_t));
        if (escf->servers == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    addr = ngx_array_push(escf->servers);
    if (addr == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(addr, sizeof(ngx_addr_t));

    value = cf->args->elts;

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.default_port = 80;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }


    addr = u.addrs;

    return NGX_CONF_OK;
}

static void *
ngx_emp_server_create_conf(ngx_cycle_t *cycle)
{
    ngx_emp_server_conf_t  *escf;

    escf = ngx_palloc(cycle->pool, sizeof(ngx_emp_server_conf_t));
    if (escf == NULL) {
        return NULL;
    }

	//escf->servers = ngx_array_create(cycle->pool, 1,
    //                                     sizeof(ngx_addr_t));
    //if (escf->servers == NULL) {
    //        return NULL;
    //}
	
    return escf;
}

static char *ngx_emp_server_init_conf(ngx_cycle_t *cycle, void *conf)
{
	return NGX_CONF_OK;
}


