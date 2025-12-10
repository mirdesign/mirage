#include "MultiThreading.h"
chrono::milliseconds MultiThreading::THREAD_START_FAILED_WAIT_SLEEP = chrono::milliseconds(200);
chrono::milliseconds MultiThreading::THREAD_WAIT_SLEEP = chrono::milliseconds(1);
chrono::microseconds MultiThreading::THREAD_WAIT_FILE_RETRY_SLEEP = chrono::microseconds(1);
chrono::microseconds MultiThreading::THREAD_WAIT_FILE_RETRY_SLEEP_LONG = chrono::microseconds(500);
int32_t MultiThreading::THREAD_MAX_RETRY_COUNT = 50;
int32_t MultiThreading::THREAD_MAX_RETRY_COUNT_LONG = 1000;
mutex MultiThreading::mutHashmapCache;


///
/// Make sure finished threads are being ended correctly
/// and removed from the thread list
///
void MultiThreading::TerminateFinishedThreads(bool CPUThreads)
{
	// Lock mutex
	mutMainMultithreads.lock();
	mutGPUResults.lock();
	vector<double> _threadsFinished;

	// Get threads to local method
	_threadsFinished.swap(threadsFinished);

	mutGPUResults.unlock();
	mutMainMultithreads.unlock();

	if ((CPUThreads && (_threadsFinished.size() == 0)) ||
		(!CPUThreads && (_threadsFinished.size() == 0 || activeThreads.size() == 0)))
	{
		return; // Nothing to do here
	}

	// Terminate Threads
	for (int i = _threadsFinished.size() - 1; i >= 0; i--)
	{
		int threadArrayIndex = _threadsFinished[i];
#ifdef __linux__
		// Not required, since we detach the Unix thread
		//pthread_t t = activeThreads[threadArrayIndex];
#else
		thread* t = activeThreads[threadArrayIndex];
		if (t != 0)
		{
			t->join();
			delete t;
		}
#endif

		// Remove thread from the list
		_threadsFinished.erase(_threadsFinished.begin() + i);
	}
}



MultiThreading::MultiThreading()
{
	runningLogResults = 0;
	runningThreadsGPUPreAnalysis = 0;
}


MultiThreading::~MultiThreading()
{
	if (GPUsActive_NumberSet)
		delete[] GPUsActive;
}


void MultiThreading::SleepTPrepareWorkerThread()
{
	std::this_thread::sleep_for(waitThreadsTPrepareWorker);
}
void MultiThreading::SleepWaitForCPUThreads()
{
	std::this_thread::sleep_for(waitThreadsToEnd);
}
void MultiThreading::SleepGPU()
{
	std::this_thread::sleep_for(GPU_SLEEP);
}


void MultiThreading::SetGPUActive_AllStates(bool active)
{
	if (!GPUsActive_NumberSet)
		throw runtime_error("GPUsActive array not set. Use SetGPUActive_NumberOfGPUs before using this method!");

	mutGPUsActive.lock();
	for (int i = 0; i < GPUsActive_numberOfGPUs; i++)
		*(GPUsActive + i) = active;

	mutGPUsActive.unlock();
}

void MultiThreading::SetGPUActive_NumberOfGPUs(int numberOfGPUs)
{
	GPUsActive_numberOfGPUs = numberOfGPUs;
	GPUsActive_NumberSet = true;
	GPUsActive = new bool[numberOfGPUs]();
	SetGPUActive_AllStates(false);
}


void MultiThreading::SetGPUActive(bool active, int GPUID)
{
	if (!GPUsActive_NumberSet)
		throw runtime_error("GPUsActive array not set. Use SetGPUActive_NumberOfGPUs before using this method!");
	if (!(GPUID < GPUsActive_numberOfGPUs))
		throw runtime_error("GPUsActive array to small. Use SetGPUActive_NumberOfGPUs before using this method!");

	mutGPUsActive.lock();
	*(GPUsActive + GPUID) = active;
	mutGPUsActive.unlock();
}

bool MultiThreading::IsGPUActive(int GPUID)
{
	if (!GPUsActive_NumberSet)
		throw runtime_error("GPUsActive array not set. Use SetGPUActive_NumberOfGPUs before using this method!");
	if (!(GPUID < GPUsActive_numberOfGPUs))
		throw runtime_error("GPUsActive array to small. Use SetGPUActive_NumberOfGPUs before using this method!");
	bool retVal = false;
	mutGPUsActive.lock();
	retVal = *(GPUsActive + GPUID);
	mutGPUsActive.unlock();

	return retVal;
}


bool MultiThreading::AllGPUsActive()
{
	if (!GPUsActive_NumberSet)
		throw runtime_error("GPUsActive array not set. Use SetGPUActive_NumberOfGPUs before using this method!");

	for (int i = 0; i < GPUsActive_numberOfGPUs; i++)
		if (!*(GPUsActive + i))
			return false;

	return true;
}
