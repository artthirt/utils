#include "common.h"

#include <fstream>
#include <thread>
#include <chrono>

void _msleep(long ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int64_t get_curtime_usec()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

int64_t get_curtime_msec()
{
	int64_t msec = get_curtime_usec();
	return msec / 1000;
}

void write_file(const std::string fn, const bytearray& data)
{
	using namespace std;

	fstream f;
	f.open(fn, ios_base::out | ios_base::binary);
	if(!f.is_open())
		return;

	f.write(&data[0], data.size());
	f.close();
}
