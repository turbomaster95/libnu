#include <nu.h>
#include <time.h>
#include <errno.h>

static inline uint64_t nu_timespec_to_ns(const struct timespec *ts) {
    return ((uint64_t)ts->tv_sec * 1000000000ULL) + (uint64_t)ts->tv_nsec;
}

uint64_t nu_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return nu_timespec_to_ns(&ts);
}

uint64_t nu_time_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return nu_timespec_to_ns(&ts);
}

void nu_sleep_ns(uint64_t nanoseconds) {
    struct timespec req, rem;
    req.tv_sec = nanoseconds / 1000000000ULL;
    req.tv_nsec = nanoseconds % 1000000000ULL;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}

