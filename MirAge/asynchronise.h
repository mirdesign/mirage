#pragma once
#ifndef ASYNC_H
#define ASYNC_H

using namespace std;


#include <future>
#include <vector>

#ifdef __CUDA__
#include <cuda_runtime.h>
#endif

class Asynchronise
{
public:

	static int __FUTURES_MAX_SIZE;
	static int __FUTURES_MAX_SIZE_LOG_TO_FILE;
	static int __FUTURES_MAX_SIZE_CreateResultsScores;
	
	template<typename T>
	static void wait_for_futures_on_full_vector(vector<future<T>> *futures, int MaxFutures);

	template<typename T>
	static void wait_for_futures(vector<future<T>> *futures);
	template<typename T>
	static void wait_for_futures(vector<future<T*>> *futures);
	template<typename T>
	static void start_futures(vector<future<T>> *futures);

	template<typename T>
	static void wait_for_futures_perRange(vector<future<T>> *futures, size_t futures_Max_Start);


	Asynchronise();
	~Asynchronise();
};


template<typename T>
void Asynchronise::wait_for_futures_on_full_vector(vector<future<T>> *futures, int MaxFutures)
{
	if (futures->size() < MaxFutures)
		return;
	wait_for_futures(futures);
}

template<typename T>
void Asynchronise::wait_for_futures(vector<future<T>> *futures)
{
	size_t n = futures->size();
	if (n > 0)
	{
		future<T>* p = &(*futures)[0];
		for (size_t i = 0; i < n; i++)
		{
			p->wait();
			p++;
		}
	}
	futures->clear();
}

template<typename T>
void Asynchronise::start_futures(vector<future<T>>* futures)
{
	size_t n = futures->size();
	if (n > 0)
	{
		future<T>* p = &(*futures)[0];
		for (size_t i = 0; i < n; i++)
		{
			p->wait_for(chrono::milliseconds(1));
			p++;
		}
	}
}

template<typename T>
void Asynchronise::wait_for_futures_perRange(vector<future<T>> *futures, size_t futures_Max_Start)
{
	//futures_max_Start = multithreading.maxCPUThreads


	// Now wait until they are all ready
	int futures_offset = 0;
	int futures_current_until = futures_Max_Start;
	size_t futures_size = futures->size();
	bool futures_all_ready(false);
	while (!futures_all_ready)
		//for (auto &f : futures)
	{
		// Start current range
		for (int i = futures_offset; i < futures_current_until && i < futures_size; i++)
			(*futures)[i].wait_for(std::chrono::milliseconds(1));



		// Wait current range
		for (int i = futures_offset; i < futures_current_until && i < futures_size; i++)
			(*futures)[i].wait();

		futures_offset += futures_Max_Start;
		futures_current_until += futures_Max_Start;

		if (futures_offset > futures_size)
			futures_all_ready = true;


	}
}


#endif