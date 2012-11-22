
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
} ngx_emp_server_conf_t;

ngx_int_t ngx_emp_server_check_appid(char *app_id, char *uri);
ngx_int_t ngx_emp_server_log_body(char *body, int body_length, char *session_id);
ngx_int_t ngx_emp_server_body_grow_step();
ngx_int_t ngx_emp_server_body_max_multiple();

#endif

