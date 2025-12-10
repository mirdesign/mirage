#include "MatrixCPU.h"

mutex mutCPUCalcThreads; // Mutex to safely store data which will be accessed by a multiThreaded method
mutex mutCPUResults; // Mutex to safely store data which will be accessed by a multiThreaded method
mutex mutClass; // Mutex to safely use this class. Only 1 usage at a time allowed


std::chrono::milliseconds CPU_SLEEP(100); // Used to pause adding new threads to queue



bool* HitBuffer_Data = new bool[1];
size_t HitBuffer_Size = 1;

///
/// Calculate score between query and reference
///
void CalculateScore(vector<char>* query, vector<Enum::HeadTailSequence>* HeadTailSequence, vector<Enum::Position>* combiStartPositions, vector<Enum::Position>* combiStartPositions_LastPositionInRange, int32_t threshold, MultiThreading* multiThreading, MatrixResult::Result* retVal, bool* HitBuffer, Sequences* sequences, SEQ_ID_t referenceID, size_t HashmapID)
{
	// Set to "no result" value -2
	retVal->Score = -2;

	// Check if we have work to do
	const size_t csp_size = combiStartPositions->size();
	if (csp_size == 0)
		return;

	// Hold while not all GPU are busy. This is used to give use all CPU power for the preperation fase for the GPU's, so GPU's don't stall.
	while (!multiThreading->AllGPUsActive())
		std::this_thread::sleep_for(CPU_SLEEP); 

	// Inititalise
	const size_t querySize = query->size();
	bool* rowHitCurrent = HitBuffer;
	size_t diagonalRepeatedTotalScore = 0;
	size_t diagonalTotalConcidered = 0;

	// Get first Head_Tail
	Enum::HeadTailSequence* htl;
	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode == Enum::Hashmap_MemoryUsageMode::Low)
		if (Analyser::SpeedMode != Enum::Analysis_Modes::UltraFast)
			htl = &(*HeadTailSequence)[0];

	// Get first start positions
	Enum::Position* ptrcombiStartPositions = &(*combiStartPositions)[0];
	Enum::Position* ptrcombiStartPositions_LastPositionInRange = &(*combiStartPositions_LastPositionInRange)[0];

	// Loop starting positions
	for (size_t cHashmapPositionsList = 0; cHashmapPositionsList < csp_size; cHashmapPositionsList++)
	{
		SEQ_POS_t rowStart = ptrcombiStartPositions->Query;
		SEQ_POS_t rowEnd = ptrcombiStartPositions_LastPositionInRange->Query;
		SEQ_POS_t row = rowEnd;
		SEQ_POS_t cntReferencePos = ptrcombiStartPositions_LastPositionInRange->Reference;

		// prevent miscalculations
		if (rowStart > rowEnd)
			continue;

		// The middle part
		size_t numMiddle = rowEnd - rowStart; // This range has been found as 1 succesive region of hashes in the hashmap

		// The left part
		size_t numHead = 0;

		// The Right part
		size_t numTail = 0;

		if (Analyser::SpeedMode != Enum::Analysis_Modes::UltraFast)
		{
			size_t index = 0;

			// The right part
			size_t refSize;
			size_t tailSize;
			char* tail;

			// Get info from huge or small buffer 
			if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::Low)
			{
				SEQ_POS_t refSeqBuffer_StartPos = sequences->referenceHashStreamPos[referenceID] - Hashmap::referenceSequences_First_HashStreamPos[HashmapID];
				refSize = sequences->referenceSequencesSizes[referenceID];
				tailSize = min((size_t)threshold, refSize - (size_t)ptrcombiStartPositions_LastPositionInRange->Reference);
				tail = &sequences->referenceSequences_Buffer[HashmapID][refSeqBuffer_StartPos + ptrcombiStartPositions_LastPositionInRange->Reference];
			}
			else
			{
				refSize = htl->SequenceLength;
				tailSize = htl->Tail_Size;
				tail = &htl->Tail[index];
			}

			// Walk out the tail
			char* qr = &(*query)[row];
			while (true)
			{

				if (row >= querySize || cntReferencePos >= refSize || index >= tailSize)
					break;

				if (*qr != *tail)
					break;

				// Step to the next position
				qr++;
				tail++;

				index++;
				row++;
				cntReferencePos++;
			}
			numTail = index;


			// The left part
			if (rowStart > 0)
			{
				SEQ_POS_t rowReverse = 1;
				char* head;
				size_t headSize;

				row = rowStart - 1;
				cntReferencePos = ptrcombiStartPositions->Reference;


				// Get info from huge or small buffer 
				if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::Low)
				{
					SEQ_POS_t refSeqBuffer_StartPos = sequences->referenceHashStreamPos[referenceID] - Hashmap::referenceSequences_First_HashStreamPos[HashmapID];
					headSize = min((size_t)threshold, (size_t)ptrcombiStartPositions->Reference);
					head = &sequences->referenceSequences_Buffer[HashmapID][refSeqBuffer_StartPos + ptrcombiStartPositions->Reference];
				}
				else
				{
					headSize = htl->Head_Size;
					head = &htl->Head[index];
				}


				index = headSize - 1;

				if (headSize > 0)
				{
					qr = &(*query)[row];

					// Walk out the head
					while (true)
					{
						if (*qr != *head)
						{
							numHead += rowReverse - 1;
							break;
						}

						if (row == 0 || cntReferencePos == 0 || index == 0)
						{
							numHead += rowReverse - 1;
							break;
						}

						// Step to the previous position
						qr--;
						head--;

						cntReferencePos--;
						row--;
						index--;

						rowReverse++;
					}
				}
			}
		}

		size_t totalMatched = numTail + numHead + numMiddle;

		// Did we pass the threshold?
		if (totalMatched >= (threshold))
		{
			// Append this head_tail pair score to the overall score
			diagonalTotalConcidered += totalMatched;
			memset(rowHitCurrent + (rowStart - numHead), true, totalMatched);
			diagonalRepeatedTotalScore += (totalMatched * (totalMatched + 1)) / 2;
		}

		// Go to next buffers
		if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode == Enum::Hashmap_MemoryUsageMode::Low)
			htl++;

		ptrcombiStartPositions++;
		ptrcombiStartPositions_LastPositionInRange++;
	}

	// Did we get a score to work with?
	if (diagonalTotalConcidered > 0)
	{
		SEQ_AMOUNT_t rowHitsFlat = 0;

		// Get the number of positions which let to the score
		bool* rhc = &rowHitCurrent[0];
		for (SEQ_ID_t i = 0; i < querySize; i++)
		{
			if (*rhc)
				rowHitsFlat++;
			rhc++;
		}

		// Save the score
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		retVal->percentageMatch = (100.0 / (float)querySize) * (float)rowHitsFlat;
		retVal->Score = ((score / 100.0) * retVal->percentageMatch);
	}
}

///
/// Used as parameter object for thread start
///
struct TGetScoreArgs {
	double vectorThreadID;
	SEQ_ID_t queryID_Offset;
	size_t queryIDs_CurrentRun;
	SEQ_ID_t queryReferenceCombi_refeferenceID;
	SEQ_ID_t queryReferenceCombi_refeferenceOffsetID;
	int ConsiderThreshold;

	bool* HitBuffer_start;
	vector<vector<char>*> *SeqQuery;
	Sequences *sequences;
	vector<vector<SEQ_ID_t>> *QueryReferenceCombinations;

	vector<vector<vector <Enum::HeadTailSequence>>>* MatchedReferencesPerQuery_HeadTailSequences;
	vector<vector<vector<Enum::Position>>> *QueryReferenceCombinationsCombiPositions_LastPositionInRange;
	vector<vector<vector<Enum::Position>>> *QueryReferenceCombinationsCombiPositions;
	vector<vector<vector<SEQ_POS_t>>> *QueryReferenceCombinationsRefPositions;
	vector<vector<vector<SEQ_POS_t>>> *QueryReferenceCombinationsQueryPositions;
	MultiThreading* multiThreading;

	vector<float>* scoresFromThreads;
	vector<float>* percentageMatchFromThreads;
};



///
/// Thread to calculate scores for queries
///
void TGetScore(TGetScoreArgs* args)
{

	TGetScoreArgs* a = (TGetScoreArgs*)args;

	// Determine range
	size_t ToQueryID = a->queryID_Offset + a->queryIDs_CurrentRun;
	size_t resultID = a->queryReferenceCombi_refeferenceOffsetID;

	// Get the part this thread was assigned to to use the reuseable hitbuffer
	bool* HitBuffer_Current = a->HitBuffer_start;

	// Loop queries
	for (size_t qID = a->queryID_Offset; qID < ToQueryID; qID++)
	{
		// Initiliaze current querie and meta information
		vector<SEQ_ID_t>* ptrR = &(*a->QueryReferenceCombinations)[qID];
		size_t rSize = ptrR->size();

		vector<char>* ptrQuery = (*a->SeqQuery)[qID];
		vector<Enum::HeadTailSequence>* ptrHTL = &(*a->MatchedReferencesPerQuery_HeadTailSequences)[qID][0];
		vector<Enum::Position>* ptrPos = &(*a->QueryReferenceCombinationsCombiPositions)[qID][0];
		vector<Enum::Position>* ptrPos_Last = &(*a->QueryReferenceCombinationsCombiPositions_LastPositionInRange)[qID][0];
		size_t querySize = ptrQuery->size();



		// Loop each reference this querie was matched to
		for (SEQ_ID_t rID = 0; rID < rSize; rID++)
		{
			// Reset hitbuffer
			if (rID != 0)
				memset(HitBuffer_Current, false, querySize);

			// Calculate the score
			MatrixResult::Result matrixResult;
			size_t HashmapID = Hashmap::GetHashmapID((*ptrR)[rID]);
			CalculateScore(
				ptrQuery,
				ptrHTL,
				ptrPos,
				ptrPos_Last,
				a->ConsiderThreshold,
				a->multiThreading,
				&matrixResult,
				HitBuffer_Current,
				args->sequences,
				(*ptrR)[rID],
				HashmapID);


			(*a->scoresFromThreads)[resultID] = matrixResult.Score;
			(*a->percentageMatchFromThreads)[resultID] = matrixResult.percentageMatch;

			// Go to the next reference and meta information
			resultID++;

			// Stop if we are done
			if (rID + 1 == rSize)
				break;

			ptrHTL++;
			ptrPos++;
			ptrPos_Last++;

		}

		HitBuffer_Current += querySize;
	}
	delete a;
}

///
/// Retrieve all results from the threads and create a scoring overview
///
void MatrixCPU::CreateResultsScores(vector<SEQ_ID_t>* queryReferenceCombination, vector<vector<Enum::HeadTailSequence>>* matchedReferencesPerQuery_HeadTailSequences, SEQ_ID_t queryID, vector<float>* scoresFromThreads, vector<vector<char>*>* seqQuery, Sequences* sequences, size_t threadIDOffset, MatrixResult* retVal)
{
	if (!Hashmap::Use_Experimental_LazyLoading) // todo: remove this Use_Experimental_LazyLoading option, it is always used due to advanced async code
		throw new invalid_argument("Use_Experimental_LazyLoading Not supported");
	
	// Setup return value
	*retVal = MatrixResult();
	retVal->QueryID = queryID;

	// Initialize
	float HighestScore = -1;
	size_t scoreSize = 0;

	// Start getting results from offset
	float* ptrSFT = &(*scoresFromThreads)[threadIDOffset];

	// Get required score size
	const size_t size_QFC = queryReferenceCombination->size();
	for (SEQ_ID_t rID = 0; rID < size_QFC; rID++)
	{
		if (*ptrSFT > -1)
			scoreSize++;
		ptrSFT++;
	}
	retVal->Scores.reserve(scoreSize);

	// Calculate relative scores
	size_t querySize = (*seqQuery)[queryID]->size();
	ptrSFT = &(*scoresFromThreads)[threadIDOffset];
	SEQ_ID_t* ptrQFC = &(*queryReferenceCombination)[0];
	for (SEQ_ID_t rID = 0; rID < size_QFC; rID++)
	{
		float relativeScore = -1;
		if (*ptrSFT > -1)
		{
			const size_t referenceSize = sequences->referenceSequencesSizes[*ptrQFC];
			const float relativeScore = MatrixResult::GetRelativeScore(querySize, referenceSize, *ptrSFT);

			if (relativeScore > HighestScore)
				HighestScore = relativeScore;

			retVal->Scores.emplace_back(relativeScore, *ptrQFC);
		}
		ptrSFT++;
		ptrQFC++;
	}
	retVal->HighestScore = HighestScore;
}

///
///	Resize vector
///
void vector_Resize_scoresFromThreads(vector<float>* toResize, size_t size)
{
	toResize->resize(size, -3);
}

///
/// Create hit buffer for all queries
///
void CreateHitBuffer(vector<SEQ_ID_t>* numRefPerQuery, vector<vector<char> *> * seqQuery, vector<vector<SEQ_ID_t>> * queryReferenceCombinations, bool ** HitBuffer, bool ** HitBuffer_current)
{
	// Initialise
	size_t HitBuffer_RequiredSize = 0;
	const size_t SQ_Size = seqQuery->size();
	numRefPerQuery->reserve(SQ_Size);
	vector<SEQ_ID_t>* ptrQRC = &(*queryReferenceCombinations)[0];
	vector<char>** ptrSQ = &(*seqQuery)[0];

	// Loop queries
	for (SEQ_ID_t qID = 0; qID < SQ_Size; qID++)
	{
		HitBuffer_RequiredSize += (*ptrSQ)->size();
		ptrSQ++;
	}

	// Delete old buffer and create bigger buffer
	if (HitBuffer_Size < HitBuffer_RequiredSize)
	{
		delete[] * HitBuffer;
		HitBuffer_Size = HitBuffer_RequiredSize * 2;
		*HitBuffer = new bool[HitBuffer_Size];
	}

	*HitBuffer_current = *HitBuffer;

	// Reset all values in buffer
	memset(*HitBuffer, false, HitBuffer_RequiredSize);
}


///
/// Calculate scores for queries and their matched references
///
vector<MatrixResult> MatrixCPU::CPUGetHighestScore(vector<vector<char>*>* seqQuery, Sequences* sequences, bool debug, int cudaDeviceID, int32_t ConsiderThreshold, vector<vector<SEQ_ID_t>>* queryReferenceCombinations,
	vector<vector<vector<Enum::Position>>>* queryReferenceCombinationsCombiPositions, vector<vector<vector<Enum::Position>>>* matchedReferencesPerQuery_CombiPositions_LastPositionInRange, vector<vector<vector <Enum::HeadTailSequence>>>*  matchedReferencesPerQuery_HeadTailSequences, int threadSize)
{
	// Only allow 1 instance
	mutClass.lock();

	// Keep time
	clock_t test_time = clock();
	auto begin = high_resolution_clock::now();

	// Initialise
	double threadID = 0;
	size_t referenceOffsetID = 0;
	vector<MatrixResult> retVal;
	size_t numOfScores = 0;
	bool* HitBuffer_current;
	vector<SEQ_ID_t> numRefPerQuery;

	// Create hit buffer to be reused. Saves a lot of memory acces if each thread is going to do this on its own
	future<void> future_CreateHitBuffer = std::async(launch::async, CreateHitBuffer, &numRefPerQuery, seqQuery, queryReferenceCombinations, &HitBuffer_Data, &HitBuffer_current);

	//
	// Prepare buffers which are required to store the results in
	//
	const size_t qrcSize = queryReferenceCombinations->size();
	vector<SEQ_ID_t>* ptrQRC = &(*queryReferenceCombinations)[0];

	for (SEQ_ID_t qID = 0; qID < qrcSize; qID++)
	{
		size_t cQRC_Size = ptrQRC->size();
		numOfScores += cQRC_Size;
		ptrQRC++;
	}

	future<void> future_Resize_scoresFromThreads = std::async(launch::async, vector_Resize_scoresFromThreads, &scoresFromThreads, numOfScores);
	future<void> future_Resize_percentageMatchFromThreads = std::async(launch::async, vector_Resize_scoresFromThreads, &percentageMatchFromThreads, numOfScores);

	vector<future<void>> fGetScores;
	fGetScores.reserve(seqQuery->size());






	//std::cout << "tGetScore start: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	// Wait for threads to finish
	future_CreateHitBuffer.wait();
	future_Resize_scoresFromThreads.wait();
	future_Resize_percentageMatchFromThreads.wait();


	//std::cout << "tGetScore prepare mem wait: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();


	// Create arguments for the new thread
	TGetScoreArgs *argsForThread_Base = new TGetScoreArgs();
	argsForThread_Base->SeqQuery = seqQuery;
	argsForThread_Base->sequences = sequences;
	argsForThread_Base->MatchedReferencesPerQuery_HeadTailSequences = matchedReferencesPerQuery_HeadTailSequences;
	argsForThread_Base->QueryReferenceCombinations = queryReferenceCombinations;
	argsForThread_Base->QueryReferenceCombinationsCombiPositions = queryReferenceCombinationsCombiPositions;
	argsForThread_Base->QueryReferenceCombinationsCombiPositions_LastPositionInRange = matchedReferencesPerQuery_CombiPositions_LastPositionInRange;
	argsForThread_Base->vectorThreadID = threadID;
	argsForThread_Base->ConsiderThreshold = ConsiderThreshold;
	argsForThread_Base->multiThreading = multiThreading;
	argsForThread_Base->scoresFromThreads = &scoresFromThreads;
	argsForThread_Base->percentageMatchFromThreads = &percentageMatchFromThreads;



	size_t qID_per_loop = max((size_t)1, seqQuery->size() / 50);
	//cout << "qID_per_loop: " << qID_per_loop << endl;
	// Loop the queries
	for (SEQ_ID_t qID_Offset = 0; qID_Offset < seqQuery->size(); qID_Offset += qID_per_loop)
	{
		if (qID_Offset + qID_per_loop > seqQuery->size())
			qID_per_loop = seqQuery->size() - qID_Offset;

		while (!multiThreading->AllGPUsActive())
			std::this_thread::sleep_for(CPU_SLEEP); // Hold while not all GPU are busy. This is used to give use all CPU power for the preperation fase for the GPU's, so GPU's don't stall.

		// Start new thread to calculate the scores for this query
		TGetScoreArgs *argsForThread;
		argsForThread = new TGetScoreArgs(*argsForThread_Base);
		argsForThread->HitBuffer_start = HitBuffer_current;
		argsForThread->queryID_Offset = qID_Offset;
		argsForThread->queryIDs_CurrentRun = qID_per_loop;
		argsForThread->queryReferenceCombi_refeferenceOffsetID = referenceOffsetID;

		fGetScores.emplace_back(std::async(TGetScore, argsForThread));

		// Do not increase pointers on end
		if (qID_Offset + qID_per_loop >= seqQuery->size())
			break;

		// Walk through the buffers to the new position for the next thread
		for (size_t qID = qID_Offset; qID < qID_Offset + qID_per_loop; qID++)
		{
			referenceOffsetID += (*queryReferenceCombinations)[qID].size();
			HitBuffer_current += (*seqQuery)[qID]->size();
		}
	}

	// Clean up and wait
	delete argsForThread_Base;
	Asynchronise::wait_for_futures(&fGetScores);

	//std::cout << "tGetScore calc: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	// Read out results and create scores
	retVal.resize(qrcSize);
	size_t threadIDOffset = 0;
	ptrQRC = &(*queryReferenceCombinations)[0];
	vector<vector<Enum::HeadTailSequence>>* ptrQRC_HT = &(*matchedReferencesPerQuery_HeadTailSequences)[0];
	MatrixResult* ptrRetVal = &retVal[0];
	for (SEQ_ID_t qID = 0; qID < qrcSize; qID++)
	{
		CreateResultsScores(ptrQRC, ptrQRC_HT, qID, &scoresFromThreads, seqQuery, sequences, threadIDOffset, ptrRetVal);
		threadIDOffset += ptrQRC->size();
		ptrQRC++;
		ptrQRC_HT++;
		ptrRetVal++;
	}

	//std::cout << "tGetScore create: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();
	mutClass.unlock();

	return retVal;
}

MatrixCPU::MatrixCPU(Display * d, MultiThreading *m)
{
	display = d;
	multiThreading = m;
}

MatrixCPU::~MatrixCPU()
{
	//vector<float>().swap(scoresFromThreads);
	//vector<float>().swap(percentageMatchFromThreads);
}
