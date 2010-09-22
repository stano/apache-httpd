/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file mod_cache.h
 * @brief Main include file for the Apache Transparent Cache
 *
 * @defgroup MOD_CACHE mod_cache
 * @ingroup  APACHE_MODS
 * @{
 */

#ifndef MOD_CACHE_H
#define MOD_CACHE_H

#include "apr_hooks.h"
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_optional.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_config.h"
#include "ap_config.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_vhost.h"
#include "http_main.h"
#include "http_log.h"
#include "http_connection.h"
#include "util_filter.h"
#include "apr_date.h"
#include "apr_uri.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "apr_atomic.h"

#ifndef MAX
#define MAX(a,b)                ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)                ((a) < (b) ? (a) : (b))
#endif

#define MSEC_ONE_DAY    ((apr_time_t)(86400*APR_USEC_PER_SEC)) /* one day, in microseconds */
#define MSEC_ONE_HR     ((apr_time_t)(3600*APR_USEC_PER_SEC))  /* one hour, in microseconds */
#define MSEC_ONE_MIN    ((apr_time_t)(60*APR_USEC_PER_SEC))    /* one minute, in microseconds */
#define MSEC_ONE_SEC    ((apr_time_t)(APR_USEC_PER_SEC))       /* one second, in microseconds */
#define DEFAULT_CACHE_MAXEXPIRE MSEC_ONE_DAY
#define DEFAULT_CACHE_MINEXPIRE 0
#define DEFAULT_CACHE_EXPIRE    MSEC_ONE_HR
#define DEFAULT_CACHE_LMFACTOR  (0.1)
#define DEFAULT_CACHE_MAXAGE    5
#define DEFAULT_CACHE_LOCKPATH "/mod_cache-lock"
#define CACHE_LOCKNAME_KEY "mod_cache-lockname"
#define CACHE_LOCKFILE_KEY "mod_cache-lockfile"

/* Create a set of CACHE_DECLARE(type), CACHE_DECLARE_NONSTD(type) and
 * CACHE_DECLARE_DATA with appropriate export and import tags for the platform
 */
#if !defined(WIN32)
#define CACHE_DECLARE(type)            type
#define CACHE_DECLARE_NONSTD(type)     type
#define CACHE_DECLARE_DATA
#elif defined(CACHE_DECLARE_STATIC)
#define CACHE_DECLARE(type)            type __stdcall
#define CACHE_DECLARE_NONSTD(type)     type
#define CACHE_DECLARE_DATA
#elif defined(CACHE_DECLARE_EXPORT)
#define CACHE_DECLARE(type)            __declspec(dllexport) type __stdcall
#define CACHE_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define CACHE_DECLARE_DATA             __declspec(dllexport)
#else
#define CACHE_DECLARE(type)            __declspec(dllimport) type __stdcall
#define CACHE_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define CACHE_DECLARE_DATA             __declspec(dllimport)
#endif

struct cache_enable {
    apr_uri_t url;
    const char *type;
    apr_size_t pathlen;
};

struct cache_disable {
    apr_uri_t url;
    apr_size_t pathlen;
};

/* static information about the local cache */
typedef struct {
    apr_array_header_t *cacheenable;    /* URLs to cache */
    apr_array_header_t *cachedisable;   /* URLs not to cache */
    /* Maximum time to keep cached files in msecs */
    apr_time_t maxex;
    int maxex_set;
    /* default time to keep cached file in msecs */
    int defex_set;
    apr_time_t defex;
    /* factor for estimating expires date */
    double factor;
    int factor_set;
    /** ignore the last-modified header when deciding to cache this request */
    int no_last_mod_ignore_set;
    int no_last_mod_ignore;
    /** ignore client's requests for uncached responses */
    int ignorecachecontrol;
    int ignorecachecontrol_set;
    /** ignore expiration date from server */
    int store_expired;
    int store_expired_set;
    /** ignore Cache-Control: private header from server */
    int store_private;
    int store_private_set;
    /** ignore Cache-Control: no-store header from client or server */
    int store_nostore;
    int store_nostore_set;
    /* flag if CacheIgnoreHeader has been set */
    #define CACHE_IGNORE_HEADERS_SET   1
    #define CACHE_IGNORE_HEADERS_UNSET 0
    int ignore_headers_set;
    /** store the headers that should not be stored in the cache */
    apr_array_header_t *ignore_headers;
    /* Minimum time to keep cached files in msecs */
    apr_time_t minex;
    int minex_set;
    /** ignore query-string when caching */
    int ignorequerystring;
    int ignorequerystring_set;
    /* flag if CacheIgnoreURLSessionIdentifiers has been set */
    #define CACHE_IGNORE_SESSION_ID_SET   1
    #define CACHE_IGNORE_SESSION_ID_UNSET 0
    int ignore_session_id_set;
    /** store the identifiers that should not be used for key calculation */
    apr_array_header_t *ignore_session_id;
    /* thundering herd lock */
    int lock;
    int lock_set;
    const char *lockpath;
    int lockpath_set;
    int lockmaxage_set;
    apr_time_t lockmaxage;
    /** run within the quick handler */
    int quick;
    int quick_set;
} cache_server_conf;

/* cache info information */
typedef struct cache_info cache_info;
struct cache_info {
    /**
     * the original time corresponding to the 'Date:' header of the request
     * served
     */
    apr_time_t date;
    /** a time when the cached entity is due to expire */
    apr_time_t expire;
    /** r->request_time from the same request */
    apr_time_t request_time;
    /** apr_time_now() at the time the entity was acutally cached */
    apr_time_t response_time;
    /**
     * HTTP status code of the cached entity. Though not necessarily the
     * status code finally issued to the request.
     */
    int status;
};

/* cache handle information */

typedef struct cache_object cache_object_t;
struct cache_object {
    const char *key;
    cache_object_t *next;
    cache_info info;
    /* Opaque portion (specific to the implementation) of the cache object */
    void *vobj;
};

typedef struct cache_handle cache_handle_t;
struct cache_handle {
    cache_object_t *cache_obj;
    apr_table_t *req_hdrs;        /* cached request headers */
    apr_table_t *resp_hdrs;       /* cached response headers */
};

#define CACHE_PROVIDER_GROUP "cache"

typedef struct {
    int (*remove_entity) (cache_handle_t *h);
    apr_status_t (*store_headers)(cache_handle_t *h, request_rec *r, cache_info *i);
    apr_status_t (*store_body)(cache_handle_t *h, request_rec *r, apr_bucket_brigade *in,
                           apr_bucket_brigade *out);
    apr_status_t (*recall_headers) (cache_handle_t *h, request_rec *r);
    apr_status_t (*recall_body) (cache_handle_t *h, apr_pool_t *p, apr_bucket_brigade *bb);
    int (*create_entity) (cache_handle_t *h, request_rec *r,
                           const char *urlkey, apr_off_t len, apr_bucket_brigade *bb);
    int (*open_entity) (cache_handle_t *h, request_rec *r,
                           const char *urlkey);
    int (*remove_url) (cache_handle_t *h, apr_pool_t *p);
    apr_status_t (*commit_entity)(cache_handle_t *h, request_rec *r);
} cache_provider;

/* A linked-list of authn providers. */
typedef struct cache_provider_list cache_provider_list;

struct cache_provider_list {
    const char *provider_name;
    const cache_provider *provider;
    cache_provider_list *next;
};

/* per request cache information */
typedef struct {
    cache_provider_list *providers;     /* possible cache providers */
    const cache_provider *provider;     /* current cache provider */
    const char *provider_name;          /* current cache provider name */
    int fresh;                          /* is the entity fresh? */
    cache_handle_t *handle;             /* current cache handle */
    cache_handle_t *stale_handle;       /* stale cache handle */
    apr_table_t *stale_headers;         /* original request headers. */
    int in_checked;                     /* CACHE_SAVE must cache the entity */
    int block_response;                 /* CACHE_SAVE must block response. */
    apr_bucket_brigade *saved_brigade;  /* copy of partial response */
    apr_off_t saved_size;               /* length of saved_brigade */
    apr_time_t exp;                     /* expiration */
    apr_time_t lastmod;                 /* last-modified time */
    cache_info *info;                   /* current cache info */
    ap_filter_t *remove_url_filter;     /* Enable us to remove the filter */
    char *key;                          /* The cache key created for this
                                         * request
                                         */
    apr_off_t size;                     /* the content length from the headers, or -1 */
    apr_bucket_brigade *out;            /* brigade to reuse for upstream responses */
} cache_request_rec;


/* cache_util.c */
/* do a HTTP/1.1 age calculation */
CACHE_DECLARE(apr_time_t) ap_cache_current_age(cache_info *info, const apr_time_t age_value,
                                               apr_time_t now);

/**
 * Check the whether the request allows a cached object to be served as per RFC2616
 * section 14.9.4 (Cache Revalidation and Reload Controls)
 * @param h cache_handle_t
 * @param r request_rec
 * @return 0 ==> cache object may not be served, 1 ==> cache object may be served
 */
CACHE_DECLARE(int) ap_cache_check_allowed(request_rec *r);

CACHE_DECLARE(apr_time_t) ap_cache_hex2usec(const char *x);
CACHE_DECLARE(void) ap_cache_usec2hex(apr_time_t j, char *y);
CACHE_DECLARE(char *) ap_cache_generate_name(apr_pool_t *p, int dirlevels,
                                             int dirlength,
                                             const char *name);
CACHE_DECLARE(int) ap_cache_liststr(apr_pool_t *p, const char *list,
                                    const char *key, char **val);
CACHE_DECLARE(const char *)ap_cache_tokstr(apr_pool_t *p, const char *list, const char **str);

/* Create a new table consisting of those elements from an
 * headers table that are allowed to be stored in a cache.
 */
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers(apr_pool_t *pool,
                                                        apr_table_t *t,
                                                        server_rec *s);

/* Create a new table consisting of those elements from an input
 * headers table that are allowed to be stored in a cache.
 */
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers_in(request_rec *r);

/* Create a new table consisting of those elements from an output
 * headers table that are allowed to be stored in a cache;
 * ensure there is a content type and capture any errors.
 */
CACHE_DECLARE(apr_table_t *)ap_cache_cacheable_headers_out(request_rec *r);


/* hooks */

APR_DECLARE_OPTIONAL_FN(apr_status_t,
                        ap_cache_generate_key,
                        (cache_request_rec *cache, request_rec *r,
                         apr_pool_t*p, char **key));


#endif /*MOD_CACHE_H*/
/** @} */
