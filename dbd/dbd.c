/**
 *    Copyright (C) 2021 Graham Leggett <minfrin@sharp.fm>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*
 * dbd - the database helper tool
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include <apr.h>
#include <apr_lib.h>
#include <apr_encode.h>
#include <apr_escape.h>
#include <apr_file_io.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include <apr_hash.h>

#include <apr_dbd.h>

#include "config.h"

#define OPT_FILE_OUT 'o'
#define OPT_DRIVER 'd'
#define OPT_PARAMS 'p'
#define OPT_ESCAPE 'e'
#define OPT_QUERY 'q'
#define OPT_SELECT 's'
#define OPT_TABLE 't'
#define OPT_ARGUMENT 'a'
#define OPT_FILE_ARGUMENT 'f'
#define OPT_NULL_ARGUMENT 'z'
#define OPT_END_OF_COLUMN 'c'
#define OPT_END_OF_LINE 'l'
#define OPT_NO_END_OF_LINE 'n'
#define OPT_HEADER 257
#define OPT_ENCODING 'x'

#define DBD_DRIVER "DBD_DRIVER"
#define DBD_PARAMS "DBD_PARAMS"

#define DEFAULT_ENCODING "echo"
#define DEFAULT_END_OF_COLUMN "\t"
#define DEFAULT_END_OF_LINE "\n"

#define MAX_BUFFER_SIZE 1024

typedef struct dbd_argument_t {
    const char *encoded;
    const char *decoded;
    apr_file_t *fd;
    apr_size_t size;
    apr_status_t status;
} dbd_argument_t;

typedef struct dbd_buffer_t {
    const char *buf;
    apr_size_t size;
} dbd_buffer_t;

static const apr_getopt_option_t
    cmdline_opts[] =
{
    /* commands */
    {
        "file-out",
        OPT_FILE_OUT,
        1,
        "  -o, --file-out file\t\tFile to write to. Defaults to stdout."
    },
    {
        "driver",
        OPT_DRIVER,
        1,
        "  -d, --driver driver\t\tName of the driver to use for database access. If unspecified, read from DBD_DRIVER."
    },
    {
        "params",
        OPT_PARAMS,
        1,
        "  -p, --params params\t\tParameter string to pass to the database. If unspecified, read from DBD_PARAMS."
    },
    {
        "query",
        OPT_QUERY,
        0,
        "  -q, --query\t\tQuery string to run against the database. Expected to return number of rows affected."
    },
    {
        "escape",
        OPT_ESCAPE,
        0,
        "  -e, --escape\t\tEscape the arguments against the given database, using appropriate escaping for that database."
    },
    {
        "select",
        OPT_SELECT,
        0,
        "  -s, --select\t\tRun select queries against the database. Expected to return database rows as results."
    },
    {
        "table",
        OPT_TABLE,
        0,
        "  -t, --table\t\tRun select queries against the tables in the given database. Expected to return database rows as results."
    },
    {
        "argument",
        OPT_ARGUMENT,
        1,
        "  -a, --argument val\t\tPass an argument to a prepared statement."
    },
    {
        "file-argument",
        OPT_FILE_ARGUMENT,
        1,
        "  -f, --file-argument file\t\tPass a file containing argument to a prepared statement. '-' for stdin."
    },
    {
        "null-argument",
        OPT_NULL_ARGUMENT,
        0,
        "  -z, --null-argument\t\t\tPass a NULL value as an argument to a prepared statement."
    },
    {
        "end-of-column",
        OPT_END_OF_COLUMN,
        1,
        "  -c, --end-of-column end\tUse separator between columns."
    },
    {
        "end-of-line",
        OPT_END_OF_LINE,
        1,
        "  -l, --end-of-line end\t\tUse separator between lines."
    },
    {
        "header",
        OPT_HEADER,
        0,
        "  --header\t\t\tOutput a header on the first line."
    },
    {
        "no-end-of-line",
        OPT_NO_END_OF_LINE,
        0,
        "  -n, --no-end-of-line\tNo separator on last line."
    },
    {
        "encoding",
        OPT_ENCODING,
        1,
        "  -x, --encoding encoding\tEncoding to use. One of 'none', 'base64', 'base64url', 'echo'."
    },
    { "help", 'h', 0, "  -h, --help\t\t\tDisplay this help message." },
    { "version", 'v', 0,
        "  -v, --version\t\t\tDisplay the version number." },
    { NULL }
};

static int help(apr_file_t *out, const char *name, const char *msg, int code,
        const apr_getopt_option_t opts[])
{
    const char *n;
    int i = 0;

    n = strrchr(name, '/');
    if (!n) {
        n = name;
    }
    else {
        n++;
    }

    apr_file_printf(out,
            "%s\n"
            "\n"
            "NAME\n"
            "  %s - Database helper tool.\n"
            "\n"
            "SYNOPSIS\n"
            "  %s [-v] [-h] [-q] [-t] [-s] [-e] [-o file] [-d driver] [-p params] table|query|escape\n"
            "\n"
            "DESCRIPTION\n"
            "  The tool allows queries to be made to a sql database, with the formatting\n"
            "  of the data controlled by the caller. This tool is designed to make database\n"
            "  scripting easier, avoiding the need for text manipulation.\n"
            "\n"
            "  If a table name is specified, a query will be automatically created to select\n"
            "  all data in that table. Alternatively, the query can be specified exactly using\n"
            "  the query option.\n"
            "\n"
            "OPTIONS\n", msg ? msg : "", n, n);

    while (opts[i].name) {
        apr_file_printf(out, "%s\n\n", opts[i].description);
        i++;
    }

    apr_file_printf(out,
            "RETURN VALUE\n"
            "  The dbd tool returns a non zero exit code if the tool is unable to successfully\n"
            "  run the query, or if output cannot be written to stdout.\n"
            "\n"
            "EXAMPLES\n"
            "  In this example, we query all contents of the given table.\n"
            "\n"
            "\t~$ dbd -d \"sqlite3\" -p \"/tmp/database.sqlite3\" -t \"users\" \n"
            "\n"
            "  In this example, we submit a query with arguments.\n"
            "\n"
            "\t~$ dbd -d \"sqlite3\" -p \"/tmp/database.sqlite3\" -a \"1\" -s \"select * from users where id = %%s\" \n"
            "\n"
            "  Here we escape a dangerous string.\n"
            "\n"
            "\t~$ dbd -d \"sqlite3\" -p \"/tmp/database.sqlite3\" -e \"john';drop table users\" \n"
            "\tjohn'';drop table users\n"
            "\n"
            "AUTHOR\n"
            "  Graham Leggett <minfrin@sharp.fm>\n");

    return code;
}

static int version(apr_file_t *out)
{
    apr_file_printf(out, PACKAGE_STRING "\n");

    return 0;
}

static int abortfunc(int retcode)
{
    fprintf(stderr, "Out of memory.\n");

    return retcode;
}

static apr_status_t cleanup_buffer(void *dummy)
{
    free(dummy);

    return APR_SUCCESS;
}

static const char *encode_buffer(apr_pool_t *pool, apr_file_t *err,
        const char *encoding, const char *val, apr_size_t *size)
{
    if (!strcmp("echo", encoding)) {

        val = apr_pescape_echo(pool, val, 1);
        if (!val) {
            apr_file_printf(err,
                    "Could not quote echo escape data.\n");
        }
        *size = strlen(val);

        return val;
    }

    else if (!strcmp("base64", encoding)) {

        val = apr_pencode_base64(pool, val, APR_ENCODE_STRING, APR_ENCODE_NONE, size);
        if (!val) {
            apr_file_printf(err,
                    "Could not base64 escape data.\n");
        }

        return val;
    }

    else if (!strcmp("base64url", encoding)) {

        val = apr_pencode_base64(pool, val, APR_ENCODE_STRING, APR_ENCODE_URL, size);
        if (!val) {
            apr_file_printf(err,
                    "Could not base64url escape data.\n");
        }

        return val;
    }

    else if (!strcmp("none", encoding)) {
        return val;
    }

    apr_file_printf(err,
            "Encoding '%s' must be one of 'none', 'base64', 'base64url', 'echo'.\n", encoding);

    return NULL;
}

static int db_init(apr_pool_t *pool, apr_file_t *err, const char *driver_name, const char *params, const apr_dbd_driver_t **driver, apr_dbd_t **handle)
{
    const char *error = NULL;

    apr_status_t status;

    apr_dbd_init(pool);

    switch ((status = apr_dbd_get_driver(pool, driver_name, driver))) {
    case APR_ENOTIMPL:
        apr_file_printf(err, "DBD: No driver for '%s'\n", driver_name);
        return APR_ENOTIMPL;
    case APR_EDSOOPEN:
        apr_file_printf(err, "DBD: Can't load driver file apr_dbd_%s.so\n",
                    driver_name);
        return APR_EDSOOPEN;
    case APR_ESYMNOTFOUND:
         apr_file_printf(
                err,
                "DBD: Failed to load driver apr_dbd_%s_driver (symbol not found)\n",
                driver_name);
        return APR_ESYMNOTFOUND;
    }

    if (APR_SUCCESS != (status = apr_dbd_open_ex(*driver, pool, params, handle, &error))) {
        apr_file_printf(
                err,
                "DBD: Failed to open a connection to the database (using %s): %s\n",
                driver_name, error);
        return status;
    }

    return status;
}

static apr_status_t dbd_resolve_argument(apr_pool_t *pool, apr_file_t *err,
        dbd_argument_t *arg)
{
    /*
     * In this function we resolve an argument.
     *
     * Arguments might be simple, like a command line option. These
     * options can be resolved again and again.
     *
     * Arguments might be files, which could be read in one go, or
     * might be read with delimiters.
     *
     * We return APR_SUCCESS if we can read this again (perhaps returning
     * the same value), and APR_EOF if we can't read any more.
     */

    if (arg->encoded) {
        arg->decoded = arg->encoded;
        arg->size = strlen(arg->decoded);

        return APR_SUCCESS;
    }

    else if (arg->fd) {

        /* for now, we return the whole file */
        char *buffer;
        char *off;
        apr_size_t len = 1024;
        apr_size_t size = 0, l;

        off = buffer = malloc(len);

        while (APR_SUCCESS
                == (arg->status = apr_file_read_full(arg->fd, off, len - (off - buffer) - 1,
                        &l))) {

            size += l;
            len *= 2;

            buffer = realloc(buffer, len);
            if (!buffer) {
                return APR_ENOMEM;
            }

            off = buffer + size;
        }

        size += l;

        buffer[size] = 0;

        apr_pool_cleanup_register(pool, buffer, cleanup_buffer, cleanup_buffer);

        arg->decoded = buffer;
        arg->size = size;

        return arg->status;
    }

    return APR_SUCCESS;
}

static const void **dbd_arguments(apr_pool_t *pool, apr_file_t *err, const char *query,
        apr_array_header_t *args)
{
    const void **vals;
    int i, j, nargs = 0, nvals = 0;
    const char *q;
    apr_dbd_type_e *t;

    /* find the number of parameters in the query */
    for (q = query; *q; q++) {
        if (q[0] == '%') {
            if (apr_isalpha(q[1])) {
                nargs++;
            } else if (q[1] == '%') {
                q++;
            }
        }
    }
    nvals = nargs;

    t = apr_pcalloc(pool, sizeof(*t) * nargs);

    for (q = query, i = 0; *q; q++) {
        if (q[0] == '%') {
            if (apr_isalpha(q[1])) {
                switch (q[1]) {
                case 'd': t[i] = APR_DBD_TYPE_INT;   break;
                case 'u': t[i] = APR_DBD_TYPE_UINT;  break;
                case 'f': t[i] = APR_DBD_TYPE_FLOAT; break;
                case 'h':
                    switch (q[2]) {
                    case 'h':
                        switch (q[3]){
                        case 'd': t[i] = APR_DBD_TYPE_TINY;  q += 2; break;
                        case 'u': t[i] = APR_DBD_TYPE_UTINY; q += 2; break;
                        }
                        break;
                    case 'd': t[i] = APR_DBD_TYPE_SHORT;  q++; break;
                    case 'u': t[i] = APR_DBD_TYPE_USHORT; q++; break;
                    }
                    break;
                case 'l':
                    switch (q[2]) {
                    case 'l':
                        switch (q[3]){
                        case 'd': t[i] = APR_DBD_TYPE_LONGLONG;  q += 2; break;
                        case 'u': t[i] = APR_DBD_TYPE_ULONGLONG; q += 2; break;
                        }
                        break;
                    case 'd': t[i] = APR_DBD_TYPE_LONG;   q++; break;
                    case 'u': t[i] = APR_DBD_TYPE_ULONG;  q++; break;
                    case 'f': t[i] = APR_DBD_TYPE_DOUBLE; q++; break;
                    }
                    break;
                case 'p':
                    if (q[2] == 'D') {
                        switch (q[3]) {
                        case 't': t[i] = APR_DBD_TYPE_TEXT;       q += 2; break;
                        case 'i': t[i] = APR_DBD_TYPE_TIME;       q += 2; break;
                        case 'd': t[i] = APR_DBD_TYPE_DATE;       q += 2; break;
                        case 'a': t[i] = APR_DBD_TYPE_DATETIME;   q += 2; break;
                        case 's': t[i] = APR_DBD_TYPE_TIMESTAMP;  q += 2; break;
                        case 'z': t[i] = APR_DBD_TYPE_ZTIMESTAMP; q += 2; break;
                        case 'b': t[i] = APR_DBD_TYPE_BLOB;       q += 2; break;
                        case 'c': t[i] = APR_DBD_TYPE_CLOB;       q += 2; break;
                        case 'n': t[i] = APR_DBD_TYPE_NULL;       q += 2; break;
                        }
                    }
                    break;
                }
                q++;

                switch (t[i]) {
                case APR_DBD_TYPE_NONE: /* by default, we expect strings */
                    t[i] = APR_DBD_TYPE_STRING;
                    break;
                case APR_DBD_TYPE_BLOB:
                case APR_DBD_TYPE_CLOB: /* three (3) more values passed in */
                    nvals += 3;
                    break;
                default:
                    break;
                }

            }
        }

    }

    /* sanity check */
    if (args->nelts != nargs) {
        apr_file_printf(
                err,
                "DBD: Database query '%s' expects %d arguments, %d provided.\n",
                query, nargs, args->nelts);
        return NULL;
    }

    vals = apr_pcalloc(pool, sizeof(void*) * nvals);
    for (i = 0, j = 0; i < nargs && j < nvals; i++, j++) {
        apr_status_t status;

        status = dbd_resolve_argument(pool, err, &APR_ARRAY_IDX(args, i, dbd_argument_t));
        switch (status) {
        case APR_SUCCESS:
        case APR_EOF:
            break;
        default: {
            char errbuf[MAX_BUFFER_SIZE];

            apr_file_printf(err, "DBD: Database query '%s' failed while reading: %s\n",
                    query, apr_strerror(status, errbuf, MAX_BUFFER_SIZE));

            return NULL;
        }
        }

        switch (t[i]) {
        case APR_DBD_TYPE_BLOB:
        case APR_DBD_TYPE_CLOB: /* three (3) more values passed in */
            vals[j++] = APR_ARRAY_IDX(args, i, dbd_argument_t).decoded;
            vals[j++] = &APR_ARRAY_IDX(args, i, dbd_argument_t).size;
            vals[j++] = NULL;
            vals[j] = NULL;
            break;
        default:
            vals[i] = APR_ARRAY_IDX(args, i, dbd_argument_t).decoded;
            break;
        }

    }

    return vals;
}

static apr_status_t run_escape(apr_pool_t *pool, apr_file_t *out, apr_file_t *err,
        const char *driver_name, const char *params, const char *eoc,
        const char *eol, int noeol, int argc, const char **argv)
{

    const apr_dbd_driver_t *driver = NULL;
    apr_dbd_t *handle = NULL;

    apr_size_t size;
    apr_status_t status;

    char errbuf[MAX_BUFFER_SIZE];

    /* init the database, prepare our query */
    if ((status = db_init(pool, err, driver_name, params, &driver, &handle))) {
        return status;
    }

    /* create the query, and escape if necessary */
    while (argc--) {
         const char *escape = apr_dbd_escape(
                driver, pool, *(argv++), handle);

         if (!escape) {
             return APR_EINVAL;
         }

         size = strlen(escape);

         if (APR_SUCCESS != (status = apr_file_write(out, escape, &size))) {
             apr_file_printf(
                     err,
                     "DBD: Database escape failed while writing: %s\n",
                     apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
             return status;
         }

         if (argc) {
             size = strlen(eoc);
             if (APR_SUCCESS != (status = apr_file_write(out, eoc, &size))) {
                 apr_file_printf(err, "DBD: Database escape failed while writing end of column: %s\n",
                         apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                 return status;
             }
         }

    }

    if (!noeol) {
        size = strlen(eol);
        if (APR_SUCCESS != (status = apr_file_write(out, eol, &size))) {
            apr_file_printf(
                    err,
                    "DBD: Database escape failed while writing end of line: %s\n",
                    apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
            return status;
        }
    }

    return status;
}

static apr_status_t run_query(apr_pool_t *pool, apr_file_t *out, apr_file_t *err,
        const char *driver_name, const char *params, apr_array_header_t *args,
        const char *eoc, const char *eol, const char *encoding, int header,
        int noeol, int argc, const char **argv)
{

    const apr_dbd_driver_t *driver = NULL;
    apr_dbd_t *handle = NULL;
    const char *query;
    apr_dbd_prepared_t *statement = NULL;
    const void **pargs = NULL;

    apr_size_t size;
    apr_status_t status;

    char errbuf[MAX_BUFFER_SIZE];

    int rows = 0;

    /* init the database, prepare our query */
    if ((status = db_init(pool, err, driver_name, params, &driver, &handle))) {
        return status;
    }

    /* create the query, and escape if necessary */
    if (argc == 1) {

        /* create the query, and escape if necessary */
        query = *(argv++);

        if (APR_SUCCESS != (status = apr_dbd_prepare(driver, pool, handle, query,
                NULL, &statement))) {
            apr_file_printf(err, "DBD: Database prepare query '%s' failed (using %s): %s\n",
                    query, driver_name, apr_dbd_error(driver, handle, status));
            return status;
        }

        pargs = dbd_arguments(pool, err, query, args);
        if (!pargs) {
            return APR_EINVAL;
        }

        if (APR_SUCCESS != (status = apr_dbd_pbquery(driver, pool, handle, &rows,
            statement, pargs))) {
            apr_file_printf(err, "DBD: Database query '%s' failed (using %s): %s\n",
                    query, driver_name, apr_dbd_error(driver, handle, status));
            return status;
        }

        apr_file_printf(out, "%d", rows);

    }
    else {
        apr_file_printf(
                err,
                "DBD: one query needs to be specified.\n");
        return APR_EOF;
    }

    if (!noeol) {
        size = strlen(eol);
        if (APR_SUCCESS != (status = apr_file_write(out, eol, &size))) {
            apr_file_printf(
                    err,
                    "DBD: Database query '%s' failed while writing end of line: %s\n",
                    query,
                    apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
            return status;
        }
    }

    if (rows) {
        return APR_SUCCESS;
    }
    else {
        return APR_EOF;
    }
}

static apr_status_t run_select(apr_pool_t *pool, apr_file_t *out, apr_file_t *err,
        const char *driver_name, const char *params, int table,
        int select, apr_array_header_t *args, const char *eoc, const char *eol,
        const char *encoding, int header, int noeol, int argc, const char **argv)
{

    const apr_dbd_driver_t *driver = NULL;
    apr_dbd_t *handle = NULL;
    const char *query;
    apr_dbd_prepared_t *statement = NULL;
    const void **pargs = NULL;
    apr_dbd_results_t *res = NULL;
    apr_dbd_row_t *row = NULL;

    apr_size_t size;
    apr_status_t status;

    char errbuf[MAX_BUFFER_SIZE];

    int i, end = 0;

    /* init the database, prepare our query */
    if ((status = db_init(pool, err, driver_name, params, &driver, &handle))) {
        return status;
    }

    /* create the query, and escape if necessary */
    while (argc--) {

        /* create the query, and escape if necessary */
        if (table) {
            query = apr_psprintf(pool, "select * from %s", apr_dbd_escape(
                    driver, pool, *(argv++), handle));
        }
        else {
            query = *(argv++);
        }

        if (APR_SUCCESS != (status = apr_dbd_prepare(driver, pool, handle, query,
                NULL, &statement))) {
            apr_file_printf(err, "DBD: Database prepare select '%s' failed (using %s): %s\n",
                    query, driver_name, apr_dbd_error(driver, handle, status));
            return status;
        }

        pargs = dbd_arguments(pool, err, query, args);
        if (!pargs) {
            return APR_EINVAL;
        }

        if (APR_SUCCESS != (status = apr_dbd_pbselect(driver, pool, handle, &res,
            statement, 0, pargs))) {
            apr_file_printf(err, "DBD: Database select '%s' failed (using %s): %s\n",
                    query, driver_name, apr_dbd_error(driver, handle, status));
            return status;
        }

        if (header) {
            const char *name;
            /* get the names of the columns for the first row */
            i = 0;
            for (name = apr_dbd_get_name(driver, res, i);
                    name != NULL;
                    name = apr_dbd_get_name(driver, res, i)) {

                if (i > 0) {
                    size = strlen(eoc);
                    if (APR_SUCCESS != (status = apr_file_write(out, eoc, &size))) {
                        apr_file_printf(err, "DBD: Database select '%s' failed while writing end of column: %s\n",
                                query, apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                        return status;
                    }
                }
                name = encode_buffer(pool, err, encoding, name, &size);
                if (!name) {
                    return APR_EGENERAL;
                }
                if (APR_SUCCESS != (status = apr_file_write(out, name, &size))) {
                    apr_file_printf(err, "DBD: Database select '%s' failed while writing header: %s\n",
                            query, apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                    return status;
                }

                i++;
                end = 1;
            }
        }

        /* write the rows */
        for (status = apr_dbd_get_row(driver, pool, res, &row, -1); status != -1; status
                = apr_dbd_get_row(driver, pool, res, &row, -1)) {
            const char *entry;
            if (status != APR_SUCCESS) {
                apr_file_printf(
                        err,
                        "DBD: Database select '%s' failed while reading row (using %s): %s\n",
                        query, driver_name, apr_dbd_error(driver, handle, status));
                return status;
            }

            if (end) {
                size = strlen(eol);
                if (APR_SUCCESS != (status = apr_file_write(out, eol, &size))) {
                    apr_file_printf(
                            err,
                            "DBD: Database select '%s' failed while writing end of line: %s\n",
                            query,
                            apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                    return status;
                }
            }

            /* get the entries of each row */
            i = 0;
            for (entry = apr_dbd_get_entry(driver, row, i); entry != NULL; entry
                    = apr_dbd_get_entry(driver, row, i)) {

                if (i > 0) {
                    size = strlen(eoc);
                    if (APR_SUCCESS != (status = apr_file_write(out, eoc,
                            &size))) {
                        apr_file_printf(
                                err,
                                "DBD: Database select '%s' failed while writing end of column: %s\n",
                                query, apr_strerror(status, errbuf,
                                        MAX_BUFFER_SIZE));
                        return status;
                    }
                }
                entry = encode_buffer(pool, err, encoding, entry, &size);
                if (!entry) {
                    return APR_EGENERAL;
                }
                if (APR_SUCCESS != (status = apr_file_write(out, entry, &size))) {
                    apr_file_printf(
                            err,
                            "DBD: Database select '%s' failed while writing entry: %s\n",
                            query,
                            apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                    return status;
                }

                i++;
            }
            end = 1;
        }

    }

    if (!noeol) {
        size = strlen(eol);
        if (APR_SUCCESS != (status = apr_file_write(out, eol, &size))) {
            apr_file_printf(
                    err,
                    "DBD: Database select '%s' failed while writing end of line: %s\n",
                    query,
                    apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
            return status;
        }
    }

    return APR_SUCCESS;
}


int main(int argc, const char * const argv[])
{
    apr_status_t status;
    apr_pool_t *pool;
    apr_getopt_t *opt;
    const char *optarg;
    int optch;
    int header = 0;
    int noeol = 0;

    apr_file_t *err;
    apr_file_t *in;
    apr_file_t *out;

    apr_array_header_t *args;
    apr_hash_t *fds;

    const char *driver = getenv(DBD_DRIVER);
    const char *params = getenv(DBD_PARAMS);
    const char *eoc = DEFAULT_END_OF_COLUMN;
    const char *eol = DEFAULT_END_OF_LINE;
    const char *encoding = DEFAULT_ENCODING;

    int escape = 0;
    int query = 0;
    int select = 0;
    int table = 0;

    /* lets get APR off the ground, and make sure it terminates cleanly */
    if (APR_SUCCESS != (status = apr_app_initialize(&argc, &argv, NULL))) {
        return 1;
    }
    atexit(apr_terminate);

    if (APR_SUCCESS != (status = apr_pool_create_ex(&pool, NULL, abortfunc, NULL))) {
        return 1;
    }

    apr_file_open_stderr(&err, pool);
    apr_file_open_stdin(&in, pool);
    apr_file_open_stdout(&out, pool);

    args = apr_array_make(pool, argc, sizeof(dbd_argument_t));
    fds = apr_hash_make(pool);

    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
            == APR_SUCCESS) {

        switch (optch) {
        case 'v': {
            version(out);
            return 0;
        }
        case 'h': {
            help(out, argv[0], NULL, 0, cmdline_opts);
            return 0;
        }
        case OPT_DRIVER: {
            driver = optarg;
            break;
        }
        case OPT_PARAMS: {
            params = optarg;
            break;
        }
        case OPT_FILE_OUT: {
            if (APR_SUCCESS != (status = apr_file_open(&out, optarg, APR_WRITE
                    | APR_CREATE | APR_TRUNCATE, APR_OS_DEFAULT, pool))) {
                char errbuf[MAX_BUFFER_SIZE];
                apr_file_printf(err, "DBD: Could not open '%s': %s\n", optarg,
                        apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                exit(status);
            }
            break;
        }
        case OPT_ESCAPE: {
            escape++;
            break;
        }
        case OPT_QUERY: {
            query++;
            break;
        }
        case OPT_SELECT: {
            select++;
            break;
        }
        case OPT_TABLE: {
            table++;
            break;
        }
        case OPT_ARGUMENT: {

            dbd_argument_t *arg = apr_array_push(args);
            arg->encoded = optarg;

            break;
        }
        case OPT_FILE_ARGUMENT: {

            dbd_argument_t *arg = apr_array_push(args);

            if (!strcmp(optarg, "-")) {
                arg->fd = in;
            }
            else {
                arg->fd = apr_hash_get(fds, optarg, APR_HASH_KEY_STRING);
                if (!arg->fd) {
                    if (APR_SUCCESS != (status = apr_file_open(&arg->fd, optarg, APR_READ,
                            APR_OS_DEFAULT, pool))) {
                        char errbuf[MAX_BUFFER_SIZE];
                        apr_file_printf(err, "DBD: Could not open '%s': %s\n", optarg,
                                apr_strerror(status, errbuf, MAX_BUFFER_SIZE));
                        exit(status);
                    }
                    apr_hash_set(fds, optarg, APR_HASH_KEY_STRING, arg->fd);
                }
            }

            break;
        }
        case OPT_NULL_ARGUMENT: {

            apr_array_push(args);

            break;
        }
        case OPT_END_OF_COLUMN: {
            eoc = optarg;
            break;
        }
        case OPT_END_OF_LINE: {
            eol = optarg;
            break;
        }
        case OPT_NO_END_OF_LINE: {
            noeol++;
            break;
        }
        case OPT_HEADER: {
            header++;
            break;
        }
        case OPT_ENCODING: {
            encoding = optarg;
            break;
        }
        }

    }
    if (APR_SUCCESS != status && APR_EOF != status) {
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }

    if (!driver) {
        apr_file_printf(err, "DBD: --driver must be specified.\n");
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }
    if (!params) {
        apr_file_printf(err, "DBD: --params must be specified.\n");
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }

    if (escape) {

        status = run_escape(pool, out, err, driver, params, eoc, eol, noeol,
                argc - opt->ind, opt->argv + opt->ind);

    }

    else if (table || select) {

        status = run_select(pool, out, err, driver, params, table, select,
                args, eoc, eol, encoding, header, noeol, argc - opt->ind,
                opt->argv + opt->ind);

    }
    else if (query) {

        status = run_query(pool, out, err, driver, params, args, eoc, eol,
                            encoding, header, noeol, argc - opt->ind,
                            opt->argv + opt->ind);

    }
    else {

        apr_file_printf(err,
                "DBD: One of --escape, --table, --select, or --query must be specified.\n");
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);

    }

    apr_pool_destroy(pool);

    switch (status) {
    case APR_SUCCESS:
        exit(0);

    case APR_EINVAL:
        exit(1);

    default:
        exit(2);
    }

}
