///
/// This code is currently out of sync with the host code
///
#include "MatrixGPU.h"
#include <iostream>
using namespace std;

#include <string>
#include <fstream>
#include <streambuf>
#include <ctime>
#include <thread>
#include "MatrixResult.h"
#include <iostream>
#include <fstream>
//#include<functional> // For greater
#if defined(__CUDA__)
#include <cuda_profiler_api.h> 
#include <cuda_runtime_api.h>
#endif



MatrixGPU::MatrixGPU(MultiThreading *m, Display *d)
{
	//throw new invalid_argument("This GPU code is currently out of sync with the host code");
	multiThreading = m;
	display = d;
}

#if defined(__CUDA__)
void MatrixGPU::InitialiseCUDA(int platformID, int deviceID)
{
}
#endif

#if defined(__OPENCL__)
void MatrixGPU::InitialiseOpenCL(int platformID, int deviceID)
{

	//get all platforms (drivers)
	vector<cl::Platform> all_platforms;
	cl::Platform::get(&all_platforms);
	if (all_platforms.size() == 0) {
		display->logWarningMessage << "Unable to initialise OpenCL." << endl;
		display->logWarningMessage << "No platforms found. Check OpenCL installation!" << endl;
		display->FlushWarning(true);
	}

	//get default device of the default platform
	vector<cl::Device> all_devices;
	cl::Platform default_platform = all_platforms[platformID];
	default_platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices); // CL_DEVICE_TYPE_ALL
																   //default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices); // CL_DEVICE_TYPE_ALL
	if (all_devices.size() == 0) {
		display->logWarningMessage << "Unable to initialise OpenCL." << endl;
		display->logWarningMessage << "No GPU devices found. Check OpenCL installation!" << endl;
		display->FlushWarning(true);
	}

	//for (int i = 0; i < all_devices.size(); i++)
	//display->DisplayTitleAndValue("Initialize device:", default_platform.getInfo<CL_PLATFORM_NAME>() + " [" + all_devices[i].getInfo<CL_DEVICE_NAME>() + "]");

	cl::Device default_device = all_devices[deviceID];


	this->Name = default_platform.getInfo<CL_PLATFORM_NAME>() + " [" + default_device.getInfo<CL_DEVICE_NAME>() + "]";
	display->DisplayTitleAndValue("Initialize device:", this->Name);


	// Kernel Code 
	//#ifdef __linux__
	//	string OpenCLSourceFilePath =("MatrixGPU.cl");
	//#else
	//	string OpenCLSourceFilePath=("MatrixGPU.cl");
	//#endif
	string OpenCLSourceFilePath = ("MatrixGPU.cl");

	ifstream OpenCLStream(OpenCLSourceFilePath);

	string kernel_code;
	kernel_code.reserve(OpenCLStream.tellg());
	OpenCLStream.seekg(0, ios::beg);
	kernel_code.assign((istreambuf_iterator<char>(OpenCLStream)), istreambuf_iterator<char>());
	OpenCLStream.close();

	cl::Program::Sources sources;
	sources.push_back({ kernel_code.c_str(), kernel_code.length() });

	context = cl::Context(default_device);
	program = cl::Program(context, sources);

	string flags;
	//flags = "-cl-fast-relaxed-math"; //  -Werror   -cl-unsafe-math-optimizations
	flags = "-cl-opt-disable"; //  -Werror   -cl-unsafe-math-optimizations

	cl_int buildResult = program.build({ default_device }, &flags[0]);
	if (buildResult != CL_SUCCESS)
	{
		cl::STRING_CLASS buildLog;
		program.getBuildInfo({ default_device }, CL_PROGRAM_BUILD_LOG, &buildLog);
		display->logWarningMessage << "Unable to initialise OpenCL for current device." << endl;
		display->logWarningMessage << "Device: " << this->Name << endl;
		display->logWarningMessage << "OpenCL source file: " << OpenCLSourceFilePath << endl;
		display->logWarningMessage << "Build result: " << buildResult << endl;
		display->logWarningMessage << "Build log: " << buildLog << endl;
		display->logWarningMessage << "Error: " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device) << endl;
		display->FlushWarning(true);
	}

	//cl::STRING_CLASS buildLog;
	//program.getBuildInfo({ default_device }, CL_PROGRAM_BUILD_LOG, &buildLog);


	queue = cl::CommandQueue(context, default_device); //create queue to which we will push commands for the device.

}
#endif
/// Initialise Hardware. Returns false if initialisation fails.
bool MatrixGPU::Initialise(int platformID, int deviceID)
{

#if defined(__CUDA__)
	InitialiseCUDA(platformID, deviceID);
#elif defined(__OPENCL__)
	InitialiseOpenCL(platformID, deviceID);
#endif

	return true;
}

MatrixGPU::~MatrixGPU()
{
}

#if defined(__CUDA__)
#include <cuda_runtime.h>
vector<GPUDevice> MatrixGPU::GetDevicesCUDA()
{
	vector<GPUDevice> retVal = vector<GPUDevice>();

	int nDevices = 0;

	cudaGetDeviceCount(&nDevices);

	for (int i = 0; i < nDevices; i++) {
		cudaDeviceProp prop;
		cudaGetDeviceProperties(&prop, i);

		GPUDevice gd;
		gd.platformID = 0;
		gd.deviceID = i;
		gd.Name = prop.name;
		gd.GlobalSize = prop.totalGlobalMem;
		retVal.push_back(gd);
	}

	return retVal;
}
#endif

#if defined(__OPENCL__)
vector<GPUDevice> MatrixGPU::GetDevicesOpenCL()
{
	vector<GPUDevice> retVal = vector<GPUDevice>();

	char* platformName;
	char* deviceName;
	size_t platformNameSize;
	size_t deviceNameSize;
	size_t globalSizeSize;
	cl_uint platformCount;
	cl_platform_id* platforms;
	cl_uint deviceCount;
	cl_device_id* devices;
	cl_ulong* globalSize;

	// get all platforms
	clGetPlatformIDs(0, NULL, &platformCount);
	platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * platformCount);
	clGetPlatformIDs(platformCount, platforms, NULL);

	for (int i = 0; i < platformCount; i++) {
		// get all devices
		clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &deviceCount);
		devices = (cl_device_id*)malloc(sizeof(cl_device_id) * deviceCount);
		clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, deviceCount, devices, NULL);


		clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 0, NULL, &platformNameSize);
		platformName = (char*)malloc(platformNameSize);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, platformNameSize, platformName, NULL);

		// for each device print critical attributes
		for (int j = 0; j < deviceCount; j++) {


			clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &deviceNameSize);
			deviceName = (char*)malloc(deviceNameSize);
			clGetDeviceInfo(devices[j], CL_DEVICE_NAME, deviceNameSize, deviceName, NULL);

			clGetDeviceInfo(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, 0, NULL, &globalSizeSize);
			globalSize = (cl_ulong*)malloc(globalSizeSize);
			clGetDeviceInfo(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, globalSizeSize, globalSize, NULL);

			GPUDevice gd;

			memcpy(&gd.GlobalSize, globalSize, globalSizeSize);
			free(globalSize);



			//GetUniqueHardwareID(devices[j]);


			//gd.GlobalSize = *globalSize;
			gd.platformID = i;
			gd.deviceID = j;
			gd.Name.assign(platformName, sizeof(char) * platformNameSize - 1); // -1 to remove the \0
			gd.Name += " [";
			gd.Name.append(deviceName, sizeof(char)* deviceNameSize - 1);// -1 to remove the \0
			gd.Name += "]";

			retVal.push_back(gd);
			free(deviceName);
		}
		free(devices);
		free(platformName);

	}

	free(platforms);
	//delete &platformCount;

	return retVal;
}
#endif
vector<GPUDevice> MatrixGPU::GetDevices()
{

#if defined(__CUDA__)
	return GetDevicesCUDA();
#elif defined(__OPENCL__)
	return GetDevicesOpenCL();
#else
	return vector<GPUDevice>();
#endif
}
//
//size_t MatrixGPU::CUDAGetMaxBlockSize(int cudaDeviceID)
//{
//	return 1;
//}



//_GOOD_WORKING
//#define show_debug
//#define show_timings
#include <unordered_set>
#include <unordered_map>

vector<MatrixResult> MatrixGPU::OpenCLGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<vector<SEQ_ID_t>> queryReferenceCombinations, vector<vector<vector<SEQ_POS_t>>> queryReferenceCombinationsRefPositions, vector<vector<vector<SEQ_POS_t>>> queryReferenceCombinationsQueryPositions, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis)//, char** charsAgainst_tmp, int threadSize)
{
#ifndef __OPENCL__
	display->logWarningMessage << "Program tried to execute OpenCL while not compiled with OpenCL tags." << endl;
	display->logWarningMessage << "Please use '-D__OPENCL' during the compile of this program." << endl;
	display->FlushWarning(true);
	return vector<MatrixResult>();
#else

	/*
cout << "                                                        opencl  START " << cudaDeviceID << endl;
cout << " queryReferenceCombinationRefPositions " << queryReferenceCombinationsRefPositions.size() << endl;
cout << " queryReferenceCombinationQueryPositions " <<  queryReferenceCombinationsQueryPositions.size() << endl;
cout << " queryReferenceCombination " << queryReferenceCombinations.size() << endl;
cout << " QuerySequences " << seqQuery.size() << endl;
*/
//cout << endl;
//for (int j = 0; j <  queryReferenceCombinationsRefPositions.size(); j++)
//	for (int q = 0; q <  queryReferenceCombinationsRefPositions[j].size(); q++)
//		for (int f = 0; f <  queryReferenceCombinationsRefPositions[j][q].size(); f++)
//			cout << queryReferenceCombinationsRefPositions[j][q][f] << endl;
//cout << endl;
//for (int j = 0; j <  queryReferenceCombinationsQueryPositions.size(); j++)
//	for (int q = 0; q <  queryReferenceCombinationsQueryPositions[j].size(); q++)
//		for (int f = 0; f <  queryReferenceCombinationsQueryPositions[j][q].size(); f++)
//			cout << queryReferenceCombinationsQueryPositions[j][q][f] << endl;
//cout << endl;
//for (int j = 0; j <  queryReferenceCombinations.size(); j++)
//	for (int q = 0; q <  queryReferenceCombinations[j].size(); q++)
//		cout << queryReferenceCombinations[j][q] << endl;
//cout << endl;

//for (int j = 0; j <  seqQuery.size(); j++)
//	cout << seqQuery[j].size() << endl;
//cout << endl;




#ifdef show_timings
	float mDebugTime;
	clock_t begin_time_against = clock();
	clock_t begin_time_total = clock();
#endif		
#ifdef show_debug
	cout << "OpenCLGetHighestScore.";
	cout.flush();
#endif		

	vector <SEQ_ID_t> sortedUsedOriginalReferenceIDs = vector<SEQ_ID_t>(); // To get unique reference ID's to work with
	unordered_set <SEQ_ID_t> unRefIDs = unordered_set<SEQ_ID_t>(); // To get unique reference ID's to work with
	unordered_map<SEQ_ID_t, SEQ_ID_t> referenceTranslateTable = unordered_map<SEQ_ID_t, SEQ_ID_t>(); // List of newID's related to the original reference ID's (newID, originalID)
	vector<vector<SEQ_ID_t>> relativeQueryReferenceCombinations = vector<vector<SEQ_ID_t>>();
	vector<vector<vector <SEQ_POS_t>>> relativeQueryReferenceCombinationsRefPositions = vector<vector<vector<SEQ_POS_t>>>();
	vector<vector<vector <SEQ_POS_t>>> relativeQueryReferenceCombinationsQueryPositions = vector<vector<vector<SEQ_POS_t>>>();


	relativeQueryReferenceCombinations.reserve(queryReferenceCombinations.size());
	relativeQueryReferenceCombinationsRefPositions.reserve(queryReferenceCombinations.size());
	relativeQueryReferenceCombinationsQueryPositions.reserve(queryReferenceCombinations.size());

	// Get all selected reference ID's
	for (SEQ_ID_t qID = 0; qID < queryReferenceCombinations.size(); qID++)
	{
		/*	cout << " @" << qID  << " " << string(seqQuery[qID].begin(), seqQuery[qID].end()) << endl;
		cout << queryReferenceCombinations[qID].size() << endl;
		for (int j = 0; j < queryReferenceCombinations[qID].size(); j++)
		{
		cout << queryReferenceCombinations[qID][j] << endl;
		}
		*/
		vector<SEQ_ID_t> newReferenceIDs = vector<SEQ_ID_t>(); // To store the new reference ID's
		vector<vector<SEQ_POS_t>> referenceHashRefPositions = vector<vector<SEQ_POS_t>>(); // To store the new reference ID's
		vector<vector<SEQ_POS_t>> referenceHashQueryPositions = vector<vector<SEQ_POS_t>>(); // To store the new reference ID's

		newReferenceIDs.reserve(queryReferenceCombinations[qID].size());
		referenceHashRefPositions.reserve(queryReferenceCombinations[qID].size());
		referenceHashQueryPositions.reserve(queryReferenceCombinations[qID].size());

		for (SEQ_ID_t rID = 0; rID < queryReferenceCombinations[qID].size(); rID++)
		{
			SEQ_ID_t refID = queryReferenceCombinations[qID][rID];

			if (unRefIDs.insert(refID).second)
			{
				/*	for (int a = 0; a < queryReferenceCombinationsRefPositions[qID][rID].size();a++)
				cout << sortedUsedOriginalReferenceIDs.size() << " : " << queryReferenceCombinationsRefPositions[qID][rID][a] << endl;*/


				referenceTranslateTable.insert({ refID, sortedUsedOriginalReferenceIDs.size() });
				newReferenceIDs.push_back(sortedUsedOriginalReferenceIDs.size()); // Store new ID
				sortedUsedOriginalReferenceIDs.push_back(refID); // Store new ID
			}
			else
			{
				/*for (int a = 0; a < queryReferenceCombinationsRefPositions[qID][rID].size();a++)
				cout << referenceTranslateTable.find(refID)->second << " : " << queryReferenceCombinationsRefPositions[qID][rID][a] << endl;
				*/

				// We already found this original reference ID
				// So look it up and get the new relative reference ID 
				newReferenceIDs.push_back(referenceTranslateTable.find(refID)->second); // Store new ID with the already existend ID
			}

			vector<SEQ_POS_t> referenceNew;
			vector<SEQ_POS_t> queryNew;

			referenceNew.reserve(queryReferenceCombinationsRefPositions[qID][rID].size());
			queryNew.reserve(queryReferenceCombinationsQueryPositions[qID][rID].size());

			for (SEQ_ID_t a = 0; a < queryReferenceCombinationsRefPositions[qID][rID].size(); a++)
				referenceNew.push_back((SEQ_POS_t)queryReferenceCombinationsRefPositions[qID][rID][a]);
			for (SEQ_ID_t a = 0; a < queryReferenceCombinationsQueryPositions[qID][rID].size(); a++)
				queryNew.push_back((SEQ_POS_t)queryReferenceCombinationsQueryPositions[qID][rID][a]);


			referenceHashRefPositions.push_back(referenceNew); // Remember the hash positions
			referenceHashQueryPositions.push_back(queryNew); // Remember the hash positions
		}
		relativeQueryReferenceCombinations.push_back(move(newReferenceIDs));
		relativeQueryReferenceCombinationsRefPositions.push_back(move(referenceHashRefPositions));
		relativeQueryReferenceCombinationsQueryPositions.push_back(move(referenceHashQueryPositions));
	}

	//cout << "unRefIDs: " << unRefIDs.size() << endl;


	// Copy required refen
	vector<vector<char>> seqReferenceSelected;
	seqReferenceSelected.reserve(sortedUsedOriginalReferenceIDs.size());
	for (SEQ_ID_t rID = 0; rID < sortedUsedOriginalReferenceIDs.size(); rID++)
	{
		//cout << sortedUsedOriginalReferenceIDs[rID] << endl;
		if (Hashmap::Use_Experimental_LazyLoading)
			seqReferenceSelected.push_back(Sequences::GetLazyContentWithCache(&sequences->referenceSequences, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceHeadersHashStreamPos,sortedUsedOriginalReferenceIDs[rID]));
		else
			seqReferenceSelected.push_back(sequences->referenceSequences[sortedUsedOriginalReferenceIDs[rID]]);
	}

#ifdef show_timings
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("pre compile reference (%f sec) \n", mDebugTime / 1000);
	begin_time_against = clock();
#endif

#ifdef show_debug

	cout << "seqReferenceSelected: " << seqReferenceSelected.size() << endl;
	cout << "Start prepare data on cuda device " << cudaDeviceID << endl;
	cout.flush();
#endif
	vector<MatrixResult> retVal;
	try
	{

		///
		/// Translate Sequence to 2 pointer lists
		/// 
		// Build input seqs length list

#ifdef __linux__
		//cudaProfilerStart();
#endif
		SEQ_ID_t NumOfSequencesReferences = 0;
		SEQ_POS_t  NumOfSequencesQuery = seqQuery.size();
		SEQ_POS_t  seqLenAgainst = 0;

		//NumOfSequencesReferences = seqReference.size();
		NumOfSequencesReferences = unRefIDs.size();




		// Against
		SEQ_POS_t* seqLengthsAgainst = new SEQ_POS_t[NumOfSequencesReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t)*NumOfSequencesReferences);
		SEQ_POS_t* SeqStartingPointsAgainst = new SEQ_POS_t[NumOfSequencesReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t)*NumOfSequencesReferences);

		// Against Seqs length
		for (SEQ_ID_t i = 0; i < NumOfSequencesReferences; i++) {
			// SequenceLengths
			SEQ_POS_t lenAgainst = seqReferenceSelected[i].size();

			if (seqReferenceSelected[i][lenAgainst - 1] == '\0')
				lenAgainst--;// remove the \0

			*(seqLengthsAgainst + i) = lenAgainst;

			// Starting points
			*(SeqStartingPointsAgainst + i) = seqLenAgainst;
			seqLenAgainst += lenAgainst;

		}



		// get all reference chars after each other
		char* newReferenceChars = new char[seqLenAgainst + 1];//(char*)cemalloc(__LINE__, (sizeof(char)*seqLenAgainst) + 1);
		newReferenceChars[seqLenAgainst] = '\0';
		SEQ_ID_t refCnt = 0;
		for (SEQ_ID_t i = 0; i < NumOfSequencesReferences; i++)
		{
			SEQ_POS_t size = seqReferenceSelected[i].size();

			if (size != 0)
			{
				if (seqReferenceSelected[i][size - 1] == '\0')
					size--;// remove the \0

				if (size != 0)
				{
					memcpy(newReferenceChars + refCnt, &seqReferenceSelected[i][0], size);
					//for_each(newReferenceChars + refCnt, newReferenceChars + refCnt + size, [](char& in) { in = ::toupper(in); });// done in sequences during loading

					refCnt += size;
				}
			}
		}

		// Clean up
		seqReferenceSelected.clear();
		seqReferenceSelected.shrink_to_fit();

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqReferenceSelected (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		// COMBI //
		SEQ_ID_t longestAgainstPerQuery = 0;
		unsigned int* seqNumAgainstPerQuery = new unsigned int[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, sizeof(size_t)*NumOfSequencesQuery);
		SEQ_POS_t* seqPosAgainstPerQuery = new SEQ_POS_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, sizeof(size_t)*NumOfSequencesQuery);
		SEQ_POS_t seqPosAgainstPerQueryCNT = 0;

		// Build list of reference IDs per query, so the kernel can easily lookup which reference it should read
		SEQ_ID_t totalReferences = 0;
		for (SEQ_ID_t i = 0; i < relativeQueryReferenceCombinations.size(); i++)
			if (!relativeQueryReferenceCombinations.at(i).empty())
				totalReferences += relativeQueryReferenceCombinations.at(i).size();

		SEQ_POS_t totalReferenceHashPositions = 0;
		for (SEQ_ID_t i = 0; i < relativeQueryReferenceCombinationsRefPositions.size(); i++)
			for (SEQ_ID_t j = 0; j < relativeQueryReferenceCombinationsRefPositions[i].size(); j++)
				if (!relativeQueryReferenceCombinationsRefPositions.at(i).at(j).empty())
					totalReferenceHashPositions += relativeQueryReferenceCombinationsRefPositions.at(i).at(j).size();



		SEQ_ID_t* referenceIDsPerQuery = new SEQ_ID_t[totalReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);
		SEQ_POS_t* referenceHashPositionListsPerQuery = new SEQ_POS_t[totalReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);
		SEQ_POS_t* referenceHashPositionListsSizesPerQuery = new SEQ_POS_t[totalReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);
		SEQ_POS_t* referenceHashRefPositionsPerQuery = new SEQ_POS_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		SEQ_POS_t* referenceHashQueryPositionsPerQuery = new SEQ_POS_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		SEQ_ID_t referenceIDsPerQueryCnt = 0;
		SEQ_POS_t referenceHashPositionListsPerQueryCnt = 0;
		SEQ_POS_t referenceHashPositionsPerPerQueryCnt = 0;
		for (SEQ_ID_t i = 0; i < NumOfSequencesQuery; i++)
		{
			*(seqPosAgainstPerQuery + i) = seqPosAgainstPerQueryCNT;

			if (!relativeQueryReferenceCombinations.at(i).empty())
			{
				seqPosAgainstPerQueryCNT += relativeQueryReferenceCombinations.at(i).size();
				*(seqNumAgainstPerQuery + i) = (unsigned int)relativeQueryReferenceCombinations.at(i).size();

				if (relativeQueryReferenceCombinations.at(i).size() > longestAgainstPerQuery)
					longestAgainstPerQuery = relativeQueryReferenceCombinations.at(i).size();

				for (SEQ_ID_t j = 0; j < relativeQueryReferenceCombinations.at(i).size(); j++)
				{
					*(referenceIDsPerQuery + referenceIDsPerQueryCnt) = relativeQueryReferenceCombinations.at(i)[j];
					referenceIDsPerQueryCnt++;
				}

				// Make the hash positions list
				for (SEQ_ID_t j = 0; j < relativeQueryReferenceCombinationsRefPositions.at(i).size(); j++)
				{

					*(referenceHashPositionListsPerQuery + referenceHashPositionListsPerQueryCnt) = referenceHashPositionsPerPerQueryCnt;
					*(referenceHashPositionListsSizesPerQuery + referenceHashPositionListsPerQueryCnt) = relativeQueryReferenceCombinationsRefPositions[i][j].size();
					referenceHashPositionListsPerQueryCnt++;

					for (SEQ_ID_t k = 0; k < relativeQueryReferenceCombinationsRefPositions[i][j].size(); k++)
					{
						*(referenceHashRefPositionsPerQuery + referenceHashPositionsPerPerQueryCnt) = relativeQueryReferenceCombinationsRefPositions[i][j][k];
						//cout << "@" << relativeQueryReferenceCombinationsQueryPositions[i][j][k] << endl;
						*(referenceHashQueryPositionsPerQuery + referenceHashPositionsPerPerQueryCnt) = relativeQueryReferenceCombinationsQueryPositions[i][j][k];
						referenceHashPositionsPerPerQueryCnt++;
					}
				}
			}
			else
				*(seqNumAgainstPerQuery + i) = 0;
		}


#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		// Query
		SEQ_POS_t* seqLengthsQuery = new SEQ_POS_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, (sizeof(size_t) * NumOfSequencesQuery));
		SEQ_POS_t* SeqStartingPointsQuery = new SEQ_POS_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, (sizeof(size_t) * NumOfSequencesQuery));

																		 // Query Seqs length
		SEQ_POS_t seqLenQuery = 0;
		for (SEQ_ID_t i = 0; i < NumOfSequencesQuery; i++) {
			// SequenceLengths
			SEQ_POS_t lenQuery = seqQuery[i]->size();

			if (lenQuery != 0)
			{
				if ((*seqQuery[i])[lenQuery - 1] == '\0')
					lenQuery--;// remove the \0
			}
			*(seqLengthsQuery + i) = lenQuery;

			// Starting points
			*(SeqStartingPointsQuery + i) = seqLenQuery;
			seqLenQuery += lenQuery;

		}

		//cout << " seqLenQuery: " << seqLenQuery << endl;

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery2 (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		char* charsQuery = new char[seqLenQuery + 1];//(char*)cemalloc(__LINE__, (sizeof(char)*seqLenQuery) + 1);
		charsQuery[seqLenQuery] = '\0';
		SEQ_ID_t qryCnt = 0;
		for (SEQ_ID_t i = 0; i < NumOfSequencesQuery; i++)
		{
			SEQ_POS_t size = seqQuery[i]->size();

			if (size != 0)
			{
				if ((*seqQuery[i])[size - 1] == '\0')
					size--;// remove the \0

				if (size != 0)
				{
					memcpy(charsQuery + qryCnt, &(*seqQuery[i])[0], size);
					//for_each(charsQuery + qryCnt, charsQuery + qryCnt + size, [](char& in) { in = ::toupper(in); }); // done in sequences during loading
					qryCnt += size;
				}
			}
		}





#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery3 (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		//int threadSize = __THREADSIZE;//240;//1024;//486;// 672;

		SEQ_ID_t sizeHighest = 0;
		bool cudaOptimisedKernelUsage(false); // cuda= true ,opencl = false

		if (cudaOptimisedKernelUsage)
			sizeHighest = NumOfSequencesQuery * ((NumOfSequencesReferences / threadSize) + 1) * threadSize;
		else
			sizeHighest = longestAgainstPerQuery * NumOfSequencesQuery;
		//sizeHighest = NumOfSequencesReferences * NumOfSequencesQuery;

	///
	/// Write data to GPU memory
	///
	//cout << "Send data to GPU memory.." << endl;
	// create buffers on the device

		size_t totalSizeSend = 0;
		float* startValues = (float*)malloc(sizeHighest*sizeof(float));
		for (SEQ_ID_t i = 0; i < sizeHighest; i++)
			startValues[i] = -5;

		cl::Buffer buffer_HighestScore(context, CL_MEM_READ_WRITE, sizeof(float) * sizeHighest);
		queue.enqueueWriteBuffer(buffer_HighestScore, CL_TRUE, 0, sizeof(float) * sizeHighest, startValues);
		totalSizeSend += sizeof(float) * sizeHighest;

		cl::Buffer buffer_PercentageMatches(context, CL_MEM_READ_WRITE, sizeof(float) * sizeHighest);
		queue.enqueueWriteBuffer(buffer_HighestScore, CL_TRUE, 0, sizeof(float) * sizeHighest, startValues);
		totalSizeSend += sizeof(float) * sizeHighest;

		free(startValues);

		cl::Buffer buffer_charsAgainst(context, CL_MEM_READ_ONLY, sizeof(char) * seqLenAgainst);
		queue.enqueueWriteBuffer(buffer_charsAgainst, CL_TRUE, 0, sizeof(char) * seqLenAgainst, newReferenceChars);
		totalSizeSend += sizeof(char) * seqLenAgainst;


		cl::Buffer buffer_seqLengthsAgainst(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * NumOfSequencesReferences);
		queue.enqueueWriteBuffer(buffer_seqLengthsAgainst, CL_TRUE, 0, sizeof(SEQ_POS_t) * NumOfSequencesReferences, seqLengthsAgainst);
		totalSizeSend += sizeof(SEQ_POS_t) * NumOfSequencesReferences;

		cl::Buffer buffer_seqStartingPointsAgainst(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * NumOfSequencesReferences);
		queue.enqueueWriteBuffer(buffer_seqStartingPointsAgainst, CL_TRUE, 0, sizeof(SEQ_POS_t) * NumOfSequencesReferences, SeqStartingPointsAgainst);
		totalSizeSend += sizeof(SEQ_POS_t) * NumOfSequencesReferences;

		cl::Buffer buffer_SeqStartingPointsQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * NumOfSequencesQuery);
		queue.enqueueWriteBuffer(buffer_SeqStartingPointsQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * NumOfSequencesQuery, SeqStartingPointsQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * NumOfSequencesQuery;

		cl::Buffer buffer_seqLengthsQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * NumOfSequencesQuery);
		queue.enqueueWriteBuffer(buffer_seqLengthsQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * NumOfSequencesQuery, seqLengthsQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * NumOfSequencesQuery;

		cl::Buffer buffer_charsQuery(context, CL_MEM_READ_ONLY, sizeof(char) * seqLenQuery);
		queue.enqueueWriteBuffer(buffer_charsQuery, CL_TRUE, 0, sizeof(char) * seqLenQuery, charsQuery);
		totalSizeSend += sizeof(char) * seqLenQuery;

		cl::Buffer buffer_seqLenQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t));
		queue.enqueueWriteBuffer(buffer_seqLenQuery, CL_TRUE, 0, sizeof(SEQ_POS_t), &seqLenQuery);
		totalSizeSend += sizeof(SEQ_POS_t);

		cl::Buffer buffer_ConsiderThreshold(context, CL_MEM_READ_ONLY, sizeof(unsigned int));
		queue.enqueueWriteBuffer(buffer_ConsiderThreshold, CL_TRUE, 0, sizeof(unsigned int), &ConsiderThreshold);
		totalSizeSend += sizeof(unsigned int);

		cl::Buffer buffer_NumOfSequencesAgainst(context, CL_MEM_READ_ONLY, sizeof(SEQ_ID_t));
		queue.enqueueWriteBuffer(buffer_NumOfSequencesAgainst, CL_TRUE, 0, sizeof(SEQ_ID_t), &NumOfSequencesReferences);
		totalSizeSend += sizeof(SEQ_ID_t);

		cl::Buffer buffer_seqNumAgainstPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_ID_t) * NumOfSequencesQuery);
		queue.enqueueWriteBuffer(buffer_seqNumAgainstPerQuery, CL_TRUE, 0, sizeof(SEQ_ID_t) * NumOfSequencesQuery, seqNumAgainstPerQuery);
		totalSizeSend += sizeof(SEQ_ID_t) * NumOfSequencesQuery;

		cl::Buffer buffer_seqPosAgainstPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * NumOfSequencesQuery);
		queue.enqueueWriteBuffer(buffer_seqPosAgainstPerQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * NumOfSequencesQuery, seqPosAgainstPerQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * NumOfSequencesQuery;

		//ok, nu 1 query en 2 referenties pakken..kijken of de nummering klopt..
		//	dan 1 query en 2000 referenties..klopt ?
		//	dan 2 queries en 2 referenties..klopt ?
		//	dan de rest

		//	dan kijken; in cuda had ik querys -> referenties / threads -> threads...  moet dat hier ook weer? Hoe onderverdelen we dit in opencl?
		//	
		//	gaat de enqueueFillBuffer goed voor de scores? Even checken.


		cl::Buffer buffer_referenceIDsPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_ID_t) * totalReferences);
		queue.enqueueWriteBuffer(buffer_referenceIDsPerQuery, CL_TRUE, 0, sizeof(SEQ_ID_t) * totalReferences, referenceIDsPerQuery);
		totalSizeSend += sizeof(SEQ_ID_t) * totalReferences;

		cl::Buffer buffer_referenceHashPositionListsPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * totalReferences);
		queue.enqueueWriteBuffer(buffer_referenceHashPositionListsPerQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * totalReferences, referenceHashPositionListsPerQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * totalReferences;

		cl::Buffer buffer_referenceHashPositionListsSizesPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * totalReferences);
		queue.enqueueWriteBuffer(buffer_referenceHashPositionListsSizesPerQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * totalReferences, referenceHashPositionListsSizesPerQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * totalReferences;

		cl::Buffer buffer_referenceHashRefPositionsPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * totalReferenceHashPositions);
		queue.enqueueWriteBuffer(buffer_referenceHashRefPositionsPerQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * totalReferenceHashPositions, referenceHashRefPositionsPerQuery);
		totalSizeSend += sizeof(SEQ_POS_t) * totalReferenceHashPositions;



		cl::Buffer buffer_referenceHashQueryPositionsPerQuery(context, CL_MEM_READ_ONLY, sizeof(SEQ_POS_t) * totalReferenceHashPositions);
		queue.enqueueWriteBuffer(buffer_referenceHashQueryPositionsPerQuery, CL_TRUE, 0, sizeof(SEQ_POS_t) * totalReferenceHashPositions, referenceHashQueryPositionsPerQuery);

		totalSizeSend += sizeof(SEQ_POS_t) * totalReferenceHashPositions;

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("Send to gpu mem (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif


		delete[] seqLengthsAgainst;
		delete[] SeqStartingPointsAgainst;
		delete[] seqNumAgainstPerQuery;
		delete[] seqPosAgainstPerQuery;
		delete[] referenceIDsPerQuery;
		delete[] referenceHashPositionListsPerQuery;
		delete[] referenceHashRefPositionsPerQuery;
		delete[] referenceHashQueryPositionsPerQuery;
		delete[] referenceHashPositionListsSizesPerQuery;
		delete[] seqLengthsQuery;
		delete[] SeqStartingPointsQuery;
		delete[] charsQuery;
		delete[] newReferenceChars;

		const clock_t begin_time = clock();


		int argCnt = 0;
		cl_int errKernel;
		cl::Kernel kernel_add = cl::Kernel(program, "GetHighestMatrixScore", &errKernel);

		kernel_add.setArg(argCnt++, buffer_charsQuery);
		kernel_add.setArg(argCnt++, buffer_charsAgainst);
		kernel_add.setArg(argCnt++, buffer_seqLengthsQuery);
		kernel_add.setArg(argCnt++, buffer_seqLengthsAgainst);
		kernel_add.setArg(argCnt++, buffer_seqStartingPointsAgainst);
		kernel_add.setArg(argCnt++, buffer_SeqStartingPointsQuery);
		kernel_add.setArg(argCnt++, buffer_NumOfSequencesAgainst);
		kernel_add.setArg(argCnt++, buffer_HighestScore);
		kernel_add.setArg(argCnt++, buffer_PercentageMatches);
		kernel_add.setArg(argCnt++, buffer_ConsiderThreshold);
		kernel_add.setArg(argCnt++, buffer_seqNumAgainstPerQuery);
		kernel_add.setArg(argCnt++, buffer_seqPosAgainstPerQuery);
		kernel_add.setArg(argCnt++, buffer_referenceIDsPerQuery);
		kernel_add.setArg(argCnt++, buffer_referenceHashPositionListsPerQuery);
		kernel_add.setArg(argCnt++, buffer_referenceHashPositionListsSizesPerQuery);
		kernel_add.setArg(argCnt++, buffer_referenceHashRefPositionsPerQuery);
		kernel_add.setArg(argCnt++, buffer_referenceHashQueryPositionsPerQuery);
		kernel_add.setArg(argCnt++, buffer_seqLenQuery);


		//dim3 dimGrid(NumOfSequencesQuery, (longestAgainstPerQuery / threadSize) + 1);
		//dim3 dimBlock(threadSize, 1);
		multiThreading->SetGPUActive(true, cudaDeviceID);

		queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(NumOfSequencesQuery, longestAgainstPerQuery), cl::NullRange); // LATEST
		//queue.enqueueNDRangeKernel(kernel_add, cl::NullRange, cl::NDRange(NumOfSequencesQuery, NumOfSequencesAgainst), cl::NullRange); // LATEST
		int resultState = queue.finish();

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("Analysed on GPU (%f sec)  with exitstate: %f\n", mDebugTime / 1000, resultState);
		begin_time_against = clock();
#endif


		//Read data from GPU memory

		float* RESULT_SCORES = new float[sizeHighest];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);
		float* RESULT_PERCENTAGE_MATCHES = new float[sizeHighest];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);

		multiThreading->SetGPUActive(false, cudaDeviceID);



		if (resultState == 0)
		{
#ifdef show_timings
			mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
			printf("Reading GPU mem... \n", mDebugTime / 1000);
			begin_time_against = clock();
#endif

			queue.enqueueReadBuffer(buffer_HighestScore, CL_TRUE, 0, sizeof(float) * sizeHighest, RESULT_SCORES);
#ifdef show_timings
			mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
			printf("Reading mem 1 GPU (%f sec) \n", mDebugTime / 1000);
			begin_time_against = clock();
#endif

			queue.enqueueReadBuffer(buffer_PercentageMatches, CL_TRUE, 0, sizeof(float) * sizeHighest, RESULT_PERCENTAGE_MATCHES);

#ifdef show_timings
			mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
			printf("Reading mem 2 GPU (%f sec) \n", mDebugTime / 1000);
			begin_time_against = clock();
#endif

			//int cc = 0;
			for (SEQ_ID_t qID = 0; qID < NumOfSequencesQuery; qID++)
			{

				//vector<size_t> refIDs;
				float HighestScore = -1;
				//float Highest = -1;
				bool AgainstHitFound(false);
				//size_t HighestAgainstID;
				SEQ_ID_t iFrom;

				SEQ_ID_t querySize = seqQuery[qID]->size();





				if (cudaOptimisedKernelUsage)
					iFrom = (qID * ((longestAgainstPerQuery / threadSize) + 1) * threadSize);
				else
					iFrom = qID * longestAgainstPerQuery;

				SEQ_ID_t iTo = iFrom + longestAgainstPerQuery;


				//if (cudaOptimisedKernelUsage)
				//	iFrom = (qID * ((NumOfSequencesAgainst / threadSize) + 1) * threadSize);
				//else
				//	iFrom = qID * NumOfSequencesAgainst;
				//int iTo = iFrom + NumOfSequencesAgainst;





				unordered_map<float, MatrixResult::Results> scores;
				//map<float, MatrixResult::Results, greater<float>> scores;
				//map<float, vector<SEQ_ID_t>, greater<float>> scores;


/*
				ok, het draait nu..echter; GPU load is laag (betere verdeling van threading in opencl?)
					en de resultaten voor all_databases klopt niet.. komt dat door de referenceID verandering?
*/


				iTo = iFrom + (relativeQueryReferenceCombinations.at(qID).size());
				for (SEQ_ID_t i = iFrom; i < iTo; i++) // changed <  to <=
				{

					float score = -3;
					try {

						score = (std::isnan(RESULT_SCORES[i]) ? -3 : RESULT_SCORES[i]);//((std::isnan(Crc[i]) || C[i] > Crc[i]) ? C[i] : Crc[i]);
																							//float score = C[i];//((std::isnan(Crc[i]) || C[i] > Crc[i]) ? C[i] : Crc[i]);
					   //cout << qID << ";" << i << ";" << score << ";" << relativeQueryReferenceCombinations[qID][i - iFrom] << ";" << relativeQueryReferenceCombinations[qID].size() << endl;


					}

					catch (exception e)
					{
						cout << "i: " << i << endl;
						cout << "iFrom: " << iFrom << endl;
						cout << "iTo: " << iTo << endl;
						cout << "NumOfSequencesQuery: " << NumOfSequencesQuery << endl;
						cout << "(relativeQueryReferenceCombinations.at(qID).size()): " << (relativeQueryReferenceCombinations.at(qID).size()) << endl;

						cout << "RESULT_SCORES[i]: " << RESULT_SCORES[i] << endl;
					}

					if (score > -1)
					{
						float percentageMatch = 0;
						try {

							percentageMatch = RESULT_PERCENTAGE_MATCHES[i];





						}

						catch (exception e)
						{
							cout << "i: " << i << endl;
							cout << "iFrom: " << iFrom << endl;
							cout << "iTo: " << iTo << endl;
							cout << "NumOfSequencesQuery: " << NumOfSequencesQuery << endl;

							cout << "(relativeQueryReferenceCombinations.at(qID).size()): " << (relativeQueryReferenceCombinations.at(qID).size()) << endl;
							cout << "RESULT_SCORES[i]: " << RESULT_SCORES[i] << endl;
						}



						SEQ_ID_t referenceID = i - iFrom;//(cudaOptimisedKernelUsage ? i - iFrom : i);
						referenceID = sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinations.at(qID)[referenceID]];
						SEQ_POS_t referenceSize;
						if (Hashmap::Use_Experimental_LazyLoading)
							referenceSize = Sequences::GetSizeOfItem(&sequences->referenceSequences, &sequences->referenceSequences_Lazy, referenceID);
						else
							referenceSize = sequences->referenceSequences[referenceID].size();

						float relativeScore = MatrixResult::GetRelativeScore(querySize, referenceSize, score);

						//cout << "[" << i << "-s " << score << " -rs " << relativeScore << " -qs " << querySize << " -rs " << referenceSize << "]" << endl;

						auto scoreInsert = scores.insert({ relativeScore, MatrixResult::Results{ vector<SEQ_ID_t>({ (SEQ_ID_t)referenceID }), vector<float>({ percentageMatch }) } }); // Try to insert, if it fails; we know it's already in memory and get the location as return value
						if (!scoreInsert.second)
						{
							scoreInsert.first->second.referenceIDs.push_back((SEQ_ID_t)referenceID);
							scoreInsert.first->second.percentageMatch.push_back(percentageMatch);
						}


						//auto scoreInsert = scores.insert({ relativeScore,  vector<SEQ_ID_t>({ (SEQ_ID_t) referenceID }) }); // Try to insert, if it fails; we know it's already in memory and get the location as return value
						//if (!scoreInsert.second)
						//	scoreInsert.first->second.push_back((SEQ_ID_t)referenceID);

						AgainstHitFound = true;

						if (relativeScore > HighestScore)
							HighestScore = relativeScore;


						//if (score > Highest)
						//{
						//	refIDs.clear();
						//	refIDs.shrink_to_fit();
						//	Highest = score;
						//	HighestAgainstID = sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinations.at(qID)[referenceID]];
						//	refIDs.push_back(sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinations.at(qID)[referenceID]]);
						//	AgainstHitFound = true;

						//}
						//else if (score == Highest)
						//	refIDs.push_back(sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinations.at(qID)[referenceID]]);
						//}
					}

				}

				if (AgainstHitFound) {
					MatrixResult mr(qID, HighestScore, scores);//, &(*seqQuery)[qID], seqReference);

															   //MatrixResult mr(qID, HighestScore, scores, &seqQuery[qID], &seqReference);
															   //MatrixResult mr(scores, &seqQuery[qID], &seqReference[HighestAgainstID]);
															   //mr.QueryID = qID;
															   //mr.ReferenceIDs.insert(mr.ReferenceIDs.begin(), refIDs.begin(), refIDs.end());

					retVal.push_back(mr);
				}
			}
		}

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("Processing readed GPU memory (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

		// clean up
		sortedUsedOriginalReferenceIDs.clear();
		//unRefIDs.clear();
		referenceTranslateTable.clear();
		relativeQueryReferenceCombinations.clear();
		relativeQueryReferenceCombinationsRefPositions.clear();
		relativeQueryReferenceCombinationsQueryPositions.clear();
		sortedUsedOriginalReferenceIDs.shrink_to_fit();
		relativeQueryReferenceCombinations.shrink_to_fit();
		relativeQueryReferenceCombinationsRefPositions.shrink_to_fit();
		relativeQueryReferenceCombinationsQueryPositions.shrink_to_fit();

		//free(C);
		delete[] RESULT_SCORES;
		delete[] RESULT_PERCENTAGE_MATCHES;

		//cudaFree(charsQuery_Cuda); //char* seqQuerys
		//cudaFree(charsAgainst_Cuda);  //char* seqAgainst
		//cudaFree(seqLengthsQuery_Cuda); //int* seqLengthQuery
		//cudaFree(seqLengthsAgainst_Cuda); //int* seqLengthsAgainst
		//cudaFree(SeqStartingPointsAgainst_Cuda); //int* seqStartingPointsAgainst
		//cudaFree(SeqStartingPointsQuery_Cuda); //int* seqStartingPointsQuerys
		//cudaFree(NumOfSequencesAgainst_Cuda);  //int* numAgainst
		//cudaFree(Scores_Cuda); //float* Scores
		//cudaFree(seqLenQuery_Cuda);
		//cudaFree(ConsiderThreshold_Cuda);
		////cudaFree(rowHitSizePerBlock_Cuda);
		////cudaFree(queryReferenceCombinationsFlat_Cuda);
		//cudaFree(seqNumAgainstPerQuery_Cuda);
		//cudaFree(seqPosAgainstPerQuery_Cuda);
		//cudaFree(referenceIDsPerQuery_Cuda);
		//cudaFree(referenceHashPositionListsPerQuery_Cuda);
		//cudaFree(referenceHashPositionListsSizesPerQuery_Cuda);
		//cudaFree(referenceHashRefPositionsPerQuery_Cuda);
		//cudaFree(referenceHashQueryPositionsPerQuery_Cuda);
		//cudaFree(rowHit_Cuda);

		//cout << cc << endl;
		//
		//		cudaFree(charsQuery_Cuda);
		//		cudaFree(charsAgainst_Cuda);
		//		cudaFree(seqLengthsQuery_Cuda);
		//		cudaFree(seqLengthsAgainst_Cuda);
		//		cudaFree(SeqStartingPointsAgainst_Cuda);
		//		cudaFree(SeqStartingPointsQuery_Cuda);
		//		cudaFree(NumOfSequencesAgainst_Cuda);
		//		cudaFree(Scores_Cuda);
		//		cudaFree(seqLenQuery_Cuda);
		//		cudaFree(ConsiderThreshold_Cuda);
		//		cudaFree(rowHit_Cuda);
		//		cudaFree(rowHitSizePerBlock_Cuda);
		//		//cudaFree(queryReferenceCombinationsFlat_Cuda);
		//
		//
		//
		//		cout << "cude freeed";
		//		cout.flush();
		//#ifdef __linux__
		//		//cudaDeviceReset();
		//#endif
		//		delete charsAgainst_tmp;
		//		delete seqLengthsAgainst;
		//		delete SeqStartingPointsAgainst;
		//		delete seqLengthsQuery;
		//		delete SeqStartingPointsQuery;
		//		delete charsQuery;
		//		delete[] C;



		/*
		cout << "cude freeed";
		cout.flush();*/
#ifdef __CUDA__
		//cudaDeviceReset();
		//cudaThreadExit();
#endif
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("read results (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_total) / (CLOCKS_PER_SEC / 1000));
		printf("Total GPU took (%f sec) \n", mDebugTime / 1000);
		begin_time_total = clock();
#endif


	}
	catch (exception &ex)
	{
#ifdef show_debug

		display->logWarningMessage << "Executing kernel using CUDA FAILED!" << endl;
		display->logWarningMessage << "Error: " << ex.what() << endl;
		display->FlushWarning();
#endif
		throw ex;
	}
	//cout << "                                                        cudaDeviceID  END " << cudaDeviceID << endl;


	return retVal;
#endif
}



