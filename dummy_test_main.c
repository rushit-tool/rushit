#include <stdlib.h>
#include "flags.h"
#include "lib.h"
#include "logging.h"

extern int dummy_test(struct options *opts, struct callbacks *cb);

static void check_options(struct options *opts, struct callbacks *cb)
{
        CHECK(cb, opts->test_length >= 1,
              "Test length must be at least 1 second.");
        CHECK(cb, opts->maxevents >= 1,
              "Number of epoll events must be positive.");
        CHECK(cb, opts->num_threads >= 1,
              "There must be at least 1 thread.");
        CHECK(cb, opts->client || (opts->local_host == NULL),
              "local_host may only be set for clients.");
        CHECK(cb, opts->listen_backlog <= procfile_int(PROCFILE_SOMAXCONN, cb),
              "listen() backlog cannot exceed " PROCFILE_SOMAXCONN);
}

int main(int argc, char **argv)
{
	struct options opts = {0};
	struct callbacks cb = {0};
	struct flags_parser *fp;
	int exit_code = 0;

	logging_init(&cb);

        /* Define only flags that are implicitly required because:
         * (1) they are accessed by common (shared) routines, or
         * (2) the uninitialized value (0/false/NULL) is not acceptable.
         */
	fp = flags_parser_create(&opts, &cb);
        DEFINE_FLAG(fp, int,          magic,         42,       0,  "Magic number used by control connections");
        DEFINE_FLAG(fp, int,          maxevents,     1000,     0,  "Number of epoll events per epoll_wait() call");
        DEFINE_FLAG(fp, int,          num_threads,   1,       'T', "Number of threads");
        DEFINE_FLAG(fp, int,          num_clients,   1,        0,  "Number of clients");
        DEFINE_FLAG(fp, int,          test_length,   10,      'l', "Test length in seconds");
        DEFINE_FLAG(fp, int,          listen_backlog, 128,     0,  "Backlog size for listen()");
        DEFINE_FLAG(fp, bool,         ipv4,          false,   '4', "Set desired address family to AF_INET");
        DEFINE_FLAG(fp, bool,         ipv6,          false,   '6', "Set desired address family to AF_INET6");
        DEFINE_FLAG(fp, bool,         client,        false,   'c', "Is client?");
        DEFINE_FLAG(fp, bool,         pin_cpu,       false,   'U', "Pin threads to CPU cores");
        DEFINE_FLAG(fp, bool,         logtostderr,   false,    0,  "Log to stderr");
        DEFINE_FLAG(fp, bool,         nonblocking,   false,    0,  "Make sure syscalls are all nonblocking");
        DEFINE_FLAG(fp, const char *, local_host,    NULL,    'L', "Local hostname or IP address");
        DEFINE_FLAG(fp, const char *, host,          NULL,    'H', "Server hostname or IP address");
        DEFINE_FLAG(fp, const char *, control_port,  "12866", 'C', "Server control port");
        DEFINE_FLAG(fp, const char *, port,          "12867", 'P', "Server data port");
	flags_parser_run(fp, argc, argv);

	/* Hangle unchecked options */
	if (opts.logtostderr)
		cb.logtostderr(cb.logger);

	flags_parser_dump(fp);
	flags_parser_destroy(fp);
	fp = NULL;

	check_options(&opts, &cb);
	/* STUB: Handle checked options */

	exit_code = dummy_test(&opts, &cb);

	logging_exit(&cb);

	return exit_code;
}
