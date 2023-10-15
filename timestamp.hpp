#ifndef _timestamp_hpp_
#define _timestamp_hpp_

//高精度计时器 windows版本和C++11版本
//#include<windows.h>
#include<chrono>
using namespace std::chrono;

class TimeStamp
{
public:
	TimeStamp()
	{
		//QueryPerformanceFrequency(&frequency);
		//QueryPerformanceCounter(&startCount);
		update();
	}
	~TimeStamp()
	{

	}
	void update()
	{
		/*QueryPerformanceCounter(&startCount);*/
		begin = high_resolution_clock::now();
	}
	//获取当前秒
	double getSecond()
	{
		return getMircoSecond() * 0.000001;
	}

	//获取毫秒
	double getMilliSecond()
	{
		return this->getMilliSecond() * 0.001;
	}

	//获取微秒
	double getMircoSecond()
	{
		//LARGE_INTEGER endCount;
		//QueryPerformanceCounter(&endCount);

		//double startTimeInMircoSecond = startCount.QuadPart * (1000000.0 / frequency.QuadPart);
		//double endTimeInMircoSecond = endCount.QuadPart * (1000000.0 / frequency.QuadPart);

		//return endTimeInMircoSecond - startTimeInMircoSecond;

		return duration_cast<microseconds>(high_resolution_clock::now() - begin).count();
	}
protected:
	//LARGE_INTEGER frequency;
	//LARGE_INTEGER startCount;
	time_point<high_resolution_clock> begin;
};
#endif // !_timestamp_hpp_


