#pragma once

#ifndef HARDWARE_H
#define HARDWARE_H

#include <string>
#include <vector>
#include "MatrixGPU.h"
#include "MultiThreading.h"
#include "Display.h"
#include <chrono>

using namespace std;
using namespace std::chrono;

extern size_t CUDAGetMaxBlockSize(int cudaDeviceID);
class Sequences;
class MatrixGPU;

class Hardware
{

private:
	// When you have a runtime per block which is already quite high,
	// this will have a low effect.But when you have a very short execution time per block,
	// you would like not to stabilize on very small differences in time.This value makes it a bit more robust.
	double substractFromTime = 0.00001;// 0.1; // This is used to iron out some small differences.  

	Display *display;
	MultiThreading *multiThreading;
	typedef size_t SEQ_AMOUNT_t; // Should be identifiable by SEQ_ID_t

public:
	static size_t MaxDeviceQueueSize;
	enum HardwareTypes { CPU = 0, GPU = 1 };
	enum HardwareControl { CUDA = 0, OpenCL = 1, NoGPU = 2 };

	/** Blocksize, number of sequences which are analysed per batch **/
	vector<SEQ_AMOUNT_t> BlockSize_Current;
	vector<string> Type_Configuration;
	int BlockSize_MAX = 65000; // todo-later: take max size of GPU block

										/** Blocksize Autoincrease, used to determine the optimal blocksize during runtime **/
	bool Auto_Detect_Hardware = false;
	bool BlockSize_AutoIncrease_Enabled_Default = false;
	int BlockSize_Default_CPU = 1;
	int BlockSize_Default_GPU = 1;
	int BlockSize_AutoIncrease_IncreasePercentage = 10;
	vector<int> AutoIncrease_BlockSize_LastNoError;
	vector<bool> BlockSize_AutoIncrease_Stabilized;
	vector<bool> BlockSize_AutoIncrease_Enabled;
	vector<float> BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery;
	int BlockSize_AutoIncrease_MinimalPercentage = 10; // Bottom, after this we call it stabilized
	int BlockSize_AutoIncrease_DecreaseAfterStabilization = 10;
	bool BlockSize_AutoIncrease_AllDevicesStabilized = false;
	bool BlockSize_AutoIncrease_IsActive = false;
	vector<string> TypeName;
	vector<Hardware::HardwareTypes> Type;
	Hardware::HardwareControl Control;
	vector<string> DisplayName;
	vector<size_t> AvailableMemory;
	vector<bool> DeviceEnabled;
	vector<bool> DeviceFailedToInitialiseOpenCL;
	vector<int> DeviceQueueSize;



#ifdef __linux__
	vector< chrono::system_clock::time_point> Device_begin_time;
#else
	vector<std::chrono::time_point<std::chrono::steady_clock>> Device_begin_time;
#endif

	vector<size_t> Device_QuerySize;

	vector<MatrixGPU> GPUDevices;
	vector<MatrixGPU> Devices;
	vector<int> DeviceID;
	int AvailableHardware = 0;
	bool GPUAcceleration_Enabled = true; // True = Load and use configured GPU hardware
	bool CPU_Enabled = false;
	bool CPU_InConfiguration = false;


	bool IsGPUDeviceUsed();

	void ShowExampleConfiguration();

	void CheckCorrectHardwareConfiguration();

	string GetControlDisplayName();


	void DetectAvailableHardware();
	void isAutoIncreaseBlocksizeReady();

	void TryAutoIncreaseBlocksize(int hardwareID, double AnalysisSpentTime, double PrepareTimeSpent, size_t numAnalysedQueries, int maxCPUThreads);
	void HardwareAutoConfigure(int numberOfGPUs);
#ifndef __CUDA__

	int CUDAGetMaxBlockSize(int i);
#endif
	void InitialiseCUDASettings();
	void Initialise(int maxCPUThreads);
	void DeviceIsStabilized(int hardwareID);

	Hardware(Display *dspl, MultiThreading *m);
};

#endif