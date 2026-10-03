/*
 * Termux-MiningDroid / droider
 *
 * C launcher and CPU duty-cycle controller.
 * The actual VerusHash 2.2 mining backend is built separately as
 * ./droider-backend.
 *
 * MIT License for this launcher/integration code.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s -a verushash -o stratum+tcp://POOL:PORT -u WALLET "
        "[-t THREADS] [-p CPU_PERCENT] [-P PASSWORD]\\n\\n"
        "  -a  algorithm: verushash/verus\\n"
        "  -o  Stratum pool URL\\n"
        "  -u  wallet[.worker]\\n"
        "  -t  mining threads\\n"
        "  -p  CPU duty cycle, 1..100\\n"
        "  -P  pool password (default: x)\\n",
        p);
}

static int parse_int(const char *s, int min, int max, int *out) {
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);

    if (errno || end == s || *end != '\\0' || v < min || v > max)
        return 0;

    *out = (int)v;
    return 1;
}

static long long ms_now(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int main(int argc, char **argv) {
    const char *algo = NULL;
    const char *url = NULL;
    const char *user = NULL;
    const char *pass = "x";
    int threads = 0;
    int percent = 100;
    int opt;

    while ((opt = getopt(argc, argv, "a:o:u:t:p:P:h")) != -1) {
        switch (opt) {
            case 'a':
                algo = optarg;
                break;

            case 'o':
                url = optarg;
                break;

            case 'u':
                user = optarg;
                break;

            case 'P':
                pass = optarg;
                break;

            case 't':
                if (!parse_int(optarg, 1, 1024, &threads)) {
                    fprintf(stderr, "Invalid thread count: %s\\n", optarg);
                    return 2;
                }
                break;

            case 'p':
                if (!parse_int(optarg, 1, 100, &percent)) {
                    fprintf(stderr, "CPU percentage must be 1..100\\n");
                    return 2;
                }
                break;

            case 'h':
            default:
                usage(argv[0]);
                return opt == 'h' ? 0 : 2;
        }
    }

    if (!algo ||
        (strcmp(algo, "verushash") != 0 && strcmp(algo, "verus") != 0) ||
        !url ||
        !user) {
        usage(argv[0]);
        return 2;
    }

    if (threads == 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        threads = n > 0 ? (int)n : 1;
    }

    /*
     * The upstream backend uses "verus" as the algorithm name.
     * The public droider CLI accepts the clearer "verushash".
     */
    char threads_s[32];

    snprintf(threads_s, sizeof(threads_s), "%d", threads);

    char *backend_argv[] = {
        "./droider-backend",
        "-a", "verus",
        "-o", (char *)url,
        "-u", (char *)user,
        "-p", (char *)pass,
        "-t", threads_s,
        NULL
    };

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /*
         * Put the backend in its own process group so the controller can
         * pause/resume the miner without stopping the parent shell.
         */
        setpgid(0, 0);
        execv("./droider-backend", backend_argv);
        perror("exec ./droider-backend");
        _exit(127);
    }

    setpgid(pid, pid);

    fprintf(stderr,
        "MiningDroid: VerusHash 2.2 | threads=%d | CPU duty=%d%%\\n",
        threads, percent);

    if (percent == 100) {
        int status;

        waitpid(pid, &status, 0);

        if (WIFEXITED(status))
            return WEXITSTATUS(status);

        if (WIFSIGNALED(status))
            return 128 + WTERMSIG(status);

        return 1;
    }

    /*
     * 100 ms control window.
     * At 50%, the backend runs for about 50 ms and is paused for about
     * 50 ms. This controls process duty cycle, not CPU frequency.
     */
    const long long window = 100;

    while (1) {
        long long run_ms = window * percent / 100;
        long long sleep_ms = window - run_ms;

        if (kill(-pid, SIGCONT) != 0 && errno == ESRCH)
            break;

        long long end = ms_now() + run_ms;

        while (ms_now() < end) {
            int status;
            pid_t r = waitpid(pid, &status, WNOHANG);

            if (r == pid) {
                if (WIFEXITED(status))
                    return WEXITSTATUS(status);

                if (WIFSIGNALED(status))
                    return 128 + WTERMSIG(status);

                return 1;
            }

            usleep(1000);
        }

        if (kill(-pid, SIGSTOP) != 0) {
            if (errno == ESRCH)
                break;

            perror("SIGSTOP");
            break;
        }

        long long sleep_end = ms_now() + sleep_ms;

        while (ms_now() < sleep_end) {
            int status;
            pid_t r = waitpid(pid, &status, WNOHANG);

            if (r == pid) {
                if (WIFEXITED(status))
                    return WEXITSTATUS(status);

                if (WIFSIGNALED(status))
                    return 128 + WTERMSIG(status);

                return 1;
            }

            usleep(1000);
        }
    }

    {
        int status;

        waitpid(pid, &status, 0);

        if (WIFEXITED(status))
            return WEXITSTATUS(status);

        if (WIFSIGNALED(status))
            return 128 + WTERMSIG(status);
    }

    return 1;
}
