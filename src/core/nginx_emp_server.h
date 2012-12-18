
#ifndef _NGX_EMP_SERVER_H_INCLUDED_
#define _NGX_EMP_SERVER_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_EMP_SERVER_MODULE      0x544E5646  /* "EVNT" */
#define NGX_EMP_SERVER_CONF        0x04000000

extern ngx_module_t           ngx_emp_server_module;
extern ngx_module_t           ngx_emp_server_core_module;

typedef struct {
    ngx_addr_t                      *addrs;
    ngx_uint_t                       naddrs;
} ngx_emp_server_t;

typedef struct {
	char app_id[64];
    ngx_addr_t                      *addrs;
    ngx_uint_t                       naddrs;
} ngx_emp_appid_ip_t;


typedef struct {
    ngx_str_t              *name;

    void                 *(*create_conf)(ngx_cycle_t *cycle);
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);
} ngx_emp_server_module_t;

#define ngx_emp_server_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_emp_server_module))) [module.ctx_index];


typedef struct {
	u_char       *name;
	int heart_beat_interval;
	int body_memory_grow_step;
	int body_memory_max_multiple;
	ngx_array_t   *servers;
	ngx_array_t	  *appid_ip_maps;
} ngx_emp_server_conf_t;

typedef struct  {
	char app_id[64];
	char access_token[64];
	char request_method[8];
	char time_local[32];
	char http_xforwarded_for[128];
	ngx_str_t args;
	char *verify_body;
	int verify_body_len;
}ngx_emp_api_verify_t;

typedef struct  {
	char verify_code[64];
	char request_time[16];
	char status[8];
	char body_bytes_sent[16];
	ngx_str_t content_encoding;
	ngx_str_t content_type;
} ngx_emp_api_log_body_t;

ngx_int_t ngx_emp_server_api_verify(ngx_emp_api_verify_t *api_verify, char *verify_code);
ngx_int_t ngx_emp_server_log_body(char *body, int body_length, ngx_emp_api_log_body_t *log_body_t);
ngx_int_t ngx_emp_server_body_grow_step();
ngx_int_t ngx_emp_server_body_max_multiple();

#endif

