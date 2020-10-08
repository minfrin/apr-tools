/**
 *    Copyright (C) 2020 Graham Leggett <minfrin@sharp.fm>
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
 * endec - the encoder / decoder / escape / unescape tool
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include <apr.h>
#include <apr_encode.h>
#include <apr_escape.h>
#include <apr_file_io.h>
#include <apr_getopt.h>
#include <apr_strings.h>

#include "config.h"


#define OPT_URL 'u'
#define OPT_DECODE_URL 'U'
#define OPT_FORM 'f'
#define OPT_DECODE_FORM 'F'
#define OPT_PATH 'p'
#define OPT_ENTITY 'e'
#define OPT_DECODE_ENTITY 'E'
#define OPT_ECHO 'c'
#define OPT_ECHOQUOTE 244
#define OPT_LDAP 'l'
#define OPT_LDAP_DN 245
#define OPT_LDAP_FILTER 246
#define OPT_BASE64 'b'
#define OPT_BASE64URL 247
#define OPT_BASE64URL_NOPAD 248
#define OPT_DECODE_BASE64 'B'
#define OPT_BASE32 't'
#define OPT_BASE32HEX 249
#define OPT_BASE32HEX_NOPAD 250
#define OPT_DECODE_BASE32 'T'
#define OPT_DECODE_BASE32HEX 251
#define OPT_BASE16 's'
#define OPT_BASE16COLON 254
#define OPT_BASE16LOWER 253
#define OPT_BASE16COLONLOWER 252
#define OPT_DECODE_BASE16 'S'

static const apr_getopt_option_t
    cmdline_opts[] =
{
    /* commands */
    {
        "url-escape",
        OPT_URL,
        0,
        "  -u, --url-escape  URL escape data as defined in HTML5"
    },
    {
        "url-unescape",
        OPT_DECODE_URL,
        0,
        "  -U, --url-unescape  URL unescape data as defined in HTML5"
    },
    {
        "form-escape",
        OPT_FORM,
        0,
        "  -f, --form-escape  URL escape data as defined in HTML5, with spaces converted to '+'"
    },
    {
        "form-unescape",
        OPT_DECODE_FORM,
        0,
        "  -F, --form-unescape  URL unescape data as defined in HTML5, with '+' converted to spaces"
    },
    {
        "path-escape",
        OPT_PATH,
        0,
        "  -p, --path-escape  Escape a filesystem path to be embedded in a URL"
    },
    {
        "entity-escape",
        OPT_ENTITY,
        0,
        "  -e, --entity-escape  Entity escape data for XML"
    },
    {
        "entity-unescape",
        OPT_DECODE_ENTITY,
        0,
        "  -E, --entity-unescape  Entity unescape data for XML"
    },
    {
        "echo-escape",
        OPT_ECHO,
        0,
        "  -c, --echo-escape  Shell escape data as per echo"
    },
    {
        "echoquote-escape",
        OPT_ECHOQUOTE,
        0,
        "  --echoquote-escape  Shell escape data as per echo, including quotes"
    },
    {
        "ldap-escape",
        OPT_LDAP,
        0,
        "  -l, --ldap-escape  LDAP escape data as per RFC4514 and RFC4515"
    },
    {
        "ldapdn-escape",
        OPT_LDAP_DN,
        0,
        "  --ldapdn-escape  LDAP escape distinguished name data as per RFC4514"
    },
    {
        "ldapfilter-escape",
        OPT_LDAP_FILTER,
        0,
        "  --ldapfilter-escape  LDAP escape filter data as per RFC4515"
    },
    {
        "base64-encode",
        OPT_BASE64,
        0,
        "  -b, --base64-encode  Encode data as base64 as per RFC4648 section 4"
    },
    {
        "base64url-encode",
        OPT_BASE64URL,
        0,
        "  --base64url-encode  Encode data as base64url as per RFC4648 section 5"
    },
    {
        "base64url-nopad-encode",
        OPT_BASE64URL_NOPAD,
        0,
        "  --base64url-nopad-encode  Encode data as base64url with no padding as per rfc7515 appendix C"
    },
    {
        "base64-decode",
        OPT_DECODE_BASE64,
        0,
        "  -B, --base64-decode  Decode data as base64 or base64url"
    },
    {
        "base32-encode",
        OPT_BASE32,
        0,
        "  -t, --base32-encode  Encode data as base32 as per RFC4648 section 6"
    },
    {
        "base32hex-encode",
        OPT_BASE32HEX,
        0,
        "  --base32hex-encode  Encode data as base32hex as per RFC4648 section 7"
    },
    {
        "base32hex-nopad-encode",
        OPT_BASE32HEX_NOPAD,
        0,
        "  --base32hex-nopad-encode  Encode data as base32hex with no padding as per RFC4648 section 7"
    },
    {
        "base32-decode",
        OPT_DECODE_BASE32,
        0,
        "  -T, --base32-decode  Decode data as base32"
    },
    {
        "base32hex-decode",
        OPT_DECODE_BASE32HEX,
        0,
        "  --base32hex-decode  Decode data as base32hex"
    },
    {
        "base16-encode",
        OPT_BASE16,
        0,
        "  -s, --base16-encode  Encode data as base16 as per RFC4648 section 8"
    },
    {
        "base16colon-encode",
        OPT_BASE16COLON,
        0,
        "  --base16colon-encode  Encode data as base16 separated with colons"
    },
    {
        "base16-lower-encode",
        OPT_BASE16LOWER,
        0,
        "  --base16-lower-encode  Encode data as base16 in lower case"
    },
    {
        "base16colon-lower-encode",
        OPT_BASE16COLONLOWER,
        0,
        "  --base16colon-lower-encode  Encode data as base16 with colons in lower case"
    },
    {
        "base16-decode",
        OPT_DECODE_BASE16,
        0,
        "  -S, --base16-decode  Decode data as base16"
    },
    {
        "read",
        'r',
        1,
        "  -r, --read  File to read from. Defaults to stdin."
    },
    {
        "write",
        'w',
        1,
        "  -w, --write  File to write to. Defaults to stdout."
    },
    { "help", 'h', 0, "  -h, --help  Display this help message." },
    { "version", 'v', 0,
        "  -v, --version  Display the version number." },
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
            "  %s - Encode / decode / escape / unescape data.\n"
            "\n"
            "SYNOPSIS\n"
            "  %s [-v] [-h] [-r file] [-w file] [...] [string]\n"
            "\n"
            "DESCRIPTION\n"
            "  The tool applies each specified transformation to the given data in turn,\n"
            "  returning the result on stdout.\n"
            "\n"
            "  In most typical scenarios, data in one format needs to be decoded or\n"
            "  unescaped from a source format and then immediately encoded or escaped\n"
            "  into another format for safe use. By specifying multiple transformations\n"
            "  data can be be passed from one encoding to another.\n"
            "\n"
            "  Decoding and unescaping is performed securely, meaning that any input data\n"
            "  that cannot be decoded or unescaped will cause the tool to exit with a non\n"
            "  zero exit code.\n"
            "\n"
            "OPTIONS\n", msg ? msg : "", n, n);

    while (opts[i].name) {
        apr_file_printf(out, "%s\n\n", opts[i].description);
        i++;
    }

    apr_file_printf(out,
            "RETURN VALUE\n"
            "  The endec tool returns a non zero exit code if invalid data was encountered\n"
            "  during decoding.\n"
            "\n"
            "EXAMPLES\n"
            "  In this example, we decode the base64 string, then entity encode the result.\n"
            "\n"
            "\t~$ endec --base64-decode --entity-escape \"VGhpcyAmIHRoYXQK\"\n"
            "\tThis &amp; that\n"
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
    fprintf(stderr, "Out of memory.");

    return retcode;
}

static apr_status_t cleanup_buffer(void *dummy)
{
    free(dummy);

    return APR_SUCCESS;
}

int main(int argc, const char * const argv[])
{
    apr_status_t status;
    apr_pool_t *pool;
    apr_getopt_t *opt;
    const char *optarg;
    int optch;

    apr_file_t *err;
    apr_file_t *in;
    apr_file_t *out;
    apr_file_t *wr;
    apr_file_t *rd;

    char *buffer;
    const char *result;
    apr_size_t size = 0, l;

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

    rd = in;
    wr = out;

    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
            == APR_SUCCESS) {

        switch (optch) {
        case 'r': {
            status = apr_file_open(&rd, optarg, APR_FOPEN_READ,
                    APR_OS_DEFAULT, pool);
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                        "Could not open file '%s' for read: %pm\n", optarg, &status);
                return 1;
            }
            break;
        }
        case 'w': {
            status = apr_file_open(&wr, optarg, APR_FOPEN_WRITE,
                    APR_OS_DEFAULT, pool);
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                        "Could not open file '%s' for write: %pm\n", optarg, &status);
                return 1;
            }
            break;
        }
        case 'v': {
            version(out);
            return 0;
        }
        case 'h': {
            help(out, argv[0], NULL, 0, cmdline_opts);
            return 0;
        }
        }

    }
    if (APR_SUCCESS != status && APR_EOF != status) {
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }

    /* read the source */
    if (opt->ind < argc) {
        char *off;
        int ind = opt->ind;

        while (ind < argc) {
            if (ind > opt->ind) {
                size++;
            }
            size += strlen(opt->argv[ind]);
            ind++;
        }

        off = buffer = malloc(size + 1);
        ind = opt->ind;

        while (ind < argc) {
            if (ind > opt->ind) {
                strcpy(off++, " ");
            }
            strcpy(off, opt->argv[ind]);
            off += strlen(opt->argv[ind]);
            ind++;
        }

    } else {
        char *off;
        apr_size_t len = 1024;

        off = buffer = malloc(len);

        while (APR_SUCCESS
                == (status = apr_file_read_full(rd, off, len - (off - buffer) - 1,
                        &l))) {

            size += l;
            len *= 2;

            buffer = realloc(buffer, len);
            if (!buffer) {
                return 1;
            }

            off = buffer + size;
        }

        size += l;

        buffer[size] = 0;

    }

    apr_pool_cleanup_register(pool, buffer, cleanup_buffer, cleanup_buffer);
    result = buffer;

    /* apply the transformation */
    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
            == APR_SUCCESS) {

        switch (optch) {
        case OPT_URL: {

            result = apr_pescape_path_segment(pool, result);
            if (!result) {
                apr_file_printf(err,
                        "Could not url escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_DECODE_URL: {

            result = apr_punescape_url(pool, result, NULL, NULL, 0);
            if (!result) {
                apr_file_printf(err,
                        "Could not url unescape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_FORM: {

            result = apr_pescape_urlencoded(pool, result);
            if (!result) {
                apr_file_printf(err,
                        "Could not form url escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_DECODE_FORM: {

            result = apr_punescape_url(pool, result, NULL, NULL, 1);
            if (!result) {
                apr_file_printf(err,
                        "Could not form url unescape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_PATH: {

            result = apr_pescape_path(pool, result, 1);
            if (!result) {
                apr_file_printf(err,
                        "Could not escape the path.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_ENTITY: {

            result = apr_pescape_entity(pool, result, 1);
            if (!result) {
                apr_file_printf(err,
                        "Could not entity escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_DECODE_ENTITY: {

            result = apr_punescape_entity(pool, result);
            if (!result) {
                apr_file_printf(err,
                        "Could not entity unescape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_ECHO: {

            result = apr_pescape_echo(pool, result, 0);
            if (!result) {
                apr_file_printf(err,
                        "Could not echo escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_ECHOQUOTE: {

            result = apr_pescape_echo(pool, result, 1);
            if (!result) {
                apr_file_printf(err,
                        "Could not quote echo escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_LDAP: {

            result = apr_pescape_ldap(pool, result, size, APR_ESCAPE_LDAP_ALL);
            if (!result) {
                apr_file_printf(err,
                        "Could not ldap escape data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_LDAP_DN: {

            result = apr_pescape_ldap(pool, result, size, APR_ESCAPE_LDAP_DN);
            if (!result) {
                apr_file_printf(err,
                        "Could not ldap escape distinguished name data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_LDAP_FILTER: {

            result = apr_pescape_ldap(pool, result, size, APR_ESCAPE_LDAP_FILTER);
            if (!result) {
                apr_file_printf(err,
                        "Could not ldap escape filter data.\n");
                return 1;
            }
            size = strlen(result);

            break;
        }
        case OPT_BASE64: {

            result = apr_pencode_base64(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base64 encode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE64URL: {

            result = apr_pencode_base64(pool, result, size, APR_ENCODE_URL, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base64url encode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE64URL_NOPAD: {

            result = apr_pencode_base64(pool, result, size, APR_ENCODE_BASE64URL, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base64url encode data with no padding.\n");
                return 1;
            }

            break;
        }
        case OPT_DECODE_BASE64: {

            result = apr_pdecode_base64(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base64 decode data, bad characters encountered.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE32: {

            result = apr_pencode_base32(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base64 encode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE32HEX: {

            result = apr_pencode_base32(pool, result, size, APR_ENCODE_BASE32HEX, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base32hex encode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE32HEX_NOPAD: {

            result = apr_pencode_base32(pool, result, size, APR_ENCODE_BASE32HEX | APR_ENCODE_NOPADDING, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base32hex encode data with no padding.\n");
                return 1;
            }

            break;
        }
        case OPT_DECODE_BASE32: {

            result = apr_pdecode_base32(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base32 decode data, bad characters encountered.\n");
                return 1;
            }

            break;
        }
        case OPT_DECODE_BASE32HEX: {

            result = apr_pdecode_base32(pool, result, size, APR_ENCODE_BASE32HEX, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base32hex decode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE16: {

            result = apr_pencode_base16(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base16 encode data.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE16COLON: {

            result = apr_pencode_base16(pool, result, size, APR_ENCODE_COLON, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base16 encode data separated with colons.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE16LOWER: {

            result = apr_pencode_base16(pool, result, size, APR_ENCODE_LOWER, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base16 encode data in lower case.\n");
                return 1;
            }

            break;
        }
        case OPT_BASE16COLONLOWER: {

            result = apr_pencode_base16(pool, result, size, APR_ENCODE_COLON | APR_ENCODE_LOWER, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base16 encode data separated with colons in lower case.\n");
                return 1;
            }

            break;
        }
        case OPT_DECODE_BASE16: {

            result = apr_pdecode_base16(pool, result, size, APR_ENCODE_NONE, &size);
            if (!result) {
                apr_file_printf(err,
                        "Could not base16 decode data, bad characters encountered.\n");
                return 1;
            }

            break;
        }
        }

    }

    /* write the destination */
    status = apr_file_write_full(wr, result, size, &l);
    if (status != APR_SUCCESS) {
        apr_file_printf(err,
                "Could not write: %pm\n", &status);
        return 1;
    }

    return 0;
}
