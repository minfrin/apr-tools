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
 * jose - the json object signing and encryption tool
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include <apr.h>
#include <apu_version.h>

/* apr_jose support requires >= 1.7 */
#if APU_MAJOR_VERSION > 1 || \
    (APU_MAJOR_VERSION == 1 && APU_MINOR_VERSION > 6)
#define HAVE_APU_JOSE 1
#endif

#ifdef HAVE_APU_JOSE

#include <apr_crypto.h>
#include <apr_jose.h>

#include <apr_file_io.h>
#include <apr_getopt.h>
#include <apr_strings.h>

#include "config.h"

#define OPT_CLAIM 'c'
#define OPT_NEWLINE 'n'
#define OPT_READ 'r'
#define OPT_WRITE 'w'
#define OPT_PAYLOAD 'p'
#define OPT_CONTENT_TYPE 't'
#define OPT_SIGNATURE 256
#define OPT_SIGN_COMPACT 257
#define OPT_SIGN_GENERAL 258
#define OPT_SIGN_FLATTENED 259
#define OPT_ENCRYPT 'e'

#define HUGE_STRING_LEN 8192

typedef struct {
    apr_file_t *err;
    const char *library;
    const char *params;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *crypto;
}      crypt_rec;

typedef struct {
    unsigned char *secret;
    apr_size_t secret_len;
    apr_crypto_block_key_digest_e digest;
}      secret_rec;

static const apr_getopt_option_t
      cmdline_opts[] =
{
    /* commands */
    {
        "claim",
        OPT_CLAIM,
        1,
        "  -c, --claim name=val\t\tSet the claim with the given name to the given value."
    },
    {
        "no-newline",
        OPT_NEWLINE,
        0,
        "  -n, --no-newline\t\tSuppress the newline at the end of output."
    },
    {
        "read",
        OPT_READ,
        1,
        "  -r, --read file\t\tName of file to read payload from. '-' for stdin."
    },
    {
        "write",
        OPT_WRITE,
        1,
        "  -w, --write file\t\tName of file to write payload to. '-' for stdout."
    },
    {
        "type",
        OPT_PAYLOAD,
        1,
        "  -p, --payload type\t\tType of payload: 'jwt', 'data', 'text' or 'json'.\n\t\t\t\tDefault to 'jwt'."
    },
    {
        "content-type",
        OPT_CONTENT_TYPE,
        1,
        "  -t, --content-type type\tMIME type of payload. If the MIME type starts with\n\t\t\t\t'application/', that can be omitted. The special\n\t\t\t\tuppercase value 'JWT' is reserved for JWT\n\t\t\t\ttokens, and is the default if unspecified."
    },
    {
        "signature",
        OPT_SIGNATURE,
        1,
        "  --signature alg:key\t\tSign the payload using the algorithm and key in the\n\t\t\t\tfile specified. Algorithm is one of 'none', 'hs256',\n\t\t\t\t'hs384', 'hs512'."
    },
    {
        "sign-compact",
        OPT_SIGN_COMPACT,
        0,
        "  --sign-compact\t\tSign the payload using compact serialisation."
    },
    {
        "sign-general",
        OPT_SIGN_GENERAL,
        0,
        "  --sign-general\t\tSign the payload using general serialisation."
    },
    {
        "sign-flattened",
        OPT_SIGN_FLATTENED,
        0,
        "  --sign-flattened\t\tSign the payload using flattened serialisation."
    },
    {"help", 'h', 0, "  -h, --help\t\t\tDisplay this help message."},
    {"version", 'v', 0,
    "  -v, --version\t\t\tDisplay the version number."},
    {NULL}
};

static int help(apr_file_t * out, const char *name, const char *msg, int code,
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
                    "  %s - JSON Object Signing and Encryption tool.\n"
                    "\n"
                    "SYNOPSIS\n"
                    "  %s [--version] [--help] [...]\n"
                    "\n"
                    "DESCRIPTION\n"
                    "  The tool allows the creation of JOSE objects, such as JWS, JWE and JWT.\n"
                    "\n"
          "  JOSE is described in https://www.rfc-editor.org/rfc/rfc7515,\n"
                    "  https://www.rfc-editor.org/rfc/rfc7516, https://www.rfc-editor.org/rfc/rfc7517\n"
           "  and https://www.rfc-editor.org/rfc/rfc7519, amongst others.\n"
                    "\n"
                    "OPTIONS\n", msg ? msg : "", n, n);

    while (opts[i].name) {
        apr_file_printf(out, "%s\n\n", opts[i].description);
        i++;
    }

    apr_file_printf(out,
                    "RETURN VALUE\n"
                  "  The jose tool returns a non zero exit code on error.\n"
                    "\n"
                    "EXAMPLES\n"
                    "  In the most basic example, we create a JWT payload containing the claim\n"
                    "  'sub' with the value 'principal@example.com'.\n"
                    "\n"
                    "\t~$ jose --claim sub=principal@example.com\n"
                    "\t{\"sub\":\"principal@example.com\"}\n"
                    "\n"
     "  Encoding the JWT payload into a plain compact JWT. The payload is\n"
                    "  unprotected. Output split for readability.\n"
                    "\n"
                    "\t~$ jose --claim sub=principal@example.com --signature none --sign-compact\n"
                    "\teyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.\n\teyJzdWIiOiJwcmluY2lwYWxAZXhhbXBsZS5jb20ifQ.\n"
                    "\n"
                    "  Encoding the JWT payload into a compact JWT protected by a SHA256 HMAC and a\n"
            "  32 byte secret key (of 32 zeros) in the file 'secret.key'.\n"
                    "\n"
                    "\t~$ jose --claim sub=principal@example.com --signature hs256:secret.key --sign-compact\n"
                    "\teyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.\n\teyJzdWIiOiJwcmluY2lwYWxAZXhhbXBsZS5jb20ifQ.\n\tbwLcYDp1nWgT-DIasqbtPQjo3ZvGDRyYNrqYzWyrZY4\n"
                    "\n"
                    "AUTHOR\n"
                    "  Graham Leggett <minfrin@sharp.fm>\n");

    return code;
}

static int version(apr_file_t * out)
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

static apr_status_t read_buffer(apr_file_t * rd, char **buffer,
                                     apr_size_t * length, apr_pool_t * pool)
{
    char *off;
    char *buf;
    apr_size_t len = 1024;
    apr_size_t size = 0, l;

    apr_status_t status;

    off = buf = malloc(len);

    while (APR_SUCCESS
           == (status = apr_file_read_full(rd, off, len - (off - buf),
                                           &l))) {

        size += l;
        len *= 2;

        buf = realloc(buf, len);
        if (!buf) {
            return status;
        }

        off = buf + size;
    }

    apr_pool_cleanup_register(pool, buf, cleanup_buffer, cleanup_buffer);

    size += l;

    *buffer = buf;
    *length = size;

    return status;
}

static apr_status_t read_file(const char *fname, apr_file_t * rd, char **buffer,
                                       apr_size_t * size, apr_pool_t * pool)
{
    apr_status_t status;

    if (strcmp("-", fname)) {
        status = apr_file_open(&rd, fname, APR_FOPEN_READ,
                               APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }
    }

    status = read_buffer(rd, buffer, size, pool);
    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    return APR_SUCCESS;
}

static apr_status_t write_buffer(apr_file_t * out, const char *buffer,
                                              apr_size_t length, int no_newline)
{
    apr_status_t status;
    apr_size_t l;

    /* write the destination */
    status = apr_file_write_full(out, buffer, length, &l);
    if (status != APR_SUCCESS) {
        return status;
    }

    /* write the newline */
    if (!no_newline && length) {
        apr_file_printf(out, "\n");
    }

    return status;
}

static apr_status_t crypt_init(apr_pool_t * pool, crypt_rec * crypt)
{

    if (!crypt->driver) {

        const apu_err_t *err = NULL;
        apr_status_t status;

        status = apr_crypto_init(pool);
        if (APR_SUCCESS != status) {
            apr_file_printf(crypt->err,
                            "Could not crypto init: %pm\n", &status);
            return status;
        }

        status = apr_crypto_get_driver(&crypt->driver, crypt->library, crypt->params,
                                       &err, pool);
        if (APR_EREINIT == status) {
            status = APR_SUCCESS;
        }
        if (APR_SUCCESS != status && err) {
            apr_file_printf(crypt->err,
               "The crypto library '%s' could not be loaded: %s (%s: %d)\n",
                            crypt->library, err->msg, err->reason, err->rc);
            return status;
        }
        if (APR_ENOTIMPL == status) {
            apr_file_printf(crypt->err,
                            "The crypto library '%s' could not be found\n",
                            crypt->library);
            return status;
        }
        if (APR_SUCCESS != status || !crypt->driver) {
            apr_file_printf(crypt->err,
                       "The crypto library '%s' could not be loaded: %pm\n",
                            crypt->library, &status);
            return status;
        }

        status = apr_crypto_make(&crypt->crypto, crypt->driver, crypt->params, pool);
        if (APR_SUCCESS != status) {
            apr_file_printf(crypt->err,
                  "The crypto library '%s' could not be initialised: %pm\n",
                            crypt->library, &status);
            return status;
        }

    }

    return APR_SUCCESS;
}

static apr_status_t sign_cb(apr_bucket_brigade * bb, apr_jose_t * jose,
             apr_jose_signature_t * signature, void *ctx, apr_pool_t * pool)
{
    apr_crypto_key_rec_t *krec;
    apr_crypto_key_t *key = NULL;
    apr_crypto_digest_rec_t *rec;
    apr_crypto_digest_t *digest = NULL;
    apr_bucket *e;
    char *buf;

    crypt_rec *crypt = ctx;
    secret_rec *secret = signature->ctx;
    apr_status_t status;

    if (secret->digest == APR_CRYPTO_DIGEST_NONE) {
        return APR_SUCCESS;
    }

    status = crypt_init(pool, crypt);
    if (APR_SUCCESS != status) {
        return status;
    }

    krec = apr_crypto_key_rec_make(APR_CRYPTO_KTYPE_HMAC, pool);

    krec->k.hmac.digest = secret->digest;
    krec->k.hmac.secret = secret->secret;
    krec->k.hmac.secretLen = secret->secret_len;

    status = apr_crypto_key(&key, krec, crypt->crypto, pool);
    if (status != APR_SUCCESS) {
        jose->result.reason = buf = apr_pcalloc(pool, HUGE_STRING_LEN);
        apr_strerror(status, buf, HUGE_STRING_LEN);
        jose->result.msg = "token could not be signed";
        return status;
    }

    rec = apr_crypto_digest_rec_make(APR_CRYPTO_DTYPE_SIGN, pool);

    status = apr_crypto_digest_init(&digest, key, rec, pool);
    if (status != APR_SUCCESS) {
        jose->result.reason = buf = apr_pcalloc(pool, HUGE_STRING_LEN);
        apr_strerror(status, buf, HUGE_STRING_LEN);
        jose->result.msg = "token could not be signed";
        return status;
    }

    for (e = APR_BRIGADE_FIRST(bb); e != APR_BRIGADE_SENTINEL(bb); e =
         APR_BUCKET_NEXT(e)) {
        const char *str;
        apr_size_t len;

        /* If we see an EOS, don't bother doing anything more. */
        if (APR_BUCKET_IS_EOS(e)) {
            break;
        }

        status = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);
        if (status != APR_SUCCESS) {
            jose->result.reason = buf = apr_pcalloc(pool, HUGE_STRING_LEN);
            apr_strerror(status, buf, HUGE_STRING_LEN);
            jose->result.msg = "token could not be signed";
            return status;
        }

        status = apr_crypto_digest_update(digest,
                                          (const unsigned char *)str, len);
        if (status != APR_SUCCESS) {
            jose->result.reason = buf = apr_pcalloc(pool, HUGE_STRING_LEN);
            apr_strerror(status, buf, HUGE_STRING_LEN);
            jose->result.msg = "token could not be signed";
            return status;
        }
    }

    status = apr_crypto_digest_final(digest);
    if (status != APR_SUCCESS) {
        jose->result.reason = buf = apr_pcalloc(pool, HUGE_STRING_LEN);
        apr_strerror(status, buf, HUGE_STRING_LEN);
        jose->result.msg = "token could not be signed";
        return status;
    }

    signature->sig.data = rec->d.sign.s;
    signature->sig.len = rec->d.sign.slen;

    return APR_SUCCESS;
}

int main(int argc, const char *const argv[])
{
    apr_status_t status;
    apr_pool_t *pool;
    apr_getopt_t *opt;
    const char *optarg;
    int optch;

    apr_file_t *err;
    apr_file_t *in;
    apr_file_t *out;

    apr_file_t *rd;
    apr_file_t *wr;

    char *buffer = NULL;
    apr_size_t size = 0;

    crypt_rec crypt = {0};
    apr_jose_t *jose = NULL;

    /* content type is JWT until further notice */
    const char *cty = APR_JOSE_JWSE_TYPE_JWT;

    apr_json_value_t *json = NULL;
    int must_json = 0;
    int no_newline = 0;

    /* encoding type is JWT until further notice */
    apr_jose_type_e type = APR_JOSE_TYPE_JWT;

    apr_bucket_alloc_t *ba;
    apr_bucket_brigade *bb;

    apr_array_header_t *secrets;
    apr_array_header_t *signatures;

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
    crypt.err = err;

    crypt.library = APU_CRYPTO_RECOMMENDED_DRIVER;

    secrets = apr_array_make(pool, 2, sizeof(secret_rec));
    signatures = apr_array_make(pool, 2, sizeof(apr_jose_signature_t *));

    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
           == APR_SUCCESS) {

        switch (optch) {
        case 'v':{
                version(out);
                return 0;
            }
        case 'h':{
                help(out, argv[0], NULL, 0, cmdline_opts);
                return 0;
            }
        default:{
                break;
            }
        }

    }
    if (APR_SUCCESS != status && APR_EOF != status) {
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }

    /* global options */
    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
           == APR_SUCCESS) {

        switch (optch) {
        case OPT_CLAIM:{
                must_json = 1;
                break;
            }
        case OPT_NEWLINE:{
                no_newline = 1;
                break;
            }
        case OPT_READ:{
                /* read the initial files */
                status = read_file(optarg, rd, &buffer, &size, pool);
                if (status != APR_SUCCESS && status != APR_EOF) {
                    apr_file_printf(err,
                        "Could not read file '%s': %pm\n", optarg, &status);
                    return 1;
                }
                break;
            }
        case OPT_WRITE:{
                if (strcmp("-", optarg)) {
                    status = apr_file_open(&wr, optarg, APR_FOPEN_WRITE | APR_FOPEN_CREATE,
                                           APR_OS_DEFAULT, pool);
                    if (status != APR_SUCCESS) {
                        apr_file_printf(err,
                        "Could not open file '%s' for write: %pm\n", optarg,
                                        &status);
                        return 1;
                    }
                }
                break;
            }
        case OPT_PAYLOAD:{
                if (optarg[0] == 'j' && !strcmp(optarg, "jwt")) {
                    type = APR_JOSE_TYPE_JWT;
                    must_json = 1;
                }
                else if (optarg[0] == 'd' && !strcmp(optarg, "data")) {
                    type = APR_JOSE_TYPE_DATA;
                }
                else if (optarg[0] == 't' && !strcmp(optarg, "text")) {
                    type = APR_JOSE_TYPE_TEXT;
                }
                else if (optarg[0] == 'j' && !strcmp(optarg, "json")) {
                    type = APR_JOSE_TYPE_JSON;
                    must_json = 1;
                }
                else {
                    apr_file_printf(err,
                                    "Type '%s' must be one of: jwt, data, text, json\n", optarg);
                    return 1;
                }
                break;
            }
        case OPT_CONTENT_TYPE:{
                cty = optarg;
                break;
            }
        default:{
                break;
            }
        }

    }

    /* parse json */
    if (must_json) {
        if (!size) {
            json = apr_json_object_create(pool);
        }
        else {
            apr_off_t offset;

            status = apr_json_decode(&json, buffer, size, &offset, APR_JSON_FLAGS_NONE, 10, pool);
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                                "Could not parse json at offset %" APR_OFF_T_FMT ": %pm\n", offset,
                                &status);
                return 1;
            }
        }
    }

    /* process claims */
    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
           == APR_SUCCESS) {

        switch (optch) {
        case OPT_CLAIM:{

                char *str = apr_pstrdup(pool, optarg);

                char *name = str;
                char *val = strchr(str, '=');
                if (val) {
                    *(val++) = 0;
                }

                apr_json_object_set(json, name, strlen(name),
                      apr_json_string_create(pool, val, strlen(val)), pool);

                break;
            }
        default:{
                break;
            }
        }

    }

    /* create initial jose payload */
    switch (type) {
    case APR_JOSE_TYPE_JWT:{
            jose = apr_jose_jwt_make(NULL, json, pool);
            break;
        }
    case APR_JOSE_TYPE_DATA:{
            jose = apr_jose_data_make(NULL, cty, (unsigned char *)buffer, size, pool);
            break;
        }
    case APR_JOSE_TYPE_TEXT:{
            jose = apr_jose_text_make(NULL, cty, buffer, size, pool);
            break;
        }
    case APR_JOSE_TYPE_JSON:{
            jose = apr_jose_json_make(NULL, cty, json, pool);
            break;
        }
    default:{
            break;
        }
    }

    /* our output is our input, until further notice */
    if (json) {

        ba = apr_bucket_alloc_create(pool);
        bb = apr_brigade_create(pool, ba);

        status = apr_json_encode(bb, NULL, NULL, json, APR_JSON_FLAGS_NONE, pool);
        if (status != APR_SUCCESS) {
            apr_file_printf(err,
                            "Could not json encode claims: %pm\n", &status);
            return 1;
        }

        status = apr_brigade_pflatten(bb, &buffer, &size, pool);
        if (status != APR_SUCCESS) {
            apr_file_printf(err,
                            "Could not json encode claims: %pm\n", &status);
            return 1;
        }

    }

    /* encode the buffer */
    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
           == APR_SUCCESS) {

        switch (optch) {
        case OPT_SIGNATURE:{

                secret_rec *secret = apr_array_push(secrets);
                apr_jose_signature_t **signature = apr_array_push(signatures);
                apr_json_value_t *protect = apr_json_object_create(pool);

                char *str = apr_pstrdup(pool, optarg);

                char *alg = str;
                char *name = strchr(str, ':');
                if (name) {
                    *(name++) = 0;
                }

                apr_json_object_set(protect, APR_JOSE_JWSE_TYPE,
                                    APR_JSON_VALUE_STRING,
                                    apr_json_string_create(pool, cty,
                                              APR_JSON_VALUE_STRING), pool);

                if (!strcmp(alg, "none")) {

                    apr_json_object_set(protect, APR_JOSE_JWKSE_ALGORITHM,
                                        APR_JSON_VALUE_STRING,
                             apr_json_string_create(pool, APR_JOSE_JWA_NONE,
                                              APR_JSON_VALUE_STRING), pool);

                    *signature = apr_jose_signature_make(NULL, NULL, protect, secret, pool);

                }
                else if (!strcmp(alg, "hs256")) {

                    apr_json_object_set(protect, APR_JOSE_JWKSE_ALGORITHM,
                                        APR_JSON_VALUE_STRING,
                            apr_json_string_create(pool, APR_JOSE_JWA_HS256,
                                              APR_JSON_VALUE_STRING), pool);

                    secret->digest = APR_CRYPTO_DIGEST_SHA256;

                    if (!name) {
                        apr_file_printf(err,
                                  "File must be specified for '%s'\n", alg);
                        return 1;
                    }

                    /* read the initial files */
                    status = read_file(name, in, (char **)&secret->secret, &secret->secret_len, pool);
                    if (status != APR_SUCCESS && status != APR_EOF) {
                        apr_file_printf(err,
                          "Could not read file '%s': %pm\n", name, &status);
                        return 1;
                    }

                    *signature = apr_jose_signature_make(NULL, NULL, protect, secret, pool);

                }
                else if (!strcmp(alg, "hs384")) {

                    apr_json_object_set(protect, APR_JOSE_JWKSE_ALGORITHM,
                                        APR_JSON_VALUE_STRING,
                            apr_json_string_create(pool, APR_JOSE_JWA_HS384,
                                              APR_JSON_VALUE_STRING), pool);

                    secret->digest = APR_CRYPTO_DIGEST_SHA384;

                    if (!name) {
                        apr_file_printf(err,
                                  "File must be specified for '%s'\n", alg);
                        return 1;
                    }

                    /* read the initial files */
                    status = read_file(name, in, (char **)&secret->secret, &secret->secret_len, pool);
                    if (status != APR_SUCCESS && status != APR_EOF) {
                        apr_file_printf(err,
                          "Could not read file '%s': %pm\n", name, &status);
                        return 1;
                    }

                    *signature = apr_jose_signature_make(NULL, NULL, protect, secret, pool);

                }
                else if (!strcmp(alg, "hs512")) {

                    apr_json_object_set(protect, APR_JOSE_JWKSE_ALGORITHM,
                                        APR_JSON_VALUE_STRING,
                            apr_json_string_create(pool, APR_JOSE_JWA_HS512,
                                              APR_JSON_VALUE_STRING), pool);

                    secret->digest = APR_CRYPTO_DIGEST_SHA512;

                    if (!name) {
                        apr_file_printf(err,
                                  "File must be specified for '%s'\n", alg);
                        return 1;
                    }

                    /* read the initial files */
                    status = read_file(name, in, (char **)&secret->secret, &secret->secret_len, pool);
                    if (status != APR_SUCCESS && status != APR_EOF) {
                        apr_file_printf(err,
                          "Could not read file '%s': %pm\n", name, &status);
                        return 1;
                    }

                    *signature = apr_jose_signature_make(NULL, NULL, protect, secret, pool);

                }
                else {
                    apr_file_printf(err,
                                    "Algorithm '%s' must be one of: 'none', 'hs256', 'hs384', 'hs512'\n", alg);
                }

                break;
            }
        case OPT_SIGN_COMPACT:
        case OPT_SIGN_GENERAL:
        case OPT_SIGN_FLATTENED:{

                apr_jose_cb_t cb = {0};

                apr_jose_signature_t *signature;

                switch (optch) {
                case OPT_SIGN_COMPACT:

                    if (signatures->nelts != 1) {
                        apr_file_printf(err,
                                        "Compact encoding requires exactly one signature (%d found)\n", signatures->nelts);
                        return 1;
                    }

                    signature = ((apr_jose_signature_t **) signatures->elts)[0];

                    jose = apr_jose_jws_make(NULL, signature, NULL, jose, pool);

                    if (strcmp(cty, APR_JOSE_JWSE_TYPE_JWT)) {
                        cty = "JOSE";
                    }

                    break;
                case OPT_SIGN_GENERAL:

                    jose = apr_jose_jws_json_make(NULL, NULL, signatures, jose, pool);

                    if (strcmp(cty, APR_JOSE_JWSE_TYPE_JWT)) {
                        cty = "JOSE+JSON";
                    }

                    break;
                case OPT_SIGN_FLATTENED:

                    if (signatures->nelts != 1) {
                        apr_file_printf(err,
                                        "Flattened encoding requires exactly one signature (%d found)\n", signatures->nelts);
                        return 1;
                    }

                    signature = ((apr_jose_signature_t **) signatures->elts)[0];

                    jose = apr_jose_jws_json_make(NULL, signature, NULL, jose, pool);

                    if (strcmp(cty, APR_JOSE_JWSE_TYPE_JWT)) {
                        cty = "JOSE+JSON";
                    }

                    break;
                }

                cb.sign = sign_cb;
                cb.ctx = &crypt;

                ba = apr_bucket_alloc_create(pool);
                bb = apr_brigade_create(pool, ba);

                status = apr_jose_encode(bb, NULL, NULL, jose, &cb, pool);
                if (status != APR_SUCCESS) {
                    apr_file_printf(err,
                                    "Could not jose encode: %pm\n", &status);
                    return 1;
                }

                status = apr_brigade_pflatten(bb, &buffer, &size, pool);
                if (status != APR_SUCCESS) {
                    apr_file_printf(err,
                                    "Could not jose encode: %pm\n", &status);
                    return 1;
                }

                apr_array_clear(signatures);

                break;
            }
        default:{
                break;
            }
        }

    }

    /* write the destination */
    status = write_buffer(wr, buffer, size, no_newline);
    if (status != APR_SUCCESS) {
        apr_file_printf(err,
                        "Could not write: %pm\n", &status);
        return 1;
    }

    return 0;
}

#else

int main(int argc, const char *const argv[])
{
    fprintf(stderr, "JOSE is not supported on this platform\n");

    return 1;
}

#endif                          /* HAVE_APU_JOSE */
