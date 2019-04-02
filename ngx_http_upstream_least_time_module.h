
/*
 * Copyright (C) Junghoon Seo
 * Copyright (C) Piolink, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_LEAST_TIME_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_LEAST_TIME_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef enum {
    HEADER_TIME = 0,
    RESPONSE_TIME
} ngx_http_upstream_least_time_conf_base_e;


typedef enum {
    CURRENT = 0,
    AVERAGE
} ngx_http_upstream_least_time_conf_mode_e;


typedef enum {
    ACTIVE_CONNS_SET = 0,
    ACTIVE_CONNS_UNSET
} ngx_http_upstream_least_time_conf_active_conns_e;


typedef struct {
    ngx_http_upstream_least_time_conf_base_e           base;
    ngx_http_upstream_least_time_conf_mode_e           mode;
    ngx_http_upstream_least_time_conf_active_conns_e   active_conns;
} ngx_http_upstream_least_time_srv_conf_t;


typedef struct {
    /* the round robin data must be first */
    ngx_http_upstream_rr_peer_data_t            rrp;
    ngx_http_upstream_least_time_srv_conf_t    *conf;
} ngx_http_upstream_least_time_peer_data_t;


#endif /* _NGX_HTTP_UPSTREAM_LEAST_TIME_H_INCLUDED_ */
