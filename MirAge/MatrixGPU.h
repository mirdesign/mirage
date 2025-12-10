#pragma once
#ifndef MATRIXGPU_H
#define MATRIXGPU_H

#include <vector>
#include "MultiThreading.h"
#include "MatrixResult.h"
#include "GPUDevice.h"
#include "Display.h"
#include <algorithm>

#if defined(__OPENCL__)
//#define CL_USE_DEPRECATED_OPENCL_2_0_APIS 
//#define CL_DEVICE_PCI_BUS_ID_NV  0x4008
//#define CL_DEVICE_PCI_SLOT_ID_NV 0x4009
#pragma comment(lib, "OpenCL.lib")
#include <CL/opencl.h>
#include <CL/cl_ext.h>
#include "cl.hpp"
#endif

using namespace std;
class MatrixResult;
class Sequences;
class MatrixGPU
{
private:
	bool AgainstTranslated = false;
	size_t NumOfSequencesAgainst = 0;
	size_t seqLenAgainst = 0;
	size_t* seqLengthsAgainst;
	size_t* SeqStartingPointsAgainst;
	char* charsAgainst;
	size_t* intAgainst;
	MultiThreading *multiThreading;
	Display *display;

#if defined(__CUDA__)
	void InitialiseCUDA(int platformID, int deviceID);
	static vector<GPUDevice> GetDevicesCUDA();
#endif
#if defined(__OPENCL__)
	cl::Program program;
	cl::CommandQueue queue;
	cl::Context context;
	void InitialiseOpenCL(int platformID, int deviceID);
	static vector<GPUDevice> GetDevicesOpenCL();
#endif
public:


	string Name;
	MatrixGPU(MultiThreading *m, Display *d);
	bool Initialise(int platformID, int deviceID);

	~MatrixGPU();
	static vector<GPUDevice> GetDevices();
	//size_t CUDAGetMaxBlockSize(int cudaDeviceID);
	vector<MatrixResult> OpenCLGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<vector<SEQ_ID_t>> queryReferenceCombinations, vector<vector<vector<SEQ_POS_t>>> queryReferenceCombinationsRefPositions, vector<vector<vector<SEQ_POS_t>>> queryReferenceCombinationsQueryPositions, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis);
};
#endif