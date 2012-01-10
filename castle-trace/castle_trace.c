#define _BSD_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <castle/castle.h>

enum {
    RUN_STATE_INITIALISING,
    RUN_STATE_STARTING,
    RUN_STATE_RUNNING,
    RUN_STATE_EXITING,
};

static int              run_state       = RUN_STATE_INITIALISING;
static int              tracers_running = 0;
static int              tracers_failed  = 0;
static pthread_cond_t   mt_cond         = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  mt_mutex        = PTHREAD_MUTEX_INITIALIZER;

static char            *trace_dir       = "trace-dir";
static char            *debugfs_dir     = "/sys/kernel/debug";

#define MAX_CPUS    32
static struct tracer {
    int cpu;
    int running;
    pthread_t thread;
    int trace_fd;
} tracers[MAX_CPUS];

static char* trace_type_name[] = {[TRACE_PERCENTAGE] = "percentage",
                                  [TRACE_VALUE] = "value",
                                  [TRACE_MARK]  = "mark",
                                  [TRACE_START] = "start",
                                  [TRACE_END]   = "end"};

static char* cache_var_name[] = {[TRACE_CACHE_CHECKPOINT_ID]            = "checkpoint",
                                 [TRACE_CACHE_DIRTY_PGS_ID]             = "dirty_pgs",
                                 [TRACE_CACHE_CLEAN_PGS_ID]             = "clean_pgs",
                                 [TRACE_CACHE_FREE_PGS_ID]              = "free_pgs",
                                 [TRACE_CACHE_RESERVE_PGS_ID]           = "reserve_pgs",
                                 [TRACE_CACHE_CLEAN_BLKS_ID]            = "clean_blks",
                                 [TRACE_CACHE_FREE_BLKS_ID]             = "free_blks",
                                 [TRACE_CACHE_RESERVE_BLKS_ID]          = "reserve_blks",
                                 [TRACE_CACHE_SOFTPIN_BLKS_ID]          = "softpin_blks",
                                 [TRACE_CACHE_BLOCK_VICTIMS_ID]         = "victim_blks",
                                 [TRACE_CACHE_SOFTPIN_VICTIMS_ID]       = "softpin_victims",
                                 [TRACE_CACHE_READS_ID]                 = "reads",
                                 [TRACE_CACHE_WRITES_ID]                = "writes",
                                 [TRACE_CACHE_RESERVE_PGS_USED_ID]      = "used_reserve_pgs",
                                 [TRACE_CACHE_RESERVE_BLKS_USED_ID]     = "used_reserve_blks",
                                 [TRACE_CACHE_META_DATA_IOS_ID]         = "meta_extent_ios",
                                 [TRACE_CACHE_GLOBAL_BTREE_IOS_ID]      = "global_btree_ios",
                                 [TRACE_CACHE_BLOCK_DEV_IOS_ID]         = "block_dev_ios",
                                 [TRACE_CACHE_INTERNAL_NODES_IOS_ID]    = "non_t0_int_btree_ios",
                                 [TRACE_CACHE_LEAF_NODES_IOS_ID]        = "non_t0_leaf_btree_ios",
                                 [TRACE_CACHE_MEDIUM_OBJECTS_IOS_ID]    = "non_t0_medium_objs_ios",
                                 [TRACE_CACHE_T0_INTERNAL_NODES_IOS_ID] = "t0_int_btree_ios",
                                 [TRACE_CACHE_T0_LEAF_NODES_IOS_ID]     = "t0_leaf_btree_ios",
                                 [TRACE_CACHE_T0_MEDIUM_OBJECTS_IOS_ID] = "t0_medium_objs_ios",
                                 [TRACE_CACHE_LARGE_OBJECT_IOS_ID]      = "large_object_ios",
                                 [TRACE_CACHE_BLOOM_FILTER_IOS_ID]      = "bloom_filter_ios",
				 [TRACE_CACHE_BLK_T0_INT_HIT_MISS_ID]   = "", /* for hits, misses */
                                 [TRACE_CACHE_BLK_T0_INT_HITS_ID]       = "t0_int_get-hits",
                                 [TRACE_CACHE_BLK_T0_INT_MISSES_ID]     = "t0_int_get-misses",
                                 [TRACE_CACHE_BLK_T0_INT_HITS_PCT_ID]   = "t0_int_get-hit_pct",
                                 [TRACE_CACHE_BLK_T0_INT_MISSES_PCT_ID] = "t0_int_get-miss_pct",
				 [TRACE_CACHE_BLK_T0_LEAF_HIT_MISS_ID]   = "", /* for hits, misses */
                                 [TRACE_CACHE_BLK_T0_LEAF_HITS_ID]       = "t0_leaf_get-hits",
                                 [TRACE_CACHE_BLK_T0_LEAF_MISSES_ID]     = "t0_leaf_get-misses",
                                 [TRACE_CACHE_BLK_T0_LEAF_HITS_PCT_ID]   = "t0_leaf_get-hit_pct",
                                 [TRACE_CACHE_BLK_T0_LEAF_MISSES_PCT_ID] = "t0_leaf_get-miss_pct",
				 [TRACE_CACHE_BLK_INT_HIT_MISS_ID]       = "", /* for hits, misses */
                                 [TRACE_CACHE_BLK_INT_HITS_ID]           = "roct_int_get-hits",
                                 [TRACE_CACHE_BLK_INT_MISSES_ID]         = "roct_int_get-misses",
                                 [TRACE_CACHE_BLK_INT_HITS_PCT_ID]       = "roct_int_get-hit_pct",
                                 [TRACE_CACHE_BLK_INT_MISSES_PCT_ID]     = "roct_int_get-miss_pct",
				 [TRACE_CACHE_BLK_LEAF_HIT_MISS_ID]      = "", /* for hits, misses */
				 [TRACE_CACHE_BLK_LEAF_HITS_ID]          = "roct_leaf_get-hits",
                                 [TRACE_CACHE_BLK_LEAF_MISSES_ID]        = "roct_leaf_get-misses",
                                 [TRACE_CACHE_BLK_LEAF_HITS_PCT_ID]      = "roct_leaf_get-hit_pct",
                                 [TRACE_CACHE_BLK_LEAF_MISSES_PCT_ID]    = "roct_leaf_get-miss_pct",
                                 [TRACE_CACHE_BLK_GET_HIT_MISS_ID]       = "", /* for hits, misses */
                                 [TRACE_CACHE_BLK_GET_HITS_ID]           = "block_get-hits",
                                 [TRACE_CACHE_BLK_GET_MISSES_ID]         = "block_get-misses",
                                 [TRACE_CACHE_BLK_GET_HITS_PCT_ID]       = "block_get-hit_pct",
                                 [TRACE_CACHE_BLK_GET_MISSES_PCT_ID]     = "block_get-miss_pct",
				 [TRACE_CACHE_BLK_MERGE_HIT_MISS_ID]     = "",
				 [TRACE_CACHE_BLK_MERGE_HITS_ID]         = "merge_get-hits",
				 [TRACE_CACHE_BLK_MERGE_MISSES_ID]       = "merge_get-misses",
				 [TRACE_CACHE_BLK_MERGE_HITS_PCT_ID]     = "merge_get-hit_pct",
				 [TRACE_CACHE_BLK_MERGE_MISSES_PCT_ID]   = "merge_get-miss_pct",
				 [TRACE_CACHE_BLK_NON_MERGE_HIT_MISS_ID] = "",
				 [TRACE_CACHE_BLK_NON_MERGE_HITS_ID]     = "non_merge_get-hits",
				 [TRACE_CACHE_BLK_NON_MERGE_MISSES_ID]   = "non_merge_get-misses",
				 [TRACE_CACHE_BLK_NON_MERGE_HITS_PCT_ID] = "non_merge_get-hit_pct",
				 [TRACE_CACHE_BLK_NON_MERGE_MISSES_PCT_ID] = "non_merge_get-miss_pct"
};

static char* merge_var_name[] = {
                        [TRACE_DA_MERGE_ID]                             = "merge",
                        [TRACE_DA_INSERTS_DISABLED_ID]                  = "inserts_disabled",
                        [TRACE_DA_MERGE_MODLIST_ITER_INIT_ID]           = "modlist_iter_init",
                        [TRACE_DA_MERGE_UNIT_ID]                        = "merge_unit",
                        [TRACE_DA_MERGE_UNIT_C2B_SYNC_WAIT_BT_NS_ID]    = "c2b_sync_wait_BT_ns",
                        [TRACE_DA_MERGE_UNIT_C2B_SYNC_WAIT_DATA_NS_ID]  = "c2b_sync_wait_DATA_ns",
                        [TRACE_DA_MERGE_UNIT_GET_C2B_NS_ID]             = "get_c2b_ns",
                        [TRACE_DA_MERGE_UNIT_MOBJ_COPY_NS_ID]           = "mobj_copy_ns",
                        [TRACE_DA_MERGE_UNIT_CACHE_BTREE_EFFICIENCY_ID] = "pct_btree_efficiency",
                        [TRACE_DA_MERGE_UNIT_CACHE_DATA_EFFICIENCY_ID]  = "pct_data_efficiency"};

#define ts_fmt         "%lu, %lu"
#define ts_val(_evt)  (_evt)->timestamp.tv_sec, (_evt)->timestamp.tv_usec

static void decode_trace(c_trc_evt_t *evt)
{
    char s[255]; /* let's hope this is large enough */

    if (evt->magic != CASTLE_TRACE_MAGIC)
    {
        fprintf(stderr, "Wrong magic: 0x%x, can only handle 0x%x\n", evt->magic, CASTLE_TRACE_MAGIC);
        return;
    }

    /* initialise string */
    sprintf(s, "***%s(", trace_type_name[evt->type]);

    switch (evt->provider)
    {
        /* DA. */
        case TRACE_DA:
            sprintf(s, "%sda-%02lu-%s", s,
                    evt->v1,    /* da */
                    merge_var_name[evt->var]);  /* stat name */

            if (evt->type == TRACE_VALUE)
                sprintf(s, "%s, %lu", s, evt->v2);   /* value */

            printf("%s, "ts_fmt")\n", s, ts_val(evt));
            break;

        /* Merge. */
        case TRACE_DA_MERGE:
        case TRACE_DA_MERGE_UNIT:
            sprintf(s, "%sda-%02lu-m-%02lu%s-%s", s,
                    evt->v1,    /* da */
                    evt->v2,    /* level */
                    evt->provider == TRACE_DA_MERGE_UNIT ? "-unit" : "",
                    merge_var_name[evt->var]);  /* stat name */

            if (evt->type == TRACE_VALUE)
                sprintf(s, "%s, %lu", s, evt->v4);  /* value */

            printf("%s, "ts_fmt")\n", s, ts_val(evt));
            break;

        /* Cache events. */
        case TRACE_CACHE:
            if (evt->type == TRACE_PERCENTAGE)
            {
                uint64_t p1, p2;

                printf("***%s(%s, %lu, "ts_fmt")\n",
                        trace_type_name[TRACE_VALUE], cache_var_name[++evt->var],
                        evt->v1, ts_val(evt));
                printf("***%s(%s, %lu, "ts_fmt")\n",
                        trace_type_name[TRACE_VALUE], cache_var_name[++evt->var],
                        evt->v2, ts_val(evt));

		if(evt->v1 || evt->v2) { /* guarantees a non-zero denominator */
		    p1 = (100 * evt->v1) / (evt->v1 + evt->v2);
                    p2 = (100 * evt->v2) / (evt->v1 + evt->v2);
		}
		else {
		    p1 = 0;
		    p2 = 0; /* set both percentages to zero */
		}
                printf("***%s(%s, %lu, "ts_fmt")\n",
                        trace_type_name[evt->type], cache_var_name[++evt->var],
                        p1, ts_val(evt));
                printf("***%s(%s, %lu, "ts_fmt")\n",
                        trace_type_name[evt->type], cache_var_name[++evt->var],
                        p2, ts_val(evt));
            }
            else
                printf("***%s(%s, %lu, "ts_fmt")\n",
                        trace_type_name[evt->type], cache_var_name[evt->var],
                        evt->v1, ts_val(evt));

            break;

        /* Unknown. */
        default:
            fprintf(stderr, "Unknown provider: %d\n", evt->provider);
            break;
    }
}

static int handle_args(int argc, char *argv[])
{
    return 0;
}

static int open_trace_file(struct tracer *tracer)
{
#define MAX_FILENAME_LENGTH     512
    char file_name[MAX_FILENAME_LENGTH];

    if(strlen(debugfs_dir) + strlen(trace_dir) > MAX_FILENAME_LENGTH - 5)
    {
        fprintf(stderr, "Could not open the trace file, because the dir name is too long.\n");
        return -EINVAL;
    }

    snprintf(file_name, sizeof(file_name), "%s/%s/trace%d", debugfs_dir, trace_dir, tracer->cpu);
    fprintf(stderr, "Opening trace file: %s\n", file_name);
    tracer->trace_fd = open(file_name, O_RDONLY | O_NONBLOCK);
    if(tracer->trace_fd < 0)
    {
        fprintf(stderr, "Could not open trace file: %s\n", file_name);
        fprintf(stderr, "Is debugfs mounted under: %s?\n", debugfs_dir);
        fprintf(stderr, "Mount with: # mount -t debugfs none %s\n\n", debugfs_dir);
        return errno;
    }

    return 0;
}

static int read_trace(struct tracer *tracer)
{
    c_trc_evt_t evt;
    int ret, i;

    memset(&evt, 0, sizeof(c_trc_evt_t));
    ret = read(tracer->trace_fd, &evt, sizeof(c_trc_evt_t));
    if(ret == sizeof(c_trc_evt_t))
    {
        decode_trace(&evt);
        return 0;
    }

    return -1;
}

static void trace_loop(struct tracer *tracer)
{
    struct pollfd pollfd;
    int ret;
    static int print = 1;

    memset(&pollfd, 0, sizeof(struct pollfd));
    pollfd.fd = tracer->trace_fd;
    pollfd.events = POLLIN;
    while((run_state == RUN_STATE_RUNNING) && (ret = poll(&pollfd, 1, -1)) >= 0)
    {
        if(!(pollfd.revents & POLLIN))
            return;
        if(read_trace(tracer))
            return;
    }
}

static int lock_on_cpu(int cpu)
{
    cpu_set_t cpu_mask;

    CPU_ZERO(&cpu_mask);
    CPU_SET(cpu, &cpu_mask);
    if (sched_setaffinity(0, sizeof(cpu_mask), &cpu_mask) < 0)
        return errno;

    return 0;
}

enum {
    TRACER_READY,
    TRACER_FAILED,
    TRACER_STOPPED,
};

static void tracer_signal(int state)
{
    pthread_mutex_lock(&mt_mutex);
    switch(state)
    {
        case TRACER_READY:
            tracers_running++;
            break;
        case TRACER_FAILED:
            tracers_failed++;
            break;
        case TRACER_STOPPED:
            tracers_running--;
            break;
        default:
            fprintf(stderr, "ERROR: unknown tracer state: %d\n", state);
            break;
    }
    pthread_cond_signal(&mt_cond);
    pthread_mutex_unlock(&mt_mutex);
}

static inline void make_timespec(struct timespec *tsp, long delta_msec)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    tsp->tv_sec = now.tv_sec;
    tsp->tv_nsec = 1000L * now.tv_usec;

    tsp->tv_nsec += (delta_msec * 1000000L);
    if (tsp->tv_nsec > 1000000000L)
    {
        long secs = tsp->tv_nsec / 1000000000L;
        tsp->tv_sec += secs;
        tsp->tv_nsec -= (secs * 1000000000L);
    }
}

static void wait_start(void)
{
    struct timespec ts;

    pthread_mutex_lock(&mt_mutex);
    while (run_state < RUN_STATE_RUNNING)
    {
        make_timespec(&ts, 1000);
        pthread_cond_timedwait(&mt_cond, &mt_mutex, &ts);
    }
    pthread_mutex_unlock(&mt_mutex);
}

static void *tracer_main(void *priv)
{
    struct tracer *tracer = (struct tracer *)priv;
    int ret;

    tracer->running = 1;
    lock_on_cpu(tracer->cpu);
    ret = open_trace_file(tracer);
    if(ret)
        goto err_out;

    tracer_signal(TRACER_READY);
    wait_start();

    trace_loop(tracer);

    close(tracer->trace_fd);
    tracer_signal(TRACER_STOPPED);
    tracer->running = 0;

    return NULL;

err_out:
    tracer_signal(TRACER_FAILED);
    tracer->running = 0;
    return NULL;
}

static int start_tracer(int cpu)
{
    tracers[cpu].cpu = cpu;

    return pthread_create(&tracers[cpu].thread, NULL, tracer_main, tracers + cpu);
}

static void wait_tracers_start(int nr_cpus)
{
    struct timespec ts;

    pthread_mutex_lock(&mt_mutex);
    while(1)
    {
        if(run_state != RUN_STATE_STARTING)
        {
            fprintf(stderr, "ERROR: run state %d while waiting for tracers to start.\n", run_state);
            goto out;
        }
        if(tracers_running + tracers_failed == nr_cpus)
            goto out;
        make_timespec(&ts, 1000);
        pthread_cond_timedwait(&mt_cond, &mt_mutex, &ts);
    }
out:
    pthread_mutex_unlock(&mt_mutex);
}

static int start_tracers(int nr_cpus)
{
    int cpu, ret;

    /* Start all the tracers, only wait for them to initialise, if they have all
       been created. */
    for(cpu=0; cpu<nr_cpus; cpu++)
    {
        ret = start_tracer(cpu);
        if(ret)
            return ret;
    }

    run_state = RUN_STATE_STARTING;
    wait_tracers_start(nr_cpus);

    return 0;
}

static castle_connection *connection_obj = NULL;
static int open_connection(void)
{
    int ret;

    ret = castle_connect(&connection_obj);
    if(ret)
        fprintf(stderr, "Failed to open connection to the filesystem (err=%d). "
                "Make sure it's running.\n", ret);

    return ret;
}

static void close_connection(void)
{
    castle_disconnect(connection_obj);
}

static int setup_trace(void)
{
    int ret;

    ret = castle_trace_setup(connection_obj, trace_dir, strlen(trace_dir)+1);
    if(ret)
        fprintf(stderr, "Could not setup tracing in the filesystem (err=%d).\n", ret);

    return ret;
}

static int start_trace(void)
{
    int ret;

    if((ret = castle_trace_start(connection_obj)))
    {
        fprintf(stderr, "Could not start tracing in the filesystem (err=%d).\n", ret);
        return ret;
    }

    pthread_mutex_lock(&mt_mutex);
    run_state = RUN_STATE_RUNNING;
    pthread_cond_signal(&mt_cond);
    pthread_mutex_unlock(&mt_mutex);

    return 0;
}

static void stop_trace(void)
{
    int ret;

    ret = castle_trace_stop(connection_obj);
    if(ret)
        fprintf(stderr, "Error: Failed to stop the trace, err=%d\n", ret);
}

static void stop_tracers(void)
{
    pthread_mutex_lock(&mt_mutex);
    run_state = RUN_STATE_EXITING;
    pthread_cond_signal(&mt_cond);
    pthread_mutex_unlock(&mt_mutex);
}

static void teardown_trace(void)
{
    int ret;

    ret = castle_trace_teardown(connection_obj);
    if(ret)
        fprintf(stderr, "Error: Failed to teardown the trace, err=%d\n", ret);
}

static void handle_sigint(int sig)
{
    signal(sig, SIG_IGN);
    fprintf(stderr, "Quitting on signal\n");
    stop_trace();
    stop_tracers();
}

static void wait_tracers_stop(int nr_cpus)
{
    int cpu, ret;

    for(cpu=0; cpu<nr_cpus; cpu++)
    {
        if(!tracers[cpu].running)
            continue;
        ret = pthread_join(tracers[cpu].thread, NULL);
        if(ret)
            fprintf(stderr, "Failed to join thread on CPU=%d\n", cpu);
    }
}

int main(int argc, char *argv[])
{
    int ret, cpu, ncpus;

    setlinebuf(stdout);

    ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    memset(tracers, 0, sizeof(struct tracer) * ncpus);
    signal(SIGINT, handle_sigint);
    signal(SIGHUP, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGALRM, handle_sigint);
    signal(SIGPIPE, handle_sigint);

    if((ret = handle_args(argc, argv)))
        goto err0;

    if((ret = open_connection()))
        goto err0;

    if((ret = setup_trace()))
        goto err1;

    if((ret = start_tracers(ncpus)))
        goto err2;

    ret = -EINVAL;
    if((tracers_running != ncpus) ||
       (ret = start_trace()))
        goto err2;

    wait_tracers_stop(ncpus);
    teardown_trace();
    close_connection();
    fprintf(stderr, "Exiting cleanly.\n");

    return 0;

err2:
    stop_tracers();
    wait_tracers_stop(ncpus);
    teardown_trace();
err1:
    close_connection();
err0:
    fprintf(stderr, "Exiting erroneously, ret=%d.\n", ret);

    return ret;
}
