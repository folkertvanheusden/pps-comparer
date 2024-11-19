#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <thread>
#include <vector>
#include <sys/timepps.h>

#include "timespec-math.h"


// apt install pps-tools


struct result_t {
	timespec   ts;
	bool       valid;
	std::mutex lock;
	std::condition_variable cv;
};

std::atomic_bool stop { false };

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

void get_pps(const char *const filename, result_t *const r)
{
	pps_handle_t handle = open_pps(filename);

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

void sigh(int s)
{
	stop = true;
}

int main(int argc, char *argv[])
{
	result_t r1;
	r1.valid = false;
	std::thread th1(get_pps, argv[1], &r1);

	result_t r2;
	r2.valid = false;
	std::thread th2(get_pps, argv[2], &r2);

	signal(SIGINT, sigh);

	double total_difference = 0.;
	double total_sd         = 0.;
	int    n                = 0;
	std::vector<double> median;

	while(!stop) {
		timespec ts1 { };

		{
			std::unique_lock<std::mutex> lk1(r1.lock);
			r1.cv.wait(lk1, [&]{ return r1.valid; });
			ts1      = r1.ts;
			r1.valid = false;
		}

		usleep(750000); // other pulse should be within 750 ms

		timespec ts2 { };

		{
			std::unique_lock<std::mutex> lk2(r2.lock);
			if (r2.valid == false) 
				continue;
			ts2      = r2.ts;
			r2.valid = false;
		}

		if (stop)
			break;

		auto difference = timespec_subtract(ts1, ts2);
		printf("%ld.%09ld %ld.%09ld %ld.%09ld\n", ts1.tv_sec, ts1.tv_nsec, ts2.tv_sec, ts2.tv_nsec, difference.tv_sec, difference.tv_nsec);
		if (argc == 4) {
			FILE *fh = fopen(argv[3], "a+");
			if (fh) {
				fprintf(fh, "%ld.%09ld %ld.%09ld %ld.%09ld\n", ts1.tv_sec, ts1.tv_nsec, ts2.tv_sec, ts2.tv_nsec, difference.tv_sec, difference.tv_nsec);
				fclose(fh);
			}
			else {
				fprintf(stderr, "\"%s\" is in accessible\n", argv[3]);
				break;
			}
		}

		double d_difference = difference.tv_sec + difference.tv_nsec / 1000000000.;
		total_difference   += d_difference;
		total_sd           += d_difference * d_difference;
		n++;

		median.push_back(d_difference);
	}

	th2.join();
	th1.join();

	if (n) {
		double avg = total_difference / n;
		double sd  = sqrt(total_sd / n - avg * avg);

		std::sort(median.begin(), median.end());
		double med = median.at(n / 2);
		if ((n & 1) == 0)
			med = (med + median.at(n / 2 + 1)) / 2.;

		printf("count: %d, average: %.09f (%e), sd: %.09f (%e), median: %.09f (%e)\n", n, avg, avg, sd, sd, med, med);
		FILE *fh = fopen(argv[3], "a+");
		if (fh) {
			fprintf(fh, "count: %d, average: %.09f (%e), sd: %.09f (%e), median: %.09f (%e)\n", n, avg, avg, sd, sd, med, med);
			fclose(fh);
		}
	}

	return 0;
}
