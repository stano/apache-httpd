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

/* User Tracking Module (Was mod_cookies.c)
 *
 * *** IMPORTANT NOTE: This module is not designed to generate
 * *** cryptographically secure cookies.  This means you should not
 * *** use cookies generated by this module for authentication purposes
 *
 * This Apache module is designed to track users paths through a site.
 * It uses the client-side state ("Cookie") protocol developed by Netscape.
 * It is known to work on most browsers.
 *
 * Each time a page is requested we look to see if the browser is sending
 * us a Cookie: header that we previously generated.
 *
 * If we don't find one then the user hasn't been to this site since
 * starting their browser or their browser doesn't support cookies.  So
 * we generate a unique Cookie for the transaction and send it back to
 * the browser (via a "Set-Cookie" header)
 * Future requests from the same browser should keep the same Cookie line.
 *
 * By matching up all the requests with the same cookie you can
 * work out exactly what path a user took through your site.  To log
 * the cookie use the " %{Cookie}n " directive in a custom access log;
 *
 * Example 1 : If you currently use the standard Log file format (CLF)
 * and use the command "TransferLog somefilename", add the line
 *       LogFormat "%h %l %u %t \"%r\" %s %b %{Cookie}n"
 * to your config file.
 *
 * Example 2 : If you used to use the old "CookieLog" directive, you
 * can emulate it by adding the following command to your config file
 *       CustomLog filename "%{Cookie}n \"%r\" %t"
 *
 * Mark Cox, mjc@apache.org, 6 July 95
 *
 * This file replaces mod_cookies.c
 */

#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"


module AP_MODULE_DECLARE_DATA usertrack_module;

typedef struct {
    int always;
    int expires;
} cookie_log_state;

typedef enum {
    CT_UNSET,
    CT_NETSCAPE,
    CT_COOKIE,
    CT_COOKIE2
} cookie_type_e;

typedef struct {
    int enabled;
    cookie_type_e style;
    const char *cookie_name;
    const char *cookie_domain;
    char *regexp_string;  /* used to compile regexp; save for debugging */
    ap_regex_t *regexp;  /* used to find usertrack cookie in cookie header */
} cookie_dir_rec;

/* Make Cookie: Now we have to generate something that is going to be
 * pretty unique.  We can base it on the pid, time, hostip */

#define COOKIE_NAME "Apache"

static void make_cookie(request_rec *r)
{
    cookie_log_state *cls = ap_get_module_config(r->server->module_config,
                                                 &usertrack_module);
    /* 1024 == hardcoded constant */
    char cookiebuf[1024];
    char *new_cookie;
    const char *rname = ap_get_remote_host(r->connection, r->per_dir_config,
                                           REMOTE_NAME, NULL);
    cookie_dir_rec *dcfg;

    dcfg = ap_get_module_config(r->per_dir_config, &usertrack_module);

    /* XXX: hmm, this should really tie in with mod_unique_id */
    apr_snprintf(cookiebuf, sizeof(cookiebuf), "%s.%" APR_TIME_T_FMT, rname,
                 apr_time_now());

    if (cls->expires) {

        /* Cookie with date; as strftime '%a, %d-%h-%y %H:%M:%S GMT' */
        new_cookie = apr_psprintf(r->pool, "%s=%s; path=/",
                                  dcfg->cookie_name, cookiebuf);

        if ((dcfg->style == CT_UNSET) || (dcfg->style == CT_NETSCAPE)) {
            apr_time_exp_t tms;
            apr_time_exp_gmt(&tms, r->request_time
                                 + apr_time_from_sec(cls->expires));
            new_cookie = apr_psprintf(r->pool,
                                       "%s; expires=%s, "
                                       "%.2d-%s-%.2d %.2d:%.2d:%.2d GMT",
                                       new_cookie, apr_day_snames[tms.tm_wday],
                                       tms.tm_mday,
                                       apr_month_snames[tms.tm_mon],
                                       tms.tm_year % 100,
                                       tms.tm_hour, tms.tm_min, tms.tm_sec);
        }
        else {
            new_cookie = apr_psprintf(r->pool, "%s; max-age=%d",
                                      new_cookie, cls->expires);
        }
    }
    else {
        new_cookie = apr_psprintf(r->pool, "%s=%s; path=/",
                                  dcfg->cookie_name, cookiebuf);
    }
    if (dcfg->cookie_domain != NULL) {
        new_cookie = apr_pstrcat(r->pool, new_cookie, "; domain=",
                                 dcfg->cookie_domain,
                                 (dcfg->style == CT_COOKIE2
                                  ? "; version=1"
                                  : ""),
                                 NULL);
    }

    apr_table_addn(r->err_headers_out,
                   (dcfg->style == CT_COOKIE2 ? "Set-Cookie2" : "Set-Cookie"),
                   new_cookie);
    apr_table_setn(r->notes, "cookie", apr_pstrdup(r->pool, cookiebuf));   /* log first time */
    return;
}

/* dcfg->regexp is "^cookie_name=([^;]+)|;[ \t]+cookie_name=([^;]+)",
 * which has three subexpressions, $0..$2 */
#define NUM_SUBS 3

static void set_and_comp_regexp(cookie_dir_rec *dcfg,
                                apr_pool_t *p,
                                const char *cookie_name)
{
    int danger_chars = 0;
    const char *sp = cookie_name;

    /* The goal is to end up with this regexp,
     * ^cookie_name=([^;,]+)|[;,][ \t]+cookie_name=([^;,]+)
     * with cookie_name obviously substituted either
     * with the real cookie name set by the user in httpd.conf, or with the
     * default COOKIE_NAME. */

    /* Anyway, we need to escape the cookie_name before pasting it
     * into the regex
     */
    while (*sp) {
        if (!apr_isalnum(*sp)) {
            ++danger_chars;
        }
        ++sp;
    }

    if (danger_chars) {
        char *cp;
        cp = apr_palloc(p, sp - cookie_name + danger_chars + 1); /* 1 == \0 */
        sp = cookie_name;
        cookie_name = cp;
        while (*sp) {
            if (!apr_isalnum(*sp)) {
                *cp++ = '\\';
            }
            *cp++ = *sp++;
        }
        *cp = '\0';
    }

    dcfg->regexp_string = apr_pstrcat(p, "^",
                                      cookie_name,
                                      "=([^;,]+)|[;,][ \t]*",
                                      cookie_name,
                                      "=([^;,]+)", NULL);

    dcfg->regexp = ap_pregcomp(p, dcfg->regexp_string, AP_REG_EXTENDED);
    ap_assert(dcfg->regexp != NULL);
}

static int spot_cookie(request_rec *r)
{
    cookie_dir_rec *dcfg = ap_get_module_config(r->per_dir_config,
                                                &usertrack_module);
    const char *cookie_header;
    ap_regmatch_t regm[NUM_SUBS];

    /* Do not run in subrequests */
    if (!dcfg->enabled || r->main) {
        return DECLINED;
    }

    if ((cookie_header = apr_table_get(r->headers_in, "Cookie"))) {
        if (!ap_regexec(dcfg->regexp, cookie_header, NUM_SUBS, regm, 0)) {
            char *cookieval = NULL;
            /* Our regexp,
             * ^cookie_name=([^;]+)|;[ \t]+cookie_name=([^;]+)
             * only allows for $1 or $2 to be available. ($0 is always
             * filled with the entire matched expression, not just
             * the part in parentheses.) So just check for either one
             * and assign to cookieval if present. */
            if (regm[1].rm_so != -1) {
                cookieval = ap_pregsub(r->pool, "$1", cookie_header,
                                       NUM_SUBS, regm);
            }
            if (regm[2].rm_so != -1) {
                cookieval = ap_pregsub(r->pool, "$2", cookie_header,
                                       NUM_SUBS, regm);
            }
            /* Set the cookie in a note, for logging */
            apr_table_setn(r->notes, "cookie", cookieval);

            return DECLINED;    /* There's already a cookie, no new one */
        }
    }
    make_cookie(r);
    return OK;                  /* We set our cookie */
}

static void *make_cookie_log_state(apr_pool_t *p, server_rec *s)
{
    cookie_log_state *cls =
    (cookie_log_state *) apr_palloc(p, sizeof(cookie_log_state));

    cls->expires = 0;

    return (void *) cls;
}

static void *make_cookie_dir(apr_pool_t *p, char *d)
{
    cookie_dir_rec *dcfg;

    dcfg = (cookie_dir_rec *) apr_pcalloc(p, sizeof(cookie_dir_rec));
    dcfg->cookie_name = COOKIE_NAME;
    dcfg->cookie_domain = NULL;
    dcfg->style = CT_UNSET;
    dcfg->enabled = 0;

    /* In case the user does not use the CookieName directive,
     * we need to compile the regexp for the default cookie name. */
    set_and_comp_regexp(dcfg, p, COOKIE_NAME);

    return dcfg;
}

static const char *set_cookie_enable(cmd_parms *cmd, void *mconfig, int arg)
{
    cookie_dir_rec *dcfg = mconfig;

    dcfg->enabled = arg;
    return NULL;
}

static const char *set_cookie_exp(cmd_parms *parms, void *dummy,
                                  const char *arg)
{
    cookie_log_state *cls;
    time_t factor, modifier = 0;
    time_t num = 0;
    char *word;

    cls  = ap_get_module_config(parms->server->module_config,
                                &usertrack_module);
    /* The simple case first - all numbers (we assume) */
    if (apr_isdigit(arg[0]) && apr_isdigit(arg[strlen(arg) - 1])) {
        cls->expires = atol(arg);
        return NULL;
    }

    /*
     * The harder case - stolen from mod_expires
     *
     * CookieExpires "[plus] {<num> <type>}*"
     */

    word = ap_getword_conf(parms->pool, &arg);
    if (!strncasecmp(word, "plus", 1)) {
        word = ap_getword_conf(parms->pool, &arg);
    };

    /* {<num> <type>}* */
    while (word[0]) {
        /* <num> */
        if (apr_isdigit(word[0]))
            num = atoi(word);
        else
            return "bad expires code, numeric value expected.";

        /* <type> */
        word = ap_getword_conf(parms->pool, &arg);
        if (!word[0])
            return "bad expires code, missing <type>";

        factor = 0;
        if (!strncasecmp(word, "years", 1))
            factor = 60 * 60 * 24 * 365;
        else if (!strncasecmp(word, "months", 2))
            factor = 60 * 60 * 24 * 30;
        else if (!strncasecmp(word, "weeks", 1))
            factor = 60 * 60 * 24 * 7;
        else if (!strncasecmp(word, "days", 1))
            factor = 60 * 60 * 24;
        else if (!strncasecmp(word, "hours", 1))
            factor = 60 * 60;
        else if (!strncasecmp(word, "minutes", 2))
            factor = 60;
        else if (!strncasecmp(word, "seconds", 1))
            factor = 1;
        else
            return "bad expires code, unrecognized type";

        modifier = modifier + factor * num;

        /* next <num> */
        word = ap_getword_conf(parms->pool, &arg);
    }

    cls->expires = modifier;

    return NULL;
}

static const char *set_cookie_name(cmd_parms *cmd, void *mconfig,
                                   const char *name)
{
    cookie_dir_rec *dcfg = (cookie_dir_rec *) mconfig;

    dcfg->cookie_name = name;

    set_and_comp_regexp(dcfg, cmd->pool, name);

    if (dcfg->regexp == NULL) {
        return "Regular expression could not be compiled.";
    }
    if (dcfg->regexp->re_nsub + 1 != NUM_SUBS) {
        return apr_pstrcat(cmd->pool, "Invalid cookie name \"",
                           name, "\"", NULL);
    }

    return NULL;
}

/*
 * Set the value for the 'Domain=' attribute.
 */
static const char *set_cookie_domain(cmd_parms *cmd, void *mconfig,
                                     const char *name)
{
    cookie_dir_rec *dcfg;

    dcfg = (cookie_dir_rec *) mconfig;

    /*
     * Apply the restrictions on cookie domain attributes.
     */
    if (!name[0]) {
        return "CookieDomain values may not be null";
    }
    if (name[0] != '.') {
        return "CookieDomain values must begin with a dot";
    }
    if (ap_strchr_c(&name[1], '.') == NULL) {
        return "CookieDomain values must contain at least one embedded dot";
    }

    dcfg->cookie_domain = name;
    return NULL;
}

/*
 * Make a note of the cookie style we should use.
 */
static const char *set_cookie_style(cmd_parms *cmd, void *mconfig,
                                    const char *name)
{
    cookie_dir_rec *dcfg;

    dcfg = (cookie_dir_rec *) mconfig;

    if (strcasecmp(name, "Netscape") == 0) {
        dcfg->style = CT_NETSCAPE;
    }
    else if ((strcasecmp(name, "Cookie") == 0)
             || (strcasecmp(name, "RFC2109") == 0)) {
        dcfg->style = CT_COOKIE;
    }
    else if ((strcasecmp(name, "Cookie2") == 0)
             || (strcasecmp(name, "RFC2965") == 0)) {
        dcfg->style = CT_COOKIE2;
    }
    else {
        return apr_psprintf(cmd->pool, "Invalid %s keyword: '%s'",
                            cmd->cmd->name, name);
    }

    return NULL;
}

static const command_rec cookie_log_cmds[] = {
    AP_INIT_TAKE1("CookieExpires", set_cookie_exp, NULL, OR_FILEINFO,
                  "an expiry date code"),
    AP_INIT_TAKE1("CookieDomain", set_cookie_domain, NULL, OR_FILEINFO,
                  "domain to which this cookie applies"),
    AP_INIT_TAKE1("CookieStyle", set_cookie_style, NULL, OR_FILEINFO,
                  "'Netscape', 'Cookie' (RFC2109), or 'Cookie2' (RFC2965)"),
    AP_INIT_FLAG("CookieTracking", set_cookie_enable, NULL, OR_FILEINFO,
                 "whether or not to enable cookies"),
    AP_INIT_TAKE1("CookieName", set_cookie_name, NULL, OR_FILEINFO,
                  "name of the tracking cookie"),
    {NULL}
};

static void register_hooks(apr_pool_t *p)
{
    ap_hook_fixups(spot_cookie,NULL,NULL,APR_HOOK_REALLY_FIRST);
}

AP_DECLARE_MODULE(usertrack) = {
    STANDARD20_MODULE_STUFF,
    make_cookie_dir,            /* dir config creater */
    NULL,                       /* dir merger --- default is to override */
    make_cookie_log_state,      /* server config */
    NULL,                       /* merge server configs */
    cookie_log_cmds,            /* command apr_table_t */
    register_hooks              /* register hooks */
};
