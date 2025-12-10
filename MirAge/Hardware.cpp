#include "Hardware.h"
size_t Hardware::MaxDeviceQueueSize = 1;

#ifndef __CUDA__

int  Hardware::CUDAGetMaxBlockSize(int i) { return BlockSize_MAX; }

#endif

bool Hardware::IsGPUDeviceUsed()
{
	for (size_t i = 0; i < Type_Configuration.size(); i++)
		if (Type_Configuration[i] == "GPU")
			if (DeviceEnabled[i])
				return true;
	return false;
}



///
/// Check if configuration matches the found hardware configuration
///
void Hardware::CheckCorrectHardwareConfiguration()
{
	if (Type_Configuration.size() == 0)
	{
		display->logWarningMessage << "Initialising hardware failed." << endl;
		display->logWarningMessage << "Configuration file contains 0 hardware resources." << endl;
		display->logWarningMessage << "Please configure Hardware with 1 or more hardware devices." << endl;
		display->FlushWarning(true);
	}

	int HardwareEnabled = count(DeviceEnabled.begin(), DeviceEnabled.end(), true);
	if (HardwareEnabled == 0)
	{
		display->logWarningMessage << "Initialising hardware failed." << endl;
		display->logWarningMessage << "Configuration file contains 0 enabled hardware resources." << endl;
		display->logWarningMessage << "Please configure Hardware_Enabled with 1 or more hardware devices." << endl;
		display->FlushWarning(true);
	}
}

string Hardware::GetControlDisplayName()
{
	switch (Control)
	{
	case HardwareControl::CUDA: return "CUDA";
	case HardwareControl::OpenCL: return "OpenCL";
	case HardwareControl::NoGPU: return "Not compiled for GPU use";
	default: return "Unknown hardware control type";
	}
}


///
/// Find the hardware of the current machine
///
void Hardware::DetectAvailableHardware()
{
	bool hardwareInitialised(false);
	bool hardwareAvailable(false);

	display->DisplayHeader("Initialise Hardware");
	Initialise(multiThreading->maxCPUThreads);

	display->DisplayTitleAndValue("Machine memory:", to_string(Analyser::GetMemMachineGB()) + "Gb (" + to_string(Analyser::GetMemFreePercentage()) + "% free)");


	display->DisplayTitleAndValue("Hardware configuration:", (Auto_Detect_Hardware ? "Auto detection" : "Manual (in configuration file)"));
	display->DisplayTitleAndValue("Hardware control:", (GetControlDisplayName()));
	display->DisplayNewLine();

	string found = (Auto_Detect_Hardware ? " found:" : " configured:");

	for (size_t i = 0; i < Type.size(); i++)
	{
		string blocksize = to_string(BlockSize_Current[i]) + (BlockSize_AutoIncrease_Enabled[i] ? " (Auto increase active)" : "");

		if (Type[i] == Hardware::HardwareTypes::CPU)
		{
			hardwareAvailable |= CPU_Enabled;
			display->DisplayTitle("Hardware device" + found, true);
			display->DisplayTitleAndValue("\tType:", "CPU");
			display->DisplayTitleAndValue("\tStatus:", (CPU_Enabled ? "Used" : "Not used"));
			display->DisplayTitleAndValue("\tBlocksize:", blocksize);
			display->DisplayNewLine();
		}
		else  if (Type[i] == Hardware::HardwareTypes::GPU)
		{
			hardwareAvailable |= DeviceEnabled[i];

			display->DisplayTitle("Hardware device" + found, true);
			display->DisplayTitleAndValue("\tType:", "GPU");
			display->DisplayTitleAndValue("\tStatus:", (DeviceEnabled[i] ? "Used" : string("Not used") + (DeviceFailedToInitialiseOpenCL[i] ? " (!! Failed to initialise !!)" : "")));
			display->DisplayTitleAndValue("\tName:", DisplayName[i]);
			display->DisplayTitleAndValue("\tBlocksize:", blocksize);

			stringstream ss;
			ss << round(AvailableMemory[i] / 1024.0 / 1024.0) << " mb";
			display->DisplayTitleAndValue("\tMemory:", ss.str());
			display->DisplayNewLine();
		}
		else
		{
			display->logWarningMessage << "[InitialiseHardware] Unsopported device type found." << endl;
			display->logWarningMessage << "Device type found: " << Type[i] << endl;
			display->FlushWarning(true);
		}

	}


	if (!hardwareAvailable)
	{
		display->logWarningMessage << "[InitialiseHardware] No hardware device is enabled." << endl;
		display->logWarningMessage << "Please change configuration to enable a device or enable 'Hardware_Initialise_Automatically'" << endl;
		display->FlushWarning(true);
	}

}

void Hardware::isAutoIncreaseBlocksizeReady()
{
	for (size_t iStab = 0; iStab < BlockSize_AutoIncrease_Stabilized.size(); iStab++)
	{
		if (!BlockSize_AutoIncrease_Stabilized[iStab])
			break;

		if (iStab == BlockSize_AutoIncrease_Stabilized.size() - 1)
			BlockSize_AutoIncrease_AllDevicesStabilized = true;
	}
}

///
/// Automaticaly increase blocksize if time per query is lower as before
///
void Hardware::TryAutoIncreaseBlocksize(int hardwareID, double AnalysisSpentTime, double PrepareTimeSpent, size_t numAnalysedQueries, int maxCPUThreads)
{
	if (BlockSize_AutoIncrease_Stabilized[hardwareID])
		return;
	if (!BlockSize_AutoIncrease_Enabled[hardwareID])
		return;


	float spentTime = AnalysisSpentTime  +  PrepareTimeSpent;
	float spentTime_PerQuery = spentTime / float(numAnalysedQueries);


	cout << endl;
	cout << endl;
	cout << endl;
	cout << "numAnalysedQueries: " << numAnalysedQueries << endl;
	cout << "spentTime_PerQuery: " << spentTime_PerQuery << endl;
	cout << "spentTime: " << spentTime << endl;
	cout << "PrepareTimeSpent: " << PrepareTimeSpent << endl;
	cout << "BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID]: " << BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID] << endl;
	
	cout << endl;
	cout << endl;
	cout << endl;



	if ((BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID] == -1 ||
		spentTime_PerQuery - this->substractFromTime < BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID]) ||
		BlockSize_AutoIncrease_IncreasePercentage > BlockSize_AutoIncrease_MinimalPercentage)
	{
		int AddToBlockSize = ((float(BlockSize_Current[hardwareID]) / 100.0) * float(BlockSize_AutoIncrease_IncreasePercentage)) + 1.0;
		//int AddToBlockSize = ((float(BlockSize_Current[hardwareID]) / 100.0) * float(BlockSize_AutoIncrease_IncreasePercentage + 100)) + 1.0;

		// If this run was not faster, we are inside because we can stil lower the increase percentage size to its min level
		// We do this so we can try to add more little pieces at the end and perhaps can come to a higher and faster blocksize
		if (BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID] != -1 && !(spentTime_PerQuery - this->substractFromTime < BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID]))
		{
			// First, remove the previously added amount of the blocksize
			// Than we can add a little bit less
			BlockSize_Current[hardwareID] = AutoIncrease_BlockSize_LastNoError[hardwareID];

			BlockSize_AutoIncrease_IncreasePercentage -= BlockSize_AutoIncrease_DecreaseAfterStabilization;

			// Recalculate with new BlockSize_AutoIncrease_IncreasePercentage value
			AddToBlockSize = ((float(BlockSize_Current[hardwareID]) / 100.0) * float(BlockSize_AutoIncrease_IncreasePercentage)) + 1.0;
			//AddToBlockSize = ((float(BlockSize_Current[hardwareID]) / 100.0) * float(BlockSize_AutoIncrease_IncreasePercentage + 100)) + 1.0;
		}
		else
		{
			AutoIncrease_BlockSize_LastNoError[hardwareID] = BlockSize_Current[hardwareID];
			BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery[hardwareID] = spentTime_PerQuery;
		}
		BlockSize_Current[hardwareID] += AddToBlockSize; // +1 so we always decrease at least with 1 (if 10% results in 0 due to rounding features of int)
		if ((Type[hardwareID] == Hardware::HardwareTypes::GPU && BlockSize_Current[hardwareID] > CUDAGetMaxBlockSize(hardwareID - 1)) ||
			(Type[hardwareID] == Hardware::HardwareTypes::CPU && BlockSize_Current[hardwareID] > maxCPUThreads))
		{
			if (Type[hardwareID] == Hardware::HardwareTypes::GPU)
				BlockSize_Current[hardwareID] = CUDAGetMaxBlockSize(hardwareID - 1);
			else
				BlockSize_Current[hardwareID] = maxCPUThreads;

			BlockSize_AutoIncrease_Enabled[hardwareID] = false;
			BlockSize_AutoIncrease_Stabilized[hardwareID] = true;
			isAutoIncreaseBlocksizeReady();
		}

		//cout << "maxCPUThreads " << maxCPUThreads << endl;
		//cout << "c" << BlockSize_Current[hardwareID] << endl;

	}
	else
	{
		// Check if all devices are stabilized on blocksize.
		// This is used to provide an estimation of time duration
		if (!BlockSize_AutoIncrease_Stabilized[hardwareID])
		{
			BlockSize_AutoIncrease_Stabilized[hardwareID] = true;
			isAutoIncreaseBlocksizeReady();
		}
	}
}

void Hardware::DeviceIsStabilized(int hardwareID)
{
	BlockSize_AutoIncrease_Enabled[hardwareID] = false;
	BlockSize_AutoIncrease_Stabilized[hardwareID] = true;
	isAutoIncreaseBlocksizeReady();
}

void Hardware::HardwareAutoConfigure(int numberOfGPUs)
{
	DeviceFailedToInitialiseOpenCL.clear();
	BlockSize_Current.clear();
	Type_Configuration.clear();
	DeviceEnabled.clear();
	BlockSize_AutoIncrease_Enabled.clear();


	Type_Configuration.push_back("CPU");
	BlockSize_Current.push_back(BlockSize_Default_CPU);
	if (BlockSize_Default_CPU > 0)
		DeviceEnabled.push_back(true);
	else
		DeviceEnabled.push_back(false);
	BlockSize_AutoIncrease_Enabled.push_back(BlockSize_AutoIncrease_Enabled_Default);

	for (int i = 0; i < numberOfGPUs; i++)
	{
		Type_Configuration.push_back("GPU");
		BlockSize_Current.push_back(BlockSize_Default_GPU);
		DeviceFailedToInitialiseOpenCL.push_back(true);
		if (BlockSize_Default_GPU > 0)
			DeviceEnabled.push_back(true);
		else
			DeviceEnabled.push_back(false);
		BlockSize_AutoIncrease_Enabled.push_back(BlockSize_AutoIncrease_Enabled_Default);
	}

}


///
/// Initialise CUDA settings and check for known boundaries
///
void Hardware::InitialiseCUDASettings()
{

#ifdef __CUDA__

	for (size_t i = 0; i < BlockSize_Current.size(); i++)
	{
		if (Type[i] == Hardware::HardwareTypes::GPU)
		{
			size_t maxBlockSize = CUDAGetMaxBlockSize(DeviceID[i]);

			if (BlockSize_Current[i] > maxBlockSize)
			{
				display->logWarningMessage << "Maximum blocksize in Settings (" << BlockSize_Current[i] << ") exceeds maximum blocksize for current hardware (" << maxBlockSize << ")" << endl;
				display->logWarningMessage << "Blocksize for current device (" << i << ") will be set to maximum allowed." << endl;
				display->logWarningMessage << "You can change this value in your settings file." << endl;
				display->FlushWarning();
				BlockSize_Current[i] = maxBlockSize;
			}
		}
}
#endif
}

void Hardware::Initialise(int maxCPUThreads)
{
	// Detect available hardware
	vector<GPUDevice> GPUDevicesDetected = vector<GPUDevice>();
	int cGPUDeviceID = 0;
	try
	{
		GPUDevicesDetected = MatrixGPU::GetDevices();
	}
	catch (exception ex)
	{

		display->logWarningMessage << "Initialising hardware failed." << endl;
		display->logWarningMessage << "Unable to retrieve available hardware with OpenCL." << endl;
		display->FlushWarning(true);
	}

	// Initialise hardware based on configuration
	DeviceFailedToInitialiseOpenCL.assign(Type_Configuration.size(), false);

	// Auto configure hardware if needed
	if (!Auto_Detect_Hardware)
		CheckCorrectHardwareConfiguration();
	else
		HardwareAutoConfigure(GPUDevicesDetected.size());

	// Loop configured hardware devices
	for (size_t i = 0; i < Type_Configuration.size(); i++)
	{
		AvailableHardware++;

		BlockSize_AutoIncrease_Previous_TimeSpent_PerQuery.push_back(-1);
		AutoIncrease_BlockSize_LastNoError.push_back(-1);
		BlockSize_AutoIncrease_Stabilized.push_back(!(DeviceEnabled[i] && BlockSize_AutoIncrease_Enabled[i]));
		DeviceQueueSize.push_back(0);
		Device_begin_time.push_back(high_resolution_clock::now());
		Device_QuerySize.push_back(0);




		if (BlockSize_AutoIncrease_Enabled[i])
			BlockSize_AutoIncrease_IsActive = true;

		// Check hardware type
		if (Type_Configuration[i] == "CPU")
		{
			// CPU configuration
			Type.push_back(Hardware::HardwareTypes::CPU);
			CPU_Enabled = DeviceEnabled[i];

			if (CPU_Enabled)
			{
				if (BlockSize_Current[i] > maxCPUThreads)
				{
					display->logWarningMessage << "Configuration file contains wrong value in 'HARDWARE_BlockSize' setting." << endl;
					display->logWarningMessage << "Hardware Type 'CPU' is configured to use more threads (=BlockSize) than allowed by the 'Hardware_maxCPUThreads' setting." << endl;
					display->logWarningMessage << "Please lower the blocksize or increase the maxCPUThreads." << endl;
					display->logWarningMessage << "Be aware! Increasing 'Hardware_maxCPUThreads' may lead to insufficient system resources which prevent this program to run." << endl;
					display->logWarningMessage << "Be aware! This is mainly trial-error to find out the maximum size for your system." << endl;
					display->logWarningMessage << "Blocksize: " << BlockSize_Current[i] << endl;
					display->logWarningMessage << "maxCPUThreads: " << maxCPUThreads << endl;
					display->FlushWarning(true);
				}
			}

			if (CPU_InConfiguration)
			{
				display->logWarningMessage << "Configuration file contains wrong value in 'Hardware_Types' setting." << endl;
				display->logWarningMessage << "Hardware Type 'CPU' used more than once." << Type_Configuration[i] << endl;
				display->logWarningMessage << "Only 1 CPU configuration is allowed." << endl;
				display->logWarningMessage << "Program will stop." << endl;
				display->FlushWarning(true);
			}
			MatrixGPU mg(multiThreading, display);
			Devices.push_back(mg);
			DeviceID.push_back(-1);
			TypeName.push_back("CPU");
			DisplayName.push_back("CPU");
			AvailableMemory.push_back(0);
			CPU_InConfiguration = true;
		}
		else if (Type_Configuration[i] == "GPU")
		{
			// GPU configuration
			if (cGPUDeviceID < GPUDevicesDetected.size())
			{
				Type.push_back(Hardware::HardwareTypes::GPU);

				MatrixGPU mg(multiThreading, display);

				if (!GPUAcceleration_Enabled)
				{
					DeviceFailedToInitialiseOpenCL[i] = false;
					DeviceEnabled[i] = false;
				}
				else
				{
					if (!mg.Initialise(GPUDevicesDetected[cGPUDeviceID].platformID, GPUDevicesDetected[cGPUDeviceID].deviceID))
					{
						DeviceFailedToInitialiseOpenCL[i] = true;
						DeviceEnabled[i] = false;
					}
				}
				GPUDevices.push_back(mg);
				Devices.push_back(mg);
				DeviceID.push_back(cGPUDeviceID);
				TypeName.push_back("GPU");
				DisplayName.push_back(GPUDevicesDetected[cGPUDeviceID].Name);
				AvailableMemory.push_back(GPUDevicesDetected[cGPUDeviceID].GlobalSize);
				cGPUDeviceID++;
			}
			else
			{
				display->logWarningMessage << "Configuration file contains wrong value in 'Hardware_Types' setting." << endl;
				display->logWarningMessage << "Hardware Type configuration value: " << Type_Configuration[i] << endl;
				display->logWarningMessage << "Too many GPU's configured. Not able to load GPU#: " << cGPUDeviceID + 1 << endl;
				display->logWarningMessage << "Number of GPU available: " << GPUDevicesDetected.size() << endl;
				display->FlushWarning(true);
			}
		}
		else
		{
			display->logWarningMessage << "Configuration file contains wrong value in 'Hardware_Types' setting." << endl;
			display->logWarningMessage << "Hardware Type configuration value: " << Type_Configuration[i] << endl;
			display->logWarningMessage << "Expected CPU/GPU" << endl;
			display->FlushWarning(true);
		}
	}

	//multiThreading->SetGPUActive_NumberOfGPUs(Type_Configuration.size());
	multiThreading->SetGPUActive_NumberOfGPUs(GPUDevices.size());
	multiThreading->mutexCUDARunning = vector<mutex>(GPUDevices.size());
	multiThreading->mutexAnalysis = vector<bool>(Devices.size());
	//multiThreading->mutexAnalysis = vector<mutex>(Devices.size());


	for (int i = 0; i < AvailableHardware; i++)
		if (Type[i] == Hardware::HardwareTypes::GPU)
			multiThreading->SetGPUActive(!DeviceEnabled[i], DeviceID[i]);

	isAutoIncreaseBlocksizeReady();

	InitialiseCUDASettings();
}


Hardware::Hardware(Display *d, MultiThreading *m)
{
#ifdef __CUDA__
	Control = HardwareControl::CUDA;
	GPUAcceleration_Enabled = true;
#elif defined(__OPENCL__)
	Control = HardwareControl::OpenCL;
	GPUAcceleration_Enabled = true;
#else
	Control = HardwareControl::NoGPU;
	GPUAcceleration_Enabled = false;
#endif

	display = d;
	multiThreading = m;
}

