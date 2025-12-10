#pragma once
#ifndef GPUDEVICE_H
#define GPUDEVICE_H
#include <string>
using namespace std;
class GPUDevice
{
public:
	GPUDevice();
	~GPUDevice();

public:
	int platformID;
	int deviceID;
	string Name;
	unsigned long GlobalSize;
};

#endif