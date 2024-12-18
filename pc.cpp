#include "config.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <getopt.h>
#include <mutex>
#include <sched.h>
#include <thread>
#include <vector>
#include <sys/timepps.h>
#if HAVE_GPS
#include <libgpsmm.h>
#endif


// apt install pps-tools

#define MILLION (1000 * 1000)
#define BILLION (MILLION * 1000)

double diff_timespec(const struct timespec *time1, const struct timespec *time0)
{
	return (time1->tv_sec - time0->tv_sec) + (time1->tv_nsec - time0->tv_nsec) / double(BILLION);
}

struct result_t {
	timespec   ts;
	bool       valid;
	std::mutex lock;
	std::condition_variable cv;
};

std::atomic_bool stop { false };

int get_cpu_count()
{
	cpu_set_t cpuset { };
	sched_getaffinity(0, sizeof(cpuset), &cpuset);
	return CPU_COUNT(&cpuset);
}

void set_thread_affinity(const int nr)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(nr, &cpuset);

	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
		fprintf(stderr, "Failed to set thread affinity!\n");
}

void set_thread_priority(const int nr)
{
    sched_param params { };
    params.sched_priority = nr;

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &params))
	    fprintf(stderr, "Failed to set thread priority\n");
}

pps_handle_t open_pps(const char *const filename)
{
	int fd = open(filename, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Cannot open %s: %s\n", filename, strerror(errno));
		exit(1);
	}

	pps_handle_t handle { };
	time_pps_create(fd, &handle);

	int available_modes { 0 };
	time_pps_getcap(handle, &available_modes);
	if ((available_modes & PPS_CAPTUREASSERT) == 0) {
		fprintf(stderr, "Cannot CAPTUREASSERT from %s: %s\n", filename, strerror(errno));
		exit(1);
	}

	pps_params_t params { };
	time_pps_getparams(handle, &params);
	params.mode |= PPS_CAPTUREASSERT;
	time_pps_setparams(handle, &params);

	return handle;
}

void get_pps(const char *const filename, result_t *const r, const int thread_nr)
{
	pps_handle_t handle = open_pps(filename);

	// make sure they're not on the same cpu
	if (get_cpu_count() >= 3)
		set_thread_affinity(thread_nr);

	set_thread_priority(99);

	while(!stop) {
		pps_info_t infobuf { };
		if (time_pps_fetch(handle, PPS_TSFMT_TSPEC, &infobuf, nullptr) == -1) {
			fprintf(stderr, "Cannot time_pps_fetch from %s: %s\n", filename, strerror(errno));
			exit(1);
		}

		{
			std::unique_lock<std::mutex> lck(r->lock);
			r->ts.tv_sec  = infobuf.assert_timestamp.tv_sec;
			r->ts.tv_nsec = infobuf.assert_timestamp.tv_nsec;
			r->valid      = true;
			r->cv.notify_one();
		}
	}
}

#if HAVE_GPS
void get_gps(const char *const host, std::atomic_int *const fix, std::atomic_uint64_t *const hdop)
{
	gpsmm gps_rec(host, DEFAULT_GPSD_PORT);

	if (gps_rec.stream(WATCH_ENABLE | WATCH_JSON) == nullptr) {
		fprintf(stderr, "gps not running?\n");
		return;
	}

	while(!stop) {
		if (!gps_rec.waiting(MILLION))
			continue;

		gps_data_t * gpsd_data = gps_rec.read();
		if (!gpsd_data) {
			fprintf(stderr, "Read fail from gpsd\n");
			break;
		}

		*fix  = gpsd_data->fix.mode;
		*hdop = gpsd_data->dop.hdop * MILLION;
	}
}
#endif

void set_scheduling()
{
	sched_param params { };
	params.sched_priority = 99;  // maximum priority

	if (sched_setscheduler(0, SCHED_FIFO, &params))
		fprintf(stderr, "Problem setting scheduling to real-time\n");
}

void emit(const char *const log_file, const char str[])
{
	printf("%s", str);

	if (log_file) {
		FILE *fh = fopen(log_file, "a+");
		if (fh) {
			fprintf(fh, "%s", str);
			fclose(fh);
		}
		else {
			fprintf(stderr, "\"%s\" is in accessible\n", log_file);
			exit(1);
		}
	}
}

void sigh(int s)
{
	stop = true;
}

void help()
{
	printf("pps-comparer, by Folkert van Heusden\n\n");
	printf("-1 x   pps device 1\n");
	printf("-2 x   pps device 2\n");
	printf("-l x   logfile (optional)\n");
	printf("-s     \"scientific notation\"\n");
#if HAVE_GPS
	printf("-g x   connect to gpsd daemon on host 'x'\n");
#endif
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	const char *dev_1    = "/dev/pps0";
	const char *dev_2    = "/dev/pps1";
	const char *log_file = nullptr;
	int         c        = -1;
	bool        s_not    = false;
	const char *gps_host = nullptr;
	while((c = getopt(argc, argv, "1:2:l:sg:h")) != -1) {
		if (c == '1')
			dev_1 = optarg;
		else if (c == '2')
			dev_2 = optarg;
		else if (c == 'l')
			log_file = optarg;
		else if (c == 's')
			s_not = true;
		else if (c == 'g')
			gps_host = optarg;
		else if (c == 'h') {
			help();
			return 0;
		}
		else {
			help();
			return 1;
		}
	}

	result_t r1;
	r1.valid = false;
	std::thread th1(get_pps, dev_1, &r1, 1);

	result_t r2;
	r2.valid = false;
	std::thread th2(get_pps, dev_2, &r2, 2);

#if HAVE_GPS
	std::atomic_int      fix  { 0 };
	std::atomic_uint64_t hdop { 0 };
	std::thread th_gps(get_gps, gps_host, &fix, &hdop);
#endif

	printf("Number of CPUs in system: %d\n", get_cpu_count());

	set_thread_affinity(0);
	set_thread_priority(98);  // high but below that of the two threads

	signal(SIGINT, sigh);

	long double total_difference = 0.;
	long double total_sd         = 0.;
	long double total_diff_diff  = 0.;
	double      previous_diff    = 0.;
	unsigned    n                = 0;
	double      pt1              = 0;
	unsigned    n_missing_1      = 0;
	double      pt2              = 0;
	unsigned    n_missing_2      = 0;
	std::vector<double> median;

#if HAVE_GPS
	const char header[] = "ts1 ts2 difference missing1/2 difference-drift fix hdop\n";
#else
	const char header[] = "ts1 ts2 difference missing1/2 difference-drift\n";
#endif
	emit(log_file, header);
	while(!stop) {
		timespec ts1 { };

		{
			std::unique_lock<std::mutex> lk1(r1.lock);
			r1.cv.wait_for(lk1, std::chrono::milliseconds(1050), [&]{ return r1.valid; });
			if (r1.valid == false) {
				n_missing_1++;
				continue;
			}
			ts1      = r1.ts;
			r1.valid = false;
		}

		uint64_t d_ts1 = ts1.tv_sec + ts1.tv_nsec / BILLION;
		if (d_ts1 - pt1 >= 2 && pt1 > 0)
			n_missing_1++;
		pt1 = d_ts1;

		timespec ts2 { };

		{
			std::unique_lock<std::mutex> lk2(r2.lock);
			r2.cv.wait_for(lk2, std::chrono::milliseconds(900), [&]{ return r2.valid; });
			if (r2.valid == false) {
				n_missing_2++;
				continue;
			}
			ts2      = r2.ts;
			r2.valid = false;
		}

		uint64_t d_ts2 = ts2.tv_sec + ts2.tv_nsec / BILLION;
		if (d_ts2 - pt2 >= 2 && pt2 > 0)
			n_missing_2++;
		pt2 = d_ts2;

		if (stop)
			break;

		double difference = diff_timespec(&ts1, &ts2);
		char  *gps_buffer = nullptr;
#if HAVE_GPS
		asprintf(&gps_buffer, " %d %f", fix.load(), hdop / double(MILLION));
#endif
		char  *buffer     = nullptr;
		const char *fmt   = s_not ? "%u %ld.%09ld %ld.%09ld %e %u/%u %Le%s\n" : "%u %ld.%09ld %ld.%09ld %.09f %u/%u %.9Lf%s\n";
		asprintf(&buffer, fmt, n + 1, ts1.tv_sec, ts1.tv_nsec, ts2.tv_sec, ts2.tv_nsec, difference, n_missing_1, n_missing_2, n >= 1 ? total_diff_diff / n: -1., gps_buffer ? gps_buffer : "");
		emit(log_file, buffer);
		free(buffer);
#if HAVE_GPS
		free(gps_buffer);
#endif

		total_difference   += difference;
		total_sd           += difference * difference;
		n++;
		if (n >= 2)
			total_diff_diff += difference - previous_diff;
		previous_diff = difference;

		median.push_back(difference);
	}

#if HAVE_GPS
	th_gps.join();
#endif
	th2.join();
	th1.join();

	if (n) {
		double avg = total_difference / n;
		double sd  = sqrt(total_sd / n - avg * avg);

		std::sort(median.begin(), median.end());
		double med = median.at(n / 2);
		if ((n & 1) == 0)
			med = (med + median.at(n / 2 + 1)) / 2.;

		char *buffer = nullptr;
		asprintf(&buffer, "count: %d, average: %.09f (%e), sd: %.09f (%e), median: %.09f (%e), missing: %u/%u\n", n, avg, avg, sd, sd, med, med, n_missing_1, n_missing_2);
		emit(log_file, buffer);
		free(buffer);
	}

	return 0;
}
