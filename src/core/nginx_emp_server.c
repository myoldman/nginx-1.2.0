#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx_emp_server.h>


static ngx_uint_t     ngx_emp_server_max_module;

// emp server module
//static void *ngx_emp_server_create_conf(ngx_cycle_t *cycle);
static char *ngx_emp_server_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_log_servers_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


// emp server core module
static ngx_int_t ngx_emp_server_core_module_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_emp_server_core_process_init(ngx_cycle_t *cycle);
static void *ngx_emp_server_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_emp_server_core_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_log_servers_server(ngx_conf_t *cf, ngx_command_t *cmd,    void *conf);

// emp server  module
static ngx_command_t  ngx_emp_server_commands[] = {

    { ngx_string("log_servers"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_log_servers_block,
      0,
      0,
      NULL },
    ngx_null_command
};


static ngx_core_module_t  ngx_emp_server_module_ctx = {
    ngx_string("log_servers"),
    NULL,
    ngx_emp_server_init_conf,
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
// emp server module

// emp server core module
static ngx_str_t  emp_server_core_name = ngx_string("emp_server_core");


static ngx_command_t  ngx_emp_server_core_commands[] = {

    { ngx_string("server"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE1,
      ngx_log_servers_server,
      0,
      0,
      NULL },

      ngx_null_command
};


ngx_emp_server_module_t  ngx_emp_server_core_module_ctx = {
    &emp_server_core_name,
    ngx_emp_server_core_create_conf,            /* create configuration */
    ngx_emp_server_core_init_conf              /* init configuration */
};


ngx_module_t  ngx_emp_server_core_module = {
    NGX_MODULE_V1,
    &ngx_emp_server_core_module_ctx,            /* module context */
    ngx_emp_server_core_commands,               /* module directives */
    NGX_EMP_SERVER_MODULE,                      /* module type */
    NULL,                                  /* init master */
    ngx_emp_server_core_module_init,                 /* init module */
    ngx_emp_server_core_process_init,                /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};
// emp core module


static char *
ngx_log_servers_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_servers_block\n");
    char                 *rv;
    void               ***ctx;
    ngx_uint_t            i;
    ngx_conf_t            pcf;
    ngx_emp_server_module_t   *m;

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
	printf("called:ngx_log_servers_block OK\n");
    return NGX_CONF_OK;
}


static char *
ngx_log_servers_server(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_servers_server\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;
    ngx_url_t                    u;
    ngx_emp_server_t  *emp_server;

    if (escf->servers == NULL) {
        escf->servers = ngx_array_create(cf->pool, 4,
                                         sizeof(ngx_emp_server_t));
        if (escf->servers == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    emp_server = ngx_array_push(escf->servers);
    if (emp_server == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(emp_server, sizeof(ngx_emp_server_t));

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
	emp_server->addrs = u.addrs;
	emp_server->naddrs = u.naddrs;
	printf("called:ngx_log_servers_server OK\n");
    return NGX_CONF_OK;
}



static char *ngx_emp_server_init_conf(ngx_cycle_t *cycle, void *conf)
{
	printf("called:ngx_emp_server_init_conf\n");
	if (ngx_get_conf(cycle->conf_ctx, ngx_emp_server_module) == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no \"log_servers\" section in configuration");
        return NGX_CONF_ERROR;
    }
	printf("called:ngx_emp_server_init_conf OK\n");
	return NGX_CONF_OK;
}

static void *
ngx_emp_server_core_create_conf(ngx_cycle_t *cycle)
{
	printf("called:ngx_emp_server_core_create_conf\n");
    ngx_emp_server_conf_t  *ecf;

    ecf = ngx_palloc(cycle->pool, sizeof(ngx_emp_server_conf_t));
    if (ecf == NULL) {
        return NULL;
    }
	ecf->name = (void *) NGX_CONF_UNSET;
	printf("called:ngx_emp_server_core_create_conf OK\n");
    return ecf;
}


static char *
ngx_emp_server_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
	printf("called:ngx_emp_server_core_init_conf\n");
    ngx_emp_server_conf_t  *ecf = conf;

    ngx_int_t            i;
    ngx_module_t        *module;
    ngx_emp_server_module_t  *emp_server_module_temp;

    module = NULL;

    for (i = 0; ngx_modules[i]; i++) {
     	if (ngx_modules[i]->type != NGX_EMP_SERVER_MODULE) {
            continue;
        }

        emp_server_module_temp = ngx_modules[i]->ctx;
        module = ngx_modules[i];
        break;
    }

    if (module == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "no emp_server module found");
        return NGX_CONF_ERROR;
    }
	
    ngx_conf_init_ptr_value(ecf->name, emp_server_module_temp->name->data);
	printf("called:ngx_emp_server_core_init_conf OK\n");
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_emp_server_core_module_init(ngx_cycle_t *cycle)
{
	printf("called:ngx_emp_server_module_init\n");
    void              ***cf;
    ngx_emp_server_conf_t    *ecf;
	ngx_emp_server_t *server;
	ngx_uint_t i,j;
	char *server_addr;
	in_port_t port;
	
    cf = ngx_get_conf(cycle->conf_ctx, ngx_emp_server_module);
    ecf = (*cf)[ngx_emp_server_core_module.ctx_index];
		
    if (!ngx_test_config && ngx_process <= NGX_PROCESS_MASTER) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                      "using the \"%s\" emp server method", ecf->name);
    }

	server = ecf->servers->elts;
	for(i = 0; i< ecf->servers->nelts; i++) {
		for (j = 0; j < server[i].naddrs; j++) {
			server_addr = inet_ntoa(((struct sockaddr_in*)server[i].addrs[j].sockaddr)->sin_addr);
			port = ntohs(((struct sockaddr_in*)server[i].addrs[j].sockaddr)->sin_port);
			printf("server is %s:%d\n", server_addr, port);
        }
	}
	printf("called:ngx_emp_server_module_init OK\n");
    return NGX_OK;
}

static ngx_int_t
ngx_emp_server_core_process_init(ngx_cycle_t *cycle)
{
	printf("called:ngx_emp_server_process_init\n");
    ngx_core_conf_t     *ccf;
    ngx_emp_server_conf_t    *ecf;
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    ecf = ngx_emp_server_get_conf(cycle->conf_ctx, ngx_emp_server_core_module);

    if (ccf->master && ccf->worker_processes > 1) {
    } else {
    }
	printf("called:ngx_emp_server_process_init OK\n");
    return NGX_OK;
}




