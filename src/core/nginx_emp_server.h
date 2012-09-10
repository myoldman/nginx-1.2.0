
#ifndef _NGX_EMP_SERVER_H_INCLUDED_
#define _NGX_EMP_SERVER_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_EMP_SERVER_MODULE      0x544E5646  /* "EVNT" */
#define NGX_EMP_SERVER_CONF        0x04000000

extern ngx_module_t           ngx_emp_server_module;
extern ngx_module_t           ngx_emp_server_core_module;


typedef struct {
    ngx_str_t              *name;

    void                 *(*create_conf)(ngx_cycle_t *cycle);
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);
} ngx_emp_server_module_t;


typedef struct {
	ngx_array_t   *servers;
} ngx_emp_server_conf_t;


#endif

