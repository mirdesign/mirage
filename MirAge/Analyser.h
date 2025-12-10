#pragma once
#ifndef ANALYSER_H
#define ANALYSER_H
#define _ITERATOR_DEBUG_LEVEL 0

#include "publics.h"



#include "asynchronise.h"

#include "Configuration.h"

#include "MatrixGPU.h"
#include "MatrixCPU.h"
#include "LogManager.h"
#include "Display.h"
#include "Enum.h"
#include "Hashmap.h"
#include "Hardware.h"
#include "SignalHandler.h"
#include "MultiThreading.h"
#include "LogToScreen.h"
#include "Sequences.h"
#include "Configuration.h"
#include <set>
#include <chrono>
#include <memory>
using namespace std::chrono;


#ifdef __linux__


#include <sys/sysinfo.h> 
#else

#include <windows.h>
#endif


class SignalHandler;
class Configuration;
class Sequences;
class Hashmap;
class Hardware;
class MatrixGPU;
class MatrixCPU;



typedef   unordered_map<SEQ_ID_t, int> hashmap_uniques_seq_IDs;


class Analyser
{
public:



	static size_t GetMemMachineFreeB();
	static int GetMemFreePercentage();
	static unsigned long GetMemMachineGB();
	static Enum::Analysis_Modes SpeedMode;
	static bool bUseReverseComplementInQuery;
	
	int32_t ConsiderThreshold = 0;
	int32_t ConsiderThreshold_HashmapLoaded = -1;

	bool Output_HighestScoredResultsOnly = true;

	static void PreloadReferences_HeadTail_Prereserve_Mem(vector<SEQ_ID_t>* R, vector<Enum::LazyData>* lazyData, vector<SEQ_ID_t>* lazyDataSizes, vector<vector<Enum::Position>>* combiPosition, vector<vector<Enum::Position>>* combiPosition_Last, vector<vector<Enum::HeadTailSequence>>* dest, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, size_t* ReferenceSequences_HeadTail_Buffer_Size, mutex* mutex_ReferenceSequences_HeadTail_Buffer_Size);
	static void PreloadReferences_HeadTail(vector<SEQ_ID_t>* R, vector<Enum::LazyData>* lazyData, vector<size_t>* lazyDataSizes, vector<vector<Enum::Position>>* combiPosition, vector<vector<Enum::Position>>* combiPosition_Last, vector<vector<Enum::HeadTailSequence>>* dest, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, vector<hashmap_HeadTailBuffer>* HashmapHeadTailBuffers, vector<vector<char>>* tempReferenceSequences);
	static void PreloadReferences_InQueryList(Sequences* sequences, vector<vector<SEQ_ID_t>>* References, vector<vector<vector<Enum::Position>>>* comboPosition, vector<vector<vector<Enum::Position>>>* combiPosition_Last, vector<vector<vector<Enum::HeadTailSequence>>>* headTailSequences, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, char** ReferenceSequences_Buffer);

	void PreloadReferences_InQueryList(vector<vector<SEQ_ID_t>>* References, vector<HASH_STREAMPOS>* refStreamPos);
	static void PreloadReferences(vector<SEQ_ID_t>* References, Sequences* sequences, vector<HASH_STREAMPOS>* refStreamPos);

	void ExecuteAnalysis_MultiHardware(size_t timeStartProgram);
	//static void LogResultsToScreen(int InputID, MatrixResult::Results* Results, double Score, float relativeMatrixScore, Sequences* sequences, LogToScreen* logResultsToScreen);
	//static void LogResultsToFileManager(int InputID, MatrixResult::Results* Results, double Score, float relativeMatrixScore, Sequences* sequences, LogManager* logManager);
	static void LogResultsToScreen(int InputID, vector<SEQ_ID_t>* Results, double Score, float relativeMatrixScore, Sequences* sequences, LogToScreen* logResultsToScreen);
	static void LogResultsToFileManager(int InputID, vector<SEQ_ID_t> Results, double Score, float relativeMatrixScore, Sequences* sequences, vector<LogManager*> logManager);
	static void LogMatrixResults(vector<MatrixResult>* matrixResults, SEQ_ID_t inputIDOffset, vector<SEQ_ID_t>* queryIDs, MultiThreading* m, Sequences* sequences, Output_Modes* Output_Mode, LogToScreen* logResultsToScreen, LogManager* logManagerHighSensitivity, LogManager* logManagerHihhPrecision, LogManager* logManagerBalanced, LogManager* logManagerRaw, bool Output_HighestScoredResultsOnly);
	static void LogMatrixResult(MatrixResult* matrixResult, SEQ_ID_t inputIDOffset, vector<SEQ_ID_t>* queryIDs, MultiThreading* m, Sequences* sequences, Output_Modes* Output_Mode, LogToScreen* logResultsToScreen, LogManager* logManagerHighSensitivity, LogManager* logManagerHihhPrecision, LogManager* logManagerBalanced, LogManager* logManagerRaw, bool Output_HighestScoredResultsOnly);

	void CleanUp();
	Analyser(LogToScreen* lrts, Sequences* s, MultiThreading* m, LogManager* lmHS, LogManager* lmHP, LogManager* lmB, LogManager* lmR, Display* d, Hardware* h, MultiNodePlatform* mnp, SignalHandler* sh, Configuration *c);
	~Analyser();



	struct Analysis_Arguments {
		vector<vector<SEQ_ID_t>> matchedReferencesPerQuery;
		vector<vector <int>> matchedReferencesPerQuery_FileIDs;

		vector<vector<vector <Enum::Position>>> matchedReferencesPerQuery_CombiPositions_LastPositionInRange;
		vector<vector<vector <Enum::Position>>> matchedReferencesPerQuery_CombiPositions;
		vector<vector<vector <Enum::HeadTailSequence>>> matchedReferencesPerQuery_HeadTailSequences;
		vector<vector<vector <SEQ_POS_t>>> matchedReferencesPerQuery_RefPositions;
		vector<vector<vector <SEQ_POS_t>>> matchedReferencesPerQuery_QueryPositions;


		LogToScreen* logResultsToScreen;
		LogManager* logManagerHighSensitivity;
		LogManager* logManagerHighPrecision;
		LogManager* logManagerBalanced;
		LogManager* logManagerRaw;

		bool Output_HighestScoredResultsOnly;

		Output_Modes *Output_Mode;

		SEQ_AMOUNT_t* TotalQuerySequencesAnalysed;
		vector<SEQ_ID_t> queryIDs;
		vector<vector<char>*> querySequences;
		char* ReferenceSequences_Buffer;
		Sequences* sequences;
		MatrixGPU* GPUEngine;
		MultiThreading* multiThreading;
		Display* display;
		Hardware* hardware;
		HASH_t ConsiderThreshold;
		double threadID;
		SEQ_AMOUNT_t numOfQueriesPigyBacked; // If we analyse queryID 1 and 5, we've bridged 5 queries (although not really analysed all)
		int GPUHardwareID;
		int DeviceID;
		double PrepareSpentTime;
		atomic_int runningThreadsGPUPreAnalysis;
	};




	struct PrepareGPU_Arguments {
		vector< hashmap_streams*>* ReferenceHashMap_Lazy;
		vector<hashmap_MatchedHashStreamPos*>* MatchedHashStreamPos;
		Analysis_Arguments* AnalysisArguments;
		SEQ_AMOUNT_t NumQIDs_PerThread;
		Display* display;
		LogManager* logManager;
		Output_Modes* output_Mode;
		Sequences* sequences;
		Hashmap* hashmap;
		int hardwareID;
		int DeviceID;
		size_t ThreadID;
		size_t ThreadResultID;
		vector<SEQ_ID_t> QueryIDs;
		HASH_t ConsiderThreshold;
		MultiThreading* multiThreading;
		Hardware* hardware;
	};


	struct References_ToLoad
	{

		References_ToLoad(HASH_STREAMPOS a, char* b, SEQ_ID_t c, size_t d)
			: HashStreamPosition(a)
			, dstReference(b)
			, Hashmap_ID(c)
			, dstSize(d)
		{}

		References_ToLoad() {}


		HASH_STREAMPOS HashStreamPosition;
		char* dstReference;
		SEQ_ID_t Hashmap_ID;
		size_t dstSize;
	};

	static void * TPrepareGPUAnalysis(void * args);

private:

#ifdef __linux__
	vector<pthread_t> workerThreads;
#else
	vector<thread*> workerThreads;
#endif


	SEQ_AMOUNT_t TotalQuerySequencesAnalysed = 0;
	SEQ_ID_t Query_Processed_LastID = 0;
#ifdef __linux__
	static void ShowMemUsage();

#endif

	LogToScreen* logResultsToScreen;
	Sequences* sequences;
	MultiThreading* multiThreading;
	MultiNodePlatform* multiNodePlatform;
	LogManager* logManagerHighSensitivity;
	LogManager* logManagerHighPrecision;
	LogManager* logManagerBalanced;
	LogManager* logManagerRaw;
	Display* display;
	SignalHandler* signalHandler;
	static Configuration* configuration;
	Hardware* hardware;


	/********************
	*
	* Structures
	*
	*********************/


	static void * TGetHighestScoreUsingCPU(void * args);
	static void AppendToCombiPosition(vector<vector<Enum::Position>>* combiPosition, size_t* cntAppendToCombiPosition, size_t OrderNumber, size_t iCP, SEQ_ID_t hashmapHitPos, SEQ_POS_t currentCharPosition_InQuery);
	static void * TGetHighestScoreUsingGPU(void * args);


	static void InitialiseOccureBuffer(SEQ_ID_t highestRefID, size_t &occureBufferSize, int * &occure_start, int * &occure, int * &occureMax_Start, int * &occureMax, int * &occure_LastPos);


};

///
/// Declaration for CUDA support
///
#if defined(__CUDA__)
extern vector<MatrixResult> CUDAGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<SEQ_ID_t>* queryReferenceCombinations, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsRefPositions, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsQueryPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions_LastPositionInRange, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis, SEQ_AMOUNT_t queriesToAnalyse_Offset, SEQ_AMOUNT_t queriesToAnalyse_SizePerAnalysis, vector<vector<Enum::HeadTailSequence>>* matchedReferencesPerQuery_HeadTailSequences);
//extern vector<MatrixResult> CUDAGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<SEQ_ID_t>* queryReferenceCombinations, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsRefPositions, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsQueryPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis, SEQ_AMOUNT_t queriesToAnalyse_Offset, SEQ_AMOUNT_t queriesToAnalyse_SizePerAnalysis);
//extern vector<MatrixResult> CUDAGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<SEQ_ID_t>* queryReferenceCombinations, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsRefPositions, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsQueryPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis, SEQ_AMOUNT_t queriesToAnalyse_Offset, SEQ_AMOUNT_t queriesToAnalyse_SizePerAnalysis);
extern void InitialiseCuda(int cudaDeviceID);
extern size_t Get_LOCAL_DATA_rowhit_SIZE();

#endif
#endif