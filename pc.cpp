#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
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

int main(int argc, char *argv[])
{
	pps_handle_t handle1 = open_pps(argv[1]);
	pps_handle_t handle2 = open_pps(argv[2]);

	for(;;) {
		pps_info_t infobuf1 { };
		if (time_pps_fetch(handle1, PPS_TSFMT_TSPEC, &infobuf1, nullptr) == -1) {
			fprintf(stderr, "Cannot time_pps_fetch from %s: %s\n", argv[1], strerror(errno));
			exit(1);
		}

		timespec ts { 0, 100000000 };  // 100 ms sanity check
		pps_info_t infobuf2 { };
		if (time_pps_fetch(handle2, PPS_TSFMT_TSPEC, &infobuf2, &ts) == -1) {
			fprintf(stderr, "Cannot time_pps_fetch from %s: %s\n", argv[2], strerror(errno));
			continue;
		}

		printf("Assert timestamp 1: %d.%09d, sequence: %ld\n",
				infobuf1.assert_timestamp.tv_sec,
				infobuf1.assert_timestamp.tv_nsec,
				infobuf1.assert_sequence);

		printf("Assert timestamp 2: %d.%09d, sequence: %ld\n",
				infobuf2.assert_timestamp.tv_sec,
				infobuf2.assert_timestamp.tv_nsec,
				infobuf2.assert_sequence);
	}

	return 0;
}
