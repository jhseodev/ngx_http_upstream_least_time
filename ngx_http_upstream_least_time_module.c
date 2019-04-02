
/*
 * Copyright (C) JungHoon Seo
 * Copyright (C) Piolink, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_upstream_least_time_module.h>


static ngx_int_t ngx_http_upstream_least_time_init(ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_least_time_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_upstream_init_least_time_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_least_time_peer(ngx_peer_connection_t *pc,
    void *data);
static void *ngx_http_upstream_least_time_create_conf(ngx_conf_t *cf);
static char *ngx_http_upstream_least_time(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void ngx_http_upstream_least_time_set_peer_time_counter(
    ngx_http_upstream_least_time_srv_conf_t *ltcf,
    ngx_http_upstream_rr_peers_t *peers);


static ngx_command_t  ngx_http_upstream_least_time_commands[] = {

    { ngx_string("least_time"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1234,
      ngx_http_upstream_least_time,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_least_time_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_upstream_least_time_create_conf,   /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_least_time_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_least_time_module_ctx, /* module context */
    ngx_http_upstream_least_time_commands,    /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_http_upstream_least_time_init_module, /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_upstream_init_least_time(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_http_upstream_init_least_time_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_least_time_init(ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_least_time_srv_conf_t  *ltcf;

    ngx_http_upstream_rr_peers_t             *peers;

    ltcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_least_time_module);

    peers = us->peer.data;

    if (peers && peers->number > 0) {
        peers->first_request = NGX_CONF_UNSET;
        ngx_http_upstream_least_time_set_peer_time_counter(ltcf, peers);
    }

    if (peers->next && peers->next->number > 0) {
        peers->next->first_request = NGX_CONF_UNSET;
        ngx_http_upstream_least_time_set_peer_time_counter(ltcf, peers->next);
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_least_time_init_module(ngx_cycle_t *cycle)
{
    ngx_http_upstream_main_conf_t  *umcf;
    ngx_http_conf_ctx_t            *ctx;
    ngx_http_upstream_srv_conf_t  **uscfp;

    ngx_uint_t                      i;

    ctx = (ngx_http_conf_ctx_t*) cycle->conf_ctx[ngx_http_module.index];
    umcf = (ngx_http_upstream_main_conf_t*) ctx->main_conf[ngx_http_upstream_module.ctx_index];
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {
        if (! uscfp[i]->srv_conf) {
            continue;
        }

        if (ngx_http_upstream_least_time_init(uscfp[i]) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_least_time_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_least_time_srv_conf_t     *ltcf;
    ngx_http_upstream_least_time_peer_data_t    *ltp;

    ltp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_least_time_peer_data_t));
    if (ltp == NULL) {
        return NGX_ERROR;
    }

    r->upstream->peer.data = &ltp->rrp;

    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = ngx_http_upstream_get_least_time_peer;

    ltcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_least_time_module);
    ltp->conf = ltcf;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_get_least_time_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_least_time_peer_data_t  *ltp = data;

    time_t                                   now;
    uintptr_t                                m;
    ngx_uint_t                               i, n, p=0;
    ngx_msec_t                               best_time=0, peer_time=0;

    ngx_http_upstream_rr_peer_t             *peer, *best=NULL;
    ngx_http_upstream_rr_peers_t            *peers;

    if (ltp->rrp.peers->single || ltp->rrp.peers->first_request == NGX_CONF_UNSET) {
        ltp->rrp.peers->first_request = NGX_OK;
        return ngx_http_upstream_get_round_robin_peer(pc, &ltp->rrp);
    }

    ngx_http_upstream_rr_peers_wlock(ltp->rrp.peers);

    pc->cached = 0;
    pc->connection = NULL;

    now = ngx_time();

    peers = ltp->rrp.peers;

    for (peer = peers->peer, i = 0;
         peer;
         peer = peer->next, i++)
    {
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << i % (8 * sizeof(uintptr_t));

        if (ltp->rrp.tried[n] & m) {
            continue;
        }

        if (peer->down) {
            continue;
        }

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
            continue;
        }

        if (best == NULL) {
            best = peer;
            continue;
        }

        best_time = *best->least_time;
        peer_time = *peer->least_time;

        if (best_time > peer_time) {
            best = peer;
            p = i;
        }
    }

    if (peers->first_request == NGX_CONF_UNSET && best_time == 0) {
        ngx_http_upstream_rr_peers_unlock(peers);
        return ngx_http_upstream_get_round_robin_peer(pc, &ltp->rrp);
    }

    if (best == NULL) {
        ngx_http_upstream_rr_peers_unlock(peers);
        return NGX_BUSY;
    }

    pc->sockaddr = best->sockaddr;
    pc->socklen = best->socklen;
    pc->name = &best->name;

    /* max_conns */
    pc->peer = best;

    pc->average_header_time = best->average_header_time;
    pc->average_response_time = best->average_response_time;
    pc->current_header_time = best->current_header_time;
    pc->current_response_time = best->current_response_time;

    best->conns++;

    ltp->rrp.current = best;

    n = p / (8 * sizeof(uintptr_t));
    m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

    ltp->rrp.tried[n] |= m;

    ngx_http_upstream_rr_peers_unlock(peers);

    return NGX_OK;
}


static void *
ngx_http_upstream_least_time_create_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_least_time_srv_conf_t  *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_upstream_least_time_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_upstream_least_time(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_least_time_srv_conf_t  *ltcf = conf;

    ngx_str_t                     *value;
    ngx_uint_t                     i=0;
    ngx_http_upstream_srv_conf_t  *uscf;

    value = cf->args->elts;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "load balancing method redefined");
    }

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_MAX_CONNS
                  |NGX_HTTP_UPSTREAM_MAX_FAILS
                  |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                  |NGX_HTTP_UPSTREAM_DOWN;

    ltcf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstream_least_time_module);

    uscf->peer.init_upstream = ngx_http_upstream_init_least_time;

    ltcf->base = RESPONSE_TIME;
    ltcf->mode = AVERAGE;
    ltcf->active_conns = ACTIVE_CONNS_UNSET;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "base=", 5) == 0) {
            if (ngx_strcmp(value[i].data + 5, "header") == 0) {
                ltcf->base = HEADER_TIME;
            } else if (ngx_strcmp(value[i].data + 5, "response") == 0) {
                ltcf->base = RESPONSE_TIME;
            } else {
                goto invalid;
            }
        }

        if (ngx_strncmp(value[i].data, "mode=", 5) == 0) {
            if (ngx_strcmp(value[i].data + 5, "current") == 0) {
                ltcf->mode = CURRENT;
            } else if (ngx_strcmp(value[i].data + 5, "average") == 0) {
                ltcf->mode = AVERAGE;
            } else {
                goto invalid;
            }
        }
    }

    return NGX_CONF_OK;

invalid:
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);
    return NGX_CONF_ERROR;
}


static void ngx_http_upstream_least_time_set_peer_time_counter(
    ngx_http_upstream_least_time_srv_conf_t *ltcf,
    ngx_http_upstream_rr_peers_t *peers)
{
    ngx_http_upstream_rr_peer_t *peer;

    for (peer=peers->peer; peer; peer=peer->next) {
        if (ltcf->base == RESPONSE_TIME && ltcf->mode == CURRENT) {
            peer->least_time = peer->current_response_time;
        } else if (ltcf->base == HEADER_TIME && ltcf->mode == AVERAGE) {
            peer->least_time = peer->average_header_time;
        } else if (ltcf->base == HEADER_TIME && ltcf->mode == CURRENT) {
            peer->least_time = peer->current_header_time;
        } else {
            peer->least_time = peer->average_response_time;
        }
        if (peers->first_request == NGX_CONF_UNSET && *peer->least_time > 0) {
            peers->first_request = NGX_OK;
        }
    }
}
