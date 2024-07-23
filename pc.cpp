#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <thread>
#include <sys/timepps.h>


// apt istall pps-tools


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

void get_pps(const char *const filename)
{
	pps_handle_t handle = open_pps(filename);

	for(;;) {
		pps_info_t infobuf { };
		if (time_pps_fetch(handle, PPS_TSFMT_TSPEC, &infobuf, nullptr) == -1) {
			fprintf(stderr, "Cannot time_pps_fetch from %s: %s\n", filename, strerror(errno));
			exit(1);
		}

		printf("Assert timestamp from %s: %d.%09d, sequence: %ld\n",
				filename,
				infobuf.assert_timestamp.tv_sec,
				infobuf.assert_timestamp.tv_nsec,
				infobuf.assert_sequence);
	}
}

int main(int argc, char *argv[])
{
	std::thread th1(get_pps, argv[1]);

	std::thread th2(get_pps, argv[2]);

	for(;;)
		sleep(1);

	return 0;
}
