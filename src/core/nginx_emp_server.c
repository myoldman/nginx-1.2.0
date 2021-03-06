#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx_emp_server.h>
#include <event2/event.h>  
#include <event2/http.h>  
#include <event2/buffer.h>  
#include <event2/http_struct.h> 
#include <emp_server.h>
#include "dprint.h"

static ngx_uint_t     ngx_emp_server_max_module;
pthread_t emp_server_thread_id;
proxy_config_t *proxy_config_process;


typedef struct request_context_s  
{  
    struct evhttp_uri *uri;  
    struct event_base *base;  
    struct evhttp_connection *connection;  
    struct evhttp_request *req;  
    struct evbuffer *buffer;
	int ok;
	int server_down;
	char verify_code[64];
} request_context_t; 

int ms_sleep(long ms);
pthread_t create_heart_beat_thread(void *(*func)(void *), void *arg);
static void *heart_beat_thread(void *arg);
void request_callback(struct evhttp_request *req, void *arg);  
int make_request(request_context_t *ctx , 
							const char * method , 
							char * output_data,int len); 
void context_free(request_context_t *ctx);
request_context_t *create_context(const char *url ,const char * method, char * output_data, int len);
int make_request(request_context_t *ctx ,const char * method, char * output_data,int len );
emp_server_t *round_robin_select_server();



// emp server module
//static void *ngx_emp_server_create_conf(ngx_cycle_t *cycle);
static char *ngx_emp_server_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_log_servers_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_heart_beat_interval(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_body_memory_grow_step(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_body_memory_max_multiple(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_body_api_verify(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);




// emp server core module
static ngx_int_t ngx_emp_server_core_module_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_emp_server_core_process_init(ngx_cycle_t *cycle);
static void ngx_emp_server_core_process_exit(ngx_cycle_t *cycle);
static void *ngx_emp_server_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_emp_server_core_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_log_servers_server(ngx_conf_t *cf, ngx_command_t *cmd,    void *conf);
static char *ngx_log_servers_appid_ip(ngx_conf_t *cf, ngx_command_t *cmd,    void *conf);


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
      { ngx_string("appid_ip"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE2,
      ngx_log_servers_appid_ip,
      0,
      0,
      NULL },
	   { ngx_string("heart_beat_interval"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE1,
      ngx_log_heart_beat_interval,
      0,
      0,
      NULL },
      { ngx_string("body_memory_grow_step"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE1,
      ngx_log_body_memory_grow_step,
      0,
      0,
      NULL },
      { ngx_string("body_memory_max_multiple"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE1,
      ngx_log_body_memory_max_multiple,
      0,
      0,
      NULL },
      { ngx_string("api_verify"),
      NGX_EMP_SERVER_CONF|NGX_CONF_TAKE1,
      ngx_log_body_api_verify,
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
    ngx_emp_server_core_process_exit,      /* exit process */
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
ngx_log_heart_beat_interval(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_heart_beat_interval\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;

	escf->heart_beat_interval = 0;
    value = cf->args->elts;
	escf->heart_beat_interval = atoi((char *)value[1].data);
	
	printf("called:ngx_log_heart_beat_interval %s OK\n", value[1].data);
    return NGX_CONF_OK;
}

static char *
ngx_log_body_memory_grow_step(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_heart_beat_interval\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;

    value = cf->args->elts;
	escf->body_memory_grow_step = atoi((char *)value[1].data);
	
	printf("called:ngx_log_body_memory_grow_step %s OK\n", value[1].data);
    return NGX_CONF_OK;
}

static char *
ngx_log_body_memory_max_multiple(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_heart_beat_interval\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;

    value = cf->args->elts;
	escf->body_memory_max_multiple = atoi((char *)value[1].data);
	
	printf("called:ngx_log_body_memory_max_multiple %s OK\n", value[1].data);
    return NGX_CONF_OK;
}

static char *
ngx_log_body_api_verify(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_heart_beat_interval\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;

    value = cf->args->elts;
	escf->api_verify = 1;
	if(strcmp((char*)value[1].data, "0") == 0 ){
		escf->api_verify = 0;
	}

	printf("called:ngx_log_body_api_verify %d OK\n", escf->api_verify);
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

static char *
ngx_log_servers_appid_ip(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	printf("called:ngx_log_servers_appid_ip\n");
	ngx_emp_server_conf_t  *escf = conf;
    ngx_str_t                   *value;
    ngx_emp_appid_ip_t  *emp_appid_ip = NULL;
	ngx_str_t *ip;

    if (escf->appid_ip_maps == NULL) {
        escf->appid_ip_maps = ngx_array_create(cf->pool, 8,
                                         sizeof(ngx_emp_appid_ip_t));
        if (escf->appid_ip_maps == NULL) {
            return NGX_CONF_ERROR;
        }
    }

	emp_appid_ip = ngx_array_push(escf->appid_ip_maps);
	if (emp_appid_ip == NULL) {
	    return NGX_CONF_ERROR;
	}
		
    ngx_memzero(emp_appid_ip, sizeof(ngx_emp_appid_ip_t));
    value = cf->args->elts;

	strncpy(emp_appid_ip->app_id, (char *)value[1].data, 64);
	emp_appid_ip->addrs = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
	if (emp_appid_ip->addrs == NULL) {
        return NGX_CONF_ERROR;
	}

	char *needle = "|";
	char *ip_addrs = (char *)value[2].data;
	char *buf = strtok( ip_addrs, needle);
	while( buf != NULL )
	{
		ip = ngx_array_push(emp_appid_ip->addrs);
		int len = strlen(buf);
		ip->data = ngx_palloc(cf->pool,len + 1);
		ip->len = len;
		strcpy((char *)ip->data, buf);
		printf( "ip is %s\n", buf);
    	buf = strtok( NULL, needle);
		
	}
	
	printf("called:ngx_log_servers_appid_ip OK\n");
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
	ecf->servers = NULL;
	ecf->appid_ip_maps = NULL;
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
	
    cf = ngx_get_conf(cycle->conf_ctx, ngx_emp_server_module);
    ecf = (*cf)[ngx_emp_server_core_module.ctx_index];
		
    if (!ngx_test_config && ngx_process <= NGX_PROCESS_MASTER) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                      "using the \"%s\" emp server method", ecf->name);
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
	ngx_emp_server_t *server;
	ngx_str_t *ips;
	ngx_emp_appid_ip_t *appid_ip;
	char *server_addr;
	in_port_t port;
	ngx_uint_t i,j;
	
    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    ecf = ngx_emp_server_get_conf(cycle->conf_ctx, ngx_emp_server_core_module);

	proxy_config_process = NULL;
	proxy_config_process = malloc(sizeof(proxy_config_t));
	memset(proxy_config_process, 0, sizeof(proxy_config_t));
	proxy_config_process->retryinterval = ecf->heart_beat_interval;
	proxy_config_process->body_grow_step = ecf->body_memory_grow_step;
	proxy_config_process->body_max_multiple = ecf->body_memory_max_multiple;
	proxy_config_process->api_verify = ecf->api_verify;
	proxy_config_process->maxretries = 3;
	strcpy(proxy_config_process->log_facility, "LOG_LOCAL1");
	proxy_config_process->log_stderr = 1;
	proxy_config_process->debug_level = 4;
	proxy_config_process->heart_beat_running = 1;
	set_log_facility(proxy_config_process->log_facility);
	set_log_stderr(proxy_config_process->log_stderr);
    set_debug_level(proxy_config_process->debug_level);
	if(proxy_config_process->body_max_multiple == 0)
		proxy_config_process->body_max_multiple = 4;
	if( proxy_config_process->body_grow_step == 0)
		proxy_config_process->body_grow_step = 20;
	
    if (ccf->master && ccf->worker_processes > 1) {
    } else {
    }
	server = ecf->servers->elts;
	for(i = 0; i< ecf->servers->nelts; i++) {
		for (j = 0; j < server[i].naddrs; j++) {
			server_addr = inet_ntoa(((struct sockaddr_in*)server[i].addrs[j].sockaddr)->sin_addr);
			port = ntohs(((struct sockaddr_in*)server[i].addrs[j].sockaddr)->sin_port);
			emp_server_t *srv;
			srv = (emp_server_t*)malloc(sizeof(emp_server_t));
			memset(srv, 0, sizeof(emp_server_t));
			strcpy(srv->emp_host, server_addr);
			sprintf(srv->emp_port,"%d",port);
			srv->status = alive;
			pthread_mutex_init(&srv->mutex, NULL);
  			pthread_cond_init(&srv->cond, NULL);
			srv->next = proxy_config_process->serverlist;
		    proxy_config_process->serverlist = srv;
		    proxy_config_process->svr_n++;
			if(proxy_config_process->retryinterval > 0)
				srv->heart_beat_thread = create_heart_beat_thread(heart_beat_thread, srv);
			printf("server is %s:%d\n", server_addr, port);
        }
	}

	if(ecf->appid_ip_maps) {
		appid_ip = ecf->appid_ip_maps->elts;
		for(i = 0; i< ecf->appid_ip_maps->nelts; i++) {
			ips = appid_ip[i].addrs->elts;
			for (j = 0; j < appid_ip[i].addrs->nelts; j++) {
			//	server_addr = inet_ntoa(((struct sockaddr_in*)appid_ip[i].addrs[j].sockaddr)->sin_addr);
			//	port = ntohs(((struct sockaddr_in*)appid_ip[i].addrs[j].sockaddr)->sin_port);
				printf("appid %s ip allowed is %s\n", appid_ip[i].app_id,  ips[j].data);
	        }
		}
		proxy_config_process->appid_ip_maps = ecf->appid_ip_maps;
	}
	
	proxy_config_process->last_select = proxy_config_process->svr_n - 1;
	printf("proxy_config is %p pid is %d\n", proxy_config_process, getpid());
	printf("called:ngx_emp_server_process_init OK\n");
    return NGX_OK;
}

static void
ngx_emp_server_core_process_exit(ngx_cycle_t *cycle) {
	printf("called:ngx_emp_server_core_process_exit\n");

	proxy_config_process->heart_beat_running = 0;
	emp_server_t *server;
	emp_server_t *temp;
	server = proxy_config_process->serverlist;
	while(server) {
		pthread_mutex_lock(&server->mutex);
 	    pthread_cond_signal(&server->cond);
 	    pthread_mutex_unlock(&server->mutex);
		if(server->heart_beat_thread)
			pthread_join(server->heart_beat_thread, NULL);
		server->heart_beat_thread = 0;
		temp = server;
		server = server->next;
		free(temp);
	}

	printf("called:ngx_emp_server_core_process_exit OK\n");
}


int ms_sleep(long ms) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = ms; 
    return select(0, NULL, NULL, NULL, &tv);
 }


void request_callback(struct evhttp_request *req, void *arg)  
{	
    request_context_t *ctx = (request_context_t *)arg;
	if(req == NULL) {
		printf("server down return success\r\n");
		ctx->ok = 1;
		ctx->server_down = 1;
		event_base_loopexit(ctx->base, 0);  
	    return; 
	}
    //struct evhttp_uri *new_uri = NULL;  
    //const char *new_location = NULL;  
    /* response is ready */  
    switch(req->response_code)  
    {  
	    case HTTP_OK:  
	    {  
	        /*  
	         * Response is received. No futher handling is required. 
	         * Finish 
	         */  
	        const char * result = evhttp_find_header(req->input_headers,"res_code");
			if(result == NULL) {
				printf("res_code is null\r\n");
				ctx->ok = 0;
				event_base_loopexit(ctx->base, 0);
	        	break;
			}
			
			//printf("res_code:%s \n",result);
	        if( strcmp( result,"0") == 0 ){  
	            ctx->ok = 1;    
	        }else{  
	            ctx->ok = 0;     
	        }  
			const char * verify_code_res = evhttp_find_header(req->input_headers,"verify_code");
			if(verify_code_res != NULL) {
				strncpy(ctx->verify_code, verify_code_res, 32);
			}
	        event_base_loopexit(ctx->base, 0);  
	      
	        break;  
	    }  
	    case HTTP_MOVEPERM:  
	    case HTTP_MOVETEMP:  
	        break;  
	    default:  
	        /* FAILURE */  
			printf("http request failed \n");
			ctx->server_down = 1;
	        event_base_loopexit(ctx->base, 0);  
	        return;  
    }  
    evbuffer_add_buffer(ctx->buffer, req->input_buffer);  
    /* SUCCESS */  
    //ctx->ok = 1;  
} 

void context_free(request_context_t *ctx)  
{  
    evhttp_connection_free(ctx->connection);  
    event_base_free(ctx->base);  
    if (ctx->buffer)  
        evbuffer_free(ctx->buffer);  
    evhttp_uri_free(ctx->uri);  
    free(ctx);  
}

request_context_t *create_context(const char *url ,const char * method, char * output_data,int len)  
{  
    request_context_t *ctx = 0;  
    ctx = calloc(1, sizeof(*ctx));  
    if (!ctx)  
        return 0;  
	ngx_memzero(ctx->verify_code, 64);
    ctx->uri = evhttp_uri_parse(url);  
    if (!ctx->uri)  
        return 0;  
    ctx->base = event_base_new();  
    if (!ctx->base)  
        return 0;  
    ctx->buffer = evbuffer_new();  
    make_request(ctx, method , output_data , len );  
    return ctx;  
}  
  
int make_request(request_context_t *ctx ,const char * method, char * output_data,int len )  
{  
    /* free connections & request */  
    if (ctx->connection)  
        evhttp_connection_free(ctx->connection);

	char uri[1024] = {0};
    const char * host = evhttp_uri_get_host(ctx->uri);
	const char *path_part = evhttp_uri_get_path(ctx->uri);
	const char *query_part = evhttp_uri_get_query(ctx->uri);
    int port = evhttp_uri_get_port(ctx->uri); 
	if(query_part != NULL) {
		sprintf(uri, "%s?%s", path_part, query_part);
	} else {
		sprintf(uri, "%s", path_part);
	}
	
    ctx->connection = evhttp_connection_base_new(  
        ctx->base, NULL,   
        host,  
        port != -1 ? port : 80);  
    ctx->req = evhttp_request_new(request_callback, ctx);  
    evhttp_add_header(ctx->req->output_headers,"method",method);
	if(output_data != NULL)
    	evbuffer_add(ctx->req->output_buffer,output_data,len);
	if(strcmp(method, "get") == 0){
		evhttp_make_request(ctx->connection, ctx->req, EVHTTP_REQ_GET, uri);  
	} else {
		evhttp_make_request(ctx->connection, ctx->req, EVHTTP_REQ_POST, uri);  
	}
    
    evhttp_add_header(ctx->req->output_headers, "Host", host);  
    return 0;  
}  

static void *heart_beat_thread(void *arg) {

	if( arg == NULL ){
		LM_ERR( " in worker_thread arg is null \n");
		return NULL;
	}

    emp_server_t *server = (emp_server_t*)arg;
	struct timeval now;
  	struct timespec outtime;
	pthread_mutex_lock(&server->mutex);
	while(proxy_config_process->heart_beat_running){
		char request_uri[128];
		//printf("heart beat @ %s:%s on process %d \n",	server->emp_host, server->emp_port, getpid());
		sprintf(request_uri, "http://%s:%s/emp-ws-redis/NGINX/api_heart_beat", server->emp_host, server->emp_port);
		request_context_t *ctx = create_context(request_uri,"get",NULL, 0 ); 
		if (!ctx){ 
			continue;
		}
		event_base_dispatch(ctx->base);
		if(ctx->server_down) {
			server->status = dead;
		} else {
			server->status = alive;
		}
		context_free(ctx);
	    gettimeofday(&now, NULL);
	    outtime.tv_sec = now.tv_sec + proxy_config_process->retryinterval;
	    outtime.tv_nsec = now.tv_usec * 1000;
	    pthread_cond_timedwait(&server->cond, &server->mutex, &outtime);
	}
	pthread_mutex_unlock(&server->mutex);

	printf("heart beat ended\r\n");

    return NULL;
}


/***********************************************************/
/**
 * create a worker thread for the function
 * @param   func
 * @param   arg
 *
 ************************************************************/
pthread_t create_heart_beat_thread(void *(*func)(void *), void *arg) {
    pthread_t       thread;
    pthread_attr_t  attr;
    int             ret;

    pthread_attr_init(&attr);

    if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
        fprintf(stderr, "Can't create thread: %s\n",
                strerror(ret));
        return 0;
    }
	return thread;
}

emp_server_t *round_robin_select_server()
{
	int dead_server = 0;
	ngx_int_t i,j;
	emp_server_t *rr_server;
	j = proxy_config_process->last_select;
	do {
		j = (j + 1)%proxy_config_process->svr_n;
		proxy_config_process->last_select = j;
		rr_server = proxy_config_process->serverlist;
		for(i = 0; i< proxy_config_process->last_select; i++)
			rr_server = rr_server->next;
		if(rr_server->status == alive) {
			return rr_server;
		} else {
			dead_server++;
			j = proxy_config_process->last_select;
			continue;
		}
	} while (dead_server != proxy_config_process->svr_n);
	return NULL;
}

ngx_int_t ngx_emp_server_api_verify(ngx_emp_api_verify_t *api_verify, char *verify_code)
{
	char request_uri[512];

	if(api_verify == NULL || verify_code == NULL) {
		printf("parameter error return success\n");
		return 1;
	}
	emp_server_t *rr_server;
	rr_server = round_robin_select_server();
	if(rr_server == NULL) {
		printf("rr_server not selected return success\n");
		return 1;
	}

	if(rr_server->status == dead) {
		printf("rr_server not is dead return success\n");
		return 1;
	}
	

	if(api_verify->args.len <= 0) {
		sprintf(request_uri, "http://%s:%s/emp-ws-redis/NGINX/api_verify", rr_server->emp_host, rr_server->emp_port);
	} else {
		char args[1024] = {0};
		strncpy(args, (char *)api_verify->args.data, api_verify->args.len);
		snprintf(request_uri, sizeof(request_uri), "http://%s:%s/emp-ws-redis/NGINX/api_verify?%s", rr_server->emp_host, rr_server->emp_port, args);
	}

	//printf("check appid %s @ %s:%s on process %d %s\n", api_verify->app_id, rr_server->emp_host, rr_server->emp_port, getpid(), request_uri);
	
	request_context_t *ctx = create_context(request_uri,"post", api_verify->verify_body, api_verify->verify_body_len); 
	if (!ctx){ 
		return 1;
	}


	evhttp_add_header(ctx->req->output_headers, "app_id", api_verify->app_id);
	evhttp_add_header(ctx->req->output_headers, "access_token", api_verify->access_token);
	evhttp_add_header(ctx->req->output_headers, "request_method", api_verify->request_method);
	evhttp_add_header(ctx->req->output_headers, "time_local", api_verify->time_local);
	evhttp_add_header(ctx->req->output_headers, "http_xforwarded_for", api_verify->http_xforwarded_for);
	evhttp_add_header(ctx->req->output_headers, "url", api_verify->http_xforwarded_for);
	
	event_base_dispatch(ctx->base); 
	//printf("check result is %d \n", ctx->ok);
	if(ctx->ok && strlen(ctx->verify_code) > 0) {
		strncpy(verify_code, ctx->verify_code, 32);
		//printf("verify_code is %s %p\n", ctx->verify_code, api_verify);
	}
	int ret = ctx->ok;
	context_free(ctx); 
	return ret;
}

ngx_int_t ngx_emp_server_body_grow_step()
{
	return proxy_config_process->body_grow_step;
}

ngx_int_t ngx_emp_server_body_max_multiple()
{
	return proxy_config_process->body_max_multiple;
}

ngx_int_t ngx_emp_server_api_verify_on()
{
	return proxy_config_process->api_verify;
}

ngx_int_t ngx_emp_server_check_appid_ip(const char *app_id, const char *ip)
{
	ngx_str_t *ips;
	ngx_emp_appid_ip_t *appid_ip;
	ngx_uint_t i,j;
	
	printf("check appid %s ip %s\n", app_id, ip);
	if(!proxy_config_process->appid_ip_maps) {
		return 1;
	}
	appid_ip = proxy_config_process->appid_ip_maps->elts;
	for(i = 0; i< proxy_config_process->appid_ip_maps->nelts; i++) {
		if(strcmp(appid_ip[i].app_id, app_id) == 0) {
			ips = appid_ip[i].addrs->elts;
			for (j = 0; j < appid_ip[i].addrs->nelts; j++) {
				printf("appid %s ip allowed is %s\n", appid_ip[i].app_id,  ips[j].data);
				if(strcmp(ip, (char *)ips[j].data) == 0) {
					printf("appid ip match\n");
					return 1;
				}
	        }
			return 0;
		}
	}

	return 1;
}


ngx_int_t ngx_emp_server_log_body(char *body, int body_length, ngx_emp_api_log_body_t *log_body_t)
{
	char request_uri[128];
	emp_server_t *rr_server;
	rr_server = round_robin_select_server();
	if(rr_server == NULL) {
		printf("rr_server not selected return success\n");
		return 1;
	}

	if(rr_server->status == dead) {
		printf("rr_server not is dead return success\n");
		return 1;
	}
	
	//printf("log body %s @ %s:%s on process %d \n", log_body_t->verify_code, rr_server->emp_host, rr_server->emp_port, getpid());
	sprintf(request_uri, "http://%s:%s/emp-ws-redis/NGINX/api_log_info", rr_server->emp_host, rr_server->emp_port);
	
	request_context_t *ctx = create_context(request_uri,"post",body, body_length ); 
	if (!ctx){ 
		return 1;
	}
	evhttp_add_header(ctx->req->output_headers, "verify_code", log_body_t->verify_code);
	evhttp_add_header(ctx->req->output_headers, "request_time", log_body_t->request_time);
	evhttp_add_header(ctx->req->output_headers, "body_bytes_sent", log_body_t->body_bytes_sent);
	evhttp_add_header(ctx->req->output_headers, "status", log_body_t->status);
	//if(log_body_t->content_encoding.len > 0)
	//	evhttp_add_header(ctx->req->output_headers, "Content-Encoding", (const char*)log_body_t->content_encoding.data);
	//if(log_body_t->content_type.len > 0)
	//	evhttp_add_header(ctx->req->output_headers, "Content-Type", (const char*)log_body_t->content_type.data);
	event_base_dispatch(ctx->base); 
	//printf("check result is %d \n", ctx->ok);
	int ret = ctx->ok;
	context_free(ctx); 
	return ret;	
}




