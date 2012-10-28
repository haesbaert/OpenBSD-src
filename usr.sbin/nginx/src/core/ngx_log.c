
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static char *ngx_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
#if (NGX_ENABLE_SYSLOG)
static char *ngx_set_syslog(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
void log_exit(ngx_cycle_t *cycle);

typedef struct{
    ngx_str_t     name;
    ngx_int_t     macro;
} ngx_string_to_macro_t;
#endif


static ngx_command_t  ngx_errlog_commands[] = {

    {ngx_string("error_log"),
     NGX_MAIN_CONF|NGX_CONF_1MORE,
     ngx_error_log,
     0,
     0,
     NULL},

#if (NGX_ENABLE_SYSLOG)
    {ngx_string("syslog"),
     NGX_MAIN_CONF|NGX_CONF_TAKE12,
     ngx_set_syslog,
     0,
     0,
     NULL},
#endif

    ngx_null_command
};


static ngx_core_module_t  ngx_errlog_module_ctx = {
    ngx_string("errlog"),
    NULL,
    NULL
};


ngx_module_t  ngx_errlog_module = {
    NGX_MODULE_V1,
    &ngx_errlog_module_ctx,                /* module context */
    ngx_errlog_commands,                   /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
#if (NGX_ENABLE_SYSLOG)
    log_exit,                              /* exit master */
#else
    NULL,
#endif
    NGX_MODULE_V1_PADDING
};


static ngx_log_t        ngx_log;
static ngx_open_file_t  ngx_log_file;
ngx_uint_t              ngx_use_stderr = 1;

#if (NGX_ENABLE_SYSLOG)
static ngx_string_to_macro_t ngx_syslog_facilities[] = {
    {ngx_string("auth"),     LOG_AUTH},
#if !(NGX_SOLARIS)
    {ngx_string("authpriv"), LOG_AUTHPRIV},
#endif
    {ngx_string("cron"),     LOG_CRON},
    {ngx_string("daemon"),   LOG_DAEMON},
#if !(NGX_SOLARIS)
    {ngx_string("ftp"),      LOG_FTP},
#endif
    {ngx_string("kern"),     LOG_KERN},
    {ngx_string("local0"),   LOG_LOCAL0},
    {ngx_string("local1"),   LOG_LOCAL1},
    {ngx_string("local2"),   LOG_LOCAL2},
    {ngx_string("local3"),   LOG_LOCAL3},
    {ngx_string("local4"),   LOG_LOCAL4},
    {ngx_string("local5"),   LOG_LOCAL5},
    {ngx_string("local6"),   LOG_LOCAL6},
    {ngx_string("local7"),   LOG_LOCAL7},
    {ngx_string("lpr"),      LOG_LPR},
    {ngx_string("mail"),     LOG_MAIL},
    {ngx_string("news"),     LOG_NEWS},
    {ngx_string("syslog"),   LOG_SYSLOG},
    {ngx_string("user"),     LOG_USER},
    {ngx_string("uucp"),     LOG_UUCP},
    { ngx_null_string, 0}
};

static ngx_string_to_macro_t ngx_syslog_priorities[] = {
    {ngx_string("emerg"), LOG_EMERG},
    {ngx_string("alert"), LOG_ALERT},
    {ngx_string("crit"),  LOG_CRIT},
    {ngx_string("error"), LOG_ERR},
    {ngx_string("err"),   LOG_ERR},
    {ngx_string("warn"),  LOG_WARNING},
    {ngx_string("notice"),LOG_NOTICE},
    {ngx_string("info"),  LOG_INFO},
    {ngx_string("debug"), LOG_DEBUG},
    { ngx_null_string, 0}
};
#endif

static ngx_str_t err_levels[] = {
    ngx_null_string,
    ngx_string("emerg"),
    ngx_string("alert"),
    ngx_string("crit"),
    ngx_string("error"),
    ngx_string("warn"),
    ngx_string("notice"),
    ngx_string("info"),
    ngx_string("debug")
};

static const char *debug_levels[] = {
    "debug_core", "debug_alloc", "debug_mutex", "debug_event",
    "debug_http", "debug_mail", "debug_mysql"
};


#if (NGX_HAVE_VARIADIC_MACROS)

void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)

#else

void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, va_list args)

#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list  args;
#endif
    u_char  *p, *last, *msg;
#if (NGX_ENABLE_SYSLOG)
    u_char *errstr_syslog;
#endif
    u_char   errstr[NGX_MAX_ERROR_STR];

#if !(NGX_ENABLE_SYSLOG)
    if (log->file->fd == NGX_INVALID_FILE) {
        return;
    }
#endif

    last = errstr + NGX_MAX_ERROR_STR;

    ngx_memcpy(errstr, ngx_cached_err_log_time.data,
               ngx_cached_err_log_time.len);

    p = errstr + ngx_cached_err_log_time.len;

#if (NGX_ENABLE_SYSLOG)
    errstr_syslog = p;
#endif

    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);

    /* pid#tid */
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ",
                    ngx_log_pid, ngx_log_tid);

    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }

    msg = p;

#if (NGX_HAVE_VARIADIC_MACROS)

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else

    p = ngx_vslprintf(p, last, fmt, args);

#endif

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p);
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

#if (NGX_ENABLE_SYSLOG)
    if (log->file != NULL && log->file->name.len != 0) {
        (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
    }

    /* Don't send the debug level info to syslog */
    if (log->syslog_on && level < NGX_LOG_DEBUG) {
        /* write to syslog */
        syslog(log->priority, "%.*s", (int)(p - errstr_syslog), errstr_syslog);
    }
#else
    (void) ngx_write_fd(log->file->fd, errstr, p - errstr);
#endif

    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
#if (NGX_ENABLE_SYSLOG)
        || (log->file != NULL && log->file->fd == ngx_stderr))
#else
        || log->file->fd == ngx_stderr)
#endif
    {
        return;
    }

    msg -= (7 + err_levels[level].len + 3);

    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);

    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}


#if !(NGX_HAVE_VARIADIC_MACROS)

void ngx_cdecl
ngx_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
{
    va_list  args;

    if (log->log_level >= level) {
        va_start(args, fmt);
        ngx_log_error_core(level, log, err, fmt, args);
        va_end(args);
    }
}


void ngx_cdecl
ngx_log_debug_core(ngx_log_t *log, ngx_err_t err, const char *fmt, ...)
{
    va_list  args;

    va_start(args, fmt);
    ngx_log_error_core(NGX_LOG_DEBUG, log, err, fmt, args);
    va_end(args);
}

#endif


void ngx_cdecl
ngx_log_abort(ngx_err_t err, const char *fmt, ...)
{
    u_char   *p;
    va_list   args;
    u_char    errstr[NGX_MAX_CONF_ERRSTR];

    va_start(args, fmt);
    p = ngx_vsnprintf(errstr, sizeof(errstr) - 1, fmt, args);
    va_end(args);

    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, err,
                  "%*s", p - errstr, errstr);
}


void ngx_cdecl
ngx_log_stderr(ngx_err_t err, const char *fmt, ...)
{
    u_char   *p, *last;
    va_list   args;
    u_char    errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;
    p = errstr + 7;

    ngx_memcpy(errstr, "nginx: ", 7);

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

    (void) ngx_write_console(ngx_stderr, errstr, p - errstr);
}


u_char *
ngx_log_errno(u_char *buf, u_char *last, ngx_err_t err)
{
    if (buf > last - 50) {

        /* leave a space for an error code */

        buf = last - 50;
        *buf++ = '.';
        *buf++ = '.';
        *buf++ = '.';
    }

#if (NGX_WIN32)
    buf = ngx_slprintf(buf, last, ((unsigned) err < 0x80000000)
                                       ? " (%d: " : " (%Xd: ", err);
#else
    buf = ngx_slprintf(buf, last, " (%d: ", err);
#endif

    buf = ngx_strerror(err, buf, last - buf);

    if (buf < last) {
        *buf++ = ')';
    }

    return buf;
}


ngx_log_t *
ngx_log_init(u_char *prefix)
{
    u_char  *p, *name;
    size_t   nlen, plen;

    ngx_log.file = &ngx_log_file;
    ngx_log.log_level = NGX_LOG_NOTICE;

    name = (u_char *) NGX_ERROR_LOG_PATH;

    /*
     * we use ngx_strlen() here since BCC warns about
     * condition is always false and unreachable code
     */

    nlen = ngx_strlen(name);

    if (nlen == 0) {
        ngx_log_file.fd = ngx_stderr;
        return &ngx_log;
    }

    p = NULL;

#if (NGX_WIN32)
    if (name[1] != ':') {
#else
    if (name[0] != '/') {
#endif

        if (prefix) {
            plen = ngx_strlen(prefix);

        } else {
#ifdef NGX_PREFIX
            prefix = (u_char *) NGX_PREFIX;
            plen = ngx_strlen(prefix);
#else
            plen = 0;
#endif
        }

        if (plen) {
            name = malloc(plen + nlen + 2);
            if (name == NULL) {
                return NULL;
            }

            p = ngx_cpymem(name, prefix, plen);

            if (!ngx_path_separator(*(p - 1))) {
                *p++ = '/';
            }

            ngx_cpystrn(p, (u_char *) NGX_ERROR_LOG_PATH, nlen + 1);

            p = name;
        }
    }

    ngx_log_file.fd = ngx_open_file(name, NGX_FILE_APPEND,
                                    NGX_FILE_CREATE_OR_OPEN,
                                    NGX_FILE_DEFAULT_ACCESS);

    if (ngx_log_file.fd == NGX_INVALID_FILE) {
        ngx_log_stderr(ngx_errno,
                       "[alert] could not open error log file: "
                       ngx_open_file_n " \"%s\" failed", name);
#if (NGX_WIN32)
        ngx_event_log(ngx_errno,
                       "could not open error log file: "
                       ngx_open_file_n " \"%s\" failed", name);
#endif

        ngx_log_file.fd = ngx_stderr;
    }

    if (p) {
        ngx_free(p);
    }

    return &ngx_log;
}


ngx_log_t *
ngx_log_create(ngx_cycle_t *cycle, ngx_str_t *name)
{
    ngx_log_t  *log;

    log = ngx_pcalloc(cycle->pool, sizeof(ngx_log_t));
    if (log == NULL) {
        return NULL;
    }

    log->file = ngx_conf_open_file(cycle, name);
    if (log->file == NULL) {
        return NULL;
    }

    return log;
}


#if (NGX_ENABLE_SYSLOG)
ngx_int_t
ngx_log_get_priority(ngx_conf_t *cf, ngx_str_t *priority)
{
    ngx_int_t  p = 0;
    ngx_uint_t n, match = 0;

    for (n = 0; ngx_syslog_priorities[n].name.len != 0; n++) {
        if (ngx_strncmp(priority->data, ngx_syslog_priorities[n].name.data,
                    ngx_syslog_priorities[n].name.len) == 0) {
            p = ngx_syslog_priorities[n].macro;
            match = 1;
        }
    }

    if (!match) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "invalid syslog priority \"%V\"", priority);
        return -1;
    }

    return p;
}


char *
ngx_log_set_priority(ngx_conf_t *cf, ngx_str_t *priority, ngx_log_t *log)
{
    log->priority = ERR_SYSLOG_PRIORITY;

    if (priority->len == 0) {
        return NGX_CONF_OK;
    }

    log->priority = ngx_log_get_priority(cf, priority);
    if (log->priority == (-1)) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
#endif


char *
ngx_log_set_levels(ngx_conf_t *cf, ngx_log_t *log)
{
    ngx_uint_t   i, n, d, found;
    ngx_str_t   *value;

    value = cf->args->elts;

    for (i = 2; i < cf->args->nelts; i++) {
        found = 0;

        for (n = 1; n <= NGX_LOG_DEBUG; n++) {
            if (ngx_strcmp(value[i].data, err_levels[n].data) == 0) {

                if (log->log_level != 0) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "duplicate log level \"%V\"",
                                       &value[i]);
                    return NGX_CONF_ERROR;
                }

                log->log_level = n;
                found = 1;
                break;
            }
        }

        for (n = 0, d = NGX_LOG_DEBUG_FIRST; d <= NGX_LOG_DEBUG_LAST; d <<= 1) {
            if (ngx_strcmp(value[i].data, debug_levels[n++]) == 0) {
                if (log->log_level & ~NGX_LOG_DEBUG_ALL) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "invalid log level \"%V\"",
                                       &value[i]);
                    return NGX_CONF_ERROR;
                }

                log->log_level |= d;
                found = 1;
                break;
            }
        }


        if (!found) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid log level \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }
    }

    if (log->log_level == NGX_LOG_DEBUG) {
        log->log_level = NGX_LOG_DEBUG_ALL;
    }

    return NGX_CONF_OK;
}


static char *
ngx_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t  *value, name;
#if (NGX_ENABLE_SYSLOG)
    u_char     *off = NULL;
    ngx_str_t  priority;

    ngx_str_null(&name);
    ngx_str_null(&priority);
#endif

    if (cf->cycle->new_log.file) {
        return "is duplicate";
    }

    value = cf->args->elts;

#if (NGX_ENABLE_SYSLOG)
    if (ngx_strncmp(value[1].data, "syslog", sizeof("syslog") - 1) == 0) {
        if (!cf->cycle->new_log.syslog_set) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "You must set the syslog directive and enable it first.");
            return NGX_CONF_ERROR;
        }

        cf->cycle->new_log.syslog_on = 1;

        if (value[1].data[sizeof("syslog") - 1] == ':') {
            priority.len = value[1].len - sizeof("syslog");
            priority.data = value[1].data + sizeof("syslog");

            off = (u_char *)ngx_strchr(priority.data, (int) '|');
            if (off != NULL) {
                priority.len = off - priority.data;

                off++;
                name.len = value[1].data + value[1].len - off;
                name.data = off;
            }
        }
        else {
            if (value[1].len > sizeof("syslog")) {
                name.len = value[1].len - sizeof("syslog");
                name.data = value[1].data + sizeof("syslog");
            }
        }

        if (ngx_log_set_priority(cf, &priority, &cf->cycle->new_log) == NGX_CONF_ERROR) {
            return NGX_CONF_ERROR;
        }
    }
    else if (ngx_strcmp(value[1].data, "stderr") == 0) {
#else
    if (ngx_strcmp(value[1].data, "stderr") == 0) {
#endif
        ngx_str_null(&name);

    } else {
        name = value[1];
    }

    cf->cycle->new_log.file = ngx_conf_open_file(cf->cycle, &name);
    if (cf->cycle->new_log.file == NULL) {
        return NULL;
    }

    if (cf->args->nelts == 2) {
        cf->cycle->new_log.log_level = NGX_LOG_ERR;
        return NGX_CONF_OK;
    }

    cf->cycle->new_log.log_level = 0;

    return ngx_log_set_levels(cf, &cf->cycle->new_log);
}


#if (NGX_ENABLE_SYSLOG)

#define SYSLOG_IDENT_NAME "nginx"

static char *
ngx_set_syslog(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char       *program;
    ngx_str_t  *value;
    ngx_int_t   facility = LOG_LOCAL6, match = 0;
    ngx_uint_t  n;

    value = cf->args->elts;

    if (cf->cycle->new_log.syslog_set) {
        return "is duplicate";
    }

    cf->cycle->new_log.syslog_set = 1;

    for (n = 0; ngx_syslog_facilities[n].name.len != 0; n++) {
        if (ngx_strncmp(value[1].data, ngx_syslog_facilities[n].name.data,
                    ngx_syslog_facilities[n].name.len) == 0) {
            facility = ngx_syslog_facilities[n].macro;
            match = 1;
            break;
        }
    }

    if (match) {
        cf->cycle->new_log.facility = facility;
        cf->cycle->new_log.priority = ERR_SYSLOG_PRIORITY;
    }
    else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "invalid syslog facility \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    program = SYSLOG_IDENT_NAME;
    if (cf->args->nelts > 2) {
        program = (char *) value[2].data;
    }

    openlog(program, LOG_ODELAY, facility);

    return NGX_CONF_OK;
}


void log_exit(ngx_cycle_t *cycle)
{
    if (cycle->new_log.syslog_set) {
        closelog();
    }
}
#endif

