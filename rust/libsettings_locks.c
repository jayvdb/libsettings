#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#include "libsettings_wrapper.h"

//#define _DEBUG 1

#ifdef _MSC_VER
int clock_gettime_realtime(struct timespec *tv)
{
    FILETIME ft;
    ULARGE_INTEGER hnsTime;

    GetSystemTimeAsFileTime(&ft);

    hnsTime.LowPart = ft.dwLowDateTime;
    hnsTime.HighPart = ft.dwHighDateTime;

    // To get POSIX Epoch as baseline, subtract the number of hns intervals from Jan 1, 1601 to Jan 1, 1970.
    hnsTime.QuadPart -= (11644473600ULL * HNS_PER_SEC);

    // modulus by hns intervals per second first, then convert to ns, as not to lose resolution
    tv->tv_nsec = (long) ((hnsTime.QuadPart % HNS_PER_SEC) * NS_PER_HNS);
    tv->tv_sec = (long) (hnsTime.QuadPart / HNS_PER_SEC);

    return 0;
}
#endif

bool c_libsettings_init(libsettings_ctx_t *ctx) {
  assert(ctx != NULL);

  pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
  assert(mutex != NULL);

  pthread_cond_t *condvar = malloc(sizeof(pthread_cond_t));
  assert(condvar != NULL);

  ctx->lock = mutex;
  ctx->condvar = condvar;

  if (0 != pthread_mutex_init(mutex, NULL)) {
    goto fail;
  }

  if (0 != pthread_cond_init(condvar, NULL)) {
    goto fail;
  }

  return true;

fail:
  free(mutex);
  free(condvar);

  return false;
}

bool c_libsettings_lock(libsettings_ctx_t *ctx) {
  assert(ctx != NULL);

  assert(ctx->lock != NULL);
  assert(ctx->condvar != NULL);

  return 0 == pthread_mutex_lock((pthread_mutex_t *)ctx->lock);
}

bool c_libsettings_unlock(libsettings_ctx_t *ctx) {
  assert(ctx != NULL);

  assert(ctx->lock != NULL);
  assert(ctx->condvar != NULL);

  return 0 == pthread_mutex_unlock((pthread_mutex_t *)ctx->lock);
}

struct timespec timespec_add(struct timespec time1, struct timespec time2) {
  struct timespec result;
  result.tv_sec = time1.tv_sec + time2.tv_sec;
  result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
  if (result.tv_nsec >= 1000000000L) {
    result.tv_sec++;
    result.tv_nsec = result.tv_nsec - 1000000000L;
  }
  return result;
}

bool c_libsettings_wait(libsettings_ctx_t *ctx, uint32_t ms) {
  assert(ctx != NULL);

  assert(ctx->lock != NULL);
  assert(ctx->condvar != NULL);

  struct timespec ts_now = {0};
  struct timespec ts_delta = {.tv_sec = 0, .tv_nsec = 1000000 * ms};
#ifdef _MSC_VER
  int err = clock_gettime_realtime(&ts_now);
#else
  int err = clock_gettime(CLOCK_REALTIME, &ts_now);
#endif
  if (err != 0) {
#ifdef _DEBUG
    fprintf(stderr, "%s: clock_gettime = #%d (%s:%d)\n", __FUNCTION__, err,
            __FILE__, __LINE__);
#endif
  }

  struct timespec ts_exp = timespec_add(ts_now, ts_delta);
#ifdef _DEBUG
  fprintf(stderr,
          "%s: ts_now.tv_sec = %d, ts_now.tv_nsec = %d, ts_exp.tv_sec = %d, "
          "ts_exp.tv_nsec = %d (%s:%d)\n",
          __FUNCTION__, ts_now.tv_sec, ts_now.tv_nsec, ts_exp.tv_sec,
          ts_exp.tv_nsec, __FILE__, __LINE__);
#endif

  err = pthread_cond_timedwait((pthread_cond_t *)ctx->condvar,
                               (pthread_mutex_t *)ctx->lock, &ts_exp);

#ifdef _DEBUG
  if (err != 0)
    fprintf(stderr, "%s: err #%d: %s, ms=%d (%s:%d)\n", __FUNCTION__, err,
            strerror(err), ms, __FILE__, __LINE__);
  fprintf(stderr, "%s: signaled (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
#endif

  if (err == ETIMEDOUT) return true;

  return 0 == err;
}

bool c_libsettings_signal(libsettings_ctx_t *ctx) {
#ifdef _DEBUG
  fprintf(stderr, "%s: enter (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
#endif

  assert(ctx != NULL);

  assert(ctx->lock != NULL);
  assert(ctx->condvar != NULL);

  if (!c_libsettings_lock(ctx)) {
#ifdef _DEBUG
    fprintf(stderr, "%s: c_libsettings_lock failed (%s:%d)\n", __FUNCTION__,
            __FILE__, __LINE__);
#endif
    return false;
  }

  bool res = 0 == pthread_cond_signal((pthread_cond_t *)ctx->condvar);

  if (!c_libsettings_unlock(ctx)) {
#ifdef _DEBUG
    fprintf(stderr, "%s: c_libsettings_unlock failed (%s:%d)\n", __FUNCTION__,
            __FILE__, __LINE__);
#endif
    return false;
  }

#ifdef _DEBUG
  fprintf(stderr, "%s: exit, res=%d (%s:%d)\n", __FUNCTION__, res, __FILE__,
          __LINE__);
#endif

  return res;
}

bool c_libsettings_destroy(libsettings_ctx_t *ctx) {
  assert(ctx != NULL);

  assert(ctx->lock != NULL);
  assert(ctx->condvar != NULL);

  bool result = 0 != pthread_mutex_destroy((pthread_mutex_t *)ctx->lock);
  result = result && 0 != pthread_cond_destroy((pthread_cond_t *)ctx->condvar);

  free(ctx->lock);
  free(ctx->condvar);

  ctx->lock = NULL;
  ctx->condvar = NULL;

  return result;
}
