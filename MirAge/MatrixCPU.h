#pragma once
#ifndef MATRIXCPU_H
#define MATRIXCPU_H
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <ctime>
#include "MatrixResult.h"
#include "MultiThreading.h"
#include <atomic> // used for atomic
#include "Display.h"

//#include <functional> // using GREATER for matrixResults
using namespace std;

#include <unordered_map>

#include <mutex>     
#include <thread>

#ifdef __linux__
#include <unistd.h>
#endif

class MatrixResult;


class MatrixCPU
{
private:
	Display *display;
	MultiThreading *multiThreading;

	vector<float> scoresFromThreads;
	vector<float> percentageMatchFromThreads;
public:

	//void CreateHitBuffer(std::vector<SEQ_ID_t> &numRefPerQuery, std::vector<std::vector<char> *> * seqQuery, std::vector<std::vector<SEQ_ID_t>> * queryReferenceCombinations, bool * &HitBuffer, bool * &HitBuffer_current);

	vector<MatrixResult> CPUGetHighestScore(vector<vector<char>*>* seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, int32_t ConsiderThreshold, vector<vector<SEQ_ID_t>>* queryReferenceCombinations, vector<vector<vector<Enum::Position>>>* queryReferenceCombinationsCombiPositions, vector<vector<vector<Enum::Position>>>* matchedReferencesPerQuery_CombiPositions_LastPositionInRange, vector<vector<vector <Enum::HeadTailSequence>>>*  HeadTailSequence, int threadSize);
		//vector<MatrixResult> CPUGetHighestScore(vector<vector<char>*>* seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, int32_t ConsiderThreshold, vector<vector<SEQ_ID_t>>* queryReferenceCombinations, vector<vector<vector<Enum::Position >>>* queryReferenceCombinationsCombiPositions, vector<vector<vector<Enum::Position>>>* matchedReferencesPerQuery_CombiPositions_LastPositionInRange, int threadSize);
	//vector<MatrixResult> CPUGetHighestScore(vector<vector<char>*>* seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, int32_t ConsiderThreshold, vector<vector<SEQ_ID_t>>* queryReferenceCombinations, vector<vector<vector<SEQ_POS_t>>>* queryReferenceCombinationsRefPositions, vector<vector<vector<SEQ_POS_t>>>* queryReferenceCombinationsQueryPositions, int threadSize);
	//vector<MatrixResult> CPUGetHighestScore(vector<vector<char>>* seqQuery, vector<vector<char>>* seqReference, bool debug, int cudaDeviceID, int32_t ConsiderThreshold, vector<vector<SEQ_ID_t>>* queryReferenceCombinations, vector<vector<vector<SEQ_POS_t>>>* queryReferenceCombinationsRefPositions, vector<vector<vector<SEQ_POS_t>>>* queryReferenceCombinationsQueryPositions, int threadSize);
	MatrixCPU(Display *d, MultiThreading *m);
	~MatrixCPU();
	static void CreateResultsScores(vector<SEQ_ID_t>* queryReferenceCombination, vector<vector<Enum::HeadTailSequence>>* matchedReferencesPerQuery_HeadTailSequences, SEQ_ID_t queryID, vector<float>* scoresFromThreads, vector<vector<char>*>* seqQuery, Sequences* sequences, size_t threadIDOffset, MatrixResult* retVal);

};

#endif