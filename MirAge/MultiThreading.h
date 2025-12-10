#pragma once
#ifndef MULTITHREADING_H
#define MULTITHREADING_H
using namespace std;

#include <mutex>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <pthread.h>
#include <unistd.h>
#endif
class MultiThreading
{

private:
	
		chrono::milliseconds waitThreadsTPrepareWorker = chrono::milliseconds(100); // Used to pause adding new threads to queue
	chrono::milliseconds waitThreadsToEnd = chrono::milliseconds(10); // Used to pause adding new threads to queue
	chrono::milliseconds GPU_SLEEP = chrono::milliseconds(1); // Used to pause adding new threads to queue
	bool* GPUsActive;
	bool GPUsActive_NumberSet = false;
	mutex mutGPUsActive; // Mutex to safely store data which will be accessed by a multiThreaded method

public:
	int GPUsActive_numberOfGPUs;

	static std::chrono::milliseconds THREAD_START_FAILED_WAIT_SLEEP; // Used to pause adding new threads to queue
	static std::chrono::milliseconds THREAD_WAIT_SLEEP; // Used to pause adding new threads to queue
	static std::chrono::microseconds THREAD_WAIT_FILE_RETRY_SLEEP; // Used to pause adding new threads to queue
	static std::chrono::microseconds THREAD_WAIT_FILE_RETRY_SLEEP_LONG; // Used to pause adding new threads to queue
	
	static int32_t THREAD_MAX_RETRY_COUNT;
	static int32_t THREAD_MAX_RETRY_COUNT_LONG;
	static mutex mutHashmapCache; 

	int maxCPUThreads = 12;
	bool WritingToScreen = false;
	vector<double> threadsFinished; // Used to clean up fnished threads

	void SetGPUActive_NumberOfGPUs(int numberOfGPUs);
	void SetGPUActive(bool active, int GPUID);
	bool IsGPUActive(int GPUID);
	bool AllGPUsActive();
	void SetGPUActive_AllStates(bool active);




	atomic<bool> PauseCPUAnalysis; // Used to pause CPU analysis so preprocessing for GPU can utilize the CPU. This will not stall the GPU and therefore results in higher performance
	int runningThreadsAnalysing = 0; // Current amount of running threads
	int runningThreadsAnalysis = 0; // Current amount of running threads
	atomic_int runningLogResults;
	int runningThreadsBuildHashMap = 0; // Current amount of running threads
	int runningThreadsReadingFileFromMem = 0; // Current amount of running threads
	atomic_int runningThreadsGPUPreAnalysis; // Current amount of running threads

#ifdef __linux__
	vector<pthread_t> activeThreads; // Multi threading
#else
	vector<thread*> activeThreads; // Multi threading
#endif
	mutex mutexReadFileFromMem; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutexGPUPrepareSetNextQID; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutexGPUPrepare; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutHashMapEndResult; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutHashMapPositionsEndResult; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutHashMap; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutMainMultithreads; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutexWriteMatrixResultLog; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutGPUResults; // Mutex to safely store data which will be accessed by a multiThreaded method
	mutex mutexWriteMatrixResultLog_RelativeAbudance; // Mutex to recalculate the relative abudance
	vector<bool> mutexAnalysis; // Mutex to safely clear cache, this will wait until all analysis are done
	vector<mutex> mutexCUDARunning; // Mutex to lock a CUDA run so no multiple runs are executed on 1 device
	vector<mutex> mutHashmapFile;
 
	void TerminateFinishedThreads(bool CPUThreads);



	MultiThreading();
	~MultiThreading();

	void SleepWaitForCPUThreads();
	void SleepGPU();
	void SleepTPrepareWorkerThread();
};

#endif