#include <stdio.h>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

#include "MatrixResult.h"
#include "MultiThreading.h"
#include <string>
#include <ctime>
#include <future>

// for mutex
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
//#include<math_functions.h>
#include<time.h>
#include<cuda.h>
#include<cuda_runtime.h>

#include <cuda.h>
#include <cuda_profiler_api.h> 
#include <cuda_runtime_api.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <functional> // using GREATER for matrixResults
#include "Display.h"


//#define show_cuda_debug
#define show_debug
#define show_timings

//const size_t SHARED_DATA_QUERY_SIZE(1000000000000); // was 10000
//const size_t LOCAL_DATA_AGAINST_SIZE(6000);

const size_t LOCAL_DATA_rowhit_SIZE(10000);// Todo-later: Create a fix so we do not need the array in CU code
size_t Get_LOCAL_DATA_rowhit_SIZE() {
	return LOCAL_DATA_rowhit_SIZE;
}
#define __THREADSIZE 512


//size_t Get_SHARED_DATA_QUERY_SIZE() {
//	return SHARED_DATA_QUERY_SIZE / 2; // divide by 2, since we use the memory for normal and reversed complement sequences
//}
//size_t Get_LOCAL_DATA_AGAINST_SIZE() {
//	return LOCAL_DATA_AGAINST_SIZE;
//}

struct Lock {
	int *mutex;
	Lock(void) {
		int state = 0;
		cudaMalloc((void**)&mutex, sizeof(int));
		cudaMemcpy(mutex, &state, sizeof(int), cudaMemcpyHostToDevice);
	}
	~Lock(void) {
		cudaFree(mutex);
	}
	__device__ void lock(void) {
		while (atomicCAS(mutex, 0, 1) != 0);
	}
	__device__ void unlock(void) {
		atomicExch(mutex, 0);
	}
};


__device__ int atomicAddC(int *data, int val)
{
	int old, newval, curr = *data;
	//printf("%d, ", curr);
	do {
		// Generate new value from current data
		old = curr;
		newval = curr + val;


		// Attempt to swap old <-> new.
		curr = atomicCAS(data, old, newval);
		// Repeat if value has changed in the meantime.
	} while (curr != old);



	printf("%d %d %d %d %d' ", old, *data, newval, curr, val);

	return old;
}



__device__ void lock(int* mutex) {
	/* compare mutex to 0.
	   when it equals 0, set it to 1
	   we will break out of the loop after mutex gets set to 1 */
	while (atomicCAS(mutex, 0, 1) != 0) {
		/* do nothing */
	}
}

__device__ void unlock(int* mutex) {
	atomicExch(mutex, 0);
}
__global__
void  GetHighestMatrixScore(char* __restrict__ seqQuerys,
	//const char* __restrict__ seqAgainst,
	const size_t* __restrict__ seqLengthQuery, const size_t* __restrict__ seqLengthsAgainst,
	const size_t* __restrict__ seqStartingPointsAgainst, const size_t* __restrict__ seqStartingPointsQuerys,
	const size_t* __restrict__ numAgainst,
	float* Scores,
	size_t* ScoreQueryIDs,
	size_t* ScoreReferenceIDs,
	//float* PercentageMatches,
	const size_t* __restrict__ ConsiderThreshold,
	const size_t* __restrict__ seqNumAgainstPerQuery, const size_t* __restrict__ seqPosAgainstPerQuery, const size_t* __restrict__  seqPosReferenceCharsPerQuery,
	const size_t* __restrict__ referenceIDsPerQuery, const size_t* __restrict__ referenceHashPositionListsPerQuery, const size_t* __restrict__ referenceHashPositionListsSizesPerQuery,
	const size_t* __restrict__ referenceHashRefPositionsPerQuery,
	const size_t* __restrict__ referenceHashRefPositionsPerQuery_LastPositionInRange,
	const size_t* __restrict__ referenceHashQueryPositionsPerQuery,
	const size_t* __restrict__ referenceHashQueryPositionsPerQuery_LastPositionInRange,

	const size_t* __restrict__ referenceCharsPerHash_Size,
	const char* __restrict__ referenceCharsPerHash_Head,
	const char* __restrict__ referenceCharsPerHash_Tail,
	const size_t* __restrict__ referenceCharsPerHash_Head_Size,
	const size_t* __restrict__ referenceCharsPerHash_Tail_Size,



	//bool* rowHitPlaceholder, 
	const size_t* __restrict__ lenQueries, int* mutexed_ScoreID_counter)
{

	const size_t CONSTANT_THRESHOLD = (*ConsiderThreshold);
	//const size_t CONSTANT_THRESHOLD_HALF = (*ConsiderThreshold) / 2;
	__shared__ size_t numAgainstCurrentQuery;

	//__shared__ int step;
	__shared__ size_t seqStart;
	//__shared__ float lenQuery;
	//__shared__ char dataQuery[SHARED_DATA_QUERY_SIZE];

	size_t threadID = __mul24(blockDim.x, blockIdx.y) + threadIdx.x; //place it a bit above, so the GPU has some time to calculate this value before it is used

	if (threadIdx.x == 0)
	{
		numAgainstCurrentQuery = seqNumAgainstPerQuery[blockIdx.x];
		seqStart = seqStartingPointsQuerys[blockIdx.x];
	}
	__syncthreads();



	//// Thread gate, because we run 1 thread per 'against' sequence and the last thread block might run more threads than sequences are needed to be analysed
	if (threadID >= numAgainstCurrentQuery)
		return;
#ifdef show_cuda_debug

	printf("->%lu %lu <-\n", threadID, numAgainstCurrentQuery); // gelijk
	//printf("%lu %lu\n", numAgainstCurrentQuery,*numAgainst); // gelijk
#endif



	//size_t referenceCharsPositionsList = *(seqPosReferenceCharsPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID);

	size_t hashmapPositionsList = *(referenceHashPositionListsPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID);
	size_t hashmapPositionsListSize = *(referenceHashPositionListsSizesPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID);
	//size_t iAgainst = *(referenceIDsPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID);


	//const char* sQuery = seqQuerys + seqStart;
	//for (int i = threadIdx.x; i < lenQuery; i += step)
	//	dataQuery[i] = *(sQuery + i);

	__syncthreads();


	//blockIdx.x : queryID(0 / 4)
	//blockIdx.y : partly referenceIDs(3 / 8) (!!)
	//blockDim.x : threadSize(486)
	//blockDim.y : unused(1)
	//gridDim.y : numberOfThreadBlocks(9)
	//gridDim.x : numberOfQueries(5)
	//threadIdx.x : threadID(0 / 479)  (!!)
	//threadIdx.y : unused(0)
//#ifdef show_cuda_debug
//	printf("%lu %lu %d %d %d %d %d %d\n", blockIdx.x, iAgainst, blockIdx.x, blockIdx.y, blockDim.x, *numAgainst, gridDim.y, gridDim.x, blockDim.y);
//	printf("a");
//#endif
	const size_t querySize = seqLengthQuery[blockIdx.x];

	if (querySize < CONSTANT_THRESHOLD)
		return;

	//const size_t _seqLengthAgainst = seqLengthsAgainst[iAgainst];  //place it a bit above, so the GPU has some time to calculate this value before it is used

//#ifdef show_cuda_debug
//	printf(">> %lu %lu %lu %lu %lu \n", iAgainst, hashmapPositionsList, hashmapPositionsListSize, _seqLengthAgainst, _seqLengthQuery);
//#endif

	//char dataAgainst[LOCAL_DATA_AGAINST_SIZE];
	//if (_seqLengthAgainst > LOCAL_DATA_AGAINST_SIZE)
	//{
	//	printf("CUDA ERROR: LOCAL_DATA_AGAINST_SIZE to low for this reference: %i (len: %lu). This sequence will be skipped.\n", iAgainst, _seqLengthAgainst);
	//	return;
	//}



	// To remember which nucleotide hit against a reference nucleotide
	bool rowHitCurrent[LOCAL_DATA_rowhit_SIZE]; // using local mem saves 1000 mb with 2000 sequences of global memory. This way we can upscale the number of sequences drasticly
	memset(rowHitCurrent, false, LOCAL_DATA_rowhit_SIZE);
	//bool* rowHitCurrent = rowHitPlaceholder + (seqStart * (*numAgainst)) + (iAgainst * _seqLengthQuery);


	//memcpy(dataAgainst, seqAgainst + seqStartingPointsAgainst[iAgainst], sizeof(char) * _seqLengthAgainst);

	char* dataQuery = seqQuerys + seqStart;
	//char* ptDataQuery = dataQuery;
	//const char* dataAgainst = seqAgainst + seqStartingPointsAgainst[iAgainst];

	size_t diagonalRepeatedTotalScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[2]
	size_t diagonalTotalConcidered = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[4]

										//size_t ThresholdPlusOne = CONSTANT_THRESHOLD + 1;
										//size_t ThresholdAddScore = (CONSTANT_THRESHOLD * (CONSTANT_THRESHOLD + 1)) / 2;
	//size_t seqLengthQueryMinusThreshold = querySize - (CONSTANT_THRESHOLD + 1);


	//const char* ptDataAgainstEnd = dataAgainst + _seqLengthAgainst;



	//size_t thresholdHalf = (CONSTANT_THRESHOLD / 2);
	for (size_t cHashmapPositionsList = 0; cHashmapPositionsList < hashmapPositionsListSize; cHashmapPositionsList++)
	{
		//printf("%d %lu %lu \n", blockIdx.x, iAgainst, *(referenceHashRefPositionsPerQuery + hashmapPositionsList + cHashmapPositionsList));


		//size_t dataAgainstStart = *(referenceHashRefPositionsPerQuery + hashmapPositionsList + cHashmapPositionsList);





		size_t rowStart = *(referenceHashQueryPositionsPerQuery + hashmapPositionsList + cHashmapPositionsList);
		size_t rowEnd = *(referenceHashQueryPositionsPerQuery_LastPositionInRange + hashmapPositionsList + cHashmapPositionsList);
		size_t row = rowEnd;// rowStart;
		size_t cntReferencePos = *(referenceHashRefPositionsPerQuery_LastPositionInRange + hashmapPositionsList + cHashmapPositionsList);


		// prevent miscalculations
		if (rowStart > rowEnd)
			continue;



		// The middle part
		size_t numHashMatch = rowEnd - rowStart; // This range has been found as 1 succesive region of hashes in the hashmap



		// The right part
		const size_t* refSize = (referenceCharsPerHash_Size + hashmapPositionsList + cHashmapPositionsList);
		const size_t* tailSize = (referenceCharsPerHash_Tail_Size + hashmapPositionsList + cHashmapPositionsList);
		size_t index = 0;

		const char* rightPartReferenceStart = (referenceCharsPerHash_Tail + (hashmapPositionsList * CONSTANT_THRESHOLD) + (cHashmapPositionsList * CONSTANT_THRESHOLD));// rowStart;
		const char* leftPartReferenceStart = (referenceCharsPerHash_Head + (hashmapPositionsList * CONSTANT_THRESHOLD) + (cHashmapPositionsList * CONSTANT_THRESHOLD));// rowStart;

		//printf("\n1 i:%lu t:%lu c:%lu r:%lu q:%lu r:%lu %lu %lu %lu %lu\n", index, *tailSize, cntReferencePos, row, querySize, *refSize, *(referenceIDsPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID), hashmapPositionsList, cHashmapPositionsList, (hashmapPositionsList * CONSTANT_THRESHOLD) + (cHashmapPositionsList * CONSTANT_THRESHOLD));


/*
		if (threadID ==1)
		{


			printf("\n Head: ");
			for (int i = 0; i < *(referenceCharsPerHash_Head_Size + hashmapPositionsList + cHashmapPositionsList); i++)
				printf("%c", *(leftPartReferenceStart + i));

			printf("\n Tail: ");
			for (int i = 0; i < *tailSize; i++)
				printf("%c", *(rightPartReferenceStart + i));

			printf("\n query Head: ");
			for (int i = *(referenceCharsPerHash_Head_Size + hashmapPositionsList + cHashmapPositionsList); i != 0; i--)
				printf("%c",*( dataQuery + row - *(referenceCharsPerHash_Head_Size + hashmapPositionsList + cHashmapPositionsList) + i));

			printf("\n Query Tail: ");
			for (int i = 0; i < *tailSize; i++)
				printf("%c",*( dataQuery + rowEnd + i));





		}*/
		while (true)
		{
			if (row >= querySize || cntReferencePos >= *refSize || index >= *tailSize)
				break;

			//printf("%c", *(dataQuery + row));
			//printf("R %lu %c %c \n", *tailSize, *(dataQuery + row), *(rightPartReferenceStart + index));



			if (*(dataQuery + row) != *(rightPartReferenceStart + index))
			{
				//printf("break %lu %c %c %lu", index, *(dataQuery + row), *(rightPartReferenceStart + index),row);
				break;
			}
			//if ((*query)[row] != htl->Tail[index])
			//break;

			index++;
			row++;
			cntReferencePos++;
		}


		size_t numMatched = index;


		// The left part

		size_t numMatchedReverse = 0;

		if (rowStart > 0)
		{
			row = rowStart - 1;

			cntReferencePos = *(referenceHashRefPositionsPerQuery + hashmapPositionsList + cHashmapPositionsList);
			//cntReferencePos = combiStartPositions[cHashmapPositionsList].Reference;

			SEQ_POS_t rowReverse = 1;

			const size_t* headSize = (referenceCharsPerHash_Head_Size + hashmapPositionsList + cHashmapPositionsList);
			//printf(".. %lu %lu %lu %lu\n", *headSize, hashmapPositionsList, cHashmapPositionsList, hashmapPositionsList + cHashmapPositionsList);






			if (*headSize > 0)
			{
				index = (*headSize) - 1;
				//printf("\n2 %lu %lu %lu %lu\n", index, (*headSize), cntReferencePos, row);

				//printf("%.*s", stringLength, pointerToString);

				while (true)
				{
					//if (row >= querySize || cntReferencePos >= refSize)
					//{
					//	throw exception();
					//}

					//printf("%c", *(dataQuery + row));

					//printf("L %lu %c %c \n", *headSize, *(dataQuery + row), *(leftPartReferenceStart + index));

					if (*(dataQuery + row) != *(leftPartReferenceStart + index))
						//if ((*query)[row] != htl->Head[index])
					{
						numMatchedReverse += rowReverse - 1;
						break;
					}

					if (row == 0 || cntReferencePos == 0 || index == 0)
					{
						numMatchedReverse += rowReverse - 1;
						break;
					}

					cntReferencePos--;
					row--;
					index--;

					rowReverse++;
				}
			}
		}
		//printf("head %lu tail %lu  middle %lu\n", numMatched, numMatchedReverse, numHashMatch);


		// Sum up
		size_t totalMatched = numMatched + numMatchedReverse + numHashMatch;
		if (totalMatched >= CONSTANT_THRESHOLD)
		{
			// from rowStart till end
			diagonalTotalConcidered += totalMatched;// numMatched;
			memset(rowHitCurrent + (rowStart - numMatchedReverse), true, totalMatched);// numMatched);
			diagonalRepeatedTotalScore += (totalMatched * (totalMatched + 1)) / 2; //(numMatched * (numMatched + 1)) / 2;
		}

	}



	/*
	gridDim: This variable contains the dimensions of the grid.
	blockIdx : This variable contains the block index within the grid.
	blockDim : This variable and contains the dimensions of the block.
	threadIdx : This variable contains the thread index within the block.*/
	//const size_t scoreID = __mul24(blockIdx.x, __mul24(blockDim.x, gridDim.y)) + (__mul24(blockDim.x, blockIdx.y) + threadIdx.x); //  __mul24(blockDim.x, blockIdx.y) + threadIdx.x = ThreadID

	//printf("%lu %lu %lu aaa\n", threadID, numAgainstCurrentQuery, scoreID); // gelijk

	// Save score
	if (diagonalTotalConcidered > 0)
	{

		size_t rowHitsFlat = 0;
		for (size_t i = 0; i < querySize; i++)
			if (rowHitCurrent[i])
				rowHitsFlat++;

		float percentageMatch = (100.0 / (float)querySize) * (float)rowHitsFlat;
		// Top off percentage to 100%. To prevent overlapping regions in the matrix, increase the power of the score
		// Todo-removed: Keep this? Since you might take in account that a part of your sequence is ambigues in relation of the reference
		//if (percentageMatch > 100)
		//	percentageMatch = 100;

		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;// __fdividef(diagonalRepeatedTotalScore, diagonalTotalConcidered); // (float)score_concidered[0] / (float)score_concidered[2];
		//PercentageMatches[scoreID] = percentageMatch;
		score = ((score / 100.0) * percentageMatch);// -((score / 100.0)*duplicatePenaltyPercentage);//+((score / 100.0)*percentageDoubles); // (float)score_concidered[0] / (float)score_concidered[2];
															  //printf("%f %lu\n", ((score / 100.0) * percentageMatches), rowHitsFlat);
															  //printf("%f %lu %d %d %d %d %d %f %f %lu %lu\n", ((score / 100.0) * percentageMatches), scoreID, blockIdx.x, blockIdx.y, threadIdx.x, blockDim.x, gridDim.y, percentageMatches, score, _seqLengthQuery, rowHitsFlat);



		const float lenMax = (float)(*seqLengthQuery < *seqLengthsAgainst ? *seqLengthQuery : *seqLengthsAgainst);
		const float scoreMax = (((lenMax * (lenMax + 1.0)) / 2.0) / lenMax) + 1;

		// To overcome 'Log(1) = 0' and Log(<1) = -x'
		score += 1;



		register int currentIndex = atomicAdd(mutexed_ScoreID_counter, 1);

		//printf("%d. ", *mutexed_ScoreID_counter);

		ScoreQueryIDs[currentIndex] = blockIdx.x;
		//ScoreReferenceIDs[currentIndex] = iAgainst;
		ScoreReferenceIDs[currentIndex] = *(referenceIDsPerQuery + seqPosAgainstPerQuery[blockIdx.x] + threadID);


		Scores[currentIndex] = ((100.0 / log(scoreMax)) * log(score)) / 100.0;



#ifdef show_cuda_debug
		//printf("%lu %f %lu %lu %f\n", scoreID, Scores[scoreID], diagonalRepeatedTotalScore, diagonalTotalConcidered, percentageMatches);
		//printf("== %f %lu %d %d %d %d %d - %f %f %lu %lu %lu %lu %lu\n", ((score / 100.0) * percentageMatch), scoreID, blockIdx.x, blockIdx.y, threadIdx.x, blockDim.x, gridDim.y, percentageMatch, score, querySize, rowHitsFlat, diagonalRepeatedTotalScore, diagonalTotalConcidered, _seqLengthAgainst);
#endif
	}
}




void CudaDeviceResetAsync(int cudaDeviceID)
{
	cudaSetDevice(cudaDeviceID);
	cudaDeviceReset();
}


vector<vector<future<void>>> futureCudaDeviceReset;
bool CudaInitialised(false);
void InitialiseCuda(int NumDevices)
{
	CudaInitialised = true;
	futureCudaDeviceReset = vector<vector<future<void>>>();
	futureCudaDeviceReset.resize(NumDevices);
	for (int i = 0; i < NumDevices; i++)
		futureCudaDeviceReset[i] = vector<future<void>>();
}

void cudaMallocMemSafe(void** ptrCuda, size_t sizeMem, int line, int cudaDeviceID)
{
	size_t mem_tot = 0;
	size_t mem_free = 0;
	cudaMemGetInfo(&mem_free, &mem_tot);

	size_t ceiling = 300000000; // todo: this 300mb, is it a valid amount? Not really.. but for now i
	if (sizeMem > (mem_free - ceiling))
	{
		futureCudaDeviceReset[cudaDeviceID].push_back(std::async(CudaDeviceResetAsync, cudaDeviceID));
		throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(line) + "\n Out of memory (Allocate request:" + to_string(sizeMem) + ", Free:" + to_string(mem_free) + ")");
	}

	cudaError err = cudaMalloc(ptrCuda, sizeMem);
	if (cudaSuccess != err)
	{
		futureCudaDeviceReset[cudaDeviceID].push_back(std::async(CudaDeviceResetAsync, cudaDeviceID));
		throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(line) + "\n Allocation failed: " + to_string(err) + ")");
	}

}

void cudaMallocHostMemSafe(void** ptrCuda, size_t sizeMem, int line)
{
	size_t mem_tot = 0;
	size_t mem_free = 0;
	cudaMemGetInfo(&mem_free, &mem_tot);

	size_t ceiling = 300000000; // todo: this 300mb, is it a valid amount? Not really.. but for now i
	if (sizeMem > (mem_free - ceiling))
	{
		throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(line) + "\n Out of memory (Allocate request:" + to_string(sizeMem) + ", Free:" + to_string(mem_free) + ")");
	}

	cudaError err = cudaMallocHost(ptrCuda, sizeMem);
	if (cudaSuccess != err)
	{
		throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(line) + "\n Allocation failed: " + to_string(err) + ")");
	}

}



//http://stackoverflow.com/questions/7940279/should-we-check-if-memory-allocations-fail
void * cemalloc(int line, size_t amt) {
	void *v = malloc(amt);
	if (!v) {
		fprintf(stderr, "cemalloc out of mem\n");
		cout << to_string(line) << endl;
		cout.flush();
		throw(runtime_error("cemalloc emalloc out of mem" + to_string(line)));

		//exit(EXIT_FAILURE);
	}
	return v;
}


size_t CUDAGetMaxBlockSize(int cudaDeviceID)
{
	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop, cudaDeviceID);
	size_t maxSurface1D = prop.maxSurface1D;
	//delete &prope;
	return maxSurface1D;
}



void waitForDeviceReset(int cudaDeviceID)
{

	Asynchronise::wait_for_futures(&futureCudaDeviceReset[cudaDeviceID]);
}



//_GOOD_WORKING
#include <unordered_set>
#include <unordered_map>
vector<MatrixResult> CUDAGetHighestScore(vector<vector<char>*> seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, size_t ConsiderThreshold, vector<SEQ_ID_t>* queryReferenceCombinations, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsRefPositions, vector<vector<SEQ_POS_t>>* queryReferenceCombinationsQueryPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions, vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions_LastPositionInRange, int threadSize, Display *display, MultiThreading *multiThreading, SEQ_AMOUNT_t referencesToAnalyse_Offset, SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis, SEQ_AMOUNT_t queriesToAnalyse_Offset, SEQ_AMOUNT_t queriesToAnalyse_SizePerAnalysis, vector<vector<Enum::HeadTailSequence>>* matchedReferencesPerQuery_HeadTailSequences)//, char** charsAgainst_tmp, int threadSize)
{
	if (!CudaInitialised)
	{
		cout << "Cuda for CUDAGetHighestScore not initialised." << endl << flush;
		exit(1);
	}

#ifdef show_timings
	float mDebugTime;
	clock_t begin_time_against = clock();
	clock_t begin_time_total = clock();

#endif		

#ifdef show_debug

	//cout << endl << endl << endl << endl << endl << endl;
	cout << "CUDAGetHighestScore - Start prepare data on cuda device " << cudaDeviceID << endl;



	//cout << " referencesToAnalyse_Offset " << referencesToAnalyse_Offset << endl;
	//cout << " referencesToAnalyse_SizePerAnalysis " << referencesToAnalyse_SizePerAnalysis << endl;

	//cout << "                                                        cudaDeviceID  START " << cudaDeviceID << endl;
	//cout << " seqQuery.size() " << seqQuery.size() << endl;
	////cout << " queryReferenceCombinationRefPositions " << queryReferenceCombinationsRefPositions.size() << endl;
	//cout << " queryReferenceCombinationQueryPositions " << queriesToAnalyse_SizePerAnalysis << endl;
	////cout << " queryReferenceCombinationQueryPositions " << queryReferenceCombinationsQueryPositions.size() << endl;
	//cout << " QuerySequences " << seqQuery.size() << endl;
#endif

	//cout.flush();

	//size_t mem_tot_0 = 0;
	//size_t mem_free_0 = 0;



#ifdef show_timings
	//got selected pref ref(4.890000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("wait_for_futures futureCudaDeviceReset cuda (%f sec)\n", mDebugTime / 1000);
	begin_time_against = clock();

#endif


	vector<MatrixResult> retVal;

	waitForDeviceReset(cudaDeviceID);

	cudaSetDevice(cudaDeviceID);
	//cudaDeviceReset(); // Reset it again to be sure (is fast when it is already done)


#ifdef show_timings
	//got selected pref ref(4.890000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("start cuda (%f sec)\n", mDebugTime / 1000);
	begin_time_against = clock();

#endif







	vector <size_t> sortedUsedOriginalReferenceIDs = vector<size_t>(); // To get unique reference ID's to work with
	unordered_set <size_t> unRefIDs = unordered_set<size_t>(); // To get unique reference ID's to work with
	vector<size_t> vecReferenceTranslateTable(sequences->referenceSequences.size());
	vector<vector<size_t>> relativeQueryReferenceCombinationsID = vector<vector<size_t>>();
	vector<vector<vector <Enum::HeadTailSequence>*>> relativeHeadTailSequences = vector<vector<vector<Enum::HeadTailSequence>*>>();
	vector<vector<vector <Enum::Position>*>> relativeQueryReferenceCombinationsPositions = vector<vector<vector<Enum::Position>*>>();
	vector<vector<vector <Enum::Position>*>> relativeQueryReferenceCombinationsPositions_LastPositionInRange = vector<vector<vector<Enum::Position>*>>();

	size_t qrcSize = queriesToAnalyse_SizePerAnalysis;// queryReferenceCombinations.size();
	SEQ_AMOUNT_t referenceToAnalyse_UntilID = referencesToAnalyse_Offset + referencesToAnalyse_SizePerAnalysis;
	// Get all selected reference ID's

	const size_t ConsiderThreshold_Half = (ConsiderThreshold / 2);
	size_t newReferenceID = 0;
	size_t totalReferenceHashPositions = 0;
	size_t totalQRCSize = 0;
	for (SEQ_ID_t qID = 0; qID < qrcSize; qID++)
		totalQRCSize += queryReferenceCombinations[qID].size();


	sortedUsedOriginalReferenceIDs.reserve(totalQRCSize);
	relativeQueryReferenceCombinationsID.resize(qrcSize);
	relativeHeadTailSequences.resize(qrcSize);
	relativeQueryReferenceCombinationsPositions.resize(qrcSize);
	relativeQueryReferenceCombinationsPositions_LastPositionInRange.resize(qrcSize);
	unRefIDs.reserve(qrcSize);


#ifdef show_timings
	//got selected pref ref(4.890000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("Reserve cuda (%f sec)\n", mDebugTime / 1000);
	begin_time_against = clock();

#endif

	for (SEQ_ID_t qID = 0; qID < qrcSize; qID++)
	{

		vector<SEQ_ID_t>* ptrQRC = queryReferenceCombinations + qID;
		vector<size_t>* ptrRQRCID = &relativeQueryReferenceCombinationsID[qID];

		vector<vector<Enum::HeadTailSequence>*>* ptrRHTS = &relativeHeadTailSequences[qID];
		vector<vector<Enum::Position>*>* ptrRQRCP = &relativeQueryReferenceCombinationsPositions[qID];
		vector<vector<Enum::Position>*>* ptrRQRCP_Last = &relativeQueryReferenceCombinationsPositions_LastPositionInRange[qID];
		size_t currentQRCSize = ptrQRC->size();



		ptrRHTS->reserve(referencesToAnalyse_SizePerAnalysis);
		ptrRQRCID->reserve(referencesToAnalyse_SizePerAnalysis);
		ptrRQRCP->reserve(referencesToAnalyse_SizePerAnalysis); //currentQRCSize
		ptrRQRCP_Last->reserve(referencesToAnalyse_SizePerAnalysis);//currentQRCSize


		size_t ptrRQRCP_InsertID = 0;
		for (SEQ_ID_t rID = 0; rID < currentQRCSize; rID++)
		{

			SEQ_ID_t* refID = &(*ptrQRC)[rID];

			//// Only analyse references wihin reference window (used for downscaling the work for the GPU)
			if (!(*refID >= referencesToAnalyse_Offset && *refID <= referenceToAnalyse_UntilID))
				continue;

			if (unRefIDs.insert(*refID).second)
			{
				sortedUsedOriginalReferenceIDs.push_back(*refID); // Store new ID
				vecReferenceTranslateTable[*refID] = newReferenceID;

				//cout << "newReferenceID1:" << newReferenceID << endl;
				ptrRQRCID->push_back(newReferenceID++); // Store new ID


			}
			else
			{
				// We already found this original reference ID
				// So look it up and get the new relative reference ID 
				//cout << "newReferenceID2:" << vecReferenceTranslateTable[*refID] << endl;
				ptrRQRCID->push_back(vecReferenceTranslateTable[*refID]); // Store new ID with the already existend ID
			}




			(*ptrRHTS).push_back(&matchedReferencesPerQuery_HeadTailSequences[qID][rID]);
			(*ptrRQRCP_Last).push_back(&queryReferenceCombinationsCombiPositions_LastPositionInRange[qID][rID]);
			(*ptrRQRCP).push_back(&queryReferenceCombinationsCombiPositions[qID][rID]);
			totalReferenceHashPositions += (*ptrRQRCP).back()->size();
			/*
						(*ptrRQRCP_Last)[ptrRQRCP_InsertID] = &queryReferenceCombinationsCombiPositions_LastPositionInRange[qID][rID];
						(*ptrRQRCP)[ptrRQRCP_InsertID] = &queryReferenceCombinationsCombiPositions[qID][rID];
			totalReferenceHashPositions += (*ptrRQRCP)[ptrRQRCP_InsertID]->size();*/
			//ptrRQRCP_InsertID++;

		}
		//cout << "ptrRQRCP_InsertID: " << ptrRQRCP_InsertID << endl;
	}

	const size_t totalReferenceChars = totalReferenceHashPositions * ConsiderThreshold;


#ifdef show_timings
	//got selected pref ref(4.890000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("got selected pref ref (%f sec)\n", mDebugTime / 1000);
	begin_time_against = clock();

#endif

#ifdef show_debug
	cout << "unRefIDs: " << unRefIDs.size() << endl;
	cout << "sortedUsedOriginalReferenceIDs.size(): " << sortedUsedOriginalReferenceIDs.size() << endl;
	cout.flush();
#endif

	float totalSizeSend = 0;

	size_t NumOfSequencesReferences = unRefIDs.size();
	size_t  NumOfSequencesQuery = queriesToAnalyse_SizePerAnalysis;// seqQuery.size();

	size_t sizeHighest = 0;
	bool cudaOptimisedKernelUsage(true);

	if (cudaOptimisedKernelUsage)
		sizeHighest = NumOfSequencesQuery * ((NumOfSequencesReferences / threadSize) + 1) * threadSize;
	else
		sizeHighest = NumOfSequencesReferences * NumOfSequencesQuery;



	size_t* referenceHashQueryPositionsPerQuery_Cuda;
	cudaMallocMemSafe((void**)&referenceHashQueryPositionsPerQuery_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	////if (cudaSuccess != cudaMalloc((void**)&referenceHashQueryPositionsPerQuery_Cuda, sizeof(size_t) * totalReferenceHashPositions)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;

	size_t* referenceHashRefPositionsPerQuery_Cuda;
	cudaMallocMemSafe((void**)&referenceHashRefPositionsPerQuery_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashRefPositionsPerQuery_Cuda, sizeof(size_t) * totalReferenceHashPositions)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;




	size_t* referenceCharsPerHash_Size_Cuda;
	cudaMallocMemSafe((void**)&referenceCharsPerHash_Size_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceCharsPerHash_Size_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;

	char* referenceCharsPerHash_Head_Cuda;
	cudaMallocMemSafe((void**)&referenceCharsPerHash_Head_Cuda, sizeof(char) * totalReferenceChars, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(char) * totalReferenceChars;


	char* referenceCharsPerHash_Tail_Cuda;
	cudaMallocMemSafe((void**)&referenceCharsPerHash_Tail_Cuda, sizeof(char) * totalReferenceChars, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(char) * totalReferenceChars;


	size_t* referenceCharsPerHash_Head_Size_Cuda;
	cudaMallocMemSafe((void**)&referenceCharsPerHash_Head_Size_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;


	size_t* referenceCharsPerHash_Tail_Size_Cuda;
	cudaMallocMemSafe((void**)&referenceCharsPerHash_Tail_Size_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;






	size_t* referenceHashRefPositionsPerQuery_LastPositionInRange_Cuda;
	cudaMallocMemSafe((void**)&referenceHashRefPositionsPerQuery_LastPositionInRange_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashRefPositionsPerQuery_LastPositionInRange_Cuda, sizeof(size_t) * totalReferenceHashPositions)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;

	size_t* referenceHashQueryPositionsPerQuery_LastPositionInRange_Cuda;
	cudaMallocMemSafe((void**)&referenceHashQueryPositionsPerQuery_LastPositionInRange_Cuda, sizeof(size_t) * totalReferenceHashPositions, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&referenceHashQueryPositionsPerQuery_LastPositionInRange_Cuda, sizeof(size_t) * totalReferenceHashPositions)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	totalSizeSend += sizeof(size_t) * totalReferenceHashPositions;

#ifdef show_debug

	size_t mem_tot_0 = 0;
	size_t mem_free_0 = 0;
	cudaMemGetInfo(&mem_free_0, &mem_tot_0);
	cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
	float* Scores_Cuda;
	cudaMallocMemSafe((void**)&Scores_Cuda, sizeof(float) * sizeHighest, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&Scores_Cuda, sizeof(float) * sizeHighest)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	cudaMemset(Scores_Cuda, -1, sizeof(float) * sizeHighest);
#ifdef show_debug
	cudaMemGetInfo(&mem_free_0, &mem_tot_0);
	cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

	size_t* ScoreQueryIDs_Cuda;
	cudaMallocMemSafe((void**)&ScoreQueryIDs_Cuda, sizeof(size_t) * sizeHighest, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&ScoreIDs_Cuda, sizeof(float) * sizeHighest)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
#ifdef show_debug
	cudaMemGetInfo(&mem_free_0, &mem_tot_0);
	cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif


	size_t* ScoreReferenceIDs_Cuda;
	cudaMallocMemSafe((void**)&ScoreReferenceIDs_Cuda, sizeof(size_t) * sizeHighest, __LINE__, cudaDeviceID);
	//if (cudaSuccess != cudaMalloc((void**)&ScoreIDs_Cuda, sizeof(float) * sizeHighest)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
#ifdef show_debug
	cudaMemGetInfo(&mem_free_0, &mem_tot_0);
	cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif



	if (NumOfSequencesReferences == 0)
	{
#ifdef show_debug
		cout << "No sequences to analyse. Quit cuda. NumOfSequencesReferences: " << NumOfSequencesReferences << endl;
		cout.flush();
#endif
		// this can happen since we use the referencesToAnalyse_Offset and referencesToAnalyse_SizePerAnalysis to only accept references within this range.
		// if we have a query with queryReferenceCombinations which has no reference in this range; we end up with 0 references.
		return retVal;
	}
	//vector<std::future<cudaError>> futuresRegister;
	//futuresRegister.reserve(sortedUsedOriginalReferenceIDs.size() + 2); // +2 since we also use it for 2 outside the loop;

	// Now we really need the device reset to be ready, so wait for it..
	//futureCudaDeviceReset.wait();

	cudaStream_t stream1;
	cudaStreamCreate(&stream1);


	// Copy required refen

	//size_t refCnt = 0;
	bool useFuturesForReferenceLoading(false);
	bool useFuturesForRegister(false);



	size_t* seqLengthsAgainst = new size_t[NumOfSequencesReferences];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t)*NumOfSequencesReferences);
	size_t* SeqStartingPointsAgainst = new size_t[NumOfSequencesReferences];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t)*NumOfSequencesReferences);
	size_t seqLenAgainst_Hot = 0;
	//vector<char*> ptrReferenceSequences;
	//vector<size_t> refSeqSizes;
	//vector<std::future<void>> futures;
	//ptrReferenceSequences.resize(sortedUsedOriginalReferenceIDs.size());
	//refSeqSizes.resize(sortedUsedOriginalReferenceIDs.size());
	//futures.reserve(sortedUsedOriginalReferenceIDs.size());

	size_t* SeqStartingPointsAgainst_Cuda;
	cudaMallocMemSafe((void**)&SeqStartingPointsAgainst_Cuda, sizeof(size_t) * NumOfSequencesReferences, __LINE__, cudaDeviceID);
	//cudaHostRegister(SeqStartingPointsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
	//futuresRegister.push_back(std::async(launch::async, cudaHostRegister, SeqStartingPointsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault));
	//if (cudaSuccess != cudaMalloc((void**)&SeqStartingPointsAgainst_Cuda, sizeof(size_t) * NumOfSequencesReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	totalSizeSend += sizeof(size_t) * NumOfSequencesReferences;
	size_t* seqLengthsAgainst_Cuda;
	cudaMallocMemSafe((void**)&seqLengthsAgainst_Cuda, sizeof(size_t) * NumOfSequencesReferences, __LINE__, cudaDeviceID);
	//cudaHostRegister(seqLengthsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
	//futuresRegister.push_back(std::async(launch::async, cudaHostRegister, seqLengthsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault));


	//if (cudaSuccess != cudaMalloc((void**)&seqLengthsAgainst_Cuda, sizeof(size_t) * NumOfSequencesReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__, cudaDeviceID));
	totalSizeSend += sizeof(size_t) * NumOfSequencesReferences;


	//
	//#ifdef show_timings
	//	//pre compile reference(14.780000 sec)
	//	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	//	printf("prepare refSeqs async (%f sec) \n", mDebugTime / 1000);
	//	begin_time_against = clock();
	//#endif
	//
	//	for (size_t rID = 0; rID < sortedUsedOriginalReferenceIDs.size(); rID++)
	//	{
	//		if (Hashmap::Use_Experimental_LazyLoading)
	//		{
	//			bool aSyncMultiFileHandlers(true);
	//
	//			if (useFuturesForReferenceLoading)
	//			{
	//				Asynchronise::wait_for_futures_on_full_vector(&futures, Sequences::OpenIfstreamsPerHashmap_Async);
	//				futures.push_back(std::async(launch::async, Sequences::GetLazyContentForCuda, &sequences->referenceSequences, &sequences->referenceSequencesSizes, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceHashStreamPos, sortedUsedOriginalReferenceIDs[rID], &(ptrReferenceSequences[rID]), &refSeqSizes[rID], aSyncMultiFileHandlers));
	//			}
	//			else
	//			{
	//				Sequences::GetLazyContentForCuda(&sequences->referenceSequences, &sequences->referenceSequencesSizes, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceHashStreamPos, sortedUsedOriginalReferenceIDs[rID], &(ptrReferenceSequences[rID]), &refSeqSizes[rID], aSyncMultiFileHandlers);
	//			}
	//
	//		}
	//		else
	//		{
	//			// todo possible problem with registerHost, since if you register here and in another thread, we may unregister before the other thread is ready using the data
	//			// should we mlock the whole array during reading fasta? and then only register in the lazyloading branchey?
	//			ptrReferenceSequences[rID] = &sequences->referenceSequences[sortedUsedOriginalReferenceIDs[rID]][0];
	//			refSeqSizes[rID] = sequences->referenceSequences[sortedUsedOriginalReferenceIDs[rID]].size();
	//		}
	//	}
	//#ifdef show_timings
	//	//pre compile reference(14.780000 sec)
	//	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	//	printf("got refSeqs async (%f sec) %d \n", mDebugTime / 1000, sortedUsedOriginalReferenceIDs.size());
	//	begin_time_against = clock();
	//#endif
	//



		//if (useFuturesForReferenceLoading)
		//{
			//Asynchronise::wait_for_futures(&futures);
		//	Asynchronise::wait_for_futures(&futuresRegister);
		//}
		//else
		//{
	cudaHostRegister(SeqStartingPointsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
	cudaHostRegister(seqLengthsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
	//}

//#ifdef show_timings
//	//pre compile reference(14.780000 sec)
//	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
//	printf("got refSeqs synced (%f sec) \n", mDebugTime / 1000);
//	begin_time_against = clock();
//#endif
//	for (size_t rID = 0; rID < sortedUsedOriginalReferenceIDs.size(); rID++)
//	{
//		*(seqLengthsAgainst + rID) = refSeqSizes[rID];
//		*(SeqStartingPointsAgainst + rID) = seqLenAgainst_Hot; // Starting points
//		seqLenAgainst_Hot += refSeqSizes[rID];
//
//		if (useFuturesForRegister)
//		{
//			Asynchronise::wait_for_futures_on_full_vector(&futuresRegister, Asynchronise::__FUTURES_MAX_SIZE);
//			futuresRegister.push_back(std::async(launch::async, cudaHostRegister, ptrReferenceSequences[rID], refSeqSizes[rID], cudaHostRegisterDefault));
//		}
//		else
//			cudaHostRegister(ptrReferenceSequences[rID], refSeqSizes[rID], cudaHostRegisterDefault);
//
//	}
//
//
//
//
//	if (useFuturesForRegister)
//		Asynchronise::wait_for_futures(&futuresRegister);
//	//else
//	//{
//	//	cudaHostRegister(seqLengthsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
//	//	cudaHostRegister(SeqStartingPointsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaHostRegisterDefault);
//	//}


#ifdef show_timings
	//pre compile reference(14.780000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("got refSeqs host registered (%f sec) \n", mDebugTime / 1000);
	begin_time_against = clock();

#endif

	cudaMemcpyAsync(seqLengthsAgainst_Cuda, seqLengthsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaMemcpyHostToDevice, stream1);
	cudaMemcpyAsync(SeqStartingPointsAgainst_Cuda, SeqStartingPointsAgainst, sizeof(size_t) * NumOfSequencesReferences, cudaMemcpyHostToDevice, stream1);

#ifdef show_timings
	//pre compile reference(14.780000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("got refSeqs (%f sec) %lu \n", mDebugTime / 1000, NumOfSequencesReferences);
	begin_time_against = clock();
#endif
#ifdef show_debug
	cudaMemGetInfo(&mem_free_0, &mem_tot_0);
	cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif










	//char* charsAgainst_Cuda;
	//cudaMallocMemSafe((void**)&charsAgainst_Cuda, sizeof(char) * seqLenAgainst_Hot, __LINE__, cudaDeviceID);
	//totalSizeSend += sizeof(char) * seqLenAgainst_Hot;










	////char* charsAgainst_Host = new char[seqLenAgainst_Hot];// (char*)cemalloc(__LINE__, cudaDeviceID, seqLenAgainst_Hot);
	//if (false)
	//	for (size_t rID = 0; rID < sortedUsedOriginalReferenceIDs.size(); rID++)
	//	{
	//		// opencl: clEnqueueMapBuffer 
	//		if (!useFuturesForRegister)
	//			cudaHostRegister(ptrReferenceSequences[rID], *(seqLengthsAgainst + rID), cudaHostRegisterDefault);
	//		cudaMemcpyAsync(charsAgainst_Cuda + *(SeqStartingPointsAgainst + rID), ptrReferenceSequences[rID], *(seqLengthsAgainst + rID), cudaMemcpyHostToDevice, stream1);
	//	}


#ifdef show_timings
	//pre compile reference(14.780000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("copy refSeqs to host (%f sec) \n", mDebugTime / 1000);
	begin_time_against = clock();
#endif


	//cudaHostRegister(charsAgainst_Host, seqLenAgainst_Hot, cudaHostRegisterDefault);
	//cudaMemcpyAsync(charsAgainst_Cuda, charsAgainst_Host, seqLenAgainst_Hot, cudaMemcpyHostToDevice, stream1);

#ifdef show_timings
	//pre compile reference(14.780000 sec)
	mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
	printf("copy refSeqs to cuda (%f sec) \n", mDebugTime / 1000);
	begin_time_against = clock();
#endif

	try
	{

		///
		/// Translate Sequence to 2 pointer lists
		/// 
		// Build input seqs length list

#ifdef __CUDA__
		//cudaProfilerStart();
#endif


#ifdef show_debug

		cout << "relativeQueryReferenceCombinationsID" << endl;
		cout.flush();
#endif
		// COMBI //
		size_t longestAgainstPerQuery = 0;
		size_t* seqNumAgainstPerQuery = new size_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t)*NumOfSequencesQuery);
		size_t* seqPosAgainstPerQuery = new size_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t)*NumOfSequencesQuery);
		size_t seqPosAgainstPerQueryCNT = 0;
		//size_t seqPosReferenceCharsPerQueryCNT = 0;

		// Build list of reference IDs per query, so the kernel can easily lookup which reference it should read
		size_t totalReferences = 0;
		for (size_t i = 0; i < relativeQueryReferenceCombinationsID.size(); i++)
			if (!relativeQueryReferenceCombinationsID[i].empty())
				totalReferences += relativeQueryReferenceCombinationsID[i].size();



#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("build list of ref ids per query (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif


		size_t* referenceIDsPerQuery = new size_t[totalReferences];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t) * totalReferences);
		size_t* referenceHashPositionListsPerQuery = new size_t[totalReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);
		size_t* seqPosReferenceCharsPerQuery = new size_t[totalReferences];//(size_t*)cemalloc(__LINE__, cudaDeviceID, sizeof(size_t)*NumOfSequencesQuery);
		size_t* referenceHashPositionListsSizesPerQuery = new size_t[totalReferences];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);

		size_t* referenceCharsPerHash_Size = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		char* referenceCharsPerHash_Head = new char[totalReferenceChars];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		char* referenceCharsPerHash_Tail = new char[totalReferenceChars];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		size_t* referenceCharsPerHash_Head_Size = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		size_t* referenceCharsPerHash_Tail_Size = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);

		size_t* referenceHashRefPositionsPerQuery = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		size_t* referenceHashQueryPositionsPerQuery = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		size_t* referenceHashRefPositionsPerQuery_LastPositionInRange = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);
		size_t* referenceHashQueryPositionsPerQuery_LastPositionInRange = new size_t[totalReferenceHashPositions];//(size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferenceHashPositions);





		size_t referenceIDsPerQueryCnt = 0;
		size_t referenceHashPositionListsPerQueryCnt = 0;
		size_t referenceHashPositionsPerPerQueryCnt = 0;
		size_t referenceCharsPositionCnt = 0;

		size_t* referenceIDsPerQuery_Host;
		size_t* referenceIDsPerQuery_Cuda;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
		cudaMallocMemSafe((void**)&referenceIDsPerQuery_Cuda, sizeof(size_t) * totalReferences, __LINE__, cudaDeviceID);
		referenceIDsPerQuery_Host = new size_t[totalReferences];// (size_t*)cemalloc(__LINE__, sizeof(size_t) * totalReferences);
		////if (cudaSuccess != cudaMalloc((void**)&referenceIDsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * totalReferences;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
		size_t* referenceHashPositionListsSizesPerQuery_Cuda;
		cudaMallocMemSafe((void**)&referenceHashPositionListsSizesPerQuery_Cuda, sizeof(size_t) * totalReferences, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsSizesPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * totalReferences;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif



		size_t* referenceHashPositionListsPerQuery_Cuda;
		cudaMallocMemSafe((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&referenceHashPositionListsPerQuery_Cuda, sizeof(size_t) * totalReferences)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * totalReferences;

		size_t* seqPosReferenceCharsPerQuery_Cuda;
		cudaMallocMemSafe((void**)&seqPosReferenceCharsPerQuery_Cuda, sizeof(size_t) * totalReferences, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&seqPosAgainstPerQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * NumOfSequencesQuery;




#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
		size_t* seqNumAgainstPerQuery_Cuda;
		cudaMallocMemSafe((void**)&seqNumAgainstPerQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&seqNumAgainstPerQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * NumOfSequencesQuery;

#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

		size_t* seqPosAgainstPerQuery_Cuda;
		cudaMallocMemSafe((void**)&seqPosAgainstPerQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&seqPosAgainstPerQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		totalSizeSend += sizeof(size_t) * NumOfSequencesQuery;



		for (size_t i = 0; i < NumOfSequencesQuery; i++)
		{
			*(seqPosAgainstPerQuery + i) = seqPosAgainstPerQueryCNT;

			vector<size_t>* ptrRelativeQueryReferenceCombinationsID = &relativeQueryReferenceCombinationsID[i];
			if (!ptrRelativeQueryReferenceCombinationsID->empty())
			{

				size_t sizeRQRC = ptrRelativeQueryReferenceCombinationsID->size();
				seqPosAgainstPerQueryCNT += sizeRQRC;
				*(seqNumAgainstPerQuery + i) = sizeRQRC;



				if (sizeRQRC > longestAgainstPerQuery)
					longestAgainstPerQuery = sizeRQRC;

				memcpy(referenceIDsPerQuery_Host + referenceIDsPerQueryCnt, ptrRelativeQueryReferenceCombinationsID->data(), sizeof(size_t) * sizeRQRC);

				referenceIDsPerQueryCnt += sizeRQRC;

				vector<size_t>* ptrRQRCRID = &relativeQueryReferenceCombinationsID[i];
				vector<vector<Enum::HeadTailSequence>*>* ptrRHTS = &relativeHeadTailSequences[i];
				vector<vector<Enum::Position>*>* ptrRQRCRP = &relativeQueryReferenceCombinationsPositions[i];
				vector<vector<Enum::Position>*>* ptrRQRCRP_LPIR = &relativeQueryReferenceCombinationsPositions_LastPositionInRange[i];
				size_t sizeRQRCRP = ptrRQRCRP->size();

				for (size_t j = 0; j < sizeRQRCRP; j++)
				{
					size_t currentSizeRQRCRP = (*ptrRQRCRP)[j]->size();
					*(seqPosReferenceCharsPerQuery + referenceHashPositionListsPerQueryCnt) = referenceCharsPositionCnt; // Remember where this list of chars start for current Query

					*(referenceHashPositionListsPerQuery + referenceHashPositionListsPerQueryCnt) = referenceHashPositionsPerPerQueryCnt;
					*(referenceHashPositionListsSizesPerQuery + referenceHashPositionListsPerQueryCnt) = currentSizeRQRCRP;
					referenceHashPositionListsPerQueryCnt++;


					for (size_t k = 0; k < currentSizeRQRCRP; k++)
					{



						// Save hash position for the head
						*(referenceHashRefPositionsPerQuery + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRQRCRP)[j])[k].Reference;
						*(referenceHashQueryPositionsPerQuery + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRQRCRP)[j])[k].Query;


						// Save hash positions for the Tails
						*(referenceHashRefPositionsPerQuery_LastPositionInRange + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRQRCRP_LPIR)[j])[k].Reference;
						*(referenceHashQueryPositionsPerQuery_LastPositionInRange + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRQRCRP_LPIR)[j])[k].Query;


						// Save the reference chars for head
						// todo: add boundary gate below (or not? since we gate this during cuda kernel run as well)
						size_t refID = (*ptrRQRCRID)[j]; // (*ptrRelativeQueryReferenceCombinationsID)[(*ptrRQRCRID)[j]];

						*(referenceCharsPerHash_Size + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRHTS)[j])[k].SequenceLength;
						*(referenceCharsPerHash_Head_Size + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRHTS)[j])[k].Head.size();
						*(referenceCharsPerHash_Tail_Size + referenceHashPositionsPerPerQueryCnt) = (*(*ptrRHTS)[j])[k].Tail.size();

						memcpy(referenceCharsPerHash_Head + referenceCharsPositionCnt, &(*(*ptrRHTS)[j])[k].Head[0], (*(*ptrRHTS)[j])[k].Head.size()); // todo: compress and only store correct size
						memcpy(referenceCharsPerHash_Tail + referenceCharsPositionCnt, &(*(*ptrRHTS)[j])[k].Tail[0], (*(*ptrRHTS)[j])[k].Tail.size()); // todo: compress and only store correct size

						referenceHashPositionsPerPerQueryCnt++;
						referenceCharsPositionCnt += ConsiderThreshold;

					}
				}
			}
			else
				*(seqNumAgainstPerQuery + i) = 0;
		}


#ifdef show_debug
		cout << "NumOfSequencesQuery: " << NumOfSequencesQuery << endl;
		cout.flush();
#endif
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("SeqStartingPointsQuery seqsQuery to host(%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif



		//cudaHostRegister(referenceCharsPerHash_Head, sizeof(char)* totalReferenceChars, cudaHostRegisterDefault);
		//cudaHostRegister(referenceCharsPerHash_Tail, sizeof(char)* totalReferenceChars, cudaHostRegisterDefault);
		//cudaHostRegister(referenceIDsPerQuery_Host, sizeof(size_t) * totalReferences, cudaHostRegisterDefault);
		//cudaHostRegister(referenceHashQueryPositionsPerQuery, sizeof(size_t)* totalReferenceHashPositions, cudaHostRegisterDefault);
		//cudaHostRegister(referenceHashRefPositionsPerQuery, sizeof(size_t)* totalReferenceHashPositions, cudaHostRegisterDefault);
		//cudaHostRegister(referenceHashPositionListsSizesPerQuery, sizeof(size_t)* totalReferences, cudaHostRegisterDefault);
		//cudaHostRegister(referenceHashPositionListsPerQuery, sizeof(size_t)* totalReferences, cudaHostRegisterDefault);
		//cudaHostRegister(seqNumAgainstPerQuery, sizeof(size_t)* NumOfSequencesQuery, cudaHostRegisterDefault);
		//cudaHostRegister(seqPosAgainstPerQuery, sizeof(size_t)* NumOfSequencesQuery, cudaHostRegisterDefault);
		//cudaHostRegister(seqPosReferenceCharsPerQuery, sizeof(size_t)* totalReferences, cudaHostRegisterDefault);




		cudaMemcpyAsync(referenceIDsPerQuery_Cuda, referenceIDsPerQuery_Host, sizeof(size_t) * totalReferences, cudaMemcpyHostToDevice, stream1);


		cudaMemcpyAsync(referenceCharsPerHash_Size_Cuda, referenceCharsPerHash_Size, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceCharsPerHash_Head_Cuda, referenceCharsPerHash_Head, sizeof(char) * totalReferenceChars, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceCharsPerHash_Tail_Cuda, referenceCharsPerHash_Tail, sizeof(char) * totalReferenceChars, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceCharsPerHash_Head_Size_Cuda, referenceCharsPerHash_Head_Size, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceCharsPerHash_Tail_Size_Cuda, referenceCharsPerHash_Tail_Size, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);


		cudaMemcpyAsync(referenceHashQueryPositionsPerQuery_Cuda, referenceHashQueryPositionsPerQuery, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceHashRefPositionsPerQuery_Cuda, referenceHashRefPositionsPerQuery, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceHashQueryPositionsPerQuery_LastPositionInRange_Cuda, referenceHashQueryPositionsPerQuery_LastPositionInRange, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceHashRefPositionsPerQuery_LastPositionInRange_Cuda, referenceHashRefPositionsPerQuery_LastPositionInRange, sizeof(size_t) * totalReferenceHashPositions, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceHashPositionListsSizesPerQuery_Cuda, referenceHashPositionListsSizesPerQuery, sizeof(size_t) * totalReferences, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(referenceHashPositionListsPerQuery_Cuda, referenceHashPositionListsPerQuery, sizeof(size_t) * totalReferences, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(seqNumAgainstPerQuery_Cuda, seqNumAgainstPerQuery, sizeof(size_t) * NumOfSequencesQuery, cudaMemcpyHostToDevice, stream1);
		cudaMemcpyAsync(seqPosAgainstPerQuery_Cuda, seqPosAgainstPerQuery, sizeof(size_t) * NumOfSequencesQuery, cudaMemcpyHostToDevice, stream1);

		cudaMemcpyAsync(seqPosReferenceCharsPerQuery_Cuda, seqPosReferenceCharsPerQuery, sizeof(size_t) * totalReferences, cudaMemcpyHostToDevice, stream1);



		// Query
		size_t* seqLengthsQuery = new size_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, (sizeof(size_t) * NumOfSequencesQuery));
		size_t* SeqStartingPointsQuery = new size_t[NumOfSequencesQuery];//(size_t*)cemalloc(__LINE__, (sizeof(size_t) * NumOfSequencesQuery));

																		 // Query Seqs length
		size_t seqLenQuery = 0;
		for (size_t i = 0; i < NumOfSequencesQuery; i++) {
			// SequenceLengths
			size_t lenQuery = seqQuery[i + queriesToAnalyse_Offset]->size();

			if (lenQuery != 0)
			{
				if ((*seqQuery[i + queriesToAnalyse_Offset])[lenQuery - 1] == '\0')
					lenQuery--;// remove the \0
			}
			*(seqLengthsQuery + i) = lenQuery;

			// Starting points
			*(SeqStartingPointsQuery + i) = seqLenQuery;
			seqLenQuery += lenQuery;

		}


#ifdef show_debug

		cout << "charsQuery NumOfSequencesQuery: " << NumOfSequencesQuery << endl;
		cout.flush();
#endif
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery2 (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
		char* charsQuery_Cuda;
		cudaMallocMemSafe((void**)&charsQuery_Cuda, sizeof(char) * seqLenQuery, __LINE__, cudaDeviceID);
		cudaHostRegister(charsQuery_Cuda, sizeof(char)* seqLenQuery, cudaHostRegisterDefault);

#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif
		char* charsQuery = new char[seqLenQuery];//(char*)cemalloc(__LINE__, (sizeof(char)*seqLenQuery) + 1);
		//char* charsQuery = new char[seqLenQuery + 1];//(char*)cemalloc(__LINE__, (sizeof(char)*seqLenQuery) + 1);
		//charsQuery[seqLenQuery] = '\0';
		size_t qryCnt = 0;

		//char* ptrcharsQuery_Cuda = charsQuery_Cuda;

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery2.5 (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		vector<std::future<cudaError>> futuresCharsQueryCuda;
		if (false && useFuturesForReferenceLoading)
			futuresCharsQueryCuda.reserve(NumOfSequencesQuery);


		for (size_t i = 0; i < NumOfSequencesQuery; i++)
		{
			vector<char>* ptrSeqQuery = seqQuery[i + queriesToAnalyse_Offset];
			size_t size = ptrSeqQuery->size();

			/*future hier maakt het veel trager..mara terug gaan naar de niet - future ?*/

			if (false && useFuturesForReferenceLoading)
			{
				Asynchronise::wait_for_futures_on_full_vector(&futuresCharsQueryCuda, 400);
				futuresCharsQueryCuda.push_back(std::async(launch::async, cudaMemcpyAsync, charsQuery_Cuda + qryCnt, ptrSeqQuery->data(), size, cudaMemcpyHostToDevice, stream1));
			}
			else
			{
				// todo: do we need this?
				//if (size != 0)
				//{
					// todo: make sure we remove \0 during initialising
					//if ((*seqQuery[i + queriesToAnalyse_Offset])[size - 1] == '\0')
					//	size--;// remove the \0

					//if (size != 0)
					//{
			//cudaMemcpyAsync(ptrcharsQuery_Cuda, &(*seqQuery[i + queriesToAnalyse_Offset])[0], size, cudaMemcpyHostToDevice, stream1);
			//ptrcharsQuery_Cuda += qryCnt;
				cudaMemcpyAsync(charsQuery_Cuda + qryCnt, ptrSeqQuery->data(), size, cudaMemcpyHostToDevice, stream1);
				//cudaMemcpy(charsQuery_Cuda + qryCnt, &(*seqQuery[i + queriesToAnalyse_Offset])[0], size, cudaMemcpyHostToDevice);
			}

			qryCnt += size;

			//}



		}

		/*
				}
		*/



#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("seqsQuery3 (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

		///
		/// Write data to GPU memory
		///
		//cout << "Send data to GPU memory.." << endl;
		// create buffers on the device


		size_t* SeqStartingPointsQuery_Cuda;
#ifdef show_debug
		cout << "NumOfSequencesQuery: " << NumOfSequencesQuery << endl;
		cout.flush();
#endif
		cudaMallocMemSafe((void**)&SeqStartingPointsQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&SeqStartingPointsQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		cudaMemcpy(SeqStartingPointsQuery_Cuda, SeqStartingPointsQuery, sizeof(size_t) * NumOfSequencesQuery, cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(size_t) * NumOfSequencesQuery;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

		size_t* seqLengthsQuery_Cuda;
#ifdef show_debug
		cout << "NumOfSequencesQuery: " << NumOfSequencesQuery << endl;
		cout.flush();
#endif
		cudaMallocMemSafe((void**)&seqLengthsQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery, __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&seqLengthsQuery_Cuda, sizeof(size_t) * NumOfSequencesQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		cudaMemcpy(seqLengthsQuery_Cuda, seqLengthsQuery, sizeof(size_t) * NumOfSequencesQuery, cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(size_t) * NumOfSequencesQuery;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

#ifdef show_debug
		cout << "seqLenQuery: " << seqLenQuery << endl;
		cout.flush();
#endif


		//cudaMallocMemSafe((void**)&charsQuery_Cuda, sizeof(char) * seqLenQuery, __LINE__);
		////if (cudaSuccess != cudaMalloc((void**)&charsQuery_Cuda, sizeof(char) * seqLenQuery)) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		//cudaMemcpy(charsQuery_Cuda, charsQuery, sizeof(char) * seqLenQuery, cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(char) * seqLenQuery;
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

		size_t* seqLenQuery_Cuda;
#ifdef show_debug
		cout << "sizeof(long long): " << sizeof(long long) << endl;
		cout.flush();
#endif
		cudaMallocMemSafe((void**)&seqLenQuery_Cuda, sizeof(size_t), __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&seqLenQuery_Cuda, sizeof(size_t))) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		cudaMemcpy(seqLenQuery_Cuda, &seqLenQuery, sizeof(size_t), cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(long long);
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif



		size_t* ConsiderThreshold_Cuda;
		cudaMallocMemSafe((void**)&ConsiderThreshold_Cuda, sizeof(size_t), __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&ConsiderThreshold_Cuda, sizeof(size_t))) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		cudaMemcpy(ConsiderThreshold_Cuda, &ConsiderThreshold, sizeof(size_t), cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(size_t);
#ifdef show_debug
		cudaMemGetInfo(&mem_free_0, &mem_tot_0);
		cout << "Free memory: " << mem_free_0 << " Total: " << mem_tot_0 << endl;
#endif

		size_t* NumOfSequencesAgainst_Cuda;
		cudaMallocMemSafe((void**)&NumOfSequencesAgainst_Cuda, sizeof(size_t), __LINE__, cudaDeviceID);
		//if (cudaSuccess != cudaMalloc((void**)&NumOfSequencesAgainst_Cuda, sizeof(size_t))) throw runtime_error("CudaMalloc Error! At " + string(__FILE__) + ":" + to_string(__LINE__));
		cudaMemcpy(NumOfSequencesAgainst_Cuda, &NumOfSequencesReferences, sizeof(size_t), cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(size_t);



		int  mutexed_ScoreID_counter_Cuda_InitValue = 0;
		int* mutexed_ScoreID_counter_Cuda;
		cudaMallocMemSafe((void**)&mutexed_ScoreID_counter_Cuda, sizeof(int), __LINE__, cudaDeviceID);
		cudaMemcpy(mutexed_ScoreID_counter_Cuda, &mutexed_ScoreID_counter_Cuda_InitValue, sizeof(int), cudaMemcpyHostToDevice);
		totalSizeSend += sizeof(int);



		if (false && useFuturesForReferenceLoading)
			Asynchronise::wait_for_futures(&futuresCharsQueryCuda);
		cudaStreamSynchronize(stream1);


#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("cudaAlloc (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

		const clock_t begin_time = clock();

		// todo: check if we do not exceed Max dimension size of a grid size
		// k40: Max dimension size of a grid size(x, y, z) : (2147483647, 65535, 65535)
		// Just throw exception, analyser class will downshift # of refs/querys itself to try again

		/*

		NumOfSequencesQuery: 1
		longestAgainstPerQuery : 14776
		threadSize : 512
		(longestAgainstPerQuery / threadSize) + 1 : 29

		*/
#ifdef show_debug
		cout << "NumOfSequencesQuery:" << NumOfSequencesQuery << endl;
		cout << "longestAgainstPerQuery:" << longestAgainstPerQuery << endl;
		cout << "threadSize:" << threadSize << endl;
		cout.flush();
		cout << "(longestAgainstPerQuery / threadSize):" << (longestAgainstPerQuery / threadSize) << endl;
		cout.flush();
#endif
		dim3 dimGrid(NumOfSequencesQuery, (longestAgainstPerQuery / threadSize) + 1);
		//dim3 dimGrid(NumOfSequencesQuery, (NumOfSequencesAgainst / threadSize) + 1);
		dim3 dimBlock(threadSize, 1);
#ifdef show_debug
		cout << "Start GPU" << endl;
		cout.flush();
#endif




		//float* RESULT_SCORES;// = new float[sizeHighest];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);
		//cudaMallocManaged((void**)&RESULT_SCORES, sizeof(float) * sizeHighest);
		//memset(RESULT_SCORES, -2, sizeof(float) * sizeHighest);

		//cudaMemPrefetchAsync(RESULT_SCORES, sizeHighest * sizeof(float), cudaDeviceID, NULL);

		//cudaFuncSetCacheConfig(GetHighestMatrixScore, cudaFuncCachePreferL1);//slower
		//cudaFuncSetCacheConfig(GetHighestMatrixScore, cudaFuncCachePreferShared);
		multiThreading->SetGPUActive(true, cudaDeviceID);



		//Sum << <W, H >> > (dSum, size, d_index);



		GetHighestMatrixScore << <dimGrid, dimBlock >> >
			(charsQuery_Cuda, //char* seqQuerys
				//charsAgainst_Cuda,  //char* seqAgainst
				seqLengthsQuery_Cuda, //int* seqLengthQuery
				seqLengthsAgainst_Cuda, //int* seqLengthsAgainst
				SeqStartingPointsAgainst_Cuda, //int* seqStartingPointsAgainst
				SeqStartingPointsQuery_Cuda, //int* seqStartingPointsQuerys
				NumOfSequencesAgainst_Cuda,  //int* numAgainst
				//RESULT_SCORES, //float* Scores
				Scores_Cuda, //float* Scores
				ScoreQueryIDs_Cuda,
				ScoreReferenceIDs_Cuda,
				//PercentageMatches_Cuda,
				//seqLenQuery_Cuda,
				ConsiderThreshold_Cuda,
				//rowHitSizePerBlock_Cuda,
				//queryReferenceCombinationsFlat_Cuda,
				seqNumAgainstPerQuery_Cuda,
				seqPosAgainstPerQuery_Cuda,
				seqPosReferenceCharsPerQuery_Cuda,
				referenceIDsPerQuery_Cuda,
				referenceHashPositionListsPerQuery_Cuda,
				referenceHashPositionListsSizesPerQuery_Cuda,
				referenceHashRefPositionsPerQuery_Cuda,
				referenceHashRefPositionsPerQuery_LastPositionInRange_Cuda,
				referenceHashQueryPositionsPerQuery_Cuda,
				referenceHashQueryPositionsPerQuery_LastPositionInRange_Cuda,
				referenceCharsPerHash_Size_Cuda,
				referenceCharsPerHash_Head_Cuda,
				referenceCharsPerHash_Tail_Cuda,
				referenceCharsPerHash_Head_Size_Cuda,
				referenceCharsPerHash_Tail_Size_Cuda,

				//rowHit_Cuda,
				seqLenQuery_Cuda,
				mutexed_ScoreID_counter_Cuda);

		cudaError_t cudaerr = cudaDeviceSynchronize();
#ifdef show_debug
		cout << "Done GPU" << endl;
		cout.flush();
#endif
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("Execution of gpu(%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		float mSecPart = (float(clock() - begin_time) / (CLOCKS_PER_SEC / 1000));
		//float gbs = (totalSizeSend / (3.0 * 1024.0)) / mSecPart;
		//printf("GPU Engine %d ready (%f.2f GB/s) (\"%s\") (%f sec) (%.2f MB used)\n", cudaDeviceID, gbs, cudaGetErrorString(cudaerr), mSecPart / 1000, (totalSizeSend / (1024 * 1024)));

		// old log v4
		//printf("GPU Engine %d ready (\"%s\") (%f sec) (%.2f MB used)\n", cudaDeviceID, cudaGetErrorString(cudaerr), mSecPart / 1000, (totalSizeSend / (1024 * 1024)));
		cout << ">>>>>>>>>>>>>>>>> " << cudaGetErrorString(cudaerr) << endl;
		if (cudaerr != cudaSuccess)
		{
#ifdef show_debug
			cout << "Cuda was no succes.." << endl;
			cout.flush();
#endif
			//cudaDeviceReset();
			throw runtime_error(cudaGetErrorString(cudaerr));
		}
		//const clock_t time_begin_result_analysis = clock();



		//Read data from GPU memory
#ifdef show_debug
		cout << "Start reading GPU data RESULT_SCORES" << endl;
		cout << "sizeHighest: " << sizeHighest << endl;
		cout << "sizeof(float) * sizeHighest: " << sizeof(float) * sizeHighest << endl;
		cout << "Start reading GPU data" << endl;
		cout.flush();
#endif
		//float* RESULT_PERCENTAGE_MATCHES = new float[sizeHighest];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);


		//cudaMemcpy(RESULT_PERCENTAGE_MATCHES, PercentageMatches_Cuda, sizeof(float) * sizeHighest, cudaMemcpyDeviceToHost);


		//cudaMemcpy(RESULT_SCORES, Scores_Cuda, sizeof(float) * sizeHighest, cudaMemcpyDeviceToHost);

		int mutexed_ScoreID_counter_read;//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);

		cudaMemcpy(&mutexed_ScoreID_counter_read, mutexed_ScoreID_counter_Cuda, sizeof(int), cudaMemcpyDeviceToHost);
		cout << "><<<>><<" << (100.0 / sizeHighest) * mutexed_ScoreID_counter_read << "%" << endl << flush;

		float* RESULT_SCORES = new float[mutexed_ScoreID_counter_read];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);
		size_t* RESULT_SCORE_QIDS = new size_t[mutexed_ScoreID_counter_read];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);
		size_t* RESULT_SCORE_RIDS = new size_t[mutexed_ScoreID_counter_read];//(float*)cemalloc(__LINE__, sizeof(float) * sizeHighest);

		cudaHostRegister(RESULT_SCORES, sizeof(float) * mutexed_ScoreID_counter_read, cudaHostRegisterDefault);
		cudaHostRegister(RESULT_SCORE_QIDS, sizeof(size_t) * mutexed_ScoreID_counter_read, cudaHostRegisterDefault);
		cudaHostRegister(RESULT_SCORE_RIDS, sizeof(size_t) * mutexed_ScoreID_counter_read, cudaHostRegisterDefault);

		cudaMemcpyAsync(RESULT_SCORES, Scores_Cuda, sizeof(float) * mutexed_ScoreID_counter_read, cudaMemcpyDeviceToHost, stream1);
		cudaMemcpyAsync(RESULT_SCORE_QIDS, ScoreQueryIDs_Cuda, sizeof(size_t) * mutexed_ScoreID_counter_read, cudaMemcpyDeviceToHost, stream1);
		cudaMemcpyAsync(RESULT_SCORE_RIDS, ScoreReferenceIDs_Cuda, sizeof(size_t) * mutexed_ScoreID_counter_read, cudaMemcpyDeviceToHost, stream1);

		cout << "1" << endl << flush;
		cudaStreamSynchronize(stream1);

		//future<void> futureCudaDeviceReset = std::async(CudaDeviceResetAsync, cudaDeviceID);
		futureCudaDeviceReset[cudaDeviceID].push_back(std::async(CudaDeviceResetAsync, cudaDeviceID));

		vector<size_t> result_gathered_QueryIDs;
		vector<vector<size_t>> result_gathered_ReferenceIDs;
		vector<vector<float>> result_gathered_Scores;
		unordered_map<size_t, size_t> result_gathered_query_lookup;

		result_gathered_QueryIDs.reserve(mutexed_ScoreID_counter_read);
		result_gathered_ReferenceIDs.reserve(mutexed_ScoreID_counter_read);
		result_gathered_Scores.reserve(mutexed_ScoreID_counter_read);
		result_gathered_query_lookup.reserve(mutexed_ScoreID_counter_read);
		cout << "2" << endl << flush;

		// Transform GPU results into references per query list
		for (size_t scoreID_I = 0; scoreID_I < mutexed_ScoreID_counter_read; scoreID_I++)
		{
			if (RESULT_SCORES[scoreID_I] > -1)
			{
				//cout << "RESULT_SCORE_RIDS[scoreID_I]:" << RESULT_SCORE_RIDS[scoreID_I] << endl;
				auto queryInsert = result_gathered_query_lookup.insert({ RESULT_SCORE_QIDS[scoreID_I], result_gathered_QueryIDs.size() }); // Try to insert, if it fails; we know it's already in memory and get the location as return value
				if (!queryInsert.second)
				{
					result_gathered_ReferenceIDs[queryInsert.first->second].push_back(RESULT_SCORE_RIDS[scoreID_I]);
					result_gathered_Scores[queryInsert.first->second].push_back(RESULT_SCORES[scoreID_I]);
				}
				else
				{
					result_gathered_QueryIDs.push_back(RESULT_SCORE_QIDS[scoreID_I]);
					result_gathered_ReferenceIDs.push_back(vector<size_t>({ RESULT_SCORE_RIDS[scoreID_I] }));
					result_gathered_Scores.push_back(vector<float>({ RESULT_SCORES[scoreID_I] }));
				}
			}
		}


#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("read results from memory (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

#ifdef show_debug
		cout << "Copied data from GPU" << endl;
		cout.flush();
#endif

		multiThreading->SetGPUActive(false, cudaDeviceID);



		cout << "3" << endl << flush;





		unordered_map<float, float> scoreMaxBuffer;
		size_t numOfResultQueries = result_gathered_QueryIDs.size();
		//for (SEQ_ID_t qID = 0; qID < NumOfSequencesQuery; qID++)
		for (size_t scoreID_I = 0; scoreID_I < numOfResultQueries; scoreID_I++)
		{
			SEQ_ID_t qID = result_gathered_QueryIDs[scoreID_I];
			size_t numOfResultReferences = result_gathered_ReferenceIDs[scoreID_I].size();


			float HighestScore = -1;
			bool AgainstHitFound(false);


			size_t iFrom;
			if (cudaOptimisedKernelUsage)
				iFrom = (qID * ((longestAgainstPerQuery / threadSize) + 1) * threadSize);
			else
				iFrom = qID * longestAgainstPerQuery;



			//unordered_map<float, MatrixResult::Results> scores;
			vector<MatrixResult::ResultsScores> scores;
			scores.reserve(numOfResultReferences);



			for (size_t i = 0; i < numOfResultReferences; i++)
			{
				float relativeScore = (std::isnan(result_gathered_Scores[scoreID_I][i]) ? -1 : result_gathered_Scores[scoreID_I][i]);
				//float percentageMatch = 999; // RESULT_PERCENTAGE_MATCHES[i];
				size_t referenceID = result_gathered_ReferenceIDs[scoreID_I][i];// -iFrom; // (cudaOptimisedKernelUsage ? i - iFrom : i);


				//cout << " = " << referenceID << endl << flush;
				//cout << " = "<< sortedUsedOriginalReferenceIDs[referenceID] << endl << flush;

				referenceID = sortedUsedOriginalReferenceIDs[referenceID];
				//referenceID = sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinationsID.at(qID)[referenceID]];
				//referenceID = srelativeQueryReferenceCombinationsID.at(qID)[referenceID];
				//referenceID = sortedUsedOriginalReferenceIDs[referenceID];


				//cout << qID << " " << referenceID << endl << flush;
				//cout << relativeQueryReferenceCombinationsID.at(qID)[referenceID] << endl << flush;


				scores.emplace_back(relativeScore, (SEQ_ID_t)referenceID);
				//referenceID = sortedUsedOriginalReferenceIDs[relativeQueryReferenceCombinationsID.at(qID)[referenceID]];

				//auto scoreInsert = scores.insert({ score, MatrixResult::Results{ vector<SEQ_ID_t>({ (SEQ_ID_t)referenceID }), vector<float>({ percentageMatch }) } }); // Try to insert, if it fails; we know it's already in memory and get the location as return value
				//if (!scoreInsert.second)
				//{
				//	scoreInsert.first->second.referenceIDs.emplace_back((SEQ_ID_t)referenceID);
				//	scoreInsert.first->second.percentageMatch.emplace_back(percentageMatch);
				//}



				AgainstHitFound = true;

				if (relativeScore > HighestScore)
					HighestScore = relativeScore;
			}

			if (AgainstHitFound) {
				retVal.emplace_back(qID, HighestScore, &scores);
			}

			vector<MatrixResult::ResultsScores>().swap(scores);
			//unordered_map<float, MatrixResult::Results>().swap(scores);

		}





#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("read results (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
#ifdef show_debug
		cout << "Processed return data GPU" << endl;
		cout.flush();
#endif




		// clean up stream and registered
		//cudaStreamDestroy(stream1);

		//cudaHostUnregister(SeqStartingPointsAgainst);

		//cudaHostUnregister(seqLengthsAgainst);
		//cout << "41a" << endl << flush;
		//cudaHostUnregister(referenceIDsPerQuery_Host);
		//cudaHostUnregister(referenceHashQueryPositionsPerQuery);
		//cudaHostUnregister(referenceHashRefPositionsPerQuery);
		//cudaHostUnregister(referenceHashPositionListsSizesPerQuery);
		//cudaHostUnregister(referenceHashPositionListsPerQuery);
		//cudaHostUnregister(seqNumAgainstPerQuery);
		//cudaHostUnregister(seqPosAgainstPerQuery);
		//cudaHostUnregister(charsQuery_Cuda);

		//for (size_t rID = 0; rID < sortedUsedOriginalReferenceIDs.size(); rID++)
		//	cudaHostUnregister(ptrReferenceSequences[rID]);

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("clean up memory unregister (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif
		// clean up
		unordered_set<size_t>().swap(unRefIDs);
		vector<size_t>().swap(sortedUsedOriginalReferenceIDs);
		vector<vector<size_t>>().swap(relativeQueryReferenceCombinationsID);
		vector<vector<vector <Enum::Position>*>>().swap(relativeQueryReferenceCombinationsPositions);
		vector<size_t>().swap(vecReferenceTranslateTable);

#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("clean up memory  swap(%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

		cout << "6" << endl << flush;

		//delete[] seqLengthsAgainst;  // host alloc
		//delete[] SeqStartingPointsAgainst; // device alloc
		//delete[] seqNumAgainstPerQuery; // device alloc
		//delete[] seqPosAgainstPerQuery; // device alloc
		//delete[] referenceIDsPerQuery;  // device alloc
		//delete[] referenceHashPositionListsPerQuery;  // device alloc
		//delete[] referenceHashRefPositionsPerQuery; // device alloc
		//delete[] referenceHashQueryPositionsPerQuery; // device alloc
		//delete[] referenceHashPositionListsSizesPerQuery; // device alloc
		//delete[] seqLengthsQuery; // device alloc
		//delete[] SeqStartingPointsQuery; // device alloc
		//delete[] charsQuery; // device alloc
		////delete[] charsAgainst_Host; // device alloc
		//delete[] referenceIDsPerQuery_Host; // device alloc
		cout << "7" << endl << flush;

		//delete[] RESULT_SCORES;
		//delete[] RESULT_PERCENTAGE_MATCHES;
#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("clean up memory delete (%f sec) \n", mDebugTime / 1000);
		begin_time_against = clock();
#endif

		// Don't clean up cuda mem, high performance cost!
		/*vector<future<cudaError>> futureCudaFrees;

		futureCudaFrees.push_back(std::async(launch::async, cudaFree, SeqStartingPointsAgainst_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, seqLengthsAgainst_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, charsAgainst_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, referenceIDsPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, referenceHashQueryPositionsPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, referenceHashRefPositionsPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, referenceHashPositionListsSizesPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, referenceHashPositionListsPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, seqNumAgainstPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, seqPosAgainstPerQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, charsQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, Scores_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, ScoreQueryIDs_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, ScoreReferenceIDs_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, SeqStartingPointsQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, seqLengthsQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, seqLenQuery_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, ConsiderThreshold_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, NumOfSequencesAgainst_Cuda));
		futureCudaFrees.push_back(std::async(launch::async, cudaFree, mutexed_ScoreID_counter_Cuda));


		for(auto &fcf : futureCudaFrees)
			fcf.wait();
*/

//cudaDeviceReset();
		//futureCudaDeviceReset.wait();


#ifdef show_timings
		mDebugTime = (float(clock() - begin_time_against) / (CLOCKS_PER_SEC / 1000));
		printf("clean up memory  cudafree(%f sec) \n", mDebugTime / 1000);
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
#ifdef show_debug
	cout << "Cuda DONE!" << endl;
	cout.flush();
#endif
	return retVal;

}

