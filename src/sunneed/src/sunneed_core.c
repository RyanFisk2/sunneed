#include "sunneed_core.h"

extern struct sunneed_device devices[MAX_DEVICES];

struct sunneed_pip pip;

void *(*worker_thread_functions[])(void *) = {sunneed_proc_monitor, NULL};

#ifdef TESTING
int (*runtime_tests[])(void) = { NULL };

static unsigned int
testcase_count(void) {
    unsigned int testcases = 0;
    for (int (**cur)(void) = runtime_tests; *cur != NULL; cur++)
        testcases++; 
    return testcases;
}

static int 
run_testcase(unsigned int testcase) {
    if (testcase >= testcase_count()) {
        LOG_E("Cannot run testcase #%d because it does not exist", testcase);
        return 1;
    }

    return runtime_tests[testcase]();
}
#endif

static int
spawn_worker_threads(void) {
    int ret;
    int worker_thread_count = 0;
    for (void *(**cur)(void *) = worker_thread_functions; *cur != NULL; cur++)
        worker_thread_count++;

    pthread_t worker_threads[worker_thread_count];

    LOG_I("Launching %d worker threads", worker_thread_count);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < worker_thread_count; i++) {
        if ((ret = pthread_create(&worker_threads[i], &attr, worker_thread_functions[i], NULL)) != 0) {
            LOG_E("Failed to launch worker thread %d (error %d)", i, ret);
            return 1;
        };
    }

    return 0;
}


void
sunneed_init(void) {
    pip = pip_info();
}

int
main(int argc, char *argv[]) {
    int opt;
    extern int optopt;

#ifdef TESTING
    const char *optstring = ":ht:c";
#else
    const char *optstring = ":h";
#endif

    // TODO Long-form getopts.
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'h':
                printf(HELP_TEXT, argv[0]);
                exit(0);
#ifdef TESTING
            case 't': ;
                int testcase = strtol(optarg, NULL, 10);
                if (errno) {
                    LOG_E("Failed to parse testcase index: %s", strerror(errno));
                    return 1;
                }
                return run_testcase(testcase);
            case 'c':
                printf("%d\n", testcase_count());
                exit(0);
#endif
            case '?':
                fprintf(stderr, "%s: illegal option -%c\n", APP_NAME, optopt);
                exit(1);
            case ':':
                fprintf(stderr, "%s: expected argument for option -%c\n", APP_NAME, optopt);
                exit(1);
        }
    }

    int ret = 0;

    LOG_I("sunneed is initializing...");

    sunneed_init();

    LOG_I("Acquired PIP: %s", pip.name);

    LOG_I("Loading devices...");
    if ((ret = sunneed_load_devices(devices)) != 0) {
        LOG_E("Failed to load devices");
        ret = 1;
        goto end;
    }

    if ((ret = spawn_worker_threads()) != 0) {
        LOG_E("Error occurred while spawning worker threads");
        ret = 1;
        goto end;
    }

    if ((ret = sunneed_listen()) != 0) {
        LOG_E("sunneed listener encountered a fatal error. Exiting.");
        ret = 1;
        goto end;
    }

end:
    return 0;
}
