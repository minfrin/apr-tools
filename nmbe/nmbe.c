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
 * nmbe - the native messaging browser extension helper tool
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

#define OPT_FILE 'f'
#define OPT_MESSAGE 'm'
#define OPT_BASE64 'b'

static const apr_getopt_option_t
    cmdline_opts[] =
{
    /* commands */
    {
        "message",
        OPT_MESSAGE,
        1,
        "  -m, --message msg\t\tString to send as a message."
    },
    {
        "message-file",
        OPT_FILE,
        1,
        "  -f, --message-file file\tName of file or directory to send as messages. '-' for stdin."
    },
    {
        "message-base64",
        OPT_BASE64,
        1,
        "  -b, --message-base64 b64\tBase64 encoded array of bytes to send as a message."
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
            "  %s - Native Messaging Browser Extension helper tool.\n"
            "\n"
            "SYNOPSIS\n"
            "  %s [-v] [-h] [-m msg] [-f file] [-b base64]\n"
            "\n"
            "DESCRIPTION\n"
            "  The tool allows the passing of one or more messages to a native messaging\n"
            "  browser extension for the purposes of development, testing or debugging.\n"
            "\n"
            "  Each message is structured as a platform native 32 bit unsigned integer\n"
    		"  containing the length of the message, followed by the message itself. These\n"
    		"  messages are written to stdout in the expectation that they be piped to the\n"
    		"  native messaging browser extension under test.\n"
            "\n"
            "  Messages can be specified as parameters on the command line, or by reference\n"
            "  to a file or directory. Stdin can be specified with '-'.\n"
            "\n"
            "OPTIONS\n", msg ? msg : "", n, n);

    while (opts[i].name) {
        apr_file_printf(out, "%s\n\n", opts[i].description);
        i++;
    }

    apr_file_printf(out,
            "RETURN VALUE\n"
            "  The nmbe tool returns a non zero exit code if the tool is unable to read any\n"
            "  of the messages passed, or if output cannot be written to stdout.\n"
            "\n"
            "EXAMPLES\n"
            "  In this example, we send three separate messages, the first a simple string,\n"
    		"  the second a base64 byte array, and the third a file referring to stdin.\n"
            "\n"
            "\t~$ echo \"{command:'baz'}\" | nmbe --message \"{command:'foo'}\" \\\n"
    		"\t --message-base64 \"e2NvbW1hbmQ6J2Jhcid9\" --message-file -\n"
            "\n"
    		"  This results in the following three messages being sent:\n"
            "\n"
            "\t{command:'foo'}\n"
            "\t{command:'bar'}\n"
            "\t{command:'baz'}\n"
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

static apr_status_t write_buffer(apr_file_t *out, const char *buffer,
		apr_size_t length) {
	apr_status_t status;
	apr_size_t l;
	apr_uint32_t size;

	size = length;

	/* write the size */
	status = apr_file_write_full(out, &size, sizeof(size), &l);
	if (status != APR_SUCCESS) {
		return status;
	}

	/* write the destination */
	status = apr_file_write_full(out, buffer, length, &l);
	if (status != APR_SUCCESS) {
		return status;
	}

	return status;
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
        }

    }
    if (APR_SUCCESS != status && APR_EOF != status) {
        return help(err, argv[0], NULL, EXIT_FAILURE, cmdline_opts);
    }

    /* apply the transformation */
    apr_getopt_init(&opt, pool, argc, argv);
    while ((status = apr_getopt_long(opt, cmdline_opts, &optch, &optarg))
            == APR_SUCCESS) {

        switch (optch) {
        case OPT_MESSAGE: {

            /* write the destination */
            status = write_buffer(out, optarg, strlen(optarg));
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                        "Could not write: %pm\n", &status);
                return 1;
            }

        	break;
        }
        case OPT_FILE: {

            apr_file_t *rd = in;

            char *off;
            char *buffer;
            apr_size_t len = 1024;

			if (strcmp("-", optarg)) {
				status = apr_file_open(&rd, optarg, APR_FOPEN_READ,
						APR_OS_DEFAULT, pool);
				if (status != APR_SUCCESS) {
					apr_file_printf(err,
							"Could not open file '%s' for read: %pm\n", optarg,
							&status);
					return 1;
				}
			}

            off = buffer = malloc(len);

            while (APR_SUCCESS
                    == (status = apr_file_read_full(rd, off, len - (off - buffer),
                            &l))) {

                size += l;
                len *= 2;

                buffer = realloc(buffer, len);
                if (!buffer) {
                    return 1;
                }

                off = buffer + size;
            }

            apr_pool_cleanup_register(pool, buffer, cleanup_buffer, cleanup_buffer);

            if (status != APR_SUCCESS && status != APR_EOF) {
                apr_file_printf(err,
                        "Could not read: %pm\n", &status);
                return 1;
            }

            size += l;

            /* write the destination */
            status = write_buffer(out, buffer, size);
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                        "Could not write: %pm\n", &status);
                return 1;
            }

            break;
        }
        case OPT_BASE64: {

            const char *buffer;

			buffer = apr_pdecode_base64(pool, optarg, strlen(optarg),
					APR_ENCODE_NONE, &size);
            if (!buffer) {
                apr_file_printf(err,
                        "Could not base64 decode data, bad characters encountered.\n");
                return 1;
            }

            /* write the destination */
            status = write_buffer(out, buffer, size);
            if (status != APR_SUCCESS) {
                apr_file_printf(err,
                        "Could not write: %pm\n", &status);
                return 1;
            }

            break;
        }
        }

    }

    return 0;
}
