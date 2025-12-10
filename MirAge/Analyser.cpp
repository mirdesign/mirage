#include "Analyser.h"
bool ddebug(false);
//#define show_timings
#define show_cuda_crash


#include <numeric>
vector<future<void>> futureCleancombiPosition;
bool UseTrashCan(false);
Enum::Analysis_Modes Analyser::SpeedMode = Enum::Analysis_Modes::Fast;
bool Analyser::bUseReverseComplementInQuery = true;
size_t combiPosition_Size_Highest = 500;


Configuration *Analyser::configuration;



#include <iomanip>


#ifdef __linux__



size_t Analyser::GetMemMachineFreeB() {
	try
	{
		string key;
		ifstream f("/proc/meminfo");
		while (f >> key) {
			if (key == "MemAvailable:") {
				size_t mem;
				if (f >> mem) {
					return mem * 1000; // * 1000 for kb to b
				}
				else {
					return 0;
				}
			}
			// ignore rest of the line
			f.ignore(numeric_limits<streamsize>::max(), '\n');
		}
	}
	catch (...) {}
	return 0;
}




int Analyser::GetMemFreePercentage()
{
	struct sysinfo myinfo;
	unsigned long total_bytes;
	unsigned long free_bytes;
	sysinfo(&myinfo);
	total_bytes = myinfo.mem_unit * myinfo.totalram;
	return (100.0 / total_bytes) * Analyser::GetMemMachineFreeB();
}



unsigned long Analyser::GetMemMachineGB()
{
	struct sysinfo myinfo;
	unsigned long total_bytes;
	const double gigabyte = 1024 * 1024 * 1024;
	sysinfo(&myinfo);
	total_bytes = myinfo.totalram / gigabyte;
	return total_bytes;
}




void Analyser::ShowMemUsage() {
	//struct sysinfo {
	//	long uptime;             /* Seconds since boot */
	//	unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
	//	unsigned long totalram;  /* Total usable main memory size */
	//	unsigned long freeram;   /* Available memory size */
	//	unsigned long sharedram; /* Amount of shared memory */
	//	unsigned long bufferram; /* Memory used by buffers */
	//	unsigned long totalswap; /* Total swap space size */
	//	unsigned long freeswap;  /* Swap space still available */
	//	unsigned short procs;    /* Number of current processes */
	//	unsigned long totalhigh; /* Total high memory size */
	//	unsigned long freehigh;  /* Available high memory size */
	//	unsigned int mem_unit;   /* Memory unit size in bytes */
	//	char _f[20 - 2 * sizeof(long) - sizeof(int)];
	//	/* Padding to 64 bytes */
	//};


	struct sysinfo myinfo;
	unsigned long total_bytes;
	unsigned long free_bytes;
	unsigned long used_bytes;
	unsigned long shared_bytes;
	unsigned long bufferram_bytes;
	unsigned long totalhigh_bytes;
	unsigned long freehigh_bytes;

	sysinfo(&myinfo);

	total_bytes = myinfo.mem_unit * myinfo.totalram;
	free_bytes = myinfo.mem_unit * myinfo.freeram;
	used_bytes = total_bytes - free_bytes;
	shared_bytes = myinfo.mem_unit - myinfo.sharedram;
	bufferram_bytes = myinfo.mem_unit - myinfo.bufferram;
	totalhigh_bytes = myinfo.mem_unit - myinfo.totalhigh;
	freehigh_bytes = myinfo.mem_unit - myinfo.freehigh;




	printf("total usable main memory is %lu B, %lu MB, %lu GB\n", total_bytes, total_bytes / 1024 / 1024, total_bytes / 1024 / 1024 / 1024);
	printf("total free main memory is %lu B, %lu MB, %lu GB\n", free_bytes, free_bytes / 1024 / 1024, free_bytes / 1024 / 1024 / 1024);
	printf("total used main memory is %lu B, %lu MB, %lu GB\n", used_bytes, used_bytes / 1024 / 1024, used_bytes / 1024 / 1024 / 1024);
	printf("total sharedram main memory is %lu B, %lu MB, %lu GB\n", shared_bytes, shared_bytes / 1024 / 1024, shared_bytes / 1024 / 1024 / 1024);
	printf("total bufferram main memory is %lu B, %lu MB, %lu GB\n", bufferram_bytes, bufferram_bytes / 1024 / 1024, bufferram_bytes / 1024 / 1024 / 1024);
	printf("total totalhigh main memory is %lu B, %lu MB, %lu GB\n", totalhigh_bytes, totalhigh_bytes / 1024 / 1024, totalhigh_bytes / 1024 / 1024 / 1024);
	printf("total freehigh main memory is %lu B, %lu MB, %lu GB\n", freehigh_bytes, freehigh_bytes / 1024 / 1024, freehigh_bytes / 1024 / 1024 / 1024);


}

#else



size_t Analyser::GetMemMachineFreeB()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullAvailPhys;
}

int Analyser::GetMemFreePercentage()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return 100 - status.dwMemoryLoad;
}


unsigned long Analyser::GetMemMachineGB()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	const double gigabyte = 1024 * 1024 * 1024;

	return (unsigned long)status.ullTotalPhys / gigabyte;
}
#endif


atomic<float> debug_timing_1;
atomic<float> debug_timing_2a;
atomic<float> debug_timing_2;
atomic<float> debug_timing_3;
atomic<float> debug_timing_4;
atomic<float> debug_timing_5;
atomic<float> debug_timing_6;
atomic<float> debug_timing_7;
atomic<float> debug_timing_8;
atomic<float> debug_timing_9;
atomic<float> debug_timing_10;
atomic<float> debug_timing_11;

void Analyser::LogResultsToScreen(int InputID, vector<SEQ_ID_t>* Results, double Score, float relativeMatrixScore, Sequences* sequences, LogToScreen* logResultsToScreen)
{

	string References = "";
	if (Results->size() == 0)
	{
		References = "-";
	}
	else
	{
		// Write 1 or multiple header hits
		for (SEQ_ID_t hsrIDs = 0; hsrIDs < Results->size(); hsrIDs++)
		{
			SEQ_ID_t rID = (*Results)[hsrIDs];
			string referenceHeader;
			if (Hashmap::Use_Experimental_LazyLoading)
			{
				vector<char> s;
				Sequences::GetLazyContentWithCache(&sequences->referenceHeaders, &sequences->referenceHeaderSizes, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceHeadersHashStreamPos, rID, &s);
				referenceHeader = string(s.begin(), s.end());
			}
			else
				referenceHeader = string(sequences->referenceHeaders[rID]->begin(), sequences->referenceHeaders[rID]->end());

			if (References != "")
				References += ";";
			References += to_string(rID);
			//References += ";";
			//References += to_string(Results->percentageMatch[hsrIDs]);
			References += ";";
			References += referenceHeader;
			References += ";";
		}
	}


	(*logResultsToScreen)
		<< InputID
		<< ";"
		<< string(&(*sequences->inputHeaders[InputID])[0], sequences->inputHeaders[InputID]->size())
		<< ";"
		<< relativeMatrixScore
		<< ";"
		<< References
		<< endl;

}



///
/// Write an input sequence hit on a reference sequence to the log file
///
void Analyser::LogResultsToFileManager(int InputID, vector<SEQ_ID_t> Results, double Score, float relativeMatrixScore, Sequences* sequences, vector<LogManager*> logManagers)
{


	string result = "";
	string sBuffer;
	sBuffer.reserve(3000);


	sBuffer += to_string(InputID);
	sBuffer += LogManager::Output_ColumnSeperator;
	sBuffer.append(sequences->inputHeaders[InputID]->begin(), sequences->inputHeaders[InputID]->end() - 1);

	sBuffer += LogManager::Output_ColumnSeperator;
	sBuffer += to_string(relativeMatrixScore);
	sBuffer += LogManager::Output_ColumnSeperator;

	if (Results.size() == 0)
	{
		sBuffer += "?";
		sBuffer += LogManager::Output_ColumnSeperator;
	}
	else
	{

		// Get TaxIDOcc
		unordered_map<SEQ_ID_t, size_t> TaxIDOccurences;
		for (SEQ_ID_t hsrIDs = 0; hsrIDs < Results.size(); hsrIDs++)
		{
			SEQ_ID_t taxID = sequences->NCBITaxIDs[(Results)[hsrIDs]];
			TaxIDOccurences[taxID]++;
		}

		// sort by occurences
		vector<int32_t>* ptrNCBITaxIDs = &(sequences->NCBITaxIDs);

		const size_t ptrNCBITaxIDs_Size = ptrNCBITaxIDs->size();
		std::stable_sort(
			Results.begin(),
			Results.end(),
			[&TaxIDOccurences, ptrNCBITaxIDs, &ptrNCBITaxIDs_Size](SEQ_ID_t const& a, SEQ_ID_t const& b) {
			SEQ_ID_t taxIDA;
			SEQ_ID_t taxIDB;

			taxIDA = (*ptrNCBITaxIDs)[a];
			taxIDB = (*ptrNCBITaxIDs)[b];

			if (taxIDA == -1)
				return true;

			if (taxIDB == -1)
				return false;

			if (taxIDA == taxIDB) {
				return false;
			}
			if (TaxIDOccurences[taxIDA] == TaxIDOccurences[taxIDB]) {
				return false;
			}
			if (TaxIDOccurences[taxIDA] > TaxIDOccurences[taxIDB]) {
				return true;
			}
			if (TaxIDOccurences[taxIDA] < TaxIDOccurences[taxIDB]) {
				return false;
			}
			return taxIDA < taxIDB;

		});


		// Write TaxIDs
		int32_t currentTaxID = (*ptrNCBITaxIDs)[Results[0]];
		size_t maxOccurence = TaxIDOccurences[currentTaxID];
		bool ShowLargestOccurenceOnly(true);

		for (SEQ_ID_t hsrIDs = 0; hsrIDs < Results.size(); hsrIDs++)
		{

			SEQ_ID_t rID = (Results)[hsrIDs];
			const int32_t* NTI = &(*ptrNCBITaxIDs)[rID];

			if (hsrIDs > 0)
				if (*NTI == currentTaxID)
					continue;

			size_t numOccurence = TaxIDOccurences[*NTI];
			if (ShowLargestOccurenceOnly)
				if (numOccurence != maxOccurence)
					break;

			currentTaxID = *NTI;

			if (hsrIDs != 0)
				sBuffer += LogManager::Output_DuplicateSeperator;

			sBuffer += to_string(currentTaxID);
		}
		sBuffer += LogManager::Output_ColumnSeperator;

		// Write Occurences
		for (SEQ_ID_t hsrIDs = 0; hsrIDs < Results.size(); hsrIDs++)
		{

			SEQ_ID_t rID = (Results)[hsrIDs];
			const int32_t* NTI = &(*ptrNCBITaxIDs)[rID];

			if (hsrIDs > 0)
				if (*NTI == currentTaxID)
					continue;

			size_t numOccurence = TaxIDOccurences[*NTI];
			if (ShowLargestOccurenceOnly)
				if (numOccurence != maxOccurence)
					break;

			currentTaxID = *NTI;

			if (hsrIDs != 0)
				sBuffer += LogManager::Output_DuplicateSeperator;

			//sBuffer += to_string(rID);
			sBuffer += to_string(numOccurence);
		}
		sBuffer += LogManager::Output_ColumnSeperator;


		// Write Accession IDs (Or just headers from the fasta)
		for (SEQ_ID_t hsrIDs = 0; hsrIDs < Results.size(); hsrIDs++)
		{
			SEQ_ID_t rID = (Results)[hsrIDs];
			const int32_t* NTI = &(*ptrNCBITaxIDs)[rID];
			size_t numOccurence = TaxIDOccurences[*NTI];
			if (ShowLargestOccurenceOnly)
				if (numOccurence != maxOccurence)
					break;

			if (hsrIDs != 0)
				sBuffer += LogManager::Output_DuplicateSeperator;

			vector<char> s;
			Sequences::GetLazyContentWithCache(&sequences->referenceHeaders, &sequences->referenceHeaderSizes, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceHeadersHashStreamPos, rID, &s);
			sBuffer.append(s.begin(), s.end() - 1);
		}
	}

	// Log buffer to all log managers
	for (auto& l : logManagers)
		l->Log(sBuffer, true);
}

void Analyser::LogMatrixResults(vector<MatrixResult>* matrixResults, SEQ_ID_t inputIDOffset, vector<SEQ_ID_t>* queryIDs, MultiThreading* multiThreading, Sequences* sequences, Output_Modes* Output_Mode, LogToScreen* logResultsToScreen, LogManager* logManagerHighSensitivity, LogManager* logManagerHihhPrecision, LogManager* logManagerBalanced, LogManager* logManagerRaw, bool Output_HighestScoredResultsOnly)
{
	// Log each matrix result
	vector<future<void>> futures;
	futures.reserve(matrixResults->size());

	for (size_t j = 0; j < matrixResults->size(); j++)
	{
		MatrixResult* mr = &(matrixResults->at(j));

		if (mr->Scores.size() == 0 || mr->HighestScore == 0)
			continue;

		//Asynchronise::wait_for_futures_on_full_vector(&futures, min(multiThreading->maxCPUThreads, Asynchronise::__FUTURES_MAX_SIZE_LOG_TO_FILE));
		//futures.push_back(std::async(LogMatrixResult, mr, inputIDOffset, queryIDs, multiThreading, sequences, Output_Mode, logResultsToScreen, logManagerHighSensitivity, logManagerHihhPrecision, logManagerBalanced, logManagerRaw, Output_HighestScoredResultsOnly));
		LogMatrixResult(mr, inputIDOffset, queryIDs, multiThreading, sequences, Output_Mode, logResultsToScreen, logManagerHighSensitivity, logManagerHihhPrecision, logManagerBalanced, logManagerRaw, Output_HighestScoredResultsOnly);

	}

	//Asynchronise::wait_for_futures(&futures);
}


void Analyser::LogMatrixResult(MatrixResult* mr, SEQ_ID_t inputIDOffset, vector<SEQ_ID_t>* queryIDs, MultiThreading* multiThreading, Sequences* sequences, Output_Modes* Output_Mode, LogToScreen* logResultsToScreen, LogManager* logManagerHighSensitivity, LogManager* logManagerHighPrecision, LogManager* logManagerBalanced, LogManager* logManagerRaw, bool Output_HighestScoredResultsOnly)
{
	// Sort scores descending
	sort(mr->Scores.begin(), mr->Scores.end(), [&](const MatrixResult::ResultsScores &left, const MatrixResult::ResultsScores &right) { return left.score > right.score; });

	// Group scores
	vector < pair<float, vector<SEQ_ID_t>>> sortedScores;
	sortedScores.emplace_back(mr->Scores[0].score, vector<SEQ_ID_t>());
	pair<float, vector<SEQ_ID_t>>* currentSS = &sortedScores.back();
	float currentScore = currentSS->first;
	for (MatrixResult::ResultsScores scoreItem : mr->Scores)
	{
		if (scoreItem.score != currentScore)
		{
			if (Output_HighestScoredResultsOnly)
				break;
			currentScore = scoreItem.score;
			sortedScores.emplace_back(currentScore, vector<SEQ_ID_t>());
			currentSS = &sortedScores.back();
		}

		// Add current refID to the current list with the same scores
		currentSS->second.emplace_back(scoreItem.referenceID);
	}


	// Show result to screen
	if (*Output_Mode == Output_Modes::ResultsNoLog || *Output_Mode == Output_Modes::Results)
	{
		multiThreading->mutexWriteMatrixResultLog.lock();

		for (pair<float, vector<SEQ_ID_t>> scoreItem : sortedScores)
		{
			LogResultsToScreen((*queryIDs)[mr->QueryID], &(scoreItem.second), mr->HighestScore, scoreItem.first, sequences, logResultsToScreen);

			if (Output_HighestScoredResultsOnly)
				break;

		}
		multiThreading->mutexWriteMatrixResultLog.unlock();

	}

	// Save results to file
	if (configuration->Output_Mode != Output_Modes::ResultsNoLog)
	{
		vector<future<void>> futures;
		futures.reserve(sortedScores.size());


		for (pair<float, vector<SEQ_ID_t>> scoreItem : sortedScores)
		{

			//Asynchronise::wait_for_futures_on_full_vector(&futures, max(1, min(multiThreading->maxCPUThreads - 3, Asynchronise::__FUTURES_MAX_SIZE_LOG_TO_FILE - 3)));

			vector<LogManager*> logManagers;

			// High sensitivity
			if (scoreItem.first >= LogManager::LogThreshold_HighSensitivity)
				logManagers.push_back(logManagerHighSensitivity);

			// High Precision
			if (scoreItem.first >= LogManager::LogThreshold_HighPrecision)
				logManagers.push_back(logManagerHighPrecision);

			// Balanced
			if (scoreItem.first >= LogManager::LogThreshold_Balanced)
				logManagers.push_back(logManagerBalanced);

			// Raw
			logManagers.push_back(logManagerRaw);

			LogResultsToFileManager((*queryIDs)[mr->QueryID], (scoreItem.second), mr->HighestScore, scoreItem.first, sequences, logManagers);
			//futures.push_back(std::async(LogResultsToFileManager, (*queryIDs)[mr->QueryID], (scoreItem.second), mr->HighestScore, scoreItem.first, sequences, logManagers));


			//// Create percentage overview 
			//if (mr->ReferenceIDs.size() != 0)
			//{
			//	ReferenceMatrixHighestFound_FirstHit.push_back(referenceHeaders[mr->ReferenceIDs[0]]);
			//	ReferenceMatrixHighestFound_MultipleHits.push_back(Overview_ReferencesToSortedName(mr->ReferenceIDs));
			//}

			if (Output_HighestScoredResultsOnly)
				break;

		}
		//Asynchronise::wait_for_futures(&futures);

	}
}


//std::vector<std::future<void>> pending_futures;
//void FutureDelete(Analyser::Analysis_Arguments* del)
//{
//	delete del;
//}
mutex mTrashCan;
mutex mTrashCan_Analysis_Arguments;
vector<vector<pair<pair<SEQ_ID_t, Enum::Position>*, pair<SEQ_ID_t, Enum::Position>*>>> trashcan;
vector<Analyser::Analysis_Arguments*> trashcan_Analysis_Arguments;
vector<vector<Analyser::References_ToLoad>*> trashcan_Reference_ToLoad;
vector<vector<hashmap_HeadTailBuffer>*> trashcan_HashmapHeadTailBuffers;
vector<vector<unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>>> trashcan_MatchedHashStreamPos;
vector<vector<byte*>> trashcan_buffer_total_start;
vector<vector<vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>>> trashcan_loadedStreamIterators;
vector<Analyser::PrepareGPU_Arguments*> trashcan_PrepareGPU_Arguments;


///

/// Calculate highest MirAge score using CPU
///


void* Analyser::TGetHighestScoreUsingCPU(void* args)
{
	Analysis_Arguments* a = (Analysis_Arguments*)args;

	int deviceID = a->DeviceID;
	MatrixCPU mc(a->display, a->multiThreading);

	auto begin = high_resolution_clock::now();


	while (a->runningThreadsGPUPreAnalysis > 0)
	{
		// Just give the CPU some rest..
		a->multiThreading->SleepGPU();
	}

	// Show Timings
	if (false)
	{
		std::cout << "debug_timing_1: " << debug_timing_1 << "\n";
		std::cout << "debug_timing_2a: " << debug_timing_2a << "\n";
		std::cout << "debug_timing_2: " << debug_timing_2 << "\n";
		std::cout << "debug_timing_3: " << debug_timing_3 << "\n";
		std::cout << "debug_timing_4: " << debug_timing_4 << "\n";
		std::cout << "debug_timing_5: " << debug_timing_5 << "\n";
		std::cout << "debug_timing_6: " << debug_timing_6 << "\n";
		std::cout << "debug_timing_7: " << debug_timing_7 << "\n";
		std::cout << "debug_timing_8: " << debug_timing_8 << "\n";
		std::cout << "debug_timing_9: " << debug_timing_9 << "\n";
		std::cout << "debug_timing_10: " << debug_timing_10 << "\n";
		std::cout << "debug_timing_11: " << debug_timing_11 << "\n";
	}

	debug_timing_1 = 0;
	debug_timing_2a = 0;
	debug_timing_2 = 0;
	debug_timing_3 = 0;
	debug_timing_4 = 0;
	debug_timing_5 = 0;
	debug_timing_6 = 0;
	debug_timing_7 = 0;
	debug_timing_8 = 0;
	debug_timing_9 = 0;
	debug_timing_10 = 0;
	debug_timing_11 = 0;

	//std::cout << "runningThreadsGPUPreAnalysis waiting took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();
	if (Analyser::SpeedMode != Enum::Analysis_Modes::UltraFast)
		PreloadReferences_InQueryList(
			a->sequences,
			&a->matchedReferencesPerQuery,
			&a->matchedReferencesPerQuery_CombiPositions,
			&a->matchedReferencesPerQuery_CombiPositions_LastPositionInRange,
			&a->matchedReferencesPerQuery_HeadTailSequences,
			&a->sequences->referenceHashStreamPos,
			a->ConsiderThreshold,
			&a->ReferenceSequences_Buffer);



	//std::cout << "PreloadReferences_InQueryList took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	// Start analysis
	clock_t test_time = clock();

	// Get highest scores
	vector<MatrixResult> matrixResults = mc.CPUGetHighestScore(&(a->querySequences), a->sequences, true, deviceID, a->ConsiderThreshold, &(a->matchedReferencesPerQuery), &(a->matchedReferencesPerQuery_CombiPositions), &(a->matchedReferencesPerQuery_CombiPositions_LastPositionInRange), &a->matchedReferencesPerQuery_HeadTailSequences, -1);

	//std::cout << "TGETHIGHEST calc results took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	//matrixResults.erase(matrixResults.begin() + (matrixResults.size() / 2), matrixResults.end());

	if (bUseReverseComplementInQuery)
	{
		// If we do not work with references with ReverseComplement added, use the reverseComplement of the query instead.
		sort(matrixResults.begin(), matrixResults.end(), [&](const MatrixResult& i, const MatrixResult& j) {return i.QueryID < j.QueryID; });
		for (size_t i = 0; i < matrixResults.size(); i = i + 2)
		{

			size_t rc_i = i + 1;
			MatrixResult* ptrMR = &matrixResults[i];
			MatrixResult* ptrMR_RC = &matrixResults[rc_i];

			if (ptrMR->HighestScore < ptrMR_RC->HighestScore)
			{
				*ptrMR = *ptrMR_RC;
				ptrMR->QueryID = i;
			}
			ptrMR_RC->HighestScore = 0;
			//ptrMR_RC->Scores.clear();
		}
	}



	//std::cout << "TGETHIGHEST reverse compl results took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	//float spentTime = (float)duration<double, ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();


	// Unlock this CPU, so post processing takes places while the CPU can get some new data to work on
	a->multiThreading->mutGPUResults.lock();
	a->hardware->DeviceQueueSize[a->DeviceID]--;

	//a->hardware->TryAutoIncreaseBlocksize(deviceID, spentTime, a->PrepareSpentTime, a->queryIDs.size(), a->multiThreading->maxCPUThreads);

	(*a->TotalQuerySequencesAnalysed) += a->numOfQueriesPigyBacked;
	a->multiThreading->mutGPUResults.unlock();

	// Release this thread
	a->multiThreading->mutMainMultithreads.lock();
	a->multiThreading->runningThreadsAnalysis--;
	a->multiThreading->threadsFinished.push_back(a->threadID);

	// Unlock this device
	a->multiThreading->mutexAnalysis[deviceID] = false;
	a->multiThreading->mutMainMultithreads.unlock();

	//std::cout << "TGETHIGHEST unlock took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();


	a->logManagerHighSensitivity->BufferOn();
	a->logManagerHighPrecision->BufferOn();
	a->logManagerBalanced->BufferOn();
	a->logManagerRaw->BufferOn();

	if (matrixResults.size() > 0)
		Analyser::LogMatrixResults(&matrixResults, -1, &a->queryIDs, a->multiThreading, a->sequences, a->Output_Mode, a->logResultsToScreen, a->logManagerHighSensitivity, a->logManagerHighPrecision, a->logManagerBalanced, a->logManagerRaw, a->Output_HighestScoredResultsOnly);

	a->logManagerHighSensitivity->Flush();
	a->logManagerHighPrecision->Flush();
	a->logManagerBalanced->Flush();
	a->logManagerRaw->Flush();

	a->multiThreading->runningLogResults--;
	//std::cout << "TGETHIGHEST log took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	if (matrixResults.size() > 0)
	{
		matrixResults.clear();
		matrixResults.shrink_to_fit();

		vector<MatrixResult>().swap(matrixResults);
	}


	if (UseTrashCan)// && Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::High)
	{
		mTrashCan_Analysis_Arguments.lock();
		trashcan_Analysis_Arguments.emplace_back(a);
		mTrashCan_Analysis_Arguments.unlock();
	}
	else
	{
		delete[] a->ReferenceSequences_Buffer;

		vector<vector<vector <Enum::Position>>>().swap(a->matchedReferencesPerQuery_CombiPositions_LastPositionInRange);
		vector<vector<vector <Enum::Position>>>().swap(a->matchedReferencesPerQuery_CombiPositions);
		vector<vector<vector <Enum::HeadTailSequence>>>().swap(a->matchedReferencesPerQuery_HeadTailSequences);
		vector<vector<vector <SEQ_POS_t>>>().swap(a->matchedReferencesPerQuery_RefPositions);
		vector<vector<vector <SEQ_POS_t>>>().swap(a->matchedReferencesPerQuery_QueryPositions);

		delete a;
	}

	return NULL;
}


void Analyser::AppendToCombiPosition(vector<vector<Enum::Position>>* combiPosition, size_t* cntAppendToCombiPosition, size_t OrderNumber, size_t iCP, SEQ_ID_t hashmapHitPos, SEQ_POS_t currentCharPosition_InQuery)
{

	MultiThreading m;
	while (true)
	{
		if (*cntAppendToCombiPosition == OrderNumber)
		{
			(*combiPosition)[iCP].emplace_back(hashmapHitPos, currentCharPosition_InQuery);
			*cntAppendToCombiPosition = (*cntAppendToCombiPosition) + 1;
			return;
		}

		m.SleepGPU();
	}
}


///
/// Calculate highest MirAge score using GPU (CUDA/OpenCL)
///
void* Analyser::TGetHighestScoreUsingGPU(void* args)
{

	Analysis_Arguments* a = (Analysis_Arguments*)args;

	a->multiThreading->mutexCUDARunning[a->hardware->DeviceID[a->DeviceID]].lock();


	MatrixGPU* GPUEngine = a->GPUEngine;
	int DeviceID = a->DeviceID;
	SEQ_AMOUNT_t numOfQueriesBridged = a->numOfQueriesPigyBacked;
	double vectorThreadID = a->threadID;
	vector<MatrixResult> matrixResults;

	// Used to decrease blocksize on failure of the GPU code
	int GPUFailure_DecreasePercentage_Query = 20;
	int GPUFailure_DecreasePercentage_Reference = 50;
	bool queriesToAnalyse_Available(true);
	bool queriesToAnalyse_SizePerAnalysis_LowestReached(false);
	bool querySizeLocked_ResizeReference(false);
	bool LowerReferenceEnabled(false); // Todo: Turn off for now, since multiple results per query are not yet grouped together. Add this before enabling this setting again.
	SEQ_ID_t queriesToAnalyse_Offset = 0;
	SEQ_AMOUNT_t queriesToAnalyse_SizePerAnalysis = a->queryIDs.size();
	SEQ_AMOUNT_t referencesToAnalyse;
	SEQ_AMOUNT_t referencesToAnalyse_SizePerAnalysis;

	if (Hashmap::Use_Experimental_LazyLoading)
		referencesToAnalyse = Hashmap::loaded_TotalNumberOfReferences; //referencesToAnalyse = a->sequences->referenceSequences_Lazy.size();
	else
		referencesToAnalyse = a->sequences->referenceSequences.size();
	referencesToAnalyse_SizePerAnalysis = referencesToAnalyse;
	SEQ_AMOUNT_t referencesToAnalyse_Offset = 0;


	float mDebugTime;
	auto begin_time_total = high_resolution_clock::now();

	if (ddebug)
		cout << "start while" << endl;

	while (queriesToAnalyse_Available)
	{

		vector<SEQ_ID_t>* queryReferenceCombinations = &a->matchedReferencesPerQuery[queriesToAnalyse_Offset];
		vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions_LastPositionInRange = &a->matchedReferencesPerQuery_CombiPositions_LastPositionInRange[queriesToAnalyse_Offset];
		vector<vector<Enum::Position>>* queryReferenceCombinationsCombiPositions = &a->matchedReferencesPerQuery_CombiPositions[queriesToAnalyse_Offset];
		vector<vector<SEQ_POS_t>>* queryReferenceCombinationsRefPositions = &a->matchedReferencesPerQuery_RefPositions[queriesToAnalyse_Offset];
		vector<vector<SEQ_POS_t>>* queryReferenceCombinationsQueryPositions = &a->matchedReferencesPerQuery_QueryPositions[queriesToAnalyse_Offset];
		vector<vector<Enum::HeadTailSequence>>* matchedReferencesPerQuery_HeadTailSequences = &a->matchedReferencesPerQuery_HeadTailSequences[queriesToAnalyse_Offset];

		try
		{
			vector<MatrixResult> matrixCurrentResults;
			if (queriesToAnalyse_SizePerAnalysis > 0)
			{
				int threadSize = 512;
				//#ifdef show_timings
				//
				//				mDebugTime = (float(clock() - begin_time_total) / (CLOCKS_PER_SEC / 1000));
				//				printf("before cuda (%f sec)\n", mDebugTime / 1000);
				//				begin_time_total = clock();
				//#endif
				if (ddebug)
				{
					cout << "start cuda" << endl;
					cout << "queriesToAnalyse_Offset " << queriesToAnalyse_Offset << endl;
					cout << "queriesToAnalyse_SizePerAnalysis " << queriesToAnalyse_SizePerAnalysis << endl;
					cout << "referencesToAnalyse_Offset " << referencesToAnalyse_Offset << endl;
					cout << "referencesToAnalyse_SizePerAnalysis " << referencesToAnalyse_SizePerAnalysis << endl;
				}

#if defined(__CUDA__)
				matrixCurrentResults = CUDAGetHighestScore(a->querySequences, a->sequences, true, a->hardware->DeviceID[DeviceID], a->ConsiderThreshold, queryReferenceCombinations, queryReferenceCombinationsRefPositions, queryReferenceCombinationsQueryPositions, queryReferenceCombinationsCombiPositions, queryReferenceCombinationsCombiPositions_LastPositionInRange, threadSize, a->display, a->multiThreading, referencesToAnalyse_Offset, referencesToAnalyse_SizePerAnalysis, queriesToAnalyse_Offset, queriesToAnalyse_SizePerAnalysis, matchedReferencesPerQuery_HeadTailSequences);
#elif defined(__OPENCL__)
				matrixCurrentResults = GPUEngine->OpenCLGetHighestScore(a->querySequences, a->sequences, true, a->hardware->DeviceID[DeviceID], a->ConsiderThreshold, queryReferenceCombinations, queryReferenceCombinationsRefPositions, queryReferenceCombinationsQueryPositions, threadSize, a->display, a->multiThreading, referencesToAnalyse_Offset, referencesToAnalyse_SizePerAnalysis, queriesToAnalyse_Offset, queriesToAnalyse_SizePerAnalysis);
#else
				a->display->logWarningMessage << "Executing matrix on GPU FAILED!" << endl;
				a->display->logWarningMessage << "Unable to determine the hardware control type." << endl;
				a->display->logWarningMessage << "Do you have OpenCL Or CUDA installed?" << endl;
				a->display->logWarningMessage << "Is OpenCL Or CUDA environmental variable set during compile time?" << endl;
				a->display->logWarningMessage << "Program will stop." << endl;
				a->display->FlushWarning(true);
#endif

				matrixResults.insert(matrixResults.end(), matrixCurrentResults.begin(), matrixCurrentResults.end());
			}

			// Are we in a query=1 and reference=divided mode?
			if (querySizeLocked_ResizeReference)
			{
				// We will only reach this state if we had exceptions ending up into lowering the number of queries per cycle, which in the end failed as well with 1 query;
				// So now we are going to try to analyse with less references
				// Got to the next set of references
				referencesToAnalyse_Offset++;
				referencesToAnalyse_Offset += referencesToAnalyse_SizePerAnalysis;

				// Are we done?
				if (referencesToAnalyse_Offset >= referencesToAnalyse)
				{
					querySizeLocked_ResizeReference = false; // So we can go to the next query

					// Reset reference values for the next query
					referencesToAnalyse_Offset = 0;
					referencesToAnalyse_SizePerAnalysis = referencesToAnalyse;
				}
				else if (referencesToAnalyse_SizePerAnalysis + referencesToAnalyse_Offset > referencesToAnalyse) // Check if we do not overshoot the number of references
				{
					referencesToAnalyse_SizePerAnalysis = referencesToAnalyse - referencesToAnalyse_Offset; // Set the amount so we only analyse the last part of the reference
				}
			}

			if (!querySizeLocked_ResizeReference)
			{
				queriesToAnalyse_Offset += queriesToAnalyse_SizePerAnalysis;
				if (queriesToAnalyse_Offset + queriesToAnalyse_SizePerAnalysis > a->queryIDs.size())
				{
					queriesToAnalyse_SizePerAnalysis = (a->queryIDs.size() - queriesToAnalyse_Offset);

					// If we are again to large, we're done.
					if (queriesToAnalyse_SizePerAnalysis == 0 || queriesToAnalyse_Offset + queriesToAnalyse_SizePerAnalysis > a->queryIDs.size())
					{
						queriesToAnalyse_Available = false;
						break;
					}
				}
			}

		}
		catch (exception &ex)
		{
			//queriesToAnalyse_Available = false;

#ifdef show_cuda_crash
			cout << "#########" << endl;
			cout << "#########" << endl;
			cout << "Oops..cuda crashed.." << endl << flush;
			cout << "#########" << endl;
			cout << "#########" << endl;
			cout << ex.what() << endl << flush;
			cout << "Stop auto increase.." << endl;
			cout << "LowerReferenceEnabled: " << LowerReferenceEnabled << endl;
			cout << "querySizeLocked_ResizeReference: " << querySizeLocked_ResizeReference << endl;
			cout << "queriesToAnalyse_SizePerAnalysis: " << queriesToAnalyse_SizePerAnalysis << endl;
			cout << "referencesToAnalyse_SizePerAnalysis: " << referencesToAnalyse_SizePerAnalysis << endl;
#endif

			// When we receive an exception, it is by design that the CUDA/OpenCL code does not try to solve the issue.
			// We just try to lower the amount of queries (and references) and thereby assume we went out of memory on the GPU card.

			a->hardware->DeviceIsStabilized(DeviceID); // Don't auto increase anymore
			if (LowerReferenceEnabled && (querySizeLocked_ResizeReference || queriesToAnalyse_SizePerAnalysis == 1))
			{
				// We even failed with 1 query. Perhaps we are also sending to much reference data to the GPU?
				// Divide the number of references and retry again.
				//cout << "enter reference size decrease" << endl;

				if (referencesToAnalyse_SizePerAnalysis == 1)
				{
					// Ok, now we really failed. Bye!
					a->display->logWarningMessage << "Executing matrix on GPU FAILED!" << endl;
					a->display->logWarningMessage << "Unable to recover. Unable to analyse query on GPU: Lowest amount of references (=1) per analysis reached." << endl;
					a->display->logWarningMessage << "There is probably not enough GPU memory. Try to decrease the sequence size of the references or set a lower blocksize." << endl;
					a->display->logWarningMessage << "Program will stop." << endl;
					a->display->logWarningMessage << "Error: " << ex.what() << endl;
					a->display->FlushWarning(true);
				}


				querySizeLocked_ResizeReference = true;
				//cout << "old val referencesToAnalyse_SizePerAnalysis: " << referencesToAnalyse_SizePerAnalysis << endl;


				referencesToAnalyse_SizePerAnalysis = ((referencesToAnalyse_SizePerAnalysis / 100) * GPUFailure_DecreasePercentage_Reference) + 1; // +1 so we always decrease at least with 1 (if 10% results in 0 due to rounding features of int)
				//cout << "new val referencesToAnalyse_SizePerAnalysis: " << referencesToAnalyse_SizePerAnalysis << endl;
#ifdef show_cuda_crash
				cout << endl << "New values:..." << endl;
				cout << "referencesToAnalyse_SizePerAnalysis: " << referencesToAnalyse_SizePerAnalysis << endl;
				cout << "referencesToAnalyse_SizePerAnalysis: " << referencesToAnalyse_SizePerAnalysis << endl;
				cout << "GPUFailure_DecreasePercentage_Reference: " << GPUFailure_DecreasePercentage_Reference << endl;
				cout << endl << endl;

#endif
			}
			else
			{
				//cout << "enter query size decrease" << endl;
				//cout << "old val queriesToAnalyse_SizePerAnalysis: " << queriesToAnalyse_SizePerAnalysis << endl;

				// Decrease or load our last known not-error value?
				if (a->hardware->AutoIncrease_BlockSize_LastNoError[DeviceID] == -1) // (-1 = init value)
					// Decrease
					queriesToAnalyse_SizePerAnalysis -= ((queriesToAnalyse_SizePerAnalysis / 100) * GPUFailure_DecreasePercentage_Query) + 1; // +1 so we always decrease at least with 1 (if 10% results in 0 due to rounding features of int)
				else
				{
					// Did we went to high? Go back to the last know not-error value.
					queriesToAnalyse_SizePerAnalysis = a->hardware->AutoIncrease_BlockSize_LastNoError[DeviceID];
					// But if we fail again, we want to try lowering again
					a->hardware->AutoIncrease_BlockSize_LastNoError[DeviceID] = -1; // So next time we have a GPU failure, we try lowering the blockSize by x% again
				}
				//cout << "new val queriesToAnalyse_SizePerAnalysis: " << queriesToAnalyse_SizePerAnalysis << endl;


				if (queriesToAnalyse_SizePerAnalysis < 1)
				{
					if (queriesToAnalyse_SizePerAnalysis_LowestReached)
					{
						a->display->logWarningMessage << "Executing matrix on GPU FAILED!" << endl;
						a->display->logWarningMessage << "Unable to recover. Unable to analyse query on GPU: Lowest amount of queries (=1) per analysis reached." << endl;
						a->display->logWarningMessage << "There is probably not enough GPU memory. Try to decrease the sequence size of the references or set a lower blocksize." << endl;
						a->display->logWarningMessage << "Program will stop." << endl;
						a->display->logWarningMessage << "Error: " << ex.what() << endl;
						a->display->FlushWarning(true);
					}
					else
					{
						// Perhaps we ended up with 0, so set it to 1 and try for the last time
						queriesToAnalyse_SizePerAnalysis = 1;
						queriesToAnalyse_SizePerAnalysis_LowestReached = true;
					}
				}

				// Also update the blocksize for next run on this device
				a->multiThreading->mutGPUResults.lock();
				a->hardware->BlockSize_Current[DeviceID] = queriesToAnalyse_SizePerAnalysis;
				a->multiThreading->mutGPUResults.unlock();
			}
		}

	}

	float spentTime = (float)duration<double, ratio<1, 1>>(high_resolution_clock::now() - begin_time_total).count();

	// Unlock this GPU, so post processing takes places while the GPU can get some new data to work on
	//a->multiThreading->mutGPUResults.lock();
	//a->hardware->TryAutoIncreaseBlocksize(DeviceID, spentTime, a->PrepareSpentTime, a->queryIDs.size(), a->multiThreading->maxCPUThreads);
	//a->hardware->DeviceQueueSize[DeviceID]--;
	//(*a->TotalQuerySequencesAnalysed) += numOfQueriesBridged;
	//a->multiThreading->mutGPUResults.unlock();

	//vector<SEQ_ID_t> ptQueryIDsForLog = a->queryIDs;
	//// Append direct hits
	//if (a->QueryIDs_DirectHits.size() > 0)
	//{
	//	for (size_t directHit = 0; directHit < a->QueryIDs_DirectHits.size(); directHit++)
	//	{
	//		map<float, vector<size_t>, greater<float>> scores;
	//		scores.insert({ 1, a->queryWithDirectHitsReferenceCombination[directHit] });
	//		MatrixResult mr(matrixResults.size(), 1, scores);
	//		matrixResults.push_back(mr);
	//		ptQueryIDsForLog.push_back(a->QueryIDs_DirectHits[directHit]);
	//	}
	//}

	// Write results to log file
	if (matrixResults.size() > 0)
		Analyser::LogMatrixResults(&matrixResults, -1, &a->queryIDs, a->multiThreading, a->sequences, a->Output_Mode, a->logResultsToScreen, a->logManagerHighSensitivity, a->logManagerHighPrecision, a->logManagerBalanced, a->logManagerRaw, a->Output_HighestScoredResultsOnly);

	vector<vector<SEQ_ID_t>>().swap(a->matchedReferencesPerQuery);
	vector<vector<vector<Enum::Position>>>().swap(a->matchedReferencesPerQuery_CombiPositions_LastPositionInRange);
	vector<vector<vector<Enum::Position>>>().swap(a->matchedReferencesPerQuery_CombiPositions);
	vector<vector<int>>().swap(a->matchedReferencesPerQuery_FileIDs);
	vector<vector<char>*>().swap(a->querySequences);
	vector<SEQ_ID_t>().swap(a->queryIDs);

	vector<MatrixResult>().swap(matrixResults);

	// Release this thread
	a->multiThreading->mutMainMultithreads.lock();
	a->multiThreading->runningThreadsAnalysis--;
	a->multiThreading->threadsFinished.push_back(vectorThreadID);
	a->multiThreading->mutMainMultithreads.unlock();


	// Unlock this device
	a->multiThreading->mutexAnalysis[a->DeviceID] = false;
	//a->multiThreading->mutexAnalysis[a->DeviceID].unlock();
	a->multiThreading->mutexCUDARunning[a->hardware->DeviceID[DeviceID]].unlock();


	delete a;

	a->multiThreading->mutGPUResults.lock();
	a->hardware->TryAutoIncreaseBlocksize(DeviceID, spentTime, a->PrepareSpentTime, a->queryIDs.size(), a->multiThreading->maxCPUThreads);
	a->hardware->DeviceQueueSize[DeviceID]--;
	(*a->TotalQuerySequencesAnalysed) += numOfQueriesBridged;
	a->multiThreading->mutGPUResults.unlock();

	return NULL;
}

// Compare two Enum::Position objects
bool compareCombiPosition(Enum::Position i, Enum::Position j)
{
	if (i.Reference != j.Reference)
		return i.Reference < j.Reference;

	return i.Query < j.Query;
}


//void FutureDeleteargsForGPU(vector<vector<Enum::Position>>* p)
//{
//	//delete pa;
//	delete p;
//}
//template<typename T>


// Sort data based on Reference ID and Position
// When threads are less than 2 or data length is less than number of threads, do normal sort
// Otherwise, split data in half and sort each half in a separate thread, then merge the sorted halves
void mtSort_FoundRefID_Pos_Pairs(pair<SEQ_ID_t, Enum::Position>  *data, size_t len, int nbThreads)
{
	if (nbThreads < 2 || len <= nbThreads) {
		// Single thread sort
		sort(data, (&(data[len])), [&](pair<SEQ_ID_t, Enum::Position> i, pair<SEQ_ID_t, Enum::Position> j) {
			if (i.first < j.first) return true;
			if (i.first > j.first) return false;


			if (i.second.Reference != j.second.Reference)
				return i.second.Reference < j.second.Reference;

			return i.second.Query < j.second.Query;
		});
	}
	else {
		// Multi-threaded sort
		std::future<void> b = std::async(std::launch::async, mtSort_FoundRefID_Pos_Pairs, data, len / 2, nbThreads / 2);

		mtSort_FoundRefID_Pos_Pairs(&data[len / 2], len - len / 2, nbThreads / 2);
		//a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](pair<SEQ_ID_t, Enum::Position> i, pair<SEQ_ID_t, Enum::Position> j) {
			if (i.first < j.first) return true;
			if (i.first > j.first) return false;


			if (i.second.Reference != j.second.Reference)
				return i.second.Reference < j.second.Reference;

			return i.second.Query < j.second.Query;

		});
	}
}

// Sort data based on Reference ID and Query ID
// When threads are less than 2 or data length is less than number of threads, do normal sort
// Otherwise, split data in half and sort each half in a separate thread, then merge the sorted halves
void mtSort_Pos(Enum::Position *data, size_t len, int nbThreads)
{
	if (nbThreads < 2 || len <= nbThreads) {
		// Single thread sort
		sort(data, (&(data[len])), [&](Enum::Position i, Enum::Position j) {
			if (i.Reference != j.Reference)
				return i.Reference < j.Reference;

			return i.Query < j.Query;
		});
	}
	else {
		// Multi-threaded sort

		std::future<void> b = std::async(std::launch::async, mtSort_Pos, data, len / 2, nbThreads / 2);
		//std::future<void> a = std::async(std::launch::async, mtSort_FoundRefID_Pos_Pairs, &data[len / 2], len - len / 2, nbThreads / 2);

		mtSort_Pos(&data[len / 2], len - len / 2, nbThreads / 2);
		//a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](Enum::Position i, Enum::Position j) {


			if (i.Reference != j.Reference)
				return i.Reference < j.Reference;

			return i.Query < j.Query;

		});
	}
}

// Sort data based on SEQ_ID_t
// When threads are less than 2 or data length is less than number of threads, do normal sort
// Otherwise, split data in half and sort each half in a separate thread, then merge the sorted halves
void mtSort_seqID_VPos(pair<SEQ_ID_t, vector<Enum::Position>> *data, size_t len, int nbThreads)
{
	if (nbThreads < 2 || len <= nbThreads) {
		// Single thread sort
		sort(data, (&(data[len])), [&](pair<SEQ_ID_t, vector<Enum::Position>> i, pair<SEQ_ID_t, vector<Enum::Position>> j) {
			return i.first < j.first;
		});
	}
	else {
		// Multi-threaded sort
		std::future<void> b = std::async(std::launch::async, mtSort_seqID_VPos, data, len / 2, nbThreads / 2);

		mtSort_seqID_VPos(&data[len / 2], len - len / 2, nbThreads / 2);
		//a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](pair<SEQ_ID_t, vector<Enum::Position>> i, pair<SEQ_ID_t, vector<Enum::Position>> j) {
			return i.first < j.first;
		});
	}
}

// Sort data based on Hashmap_ID and HashStreamPosition
// When threads are less than 2 or data length is less than number of threads, do normal sort
// Otherwise, split data in half and sort each half in a separate thread, then merge the sorted halves
void mtSort_ToLoad(Analyser::References_ToLoad *data, size_t len, int nbThreads)
{
	if (nbThreads < 2 || len <= nbThreads) {
		// Single thread sort
		sort(data, (&(data[len])), [&](const Analyser::References_ToLoad& i, const Analyser::References_ToLoad& j) {
			if (i.Hashmap_ID < j.Hashmap_ID) return true;
			if (i.Hashmap_ID > j.Hashmap_ID) return false;

			if (i.HashStreamPosition < j.HashStreamPosition) return true;
			if (i.HashStreamPosition > j.HashStreamPosition) return false;
			return false;
		});


	}
	else {
		//Multi-threaded sort
		std::future<void> b = std::async(std::launch::async, mtSort_ToLoad, data, len / 2, nbThreads - 2);
		std::future<void> a = std::async(std::launch::async, mtSort_ToLoad, &data[len / 2], len - len / 2, nbThreads - 2);

		a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](const Analyser::References_ToLoad& i, const Analyser::References_ToLoad& j) {
			if (i.Hashmap_ID < j.Hashmap_ID) return true;
			if (i.Hashmap_ID > j.Hashmap_ID) return false;

			if (i.HashStreamPosition < j.HashStreamPosition) return true;
			if (i.HashStreamPosition > j.HashStreamPosition) return false;
			return false;
		});
	}
}
//void mtSort_ByVector(SEQ_ID_t *data, SEQ_ID_t* sortBy, size_t len, int nbThreads)
//{
//	if (nbThreads < 2 || len <= nbThreads) {
//		sort(data, (&(data[len])), [&](size_t i, size_t j) { return sortBy[i] < sortBy[j]; });
//
//
//	}
//	else {
//		std::future<void> b = std::async(std::launch::async, mtSort_ByVector, data, sortBy, len / 2, nbThreads - 2);
//		std::future<void> a = std::async(std::launch::async, mtSort_ByVector, &data[len / 2], sortBy, len - len / 2, nbThreads - 2);
//
//		a.wait();
//		b.wait();
//
//		std::inplace_merge(data, &data[len / 2], &data[len], [&](size_t i, size_t j) { return sortBy[i] < sortBy[j]; });
//	}
//}
// 

// Sort data based on values in data array
// When threads are less than 2 or data length is less than number of threads, do normal sort
// Otherwise, split data in half and sort each half in a separate thread, then merge the sorted halves
void mtSort_ByVector(SEQ_ID_t *data, size_t len, int nbThreads)
{
	if (nbThreads < 2 || len <= nbThreads) 
	{
		// Single thread sort
		sort(data, (&(data[len])), [&](size_t i, size_t j) { return i < j; });
	}
	else 
	{
		// Multi-threaded sort
		std::future<void> b = std::async(std::launch::async, mtSort_ByVector, data, len / 2, nbThreads - 2);
		std::future<void> a = std::async(std::launch::async, mtSort_ByVector, &data[len / 2], len - len / 2, nbThreads - 2);

		a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](size_t i, size_t j) { return i < j; });
	}
}

//void PreReserveFoundArrays (vector<SEQ_ID_t>* FoundRefIDs, vector<Enum::Position>* FoundPositions, size_t size);
//{
//
//
//	std::transform(FoundRefID_Pos_Pairs->begin(), FoundRefID_Pos_Pairs->end(), std::back_inserter(FoundRefIDs), [](const std::pair<SEQ_ID_t, Enum::Position>& p) {return p.first; });
//	std::transform(FoundRefID_Pos_Pairs->begin(), FoundRefID_Pos_Pairs->end(), std::back_inserter(FoundPositions), [](const std::pair<SEQ_ID_t, Enum::Position>& p) {return p.second; });
//
//
//
//	FoundRefIDs->reserve(size);
//	FoundPositions->reserve(size;
//}

// Resize combiHitsPositions
void vector_Resize_combiHitsPositions(vector<vector <Enum::Position>>* toResize, size_t size)
{
	toResize->resize(size);
}

// Transform FoundRefID_Pos_Pairs to FoundRefIDs
void transformFoundRef_To_FoundPositions(vector<pair<SEQ_ID_t, Enum::Position>>* in, vector<Enum::Position> * out)
{
	std::transform(in->begin(), in->end(), std::back_inserter(*out), [](const std::pair<SEQ_ID_t, Enum::Position>& p) {return p.second; });
}


//Prepare lookup tables for next hash positions
mutex mFoundRefID_Pos_Pairs_AddHighestOnly;
void FoundRefID_Pos_Pairs_AddHighestOnly(const SEQ_ID_t* referenceID, const SEQ_ID_t* hashmapHitPos, size_t vSeqIDs_Size, unordered_set<SEQ_ID_t>* countSuccesivePerSeqID_Highest_Only, vector<pair<SEQ_ID_t, Enum::Position>>* FoundRefID_Pos_Pairs, SEQ_POS_t currentCharPosition_InQuery)
{
	for (size_t k = 0; k < vSeqIDs_Size; k++)
	{
		bool bAppendToPairs = countSuccesivePerSeqID_Highest_Only->find(*referenceID) != countSuccesivePerSeqID_Highest_Only->end();
		//bool bAppendToPairs = binary_search(countSuccesivePerSeqID_Highest_Only.begin(), countSuccesivePerSeqID_Highest_Only.end(), *referenceID);

		if (bAppendToPairs)
		{
			mFoundRefID_Pos_Pairs_AddHighestOnly.lock();
			FoundRefID_Pos_Pairs->emplace_back(*referenceID, Enum::Position{ (*hashmapHitPos), (currentCharPosition_InQuery) });
			mFoundRefID_Pos_Pairs_AddHighestOnly.unlock();

		}
		referenceID++;
		hashmapHitPos++;
	}
}

// Sort FoundRefIDs based on values in the vector
void mt_mtSort_ByVector(vector<SEQ_ID_t>* FoundRefIDsCurrentFile_PerPos)
{
	mtSort_ByVector(&FoundRefIDsCurrentFile_PerPos->front(), FoundRefIDsCurrentFile_PerPos->size(), 4);
}

// Load hashmap data from disk into memory (high memory mode)
void LoadHashlistFromDisk_HighMem(size_t iFileID, hashmap_MatchedHashStreamPos* elems, MultiThreading* multiThreading, vector<byte*>* buffer_total, byte** buffer_total_start)
{
	size_t offset = Hashmap::Hashmap_First_PosData_HashStreamPos[iFileID];
	char* ptrStartData = Hashmap::Hashmap_Position_Data[iFileID];

	size_t buffer_total_size = 0;
	for (auto& el : *elems)
	{
		buffer_total_size += el.second.BufferSize;
	}
	buffer_total->reserve(elems->size());


	for (auto& el : *elems)
	{
		buffer_total->emplace_back((byte*)ptrStartData + (el.first - offset));
	}
}

// Load hashmap data from disk into memory (Normal memory mode)
void LoadHashlistFromDisk(size_t iFileID, vector<pair<HASH_STREAMPOS, BufferHashmapToReadFromFile*>>* elems, MultiThreading* multiThreading, byte** buffer_total, byte** buffer_total_start)
{
	char* LocalIFStreamBuffer;
	size_t LocalIFStreamBufferSize = 4096;
	ifstream* ptrvIFStream = new ifstream();

	LocalIFStreamBuffer = new char[LocalIFStreamBufferSize];

	Sequences::OpenFile(ptrvIFStream, &Hashmap::HashmapFileLocations[iFileID], LocalIFStreamBuffer, LocalIFStreamBufferSize);


	size_t buffer_total_size = 0;
	for (auto& el : *elems)
		//if (!el.second.Ignore)
	{
		buffer_total_size += el.second->BufferSize;
	}
	*buffer_total = new byte[buffer_total_size];
	*buffer_total_start = *buffer_total;


	//multiThreading->mutHashmapFile[iFileID].lock();

	for (auto& el : *elems)
	{
		ptrvIFStream->seekg(el.first);
		ptrvIFStream->read((char*)*buffer_total, el.second->BufferSize);
		*buffer_total += el.second->BufferSize;
	}

	*buffer_total = *buffer_total_start;

	//multiThreading->mutHashmapFile[iFileID].unlock();

	ptrvIFStream->close();
	delete ptrvIFStream;
	delete[] LocalIFStreamBuffer;
}


mutex mTest;

// Main function to prepare data for GPU analysis
void* Analyser::TPrepareGPUAnalysis(void* args)
{
	PrepareGPU_Arguments* argsForGPU = (PrepareGPU_Arguments*)args;
	Analysis_Arguments* gpuReturnData = argsForGPU->AnalysisArguments;


	size_t numOfReferences = argsForGPU->sequences->referenceHeaders.size();
	size_t highestNumberOfUniqueValues = 0;


	double debug_preparingReadFromDisk = 0;
	double debug_ReadFromDisk = 0;
	double debug_MatchLookUp = 0;
	double debug_Sort = 0;
	double debug_BuildCombi = 0;
	double debug_PreCalc = 0;
	double debug_RemainHeadTail = 0;

	const bool useFutures(true);
	const bool measureDebugTimings(true);
	//vector<ifstream> infile(Hashmap::HashmapFileLocations.size());

	// Check if we are on a GPU device
	if (argsForGPU->hardware->Type[argsForGPU->DeviceID] == Hardware::HardwareTypes::GPU)
		argsForGPU->multiThreading->SetGPUActive(true, argsForGPU->hardware->DeviceID[argsForGPU->DeviceID]);

#ifdef show_timings
	float mDebugTime;
	clock_t begin_prepare_time_against = clock();
#endif


	auto begin = high_resolution_clock::now();


	//vector<std::future<void>> futures;
	size_t AppendToCombiPosition_OrderNumber = 0;
	size_t AppendToCombiPosition_CurrentCnt = 0;

	size_t numOfCombi_Highest = 0;
	vector<pair<pair<SEQ_ID_t, Enum::Position>*, pair<SEQ_ID_t, Enum::Position>*>> combiPosition;
	//vector<vector<Enum::Position>> combiPosition;

	//vector<Enum::Position> nVPos_Template;
	//nVPos_Template.reserve(combiPositionsReserveSize);

	//vector<size_t> vecrelativeReferenceIDs;
	//vecrelativeReferenceIDs.resize(numOfReferences, 0);
	//size_t* ptr_vecrelativeReferenceIDs = vecrelativeReferenceIDs.data();
	//combiPosition.reserve(numOfReferences);

/*
	vector<SEQ_ID_t> vSeqIDs;
	vector<SEQ_ID_t> vPos;*/


	size_t startTimeThreads = clock();
	size_t QueryIDs_Size = argsForGPU->QueryIDs.size();

	// Todo: Whatever reason, always a mem error. Due to firstd malloc in this thread? Join wrong? Something else?
	//mTest.lock();
	//cout << "combiPosition_Size_Highest: " << combiPosition_Size_Highest << "  - " << Analyser::GetMemFreePercentage() << endl;
	//mTest.unlock();

	std::unique_ptr<std::vector<std::pair<SEQ_ID_t, Enum::Position>>> FoundRefID_Pos_Pairs = std::make_unique<std::vector<std::pair<SEQ_ID_t, Enum::Position>>>();

	//vector<pair<SEQ_ID_t, Enum::Position>>* FoundRefID_Pos_Pairs = new vector<pair<SEQ_ID_t, Enum::Position>>();
	FoundRefID_Pos_Pairs->reserve(combiPosition_Size_Highest);


	size_t hashmapHighestSteppingSize = (argsForGPU->ConsiderThreshold / 2);
	size_t ptrSteppingQuery = argsForGPU->ConsiderThreshold / 2;
	size_t hashSize = argsForGPU->ConsiderThreshold - ptrSteppingQuery + 1;


	/*
		float debug_timing_1 = 0;
		float debug_timing_2a = 0;
		float debug_timing_2 = 0;
		float debug_timing_3 = 0;
		float debug_timing_4 = 0;
		float debug_timing_5 = 0;
		float debug_timing_6 = 0;
		float debug_timing_7 = 0;
		float debug_timing_8 = 0;
		float debug_timing_9 = 0;*/


		/**************
		*
		* Find hashes per query position
		*
		***************/
	hashmap_streams* ptrreferenceHashMap_Lazy;

	//vector<hashmap_MatchedHashStreamPos> MatchedHashStreamPos;
	//MatchedHashStreamPos.resize(Hashmap::HashmapFileLocations.size());


	size_t presize_mhsp = 0;

	for (SEQ_ID_t indexQID = 0; indexQID < QueryIDs_Size; indexQID++)
	{
		// Get Query information
		SEQ_ID_t qID = argsForGPU->QueryIDs[indexQID];
		vector<char>* ptrQuerySeq = &(*argsForGPU->sequences->inputSequences)[qID];

		presize_mhsp += ptrQuerySeq->size() / ptrSteppingQuery;
	}

	////MatchedHashStreamPos[0].set_empty_key(0);
	//MatchedHashStreamPos[0].reserve(presize_mhsp);

	//for (size_t i = 1; i < Hashmap::HashmapFileLocations.size(); i++)
	//{
	//	//MatchedHashStreamPos[i].set_empty_key(0);
	//	MatchedHashStreamPos[i] = MatchedHashStreamPos[0];
	//}


	for (size_t i = 1; i < Hashmap::HashmapFileLocations.size(); i++)
	{
		(*argsForGPU->MatchedHashStreamPos)[i]->reserve(presize_mhsp);
	}
	//#else
	//
	//	for (size_t i = 1; i < Hashmap::HashmapFileLocations.size(); i++)
	//	{
	//		(*argsForGPU->MatchedHashStreamPos)[i].rehash(presize_mhsp);
	//	}
	//#endif

	vector<vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>> loadedStreamIterators;
	loadedStreamIterators.reserve(QueryIDs_Size);

	// For each query
	for (SEQ_ID_t indexQID = 0; indexQID < QueryIDs_Size; indexQID++)
	{

		// Get Query information
		SEQ_ID_t qID = argsForGPU->QueryIDs[indexQID];
		vector<char>* ptrQuerySeq = &(*argsForGPU->sequences->inputSequences)[qID];
		SEQ_POS_t iStop = ptrQuerySeq->size();

		// Make sure we don't overshoot with the threshold
		if (argsForGPU->ConsiderThreshold < iStop)
			iStop -= argsForGPU->ConsiderThreshold;


		loadedStreamIterators.emplace_back();
		vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>* ptrLSI_Q = &loadedStreamIterators.back();
		ptrLSI_Q->reserve(Hashmap::HashmapFileLocations.size());

		const size_t numPositions = (iStop / ptrSteppingQuery) + 1;
		/*	bool* HashDataOnPos = new bool[numPositions];
			memset(HashDataOnPos, false, numPositions);
			bool* HashDataOnPos_Start = HashDataOnPos;*/

		vector<HASH_t> queryHashes;
		queryHashes.reserve(numPositions);
		for (SEQ_POS_t currentCharPosition_InQuery = 0; currentCharPosition_InQuery < iStop; currentCharPosition_InQuery += ptrSteppingQuery)
			queryHashes.emplace_back(Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*ptrQuerySeq)[currentCharPosition_InQuery]), hashSize));



		// For each hashmap file, quickly find references to send to GPU
		for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
		{

			HASH_t* ptrQuerySeq_Hash = &queryHashes[0];
			//HASH_t* ptrQuerySeq_Hash = &(*(*argsForGPU->sequences->inputSequences_Hashes)[qID])[0];
			ptrreferenceHashMap_Lazy = (*Hashmap::referenceHashMap_Lazy)[iFileID];

			hashmap_streams::const_iterator ptrLG_End = ptrreferenceHashMap_Lazy->end();
			hashmap_MatchedHashStreamPos* ptrTestHashStreamPos = (*argsForGPU->MatchedHashStreamPos)[iFileID];

			ptrLSI_Q->emplace_back();
			vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>* ptrLSI_QF = &ptrLSI_Q->back();
			ptrLSI_QF->reserve(numPositions);


			size_t currentPosIndex = 0;
			for (SEQ_POS_t currentCharPosition_InQuery = 0; currentCharPosition_InQuery < iStop; currentCharPosition_InQuery += ptrSteppingQuery)
			{
				//const HASH_t hash = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*ptrQuerySeq)[currentCharPosition_InQuery]), hashSize);
				//hashmap_streams::const_iterator found = ptrreferenceHashMap_Lazy->find(hash);

				hashmap_streams::const_iterator found = ptrreferenceHashMap_Lazy->find(*ptrQuerySeq_Hash);
				//hashmap_streams::const_iterator found = ptrreferenceHashMap_Lazy->find(*ptrQuerySeq_Hash);

				if (found != ptrLG_End)
				{
					BufferHashmapToReadFromFile* ptrBHTRFF = &(*ptrTestHashStreamPos)[found->second.StreamPos];
					ptrBHTRFF->BufferSize = found->second.BufferSize; // add the steampos to our list

					//BufferHashmapToReadFromFile* ptrBHTRFF = const_cast<BufferHashmapToReadFromFile*>((found->second.DecompressedBuffer));
					if (ptrBHTRFF != NULL)
						ptrLSI_QF->emplace_back(currentCharPosition_InQuery, ptrBHTRFF);
				}
				currentPosIndex++;
				ptrQuerySeq_Hash++;
			}
		}
	}


	if (measureDebugTimings)
	{
		debug_timing_1 = debug_timing_1 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count();
		//std::cout << "tprepare Preparing Read from disk took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		begin = high_resolution_clock::now();
	}


	vector<byte*> buffer_total;
	vector<byte*> buffer_total_start;
	vector < vector<pair< HASH_STREAMPOS, BufferHashmapToReadFromFile*>>> vMatchedHashStreamPos;// (elems->begin(), elems->end());

	// Load required hashmap data from disk into memory
	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode < Enum::Hashmap_MemoryUsageMode::High)
	{
		vector<future<void>> future_LoadHashlistFromDisk;

		buffer_total.resize(Hashmap::HashmapFileLocations.size()); // Make sure no reallocations occure since we provide a pointer from this vector to the async method
		buffer_total_start.resize(Hashmap::HashmapFileLocations.size()); // Make sure no reallocations occure since we provide a pointer from this vector to the async method

		vMatchedHashStreamPos.resize(Hashmap::HashmapFileLocations.size());

		for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
		{

			//if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode == Enum::Hashmap_MemoryUsageMode::High)
			//{
			//	future_LoadHashlistFromDisk.emplace_back(std::async(launch::async, LoadHashlistFromDisk_HighMem, iFileID, &(*argsForGPU->MatchedHashStreamPos)[iFileID], argsForGPU->multiThreading, &buffer_total[iFileID], &buffer_total_start[iFileID]));
			//}
			//else

			// Create loading list
			vector<pair< HASH_STREAMPOS, BufferHashmapToReadFromFile*>>* ptrVMHSP = &vMatchedHashStreamPos[iFileID];
			ptrVMHSP->reserve((*argsForGPU->MatchedHashStreamPos)[iFileID]->size());
			for (auto& e : *(*argsForGPU->MatchedHashStreamPos)[iFileID])
			{
				ptrVMHSP->emplace_back(e.first, &e.second);
			}

			sort(ptrVMHSP->begin(), ptrVMHSP->end(), [](const pair< HASH_STREAMPOS, BufferHashmapToReadFromFile*>& i, const  pair< HASH_STREAMPOS, BufferHashmapToReadFromFile*>& j) { return i.first < j.first; });

			if (useFutures)
				future_LoadHashlistFromDisk.emplace_back(std::async(launch::async, LoadHashlistFromDisk, iFileID, ptrVMHSP, argsForGPU->multiThreading, &buffer_total[iFileID], &buffer_total_start[iFileID]));
			else
				LoadHashlistFromDisk(iFileID, ptrVMHSP, argsForGPU->multiThreading, &buffer_total[iFileID], &buffer_total_start[iFileID]);

		}

		///////////////////////////////
		//buffer_total.emplace_back();
		//buffer_total_start.emplace_back();

		//if (useFutures)
		//	future_LoadHashlistFromDisk.emplace_back(std::async(launch::async, LoadHashlistFromDisk, iFileID, &MatchedHashStreamPos[iFileID], argsForGPU->multiThreading, &buffer_total.back(), &buffer_total_start.back()));
		//else
		//	LoadHashlistFromDisk(iFileID, &MatchedHashStreamPos[iFileID], argsForGPU->multiThreading, &buffer_total.back(), &buffer_total_start.back());


		if (useFutures)
			Asynchronise::wait_for_futures(&future_LoadHashlistFromDisk);

	}


	if (measureDebugTimings)
	{
		debug_timing_2a = debug_timing_2a + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count();
		//std::cout << "tprepare Mem copy  took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		begin = high_resolution_clock::now();
	}


	// Now read the loaded hashmap data into memory structures
	for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	{
		size_t raise_StartValue = (*Hashmap::MultipleHashmaps_SequenceOffset)[iFileID];
		size_t hashStreampOffset = Hashmap::Hashmap_First_PosData_HashStreamPos[iFileID];
		char* ptrhashStreampStartData = Hashmap::Hashmap_Position_Data[iFileID];

		if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode >= Enum::Hashmap_MemoryUsageMode::High)
		{
			// Load references with high memory mode

			//vector<pair<HASH_STREAMPOS, BufferHashmapToReadFromFile*>>* ptrTestHashStreamPos = &vMatchedHashStreamPos[iFileID];
			hashmap_MatchedHashStreamPos* ptrTestHashStreamPos = (*argsForGPU->MatchedHashStreamPos)[iFileID];

			size_t count = 0;
			for (auto& el : *ptrTestHashStreamPos)
			{

				if (el.second.BufferSize == 0)
					continue;

				BYTE_CNT bytes_currentRef_PosOffset = 0;
				BYTE_CNT bytes_currentRefID = 0;
				BYTE_CNT bytes_currentRefPOS = 0;
				BYTE_CNT bytes_currentRef_NumSeconds = 0;
				BYTE_CNT bytes_PosNumberRepeation = 0;
				SEQ_ID_t refIDMin = 0;
				SEQ_ID_t refPOSMin = 0;
				size_t numIDs = 0; // Get number of reference ID's


				//unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>::iterator found = ptrTestHashStreamPos->find(el.first);
				const HASH_STREAMPOS gotoHashStreamPos = el.first;
				const size_t HashStreamPos_BufferSize = el.second.BufferSize;


				char* buffer_all = (ptrhashStreampStartData + (el.first - hashStreampOffset));
				//byte* buffer_all = (byte*)el.first; // (ptrhashStreampStartData + (el.first - hashStreampOffset));
				count++;
				//buffer_total[iFileID]++;
				//buffer_total[iFileID] += el.second.BufferSize;

				memcpy(&bytes_currentRef_PosOffset, buffer_all, 1);
				memcpy(&bytes_currentRefID, buffer_all + 1, 1);
				memcpy(&bytes_currentRefPOS, buffer_all + 2, 1);
				memcpy(&bytes_currentRef_NumSeconds, buffer_all + 3, 1);
				buffer_all += 4;


				size_t bufferSize = 0;
				bufferSize += bytes_currentRef_PosOffset;
				bufferSize += bytes_currentRef_PosOffset;
				bufferSize += bytes_currentRef_NumSeconds;


				memcpy(&refIDMin, buffer_all, bytes_currentRef_PosOffset);
				memcpy(&refPOSMin, buffer_all + bytes_currentRef_PosOffset, bytes_currentRef_PosOffset);
				memcpy(&numIDs, buffer_all + bytes_currentRef_PosOffset + bytes_currentRef_PosOffset, bytes_currentRef_NumSeconds);
				buffer_all += bufferSize;


				el.second.NumIDs = numIDs;

				// Precalculate values
				size_t listOffset = el.second.vSeqIDs.size(); // If we have multiple hashmaps files, stack the ID's
				size_t numIDs_ListOffset = numIDs + listOffset;

				// Reserve new or extended memory space
				el.second.vSeqIDs.resize(numIDs_ListOffset, 0); // Since we can write ID's to the hashmap with less bits than sizeof(size_t), set all bits to 0 so we only set the bits to 1 which will be really read from file below (otherwise you might end up with bits to 1 at random places outside the bytes_currentRefID window)
				//el.second.vPos.reserve(numIDs_ListOffset);

				SEQ_ID_t raise = raise_StartValue; // Now we have the reference ID's, but if we have multiple hashmaps, we stack all reference in 1 list, so we need to add the offset per hashmap to the newly loaded reference ID's
				raise += refIDMin; // raise due to the memory compression method used in saving the hashmap database

				// Buffer file data
				bufferSize = bytes_currentRefID * numIDs;

				vector<SEQ_ID_t>* ptrVSIT = &el.second.vSeqIDs;
				SEQ_ID_t* sit = &(*ptrVSIT)[listOffset];

				for (size_t iRefID = 0; iRefID < numIDs; iRefID++)
				{
					memcpy(sit, buffer_all, bytes_currentRefID);
					//memcpy(sit, buffer_data_3 + (iRefID *bytes_currentRefID), bytes_currentRefID);
					buffer_all += bytes_currentRefID;
					sit++;
				}


				// Add raise, since we load all reference sequences in 1 big vector
				for (auto i = ptrVSIT->begin(); i != ptrVSIT->end(); ++i)
					(*i) += raise;

				// Buffer file data
				bufferSize = 0;
				bufferSize += 1;
				bufferSize += bytes_currentRefPOS;


				// Positions
				size_t numRepeations = 0;
				memcpy(&bytes_PosNumberRepeation, buffer_all, 1);
				memcpy(&numRepeations, buffer_all + 1, bytes_currentRefPOS);
				buffer_all += bufferSize;


				// Buffer file data
				bufferSize = 0;
				bufferSize += numRepeations * bytes_PosNumberRepeation;
				bufferSize += numRepeations * bytes_currentRefPOS;


				vector<SEQ_ID_t> vRepeations(numRepeations, 0);
				vector<SEQ_ID_t> offsetPositions(numRepeations, 0);
				SEQ_ID_t* ptrNumRep = &vRepeations.front();
				SEQ_ID_t* ptroffsetPositions = &offsetPositions.front();

				size_t cnt = 0;
				size_t totalNumbers = 0;

				// Read positions and number of repeations
				for (size_t iPos = 0; iPos < numRepeations; iPos++)
				{
					memcpy(ptrNumRep, buffer_all + cnt, bytes_PosNumberRepeation);
					buffer_all += bytes_PosNumberRepeation;
					memcpy(ptroffsetPositions, buffer_all + cnt, bytes_currentRefPOS);
					buffer_all += bytes_currentRefPOS;

					*ptroffsetPositions = *ptroffsetPositions + refPOSMin;
					totalNumbers += *ptrNumRep;
					/*
									el.second.vPos.reserve(el.second.vPos.size() + *ptrNumRep);
									el.second.vPos.insert(el.second.vPos.end(), *ptrNumRep, *ptroffsetPositions);*/

					ptrNumRep++;
					ptroffsetPositions++;
				}

				// Now fill positions based on number of repeations
				size_t cPos = 0;
				size_t offset = el.second.vPos.size();
				el.second.vPos.resize(el.second.vPos.size() + totalNumbers);

				ptrNumRep = &vRepeations.front();
				ptroffsetPositions = &offsetPositions.front();
				for (size_t iPos = 0; iPos < numRepeations; iPos++)
				{
					fill(el.second.vPos.begin() + cPos + offset, el.second.vPos.begin() + cPos + offset + *ptrNumRep, *ptroffsetPositions);
					cPos += *ptrNumRep;

					ptrNumRep++;
					ptroffsetPositions++;
				}

				buffer_all++;
			}

		}
		else
		{
			// Load references with normal memory mode

			vector<pair<HASH_STREAMPOS, BufferHashmapToReadFromFile*>>* ptrTestHashStreamPos = &vMatchedHashStreamPos[iFileID];
			//hashmap_MatchedHashStreamPos* ptrTestHashStreamPos = &MatchedHashStreamPos[iFileID];

			for (auto& el : *ptrTestHashStreamPos)
			{

				if (el.second->BufferSize == 0)
					continue;

				BYTE_CNT bytes_currentRef_PosOffset = 0;
				BYTE_CNT bytes_currentRefID = 0;
				BYTE_CNT bytes_currentRefPOS = 0;
				BYTE_CNT bytes_currentRef_NumSeconds = 0;
				BYTE_CNT bytes_PosNumberRepeation = 0;
				SEQ_ID_t refIDMin = 0;
				SEQ_ID_t refPOSMin = 0;
				size_t numIDs = 0; // Get number of reference ID's


				//unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>::iterator found = ptrTestHashStreamPos->find(el.first);
				const HASH_STREAMPOS gotoHashStreamPos = el.first;
				const size_t HashStreamPos_BufferSize = el.second->BufferSize;




				byte* buffer_all = buffer_total[iFileID];
				buffer_total[iFileID] += el.second->BufferSize;

				memcpy(&bytes_currentRef_PosOffset, buffer_all, 1);
				memcpy(&bytes_currentRefID, buffer_all + 1, 1);
				memcpy(&bytes_currentRefPOS, buffer_all + 2, 1);
				memcpy(&bytes_currentRef_NumSeconds, buffer_all + 3, 1);
				buffer_all += 4;


				size_t bufferSize = 0;
				bufferSize += bytes_currentRef_PosOffset;
				bufferSize += bytes_currentRef_PosOffset;
				bufferSize += bytes_currentRef_NumSeconds;


				memcpy(&refIDMin, buffer_all, bytes_currentRef_PosOffset);
				memcpy(&refPOSMin, buffer_all + bytes_currentRef_PosOffset, bytes_currentRef_PosOffset);
				memcpy(&numIDs, buffer_all + bytes_currentRef_PosOffset + bytes_currentRef_PosOffset, bytes_currentRef_NumSeconds);
				buffer_all += bufferSize;



				el.second->NumIDs = numIDs;


				// Precalculate values
				size_t listOffset = el.second->vSeqIDs.size(); // If we have multiple hashmaps files, stack the ID's
				size_t numIDs_ListOffset = numIDs + listOffset;

				// Reserve new or extended memory space
				el.second->vSeqIDs.resize(numIDs_ListOffset, 0); // Since we can write ID's to the hashmap with less bits than sizeof(size_t), set all bits to 0 so we only set the bits to 1 which will be really read from file below (otherwise you might end up with bits to 1 at random places outside the bytes_currentRefID window)
				//el.second->vPos.reserve(numIDs_ListOffset);

				SEQ_ID_t raise = raise_StartValue; // Now we have the reference ID's, but if we have multiple hashmaps, we stack all reference in 1 list, so we need to add the offset per hashmap to the newly loaded reference ID's
				raise += refIDMin; // raise due to the memory compression method used in saving the hashmap database

				// Buffer file data
				bufferSize = bytes_currentRefID * numIDs;

				vector<SEQ_ID_t>* ptrVSIT = &el.second->vSeqIDs;
				SEQ_ID_t* sit = &(*ptrVSIT)[listOffset];

				for (size_t iRefID = 0; iRefID < numIDs; iRefID++)
				{
					memcpy(sit, buffer_all, bytes_currentRefID);
					//memcpy(sit, buffer_data_3 + (iRefID *bytes_currentRefID), bytes_currentRefID);
					buffer_all += bytes_currentRefID;
					sit++;
				}



				// Add raise, since we load all reference sequences in 1 big vector
				for (auto i = ptrVSIT->begin(); i != ptrVSIT->end(); ++i)
					(*i) += raise;

				// Buffer file data
				bufferSize = 0;
				bufferSize += 1;
				bufferSize += bytes_currentRefPOS;



				// Positions
				size_t numRepeations = 0;
				memcpy(&bytes_PosNumberRepeation, buffer_all, 1);
				memcpy(&numRepeations, buffer_all + 1, bytes_currentRefPOS);
				buffer_all += bufferSize;



				// Buffer file data
				bufferSize = 0;
				bufferSize += numRepeations * bytes_PosNumberRepeation;
				bufferSize += numRepeations * bytes_currentRefPOS;


				vector<SEQ_ID_t> vRepeations(numRepeations, 0);
				vector<SEQ_ID_t> offsetPositions(numRepeations, 0);
				SEQ_ID_t* ptrNumRep = &vRepeations.front();
				SEQ_ID_t* ptroffsetPositions = &offsetPositions.front();

				size_t cnt = 0;
				size_t totalNumbers = 0;

				// Read positions and number of repeations
				for (size_t iPos = 0; iPos < numRepeations; iPos++)
				{
					memcpy(ptrNumRep, buffer_all + cnt, bytes_PosNumberRepeation);
					buffer_all += bytes_PosNumberRepeation;
					memcpy(ptroffsetPositions, buffer_all + cnt, bytes_currentRefPOS);
					buffer_all += bytes_currentRefPOS;

					*ptroffsetPositions = *ptroffsetPositions + refPOSMin;
					totalNumbers += *ptrNumRep;
					/*
					el.second->vPos.reserve(el.second->vPos.size() + *ptrNumRep);
					el.second->vPos.insert(el.second->vPos.end(), *ptrNumRep, *ptroffsetPositions);*/

					ptrNumRep++;
					ptroffsetPositions++;
				}

				// Now fill positions based on number of repeations
				size_t cPos = 0;
				size_t offset = el.second->vPos.size();
				el.second->vPos.resize(el.second->vPos.size() + totalNumbers);

				ptrNumRep = &vRepeations.front();
				ptroffsetPositions = &offsetPositions.front();
				for (size_t iPos = 0; iPos < numRepeations; iPos++)
				{
					fill(el.second->vPos.begin() + cPos + offset, el.second->vPos.begin() + cPos + offset + *ptrNumRep, *ptroffsetPositions);
					cPos += *ptrNumRep;

					ptrNumRep++;
					ptroffsetPositions++;
				}
			}
		}

		//argsForGPU->multiThreading->mutHashmapFile[iFileID].unlock();

	}

	if (measureDebugTimings)
	{
		debug_timing_2 = debug_timing_2 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count();
		//std::cout << "tprepare Mem copy  took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		begin = high_resolution_clock::now();
	}


	size_t occureBufferSize = 1; // When more memory is required due to a higher referenceID, this will be managed in the re-initialisation
	int* occure = new int[occureBufferSize];
	int* occure_start = occure;
	int* occureMax = new int[occureBufferSize];
	int* occureMax_Start = occureMax;
	int* occure_LastPos = new int[occureBufferSize];

	SEQ_ID_t* qID = &argsForGPU->QueryIDs[0];

	size_t sizeOf_RefIDAboveHighestSuccesive = 1;
	bool* ptrRefIDAboveHighestSuccesive = new bool[sizeOf_RefIDAboveHighestSuccesive];
	bool* ptrRefIDAboveHighestSuccesive_start = ptrRefIDAboveHighestSuccesive;


	if (measureDebugTimings)
	{
		debug_timing_10 = debug_timing_10 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count();
		//std::cout << "tprepare Mem copy  took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		begin = high_resolution_clock::now();
	}

	//	For each query, find matching references
	for (SEQ_ID_t indexQID = 0; indexQID < QueryIDs_Size; indexQID++)
	{
		auto begin_time = high_resolution_clock::now();


		size_t cnt = 0;
		vector<char>* ptrQuerySeq = &(*argsForGPU->sequences->inputSequences)[*qID];


		/**************
		*
		* Find hash based similarities
		*
		***************/
		size_t newRelativeReferenceID = 1;
		size_t currentAnalysisArgumentID = (argsForGPU->NumQIDs_PerThread * argsForGPU->ThreadResultID) + indexQID;

		gpuReturnData->querySequences[currentAnalysisArgumentID] = ptrQuerySeq;
		gpuReturnData->queryIDs[currentAnalysisArgumentID] = *qID;
		qID++;

		vector<SEQ_ID_t>* refHitsIDs = &gpuReturnData->matchedReferencesPerQuery[currentAnalysisArgumentID];
		vector<vector <Enum::Position>>* combiHitsPositions = &gpuReturnData->matchedReferencesPerQuery_CombiPositions[currentAnalysisArgumentID];
		vector<vector <Enum::Position>>* combiHitsPositions_LastPositionInRange = &gpuReturnData->matchedReferencesPerQuery_CombiPositions_LastPositionInRange[currentAnalysisArgumentID];

		vector<SEQ_ID_t> FoundRefIDs;
		vector<Enum::Position> FoundPositions;

		// Reuse vector
		FoundRefID_Pos_Pairs->clear();
		FoundRefID_Pos_Pairs->reserve(combiPosition_Size_Highest); // reserve after clear, since clear does not guaranteed keep the capacity


		//map<SEQ_ID_t, vector<Enum::Position>> FoundRefID_Pos_Pairs_Test;
		// Make sure we don't overshoot with the threshold
		SEQ_POS_t iStop = ptrQuerySeq->size();
		if (argsForGPU->ConsiderThreshold < iStop)
			iStop -= argsForGPU->ConsiderThreshold;


		vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>* ptrLSI_Q = &loadedStreamIterators[indexQID];


		if (measureDebugTimings)
		{
			debug_timing_11 = debug_timing_11 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count();
			//std::cout << "tprepare Mem copy  took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
			begin = high_resolution_clock::now();
		}


		/**************
		*
		* Search per hashmap file
		*
		***************/
		for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
		{

			//vector<pair<SEQ_POS_t, const BufferHashmapToReadFromFile*>> loadedStreamIterators;
			//loadedStreamIterators.reserve((iStop / ptrSteppingQuery) + 1);

			/*vector<vector<SEQ_ID_t>> FoundRefIDsCurrentFile_PerPos;
			FoundRefIDsCurrentFile_PerPos.reserve((iStop / ptrSteppingQuery) + 1);
*/
			size_t highestSuccesive = 0;
			SEQ_ID_t highestRefID = 0;
			SEQ_ID_t lowestRefID = argsForGPU->sequences->referenceSequences.size();



			vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>* ptrLSI_QF = &(*ptrLSI_Q)[iFileID];

			/**************
			*
			* Get all reference sequences per query position and find the highest reference ID
			*
			***************/
			// Do not continue if we haven't found anything to work with
			if (ptrLSI_QF->size() == 0)
				continue;

			pair<SEQ_POS_t, BufferHashmapToReadFromFile*>* ptrLSI_QF_Pair = &(*ptrLSI_QF)[0];
			for (size_t i = 0; i < ptrLSI_QF->size(); i++)

			{
				vector<SEQ_ID_t>* ptrVSeqIDs = &(ptrLSI_QF_Pair->second->vSeqIDs);
				if (ptrVSeqIDs->size() > 0)
				{
					const auto minmax_result = std::minmax_element(ptrVSeqIDs->begin(), ptrVSeqIDs->end());
					lowestRefID = std::min<SEQ_ID_t>(lowestRefID, *(minmax_result.first));
					highestRefID = max(highestRefID, *(minmax_result.second));
				}
				ptrLSI_QF_Pair++;
			}


			if (lowestRefID > highestRefID)
				continue;

			const SEQ_ID_t lowest_highest_RefID_Diff = highestRefID - lowestRefID;

			// Prepare occure buffer
			InitialiseOccureBuffer(lowest_highest_RefID_Diff, occureBufferSize, occure_start, occure, occureMax_Start, occureMax, occure_LastPos);
			//InitialiseOccureBuffer(highestRefID, occureBufferSize, occure_start, occure, occureMax_Start, occureMax, occure_LastPos);

			//int* occure_total = new int[highestRefID[iFileID]+1];
			//memset(occure_total, 0, highestRefID[iFileID]+1);
			//int occure_total_max = 0;

			/**************
			*
			* Find number of succesive occurence per reference
			*
			***************/
			int currentPos = 0;
			size_t numItems_Prev;
			size_t numSeqIDs_Total = 0;
			for (size_t i = 0; i < ptrLSI_QF->size(); i++)
			{
				const vector<SEQ_ID_t>* ptrFRIDCF_PP = &((*ptrLSI_QF)[i].second->vSeqIDs);
				size_t numItems = ptrFRIDCF_PP->size();
				numSeqIDs_Total += numItems;
				if (i == 0)
				{
					// First, initialse all reference ID's to have occured 1
					for (size_t j = 0; j < numItems; j++)
					{
						SEQ_ID_t currentSeqID = (*ptrFRIDCF_PP)[j];
						currentSeqID -= lowestRefID;
						int* currentOccure_Last = occure_LastPos + currentSeqID;

						int* currentOccure = occure + currentSeqID;
						int* currentOccureMax = occureMax + currentSeqID;
						*currentOccure = 1;
						*currentOccureMax = 1;
						*currentOccure_Last = 0;
					}
				}
				else
				{
					// Then check for succesive occurence
					for (size_t j = 0; j < numItems; j++)
					{
						SEQ_ID_t currentSeqID = (*ptrFRIDCF_PP)[j];
						currentSeqID -= lowestRefID;
						int* currentOccure_Last = occure_LastPos + currentSeqID;

						if (*currentOccure_Last == i)
							continue;

						bool bFoundInPrev = *(currentOccure_Last) == (i - 1);
						int* currentOccure = occure + currentSeqID;
						int* currentOccureMax = occureMax + currentSeqID;

						if (bFoundInPrev)
						{
							(*currentOccure)++;
							*currentOccureMax = max(*currentOccureMax, *currentOccure);
							highestSuccesive = max(highestSuccesive, (size_t)*currentOccureMax);
						}
						else
						{
							(*currentOccure) = 1;
							*currentOccureMax = max(*currentOccureMax, *currentOccure);

						}

						*(currentOccure_Last) = i;

					}
				}
				numItems_Prev = numItems;
			}


			const size_t highestSuccesive_const = max((size_t)1, (highestSuccesive != 0 ? highestSuccesive - 1 : 0)); // So we take results with 1 difference in matrix calculation as well


			// Calculate extra required capacity
			occureMax = occureMax_Start;
			size_t FoundRefID_Pos_Pairs_requiredCapacity = FoundRefID_Pos_Pairs->size();
			pair<SEQ_POS_t, BufferHashmapToReadFromFile* >* lsi = &ptrLSI_QF->front();
			pair<SEQ_POS_t, BufferHashmapToReadFromFile*>* lsi_last = &ptrLSI_QF->back();

			if (numSeqIDs_Total > sizeOf_RefIDAboveHighestSuccesive)
			{
				sizeOf_RefIDAboveHighestSuccesive = numSeqIDs_Total;
				delete[]ptrRefIDAboveHighestSuccesive_start;
				ptrRefIDAboveHighestSuccesive = new bool[sizeOf_RefIDAboveHighestSuccesive];
				ptrRefIDAboveHighestSuccesive_start = ptrRefIDAboveHighestSuccesive;
			}



			ptrRefIDAboveHighestSuccesive = ptrRefIDAboveHighestSuccesive_start;
			memset(ptrRefIDAboveHighestSuccesive, false, ptrLSI_QF->size());
			occureMax = occureMax_Start;
			while (lsi <= lsi_last)
			{
				//Prepare lookup tables for next hash positions
				const SEQ_ID_t* referenceID = &(lsi->second->vSeqIDs[0]);
				const SEQ_ID_t* hashmapHitPos = &(lsi->second->vPos[0]);
				size_t vSeqIDs_Size = lsi->second->vSeqIDs.size();
				for (size_t k = 0; k < vSeqIDs_Size; k++)
				{
					const SEQ_ID_t refID = *referenceID;
					const SEQ_ID_t refID_occure = refID - lowestRefID;

					if ((*(occureMax + refID_occure) >= highestSuccesive_const))
					{
						*ptrRefIDAboveHighestSuccesive = true;
						FoundRefID_Pos_Pairs_requiredCapacity++;
					}

					referenceID++;
					hashmapHitPos++;
					ptrRefIDAboveHighestSuccesive++;
				}
				lsi++;
			}
			FoundRefID_Pos_Pairs->reserve(FoundRefID_Pos_Pairs_requiredCapacity);



			/**************
			*
			* Only keep the reference with positions with the highest succesive
			*
			***************/
			lsi = &ptrLSI_QF->front();
			lsi_last = &ptrLSI_QF->back();
			//occureMax = occureMax_Start;
			ptrRefIDAboveHighestSuccesive = ptrRefIDAboveHighestSuccesive_start;
			while (lsi <= lsi_last)
			{
				//Prepare lookup tables for next hash positions
				const SEQ_ID_t* referenceID = &(lsi->second->vSeqIDs[0]);
				const SEQ_ID_t* hashmapHitPos = &(lsi->second->vPos[0]);
				size_t vSeqIDs_Size = lsi->second->vSeqIDs.size();
				for (size_t k = 0; k < vSeqIDs_Size; k++)
				{
					//const SEQ_ID_t refID = *referenceID;
					//const SEQ_ID_t refID_occure = refID - lowestRefID;

					//if ((*(occureMax + refID_occure) >= highestSuccesive_const))
					if (*ptrRefIDAboveHighestSuccesive)
					{
						FoundRefID_Pos_Pairs->emplace_back(*referenceID, Enum::Position{ *hashmapHitPos, lsi->first });

						//cout << "RID: " << *referenceID << "  *hashmapHitPos: " << *hashmapHitPos << " lsi->first:" << lsi->first << " *(occureMax + refID_occure): " << *(occureMax + (*referenceID- lowestRefID)) << endl;
					}
					referenceID++;
					hashmapHitPos++;
					ptrRefIDAboveHighestSuccesive++;
				}
				lsi++;
			}
		}

		if (measureDebugTimings)
		{
			debug_timing_3 = debug_timing_3 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();
			//std::cout << "tprepare Match loop took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count() << "\n";
			begin_time = high_resolution_clock::now();
		}

		if (FoundRefID_Pos_Pairs->size() == 0)
			continue;


		//// Also check if we haven't found the previous direct neighbour hash, so we're not going to count a region of hashes multiple times
		//combiPosition_Size_Highest = max(combiPosition_Size_Highest, FoundRefID_Pos_Pairs->size());


		int numThreads = min(argsForGPU->multiThreading->maxCPUThreads, 4);
		if (numThreads < 8)
			if (numThreads < 1)
				numThreads = 1;
			else if (numThreads < 3)
				numThreads = 2;
			else
				numThreads = 4;


		size_t FoundRefID_Pos_Pairs_size = FoundRefID_Pos_Pairs->size();
		mtSort_FoundRefID_Pos_Pairs(FoundRefID_Pos_Pairs->data(), FoundRefID_Pos_Pairs_size, 1);


		if (measureDebugTimings)
		{
			debug_timing_4 = debug_timing_4 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();
			//std::cout << "tprepare sort took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count() << "\n";
			begin_time = high_resolution_clock::now();
		}
		size_t sizeFoundRefIDs = FoundRefID_Pos_Pairs->size();
		size_t numUniqueRefHitID = 0;


		// Get Number of unique FoundReferenceID
		if (sizeFoundRefIDs > 1)
		{
			pair<SEQ_ID_t, Enum::Position> * ptrFRID = (&FoundRefID_Pos_Pairs->front());
			pair<SEQ_ID_t, Enum::Position> * ptrFRID_End = (&FoundRefID_Pos_Pairs->back());
			SEQ_ID_t prevID = ptrFRID->first;
			ptrFRID++;
			while (ptrFRID <= ptrFRID_End)
			{
				if (ptrFRID->first != prevID)
				{
					prevID = ptrFRID->first;
					numUniqueRefHitID++;
				}
				ptrFRID++;
			}
		}
		numUniqueRefHitID++;


		// If we have all different reference ID's, we will never be able to find 2 succesive hashes, so there is no use to continue searching for those.
		// If no reference is found with 2 or more succesive hashes, they will all never reach the threshold (since hash is half of threshold)
		if (refHitsIDs->size() == numUniqueRefHitID)
			continue;


		if (measureDebugTimings)
		{
			debug_timing_5 = debug_timing_5 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();
			//std::cout << "tprepare get unique took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count() << "\n";
			begin_time = high_resolution_clock::now();
		}



		// We could place this above the filling of refHitsIDs, but that may result in ending the resize while we would like to continue
		future<void> future_vector_Resize_combiHitsPositions = std::async(launch::deferred, vector_Resize_combiHitsPositions, combiHitsPositions, numUniqueRefHitID);
		future<void> future_vector_Resize_combiHitsPositions_LastPositionInRange = std::async(launch::deferred, vector_Resize_combiHitsPositions, combiHitsPositions_LastPositionInRange, numUniqueRefHitID);
		future_vector_Resize_combiHitsPositions.wait_for(chrono::milliseconds(1));
		future_vector_Resize_combiHitsPositions_LastPositionInRange.wait_for(chrono::milliseconds(1));



		//
		// Transform found RefIDs and positions to sorted combiPositions
		//
		if (sizeFoundRefIDs > 0)
		{


			refHitsIDs->reserve(numUniqueRefHitID);
			numOfCombi_Highest = max(numOfCombi_Highest, numUniqueRefHitID);
			combiPosition.resize(numOfCombi_Highest);
			pair<pair<SEQ_ID_t, Enum::Position>*, pair<SEQ_ID_t, Enum::Position>*>* ptrCP = &combiPosition[0];


			pair<SEQ_ID_t, Enum::Position>* ptrFRID_POSSEQ = &(*FoundRefID_Pos_Pairs)[0];
			pair<SEQ_ID_t, Enum::Position>* firstPos = ptrFRID_POSSEQ;
			pair<SEQ_ID_t, Enum::Position>* lastPos = ptrFRID_POSSEQ;

			SEQ_ID_t prevRefID = ptrFRID_POSSEQ->first;
			refHitsIDs->emplace_back(ptrFRID_POSSEQ->first); // Store the reference ID
			for (size_t i = 0; i < sizeFoundRefIDs; i++)
			{
				if (ptrFRID_POSSEQ->first != prevRefID)
				{
					*ptrCP = { firstPos, lastPos + 1 };
					ptrCP++;
					firstPos = ptrFRID_POSSEQ;
					refHitsIDs->emplace_back(ptrFRID_POSSEQ->first); // Store the reference ID
				}
				lastPos = ptrFRID_POSSEQ;
				prevRefID = ptrFRID_POSSEQ->first;
				ptrFRID_POSSEQ++;
			}

			*ptrCP = { firstPos, lastPos + 1 };
		}




		if (measureDebugTimings)
		{
			debug_timing_6 = debug_timing_6 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();
			//std::cout << "tprepare build combi took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
			begin_time = high_resolution_clock::now();
		}



		const bool bSuccesivePreCheck(true); // todo make this a mode in config (fast/high presicion)



		//
		// Get succesive hash counts
		//

		size_t highestSuccesiveHashFound = 0;
		vector<SEQ_ID_t> succesiveHashCounts;
		succesiveHashCounts.reserve(numUniqueRefHitID);
		vector<size_t> CombiTarget_Size_Required; // Keep track of required memory for later use
		CombiTarget_Size_Required.resize(numUniqueRefHitID, 0);
		size_t* ptrCombiTarget_Size_Required = &CombiTarget_Size_Required[0];

		pair< pair<SEQ_ID_t, Enum::Position>*, pair<SEQ_ID_t, Enum::Position>*>* ptrCombi = &((combiPosition)[0]);

		for (size_t relativeReferenceID = 0; relativeReferenceID < numUniqueRefHitID; relativeReferenceID++)
		{
			size_t numOfPositions = get<1>(*ptrCombi) - get<0>(*ptrCombi);

			pair<SEQ_ID_t, Enum::Position>* ptrPosition_CombiPosition_MinOne;
			pair<SEQ_ID_t, Enum::Position>* ptrPosition_CombiPosition;

			ptrPosition_CombiPosition = get<0>(*ptrCombi);
			ptrPosition_CombiPosition_MinOne = ptrPosition_CombiPosition;
			ptrPosition_CombiPosition++;

			size_t currentHighestSuccesiveHashFound = 0;
			size_t currentSuccesiveHashFound = 1;
			for (size_t iNumRef = 1; iNumRef < numOfPositions; iNumRef++)
			{
				Enum::Position* posPrev = &ptrPosition_CombiPosition_MinOne->second;
				Enum::Position* posCur = &ptrPosition_CombiPosition->second;


				if ((posPrev->Reference != posCur->Reference - hashmapHighestSteppingSize
					|| posPrev->Query != posCur->Query - hashmapHighestSteppingSize
					)
					)
				{
					//cout << "posPrev->Reference: " << posPrev->Reference << " posCur->Reference : " << posCur->Reference << "  posPrev->Query : " << posPrev->Query << "  posCur->Query : " << posCur->Query << "  currentSuccesiveHashFound: " << currentSuccesiveHashFound << endl;

					currentHighestSuccesiveHashFound = max(currentSuccesiveHashFound, currentHighestSuccesiveHashFound);
					currentSuccesiveHashFound = 0;
					(*ptrCombiTarget_Size_Required)++;
				}

				currentSuccesiveHashFound++;
				ptrPosition_CombiPosition_MinOne++;
				ptrPosition_CombiPosition++;
			}

			(*ptrCombiTarget_Size_Required)++;

			// Also add the last position
			currentHighestSuccesiveHashFound = max(currentSuccesiveHashFound, currentHighestSuccesiveHashFound);
			succesiveHashCounts.emplace_back(currentHighestSuccesiveHashFound); // Keep track of the succesive hash sizes, so we don't starting to build combiTargets when not required

			// Track all high
			highestSuccesiveHashFound = max(currentHighestSuccesiveHashFound, highestSuccesiveHashFound);

			// Goto next
			ptrCombi++;
			ptrCombiTarget_Size_Required++;

		}


		future_vector_Resize_combiHitsPositions.wait();
		future_vector_Resize_combiHitsPositions_LastPositionInRange.wait();

		// If no reference is found with 2 or more succesive hashes, they will all never reach the threshold (since hash is half of threshold)
		if (highestSuccesiveHashFound < 2)
			continue;

		if (measureDebugTimings)
		{
			debug_timing_7 = debug_timing_7 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();

			//std::cout << "tprepare precalc max hash succesive took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
			begin_time = high_resolution_clock::now();
		}


		ptrCombi = &((combiPosition)[0]);
		vector<Enum::Position>* ptrCombiTarget_LastPositionInRange = &(*combiHitsPositions_LastPositionInRange)[0];
		vector<Enum::Position>* ptrCombiTarget = &(*combiHitsPositions)[0];

		SEQ_ID_t* ptrSHC = &succesiveHashCounts[0];
		ptrCombiTarget_Size_Required = &CombiTarget_Size_Required[0];

		const bool bSpeedmodeIsUltraFast = Analyser::SpeedMode == Enum::Analysis_Modes::UltraFast;
		for (size_t relativeReferenceID = 0; relativeReferenceID < numUniqueRefHitID; relativeReferenceID++)
		{
			// Since higher succesive parts of equal sequences will result in much higher scores, discard all low succesive parts based only on the found hashes.
			// This is a little less accurate since more succesive parts in 1 reference migh result in a higher score compared to a reference with just 1 large succesive part.
			// But this is a tradeoff between less memory usage + less analysis/preperation time (less sequences to load from disk and less scores to calculate) and high accuracy when comparing everything

			bool skipCurrent(false);
			if (bSuccesivePreCheck)
				if (*ptrSHC < highestSuccesiveHashFound)
					skipCurrent = true;

			//// If reference has no 2 or more succesive hashes, it will never reach the threshold (since hash is half of threshold)
			//if (bSpeedmodeIsUltraFast)
			if (!skipCurrent && *ptrSHC < 2)
				skipCurrent = true;

			if (!skipCurrent)
			{
				size_t numOfPositions = get<1>(*ptrCombi) - get<0>(*ptrCombi);

				// Order
				pair<SEQ_ID_t, Enum::Position>* ptrPosition_CombiPosition_MinOne;
				pair<SEQ_ID_t, Enum::Position>* ptrPosition_CombiPosition;

				ptrCombiTarget_LastPositionInRange->reserve(*ptrCombiTarget_Size_Required);
				ptrCombiTarget->reserve(*ptrCombiTarget_Size_Required);


				// Now add combitargets
				ptrPosition_CombiPosition = get<0>(*ptrCombi);
				ptrPosition_CombiPosition_MinOne = ptrPosition_CombiPosition;
				ptrCombiTarget->emplace_back(ptrPosition_CombiPosition->second.Reference, ptrPosition_CombiPosition->second.Query);
				ptrPosition_CombiPosition++;



				for (size_t iNumRef = 1; iNumRef < numOfPositions; iNumRef++)
				{
					Enum::Position* posPrev = &ptrPosition_CombiPosition_MinOne->second;
					Enum::Position* posCur = &ptrPosition_CombiPosition->second;


					if (posPrev->Reference != posCur->Reference - hashmapHighestSteppingSize
						|| posPrev->Query != posCur->Query - hashmapHighestSteppingSize)
					{
						ptrCombiTarget->emplace_back(posCur->Reference, posCur->Query);

						ptrCombiTarget_LastPositionInRange->emplace_back(posPrev->Reference, posPrev->Query);
						ptrCombiTarget_LastPositionInRange->back().Query += hashmapHighestSteppingSize;
						ptrCombiTarget_LastPositionInRange->back().Reference += hashmapHighestSteppingSize;

					}
					ptrPosition_CombiPosition_MinOne++;
					ptrPosition_CombiPosition++;
				}

				// Also add the last position
				ptrCombiTarget_LastPositionInRange->emplace_back(ptrPosition_CombiPosition_MinOne->second.Reference, ptrPosition_CombiPosition_MinOne->second.Query);
				ptrCombiTarget_LastPositionInRange->back().Query += hashmapHighestSteppingSize;
				ptrCombiTarget_LastPositionInRange->back().Reference += hashmapHighestSteppingSize;
			}

			// goto Next
			ptrCombiTarget++;
			ptrCombiTarget_LastPositionInRange++;
			ptrCombi++;
			ptrSHC++;
			ptrCombiTarget_Size_Required++;
		}

		if (measureDebugTimings)
		{
			debug_timing_8 = debug_timing_8 + duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_time).count();
			//std::cout << "tprepare remain head tail took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
			begin_time = high_resolution_clock::now();
		}

		succesiveHashCounts.clear();
	}
	delete[] ptrRefIDAboveHighestSuccesive_start;




	argsForGPU->multiThreading->mutexGPUPrepare.lock();
	argsForGPU->multiThreading->mutMainMultithreads.lock();

	if (argsForGPU->hardware->Type[argsForGPU->DeviceID] == Hardware::HardwareTypes::GPU)
		argsForGPU->multiThreading->SetGPUActive(false, argsForGPU->hardware->DeviceID[argsForGPU->DeviceID]);

	(argsForGPU->AnalysisArguments->runningThreadsGPUPreAnalysis)--;
	argsForGPU->multiThreading->threadsFinished.push_back(argsForGPU->ThreadID);
	argsForGPU->multiThreading->mutMainMultithreads.unlock();
	argsForGPU->multiThreading->mutexGPUPrepare.unlock();


	// Do not wait, since we have a static amount of worker jobs processing these threads

	//// Wait until all are ready, so we do not waste CPU time by deallocating memory, do at the last moment possible
	//while (argsForGPU->multiThreading->runningThreadsGPUPreAnalysis > 0)
	//	argsForGPU->multiThreading->SleepGPU();

	// Clean up
	delete[] occure_start;
	delete[] occureMax_Start;
	delete[] occure_LastPos;
	//delete FoundRefID_Pos_Pairs;


	for (auto e : loadedStreamIterators)
		for (auto a : e)
			for (auto d : a)
				delete d.second->bufferPosition;

	vector<vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>>().swap(loadedStreamIterators);

	// Store in trashcan for later cleanup
	if (UseTrashCan)//&& Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::High)
	{
		if (combiPosition.size() > 0)
		{
			mTrashCan.lock();

			trashcan.push_back(move(combiPosition));
			//trashcan_MatchedHashStreamPos.emplace_back(move(MatchedHashStreamPos));
			//trashcan_buffer_total_start.emplace_back(move(buffer_total_start));
			//trashcan_loadedStreamIterators.emplace_back(move(loadedStreamIterators));

			//trashcan_PrepareGPU_Arguments.emplace_back(argsForGPU);
			mTrashCan.unlock();
		}
	}
	//else
	//{
	//	//vector<hashmap_MatchedHashStreamPos>().swap(MatchedHashStreamPos);

	//	//for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	//	//	delete[] buffer_total_start[iFileID];
	//}

	//vector<hashmap_MatchedHashStreamPos>().swap(MatchedHashStreamPos);

	//for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	//	delete[] buffer_total_start[iFileID];


	// IS now deleted in the worker thread
	//delete argsForGPU;


	//if (debug_timing)
	//{
	//	std::cout << "debug_timing_1: " << debug_timing_1 << "\n";
	//	std::cout << "debug_timing_2a: " << debug_timing_2a << "\n";
	//	std::cout << "debug_timing_2: " << debug_timing_2 << "\n";
	//	std::cout << "debug_timing_3: " << debug_timing_3 << "\n";
	//	std::cout << "debug_timing_4: " << debug_timing_4 << "\n";
	//	std::cout << "debug_timing_5: " << debug_timing_5 << "\n";
	//	std::cout << "debug_timing_6: " << debug_timing_6 << "\n";
	//	std::cout << "debug_timing_7: " << debug_timing_7 << "\n";
	//	std::cout << "debug_timing_8: " << debug_timing_8 << "\n";
	//	std::cout << "debug_timing_9: " << debug_timing_9 << "\n";
	//}
	return NULL;

}
//
//void Analyser::InitOccureBuffer(std::vector<SEQ_ID_t> &highestRefID, const size_t &iFileID, size_t &occureBufferSize, int * &occure_start, int * &occure, int * &occureMax_Start, int * &occureMax, int * &occure_LastPos)
//{
//
//	size_t requiredBufferSize = highestRefID[iFileID] + 1;
//	if (requiredBufferSize > occureBufferSize)
//	{
//		occureBufferSize = requiredBufferSize;
//		delete[]  occure_start;
//		occure = new int[occureBufferSize];
//		occure_start = occure;
//
//
//		delete[]  occureMax_Start;
//		occureMax = new int[occureBufferSize];
//		occureMax_Start = occureMax;
//
//		delete[]  occure_LastPos;
//		occure_LastPos = new int[occureBufferSize];
//	}
//	else
//	{
//		occure = occure_start;
//		occureMax = occureMax_Start;
//	}
//	memset(occure, 0, requiredBufferSize * sizeof(int));
//	memset(occureMax, 0, requiredBufferSize * sizeof(int));
//	memset(occure_LastPos, -1, requiredBufferSize * sizeof(int));
//}

// Initialise occure buffer
void Analyser::InitialiseOccureBuffer(SEQ_ID_t highestRefID, size_t &occureBufferSize, int * &occure_start, int * &occure, int * &occureMax_Start, int * &occureMax, int * &occure_LastPos)
{
	size_t requiredBufferSize = highestRefID + 1;
	if (requiredBufferSize > occureBufferSize)
	{
		occureBufferSize = requiredBufferSize;
		delete[]  occure_start;
		occure = new int[occureBufferSize];
		occure_start = occure;

		delete[]  occureMax_Start;
		occureMax = new int[occureBufferSize];
		occureMax_Start = occureMax;

		delete[]  occure_LastPos;
		occure_LastPos = new int[occureBufferSize];
	}
	else
	{
		occure = occure_start;
		occureMax = occureMax_Start;
	}
	memset(occure, 0, requiredBufferSize * sizeof(int));
	memset(occureMax, 0, requiredBufferSize * sizeof(int));
	memset(occure_LastPos, -1, requiredBufferSize * sizeof(int));
}

// Open file stream with retries
void OpenStream(ifstream* s, char* file)
{
	size_t retryCount = 0;

	while (!s->is_open())
	{
		try
		{
			s->open(file, ios::in | ios::binary);
		}
		catch (...) {

			std::this_thread::sleep_for(Sequences::FILE_OPEN_FAILED_WAIT_MS); // Just give the CPU some rest..

			if (retryCount >= Sequences::MAX_RETRY_OPEN_FILE)
			{
				//display->logWarningMessage << "Unable to open file handle to hashmap!" << endl;
				//display->logWarningMessage << "Does your machine have enough resources left?" << endl;
				//display->logWarningMessage << "Too many file handles opened." << endl;
				//display->logWarningMessage << "Program will stop." << endl;
				//display->FlushWarning(true);
				cerr << "Unable to open new file stream for " << file;
				exit(1);
			}
		}
		retryCount++;
	}
}

#include<thread>

// Pre-reserve memory for head-tail sequences
void Analyser::PreloadReferences_HeadTail_Prereserve_Mem(vector<SEQ_ID_t>* R, vector<Enum::LazyData>* lazyData, vector<SEQ_ID_t>* lazyDataSizes, vector<vector<Enum::Position>>* combiPosition, vector<vector<Enum::Position>>* combiPosition_Last, vector<vector<Enum::HeadTailSequence>>* dest, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, size_t* ReferenceSequences_HeadTail_Buffer_Size, mutex* mutex_ReferenceSequences_HeadTail_Buffer_Size)
{
	if (R->size() == 0)
		return;

	dest->reserve(R->size());

	int prev_HashmapID = Hashmap::GetHashmapID((*R)[0]);

	vector<Enum::HeadTailSequence>* ptrDest;
	vector<Enum::Position>* ptrCombiPosition = &(*combiPosition)[0];
	vector<Enum::Position>* ptrCombiPosition_Last = &(*combiPosition_Last)[0];
	size_t totalHeadTailChars = 0;
	for (size_t i = 0; i < R->size(); i++)
	{
		const size_t size_combiPosition = ptrCombiPosition->size();
		dest->emplace_back();
		ptrDest = &dest->back();
		ptrDest->reserve(size_combiPosition);
		if (size_combiPosition > 0)
		{
			ptrDest->emplace_back();
			Enum::HeadTailSequence* ptrHTS = &ptrDest->back();
			Enum::Position* ptrCombiPosition_Pos = &(*ptrCombiPosition)[0];
			Enum::Position* ptrCombiPosition_Last_Pos = &(*ptrCombiPosition_Last)[0];
			size_t refSize = (*lazyDataSizes)[(*R)[i]];
			for (size_t cID = 0; cID < size_combiPosition; cID++)
			{

				size_t numCharsTail = min(considerThreshold, refSize - ptrCombiPosition_Last_Pos->Reference);
				size_t numCharsHead = std::min<size_t>(ptrCombiPosition_Pos->Reference, considerThreshold);
				totalHeadTailChars += numCharsTail;
				totalHeadTailChars += numCharsHead;

				//if (Analyser::UltraFastMode)
				//{
				//	ptrHTS->Head_Size = 0;
				//	ptrHTS->Tail_Size = 0;
				//}
				//else
				//{
				ptrHTS->Head_Size = numCharsHead;
				ptrHTS->Tail_Size = numCharsTail;
				//}
				ptrHTS++;

				ptrCombiPosition_Pos++;
				ptrCombiPosition_Last_Pos++;

			}
		}
		ptrDest++;
		ptrCombiPosition++;
		ptrCombiPosition_Last++;
	}

	mutex_ReferenceSequences_HeadTail_Buffer_Size->lock();
	*ReferenceSequences_HeadTail_Buffer_Size += totalHeadTailChars;
	mutex_ReferenceSequences_HeadTail_Buffer_Size->unlock();
}


#include<thread>
// Preload head-tail sequences from references
void Analyser::PreloadReferences_HeadTail(vector<SEQ_ID_t>* R, vector<Enum::LazyData>* lazyData, vector<size_t>* lazyDataSizes, vector<vector<Enum::Position>>* combiPosition, vector<vector<Enum::Position>>* combiPosition_Last, vector<vector<Enum::HeadTailSequence>>* dest, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, vector<hashmap_HeadTailBuffer>* HashmapHeadTailBufferst, vector<vector<char>>* tempReferenceSequences)
{
	(*dest).resize(R->size());

	int prev_HashmapID = Hashmap::GetHashmapID((*R)[0]);
	Enum::LazyData* ptrLazyData = &(*lazyData)[prev_HashmapID];

	ifstream* s = new ifstream();
	s->rdbuf()->pubsetbuf(0, 0);
	OpenStream(s, &Hashmap::HashmapFileLocations[Hashmap::GetHashmapID((*R)[0])][0]);

	vector<Enum::HeadTailSequence>* ptrDest = &(*dest)[0];
	vector<Enum::Position>* ptrCombiPosition = &(*combiPosition)[0];
	vector<Enum::Position>* ptrCombiPosition_Last = &(*combiPosition_Last)[0];


	vector<hashmap_HeadTailBuffer*>* HashmapHeadTailBuffers = new vector<hashmap_HeadTailBuffer*>();
	HashmapHeadTailBuffers->reserve(Hashmap::HashmapFileLocations.size());

	// Initialise head-tail buffers for each hashmap file
	for (int i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
	{
		hashmap_HeadTailBuffer* ghh = new hashmap_HeadTailBuffer();
		HashmapHeadTailBuffers->emplace_back(ghh);
	}
	hashmap_uniques_seq_IDs LoadedReferenceID;

	// Load head-tail sequences
	for (size_t i = 0; i < R->size(); i++)
	{
		int hashmapID = Hashmap::GetHashmapID((*R)[i]);

		if (hashmapID != prev_HashmapID)
		{
			Enum::LazyData* ptrLazyData = &(*lazyData)[hashmapID];

			s->close();
			OpenStream(s, &Hashmap::HashmapFileLocations[Hashmap::GetHashmapID((*R)[i])][0]);
			prev_HashmapID = hashmapID;
		}
		SEQ_ID_t rID = (*R)[i];
		size_t size_combiPosition = ptrCombiPosition->size();

		Enum::Position* ptrCombiPosition_Pos = &(*ptrCombiPosition)[0];
		Enum::Position* ptrCombiPosition_Last_Pos = &(*ptrCombiPosition_Last)[0];
		Enum::HeadTailSequence* ptrHTS = &(*ptrDest)[0];


		// Load head-tail sequences for each combiPosition
		for (size_t cID = 0; cID < size_combiPosition; cID++)
		{

			if ((*tempReferenceSequences)[rID].size() > 0) // check if cache is used
				Sequences::GetLazyContent_HeadTail(lazyDataSizes, rID, ptrCombiPosition_Pos, ptrCombiPosition_Last_Pos, ptrHTS, considerThreshold, &(*tempReferenceSequences)[rID]);
			else
				Sequences::GetLazyContent_HeadTail(lazyData, lazyDataSizes, rID, ptrCombiPosition_Pos, ptrCombiPosition_Last_Pos, ptrHTS, considerThreshold, s, refStreamPos, hashmapID, ((*HashmapHeadTailBuffers)[hashmapID]), ptrLazyData);
			ptrCombiPosition_Pos++;
			ptrCombiPosition_Last_Pos++;
			ptrHTS++;
		}


		ptrDest++;
		ptrCombiPosition++;
		ptrCombiPosition_Last++;
	}
	s->close();
	delete s;


	//Sequences::GetLazyContentForCuda(&sequences->referenceSequences, &Hashmap::LazyLoadingCacheCounter, &sequences->referenceSequences_Lazy, rID, nullptr, nullptr, true);
}

// Preload references from disk
void TPreloadReferences_LoadFromDisk(Analyser::References_ToLoad* rtl_Current, Analyser::References_ToLoad* rtl_end)
{
	//const size_t sizeBuffer = 20 * 1024 * 1024;
	//char Buffer[sizeBuffer];
	//char* Buffer = new char[sizeBuffer];


	ifstream* s = new ifstream();
	//s->rdbuf()->pubsetbuf(Buffer, sizeBuffer);

	int prev_HashmapID = 0;
	OpenStream(s, &Hashmap::HashmapFileLocations[0][0]);

	// Load references
	while (rtl_Current <= rtl_end)
	{
		if (rtl_Current->dstSize == 0)
			continue;

		if (rtl_Current->Hashmap_ID != prev_HashmapID)
		{
			s->close();
			OpenStream(s, &Hashmap::HashmapFileLocations[rtl_Current->Hashmap_ID][0]);
			prev_HashmapID = rtl_Current->Hashmap_ID;
		}
		Sequences::GetLazyContent_HeadTail(s, &rtl_Current->HashStreamPosition, rtl_Current->dstReference, rtl_Current->dstSize);

		rtl_Current++;
	}

	s->close();
	delete s;

}

// Preload references from disk using local buffer and memcpy
void TPreloadReferences_LoadFromDisk_LocalBuffer_Memcpy(vector<Analyser::References_ToLoad*> shortList, HASH_STREAMPOS firstPosInShortList, char* readBuffer)
{
	Analyser::References_ToLoad** ptrShortList = &shortList[0];
	size_t ShortListSize = shortList.size();
	for (size_t i = 0; i < ShortListSize; i++)
	{
		memcpy((*ptrShortList)->dstReference, readBuffer + ((*ptrShortList)->HashStreamPosition - firstPosInShortList), (*ptrShortList)->dstSize);
		ptrShortList++;
	}

	delete[] readBuffer;
}

// Preload references from disk using local buffer and grouping reads
void TPreloadReferences_LoadFromDisk_LocalBuffer(Analyser::References_ToLoad* rtl_Current, Analyser::References_ToLoad* rtl_end)
{
	ifstream s;
	char* LocalIFStreamBuffer;
	size_t LocalIFStreamBufferSize = 4096;
	LocalIFStreamBuffer = new char[LocalIFStreamBufferSize];

	int prev_HashmapID = 0;

	s.rdbuf()->pubsetbuf(LocalIFStreamBuffer, LocalIFStreamBufferSize);

	OpenStream(&s, &Hashmap::HashmapFileLocations[0][0]);
	vector<char>* prev_DstReference = nullptr;

	const size_t maxSizeBytes = 1 * 1000 * 1000; //  How many bytes can 1 disk read do
	const size_t maxByteDiffernce = 1 * 1000; // How many bytes may 2 different heads be apart from each other
	HASH_STREAMPOS firstPosInShortList = 0;
	int firstHashmapIDInShortList = 0;


	size_t currentReadBufferSize = 0;
	char* readBuffer = new char[currentReadBufferSize];


	Analyser::References_ToLoad* shortList_Start = rtl_Current;
	Analyser::References_ToLoad* rtl_Previous = rtl_Current;

	firstPosInShortList = shortList_Start->HashStreamPosition;
	firstHashmapIDInShortList = shortList_Start->Hashmap_ID;

	
	while (true)
	{

		if (rtl_Current > rtl_end ||
			rtl_Current->HashStreamPosition - rtl_Previous->HashStreamPosition > maxByteDiffernce ||
			rtl_Current->HashStreamPosition - firstPosInShortList > maxSizeBytes ||
			rtl_Current->Hashmap_ID != firstHashmapIDInShortList)
		{


			if (firstHashmapIDInShortList != prev_HashmapID)
			{
				s.close();
				OpenStream(&s, &Hashmap::HashmapFileLocations[firstHashmapIDInShortList][0]);
				prev_HashmapID = firstHashmapIDInShortList;

			}

			size_t readSize = (rtl_Current - 1)->HashStreamPosition - shortList_Start->HashStreamPosition;
			readSize += (rtl_Current - 1)->dstSize;

			// Only allocate new buffer when we need more than the previous round
			if (readSize > currentReadBufferSize)
			{
				delete[] readBuffer;
				readBuffer = new char[readSize];
			}

			Sequences::GetLazyContent_HeadTail(&s, &shortList_Start->HashStreamPosition, readBuffer, readSize);

			size_t ShortListSize = distance(shortList_Start, rtl_Current);
			Analyser::References_ToLoad* ptrShortList = shortList_Start;

			for (size_t i = 0; i < ShortListSize; i++)
			{
				memcpy(ptrShortList->dstReference, readBuffer + (ptrShortList->HashStreamPosition - firstPosInShortList), ptrShortList->dstSize);
				ptrShortList++;
			}


			if (rtl_Current > rtl_end)
				break;

			shortList_Start = rtl_Current;
			firstPosInShortList = shortList_Start->HashStreamPosition;
			firstHashmapIDInShortList = shortList_Start->Hashmap_ID;
		}

		rtl_Previous = rtl_Current;
		rtl_Current++;
	}

	delete[] readBuffer;

	s.close();
}



// Preload references in query list
void Analyser::PreloadReferences_InQueryList(Sequences* sequences, vector<vector<SEQ_ID_t>>* References, vector<vector<vector<Enum::Position>>>* comboPosition, vector<vector<vector<Enum::Position>>>* combiPosition_Last, vector<vector<vector<Enum::HeadTailSequence>>>* headTailSequences, vector<HASH_STREAMPOS>* refStreamPos, size_t considerThreshold, char** ReferenceSequences_Buffer)
{

	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::Low)
		return;

	if (comboPosition->size() == 0)
		return;

	//bool useCache(true);
	//vector<vector<char>> temp;
	//vector<vector<char>>* ptrTemp = &sequences->referenceSequences;

	size_t startTimeThreads_InQuery = clock();
	size_t startTimeThreads_InQuery_FirstPart = clock();

	float mDebugTime;

	auto begin = high_resolution_clock::now();


	//if (useCache && Analyser::GetMemFreePercentage() - 5 > Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold)
	//{
	//	(*ptrTemp).resize(sequences->referenceSequences.size());
	//	for (auto i : *References)
	//		for (auto j : i)
	//			if ((*ptrTemp)[j].size() == 0)
	//				Sequences::GetLazyContentASync(refStreamPos, &sequences->referenceSequencesSizes, j, &(*ptrTemp)[j]);
	//}

	//vector<hashmap_HeadTailBuffer>* HashmapHeadTailBuffers = new vector<hashmap_HeadTailBuffer>();
	//HashmapHeadTailBuffers->resize(Hashmap::HashmapFileLocations.size());


	size_t count = 0;
	vector<vector<Enum::Position>>* ptrCP = &(*comboPosition)[0];
	for (int i = 0; i < combiPosition_Last->size(); i++)
	{
		vector<Enum::Position>* ptrP = &(*ptrCP)[0];
		for (int j = 0; j < ptrCP->size(); j++)
		{
			count += ptrP->size();
			ptrP++;
		}
		ptrCP++;
	}
	vector<future<void>> _futures;



	begin = high_resolution_clock::now();

	vector<vector<Enum::Position>>* ptrCombiPosition = &(*comboPosition)[0];
	vector<vector<Enum::Position>>* ptrCombiPosition_Last = &(*combiPosition_Last)[0];
	vector<vector<Enum::HeadTailSequence>>* ptrheadTailSequences = &(*headTailSequences)[0];


	size_t ReferenceSequences_HeadTail_Buffer_Size = 0;
	mutex mutex_ReferenceSequences_HeadTail_Buffer;
	for (int i = 0; i < References->size(); i++)
	{
		//Asynchronise::wait_for_futures_on_full_vector(&_futures, min(multiThreading->maxCPUThreads, 10000));


		if ((*References)[i].size() != 0)
			PreloadReferences_HeadTail_Prereserve_Mem(&(*References)[i], &sequences->referenceSequences_Lazy, &sequences->referenceSequencesSizes, ptrCombiPosition, ptrCombiPosition_Last, ptrheadTailSequences, refStreamPos, considerThreshold, &ReferenceSequences_HeadTail_Buffer_Size, &mutex_ReferenceSequences_HeadTail_Buffer);
		//PreloadReferences_HeadTail_Prereserve_Mem, &(*References)[i], &sequences->referenceSequences_Lazy, &sequences->referenceSequencesSizes, ptrCombiPosition, ptrCombiPosition_Last, ptrheadTailSequences, refStreamPos, considerThreshold, &ReferenceSequences_HeadTail_Buffer_Size, &mutex_ReferenceSequences_HeadTail_Buffer)	_futures.push_back(std::async(launch::async, PreloadReferences_HeadTail_Prereserve_Mem, &(*References)[i], &sequences->referenceSequences_Lazy, &sequences->referenceSequencesSizes, ptrCombiPosition, ptrCombiPosition_Last, ptrheadTailSequences, refStreamPos, considerThreshold, &ReferenceSequences_HeadTail_Buffer_Size, &mutex_ReferenceSequences_HeadTail_Buffer));


		ptrCombiPosition++;
		ptrCombiPosition_Last++;
		ptrheadTailSequences++;
	}

	Asynchronise::wait_for_futures(&_futures);

	*ReferenceSequences_Buffer = new char[ReferenceSequences_HeadTail_Buffer_Size];
	//ReferenceSequences_Buffer = (char*)malloc(ReferenceSequences_HeadTail_Buffer_Size);



	//std::cout << " reserve mem: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();



	//cout << "Load from disk..." << endl;


	// Build list of pointers to be able to load the sequences from disk
	ptrCombiPosition = &(*comboPosition)[0];
	ptrCombiPosition_Last = &(*combiPosition_Last)[0];
	ptrheadTailSequences = &(*headTailSequences)[0];


	char* ptrHTLBuffer = *ReferenceSequences_Buffer;


	vector<References_ToLoad>* toLoad = new vector<References_ToLoad>();
	toLoad->reserve(count * 2);



	//dit kan je wel multithread..elke thread een vector<... to load...> local laten maken en op het eind een mutex en .insert.. in de master

	for (int i = 0; i < References->size(); i++)

	{

		vector<Enum::Position>* ptrCP_i = &(*ptrCombiPosition)[0];
		vector<Enum::Position>* ptrCP_L_i = &(*ptrCombiPosition_Last)[0];
		vector<Enum::HeadTailSequence>* ptrHT_i = &(*ptrheadTailSequences)[0];
		vector<SEQ_ID_t>* ptrReference = &(*References)[i];

		for (int j = 0; j < ptrCombiPosition->size(); j++)
		{
			SEQ_ID_t referenceID = (*ptrReference)[j];
			size_t referenceSize = sequences->referenceSequencesSizes[referenceID];
			HASH_STREAMPOS startStreamPos = (*refStreamPos)[referenceID];

			int hashmapID = Hashmap::GetHashmapID(referenceID);



			Enum::Position* ptrCP_ij = &(*ptrCP_i)[0];
			Enum::Position* ptrCP_L_ij = &(*ptrCP_L_i)[0];
			Enum::HeadTailSequence* ptrHT_ij = &(*ptrHT_i)[0];

			for (int t = 0; t < ptrCP_i->size(); t++)
			{
				size_t numCharsHead = std::min<size_t>(ptrCP_ij->Reference, considerThreshold);
				size_t numCharsTail = min(considerThreshold, referenceSize - ptrCP_L_ij->Reference);
				HASH_STREAMPOS StreamPos = startStreamPos;
				StreamPos += ptrCP_ij->Reference;
				StreamPos -= numCharsHead;

				ptrHT_ij->SequenceLength = referenceSize;
				ptrHT_ij->Head = ptrHTLBuffer;
				if (numCharsHead != 0)
					toLoad->emplace_back(StreamPos, ptrHTLBuffer, hashmapID, numCharsHead);
				//toLoad->emplace_back(StreamPos, &ptrHT_ij->Head, hashmapID);
				ptrHTLBuffer = ptrHTLBuffer + numCharsHead;



				StreamPos += numCharsHead;
				StreamPos += ptrCP_L_ij->Reference - ptrCP_ij->Reference;
				ptrHT_ij->Tail = ptrHTLBuffer;
				if (numCharsTail != 0)
					toLoad->emplace_back(StreamPos, ptrHTLBuffer, hashmapID, numCharsTail);
				//toLoad->emplace_back(StreamPos, &ptrHT_ij->Tail, hashmapID);
				ptrHTLBuffer = ptrHTLBuffer + numCharsTail;


				ptrCP_ij++;
				ptrCP_L_ij++;
				ptrHT_ij++;
			}

			ptrCP_i++;
			ptrCP_L_i++;
			ptrHT_i++;
		}


		ptrCombiPosition++;
		ptrCombiPosition_Last++;
		ptrheadTailSequences++;
	}



	//std::cout << " Make to load vector: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	//int numThreads = min(multiThreading->maxCPUThreads, 4);
	//if (numThreads < 4)
	//	if (numThreads < 1)
	//		numThreads = 1;
	//	else
	//		numThreads = 2;

	mtSort_ToLoad(toLoad->data(), toLoad->size(), 1);

	//std::cout << " Sorted load vector: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	begin = high_resolution_clock::now();

	size_t numOfThreads = 20;// min((size_t)multiThreading->maxCPUThreads, min(toLoad->size(), (size_t)200)); // Don't use to much, since every thread opens a new ifstream which takes time
	numOfThreads = max((size_t)1, numOfThreads);
	//size_t numOfToLoads_PerThread = 3000;
	//numOfThreads = max((size_t)1, min((size_t)multiThreading->maxCPUThreads, toLoad->size() / numOfToLoads_PerThread));


	if (numOfThreads != 0 &&
		toLoad->size() > 0)
	{
		size_t numOf_ToLoad_perThread = max((size_t)1, toLoad->size() / numOfThreads);


		Analyser::References_ToLoad* ptrBeginToLoad = &(*toLoad)[0];
		Analyser::References_ToLoad* ptrEndToLoad = &(*toLoad)[toLoad->size() - 1];

		for (int i = 0; i < numOfThreads; i++)
		{

			size_t start = i * numOf_ToLoad_perThread;
			size_t end = (i * numOf_ToLoad_perThread) + numOf_ToLoad_perThread - 1;

			if (i == numOfThreads - 1)
				_futures.push_back(std::async(launch::async, TPreloadReferences_LoadFromDisk_LocalBuffer, ptrBeginToLoad + start, ptrEndToLoad));
			else
				_futures.push_back(std::async(launch::async, TPreloadReferences_LoadFromDisk_LocalBuffer, ptrBeginToLoad + start, ptrBeginToLoad + end));
		}


		Asynchronise::wait_for_futures(&_futures);

		//std::cout << " Loaded from disk: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		begin = high_resolution_clock::now();
	}



	//ga ik nu zo snel met memcpy dat ik tegen ene probleem aan loop die out-of-sync issues geeft?
	if (UseTrashCan)// && Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::High)
	{
		mTrashCan.lock();
		//trashcan_HashmapHeadTailBuffers.push_back(HashmapHeadTailBuffers);
		trashcan_Reference_ToLoad.push_back(toLoad);
		mTrashCan.unlock();
	}
	else
	{
		delete toLoad;
		//delete HashmapHeadTailBuffers;
	}
	//}



}


vector<future<void>> fDeletetrashcan;
mutex mClearThrashCan;
atomic_bool stopClearTrashcan(false);
// Clear trashcan memory, is called in low cpu usage moments
void ClearThrashCan(bool EnsistRun = false)
{

	if (stopClearTrashcan)
		return;
	if (!EnsistRun)
	{
		if (!mClearThrashCan.try_lock())
			return;

		if (trashcan.size() == 0 &&
			trashcan_Reference_ToLoad.size() == 0 &&
			trashcan_HashmapHeadTailBuffers.size() == 0 &&
			trashcan_MatchedHashStreamPos.size() == 0 &&
			trashcan_buffer_total_start.size() == 0 &&
			trashcan_loadedStreamIterators.size() == 0 &&
			trashcan_PrepareGPU_Arguments.size() == 0 &&
			trashcan_Analysis_Arguments.size() == 0)
		{
			mClearThrashCan.unlock();
			return;
		}
	}

	vector<Analyser::PrepareGPU_Arguments*> trashcan_PrepareGPU_Arguments_Copy;

	mTrashCan.lock();
	vector<vector<vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>>> trashcan_loadedStreamIterators_Copy;


	vector<vector<pair<pair<SEQ_ID_t, Enum::Position>*, pair<SEQ_ID_t, Enum::Position>*>>> trashcan_Tmp;
	vector<vector<Analyser::References_ToLoad>*> trashcan_Reference_ToLoad_Tmp;
	vector<vector<hashmap_HeadTailBuffer>*> trashcan_HashmapHeadTailBuffers_Tmp;
	vector< Analyser::Analysis_Arguments*>  trashcan_Analysis_Arguments_Copy;
	vector<vector<unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>>> trashcan_MatchedHashStreamPos_Copy;
	vector<vector<byte*>> trashcan_buffer_total_start_Copy;


	trashcan_Tmp.swap(trashcan);
	trashcan_Reference_ToLoad_Tmp.swap(trashcan_Reference_ToLoad);
	trashcan_HashmapHeadTailBuffers_Tmp.swap(trashcan_HashmapHeadTailBuffers);
	trashcan_MatchedHashStreamPos_Copy.swap(trashcan_MatchedHashStreamPos);
	trashcan_buffer_total_start_Copy.swap(trashcan_buffer_total_start);
	trashcan_loadedStreamIterators_Copy.swap(trashcan_loadedStreamIterators);
	trashcan_PrepareGPU_Arguments_Copy.swap(trashcan_PrepareGPU_Arguments);



	mTrashCan.unlock();


	if (stopClearTrashcan)
		return;

	mTrashCan_Analysis_Arguments.lock();
	trashcan_Analysis_Arguments_Copy.swap(trashcan_Analysis_Arguments);
	mTrashCan_Analysis_Arguments.unlock();


	for (auto t : trashcan_Tmp)
	{


		if (stopClearTrashcan)
			return;
		//t.clear();
		//t.shrink_to_fit();
	}



	for (auto t : trashcan_Reference_ToLoad_Tmp)
	{

		if (stopClearTrashcan)
			return;
		//t->clear();
		//t->shrink_to_fit();
		delete t;
	}

	for (auto t : trashcan_PrepareGPU_Arguments_Copy)
	{

		if (stopClearTrashcan)
			return;

		delete t;
	}

	for (auto t : trashcan_HashmapHeadTailBuffers_Tmp)
	{

		if (stopClearTrashcan)
			return;
		//t->clear();
		//t->shrink_to_fit();
		delete t;
	}

	for (auto t : trashcan_loadedStreamIterators_Copy)
	{

		if (stopClearTrashcan)
			return;
		vector<vector<vector<pair<SEQ_POS_t, BufferHashmapToReadFromFile*>>>>().swap(t);
	}
	for (auto t : trashcan_Analysis_Arguments_Copy)
	{

		if (stopClearTrashcan)
			return;

		delete[] t->ReferenceSequences_Buffer;

		delete t;
	}

	for (auto t : trashcan_buffer_total_start_Copy)
	{
		if (stopClearTrashcan)
			return;
		for (auto e : t)
		{
			delete[] e;
		}
	}


	if (!EnsistRun)
		mClearThrashCan.unlock();
}

mutex fWorkerThreads_tPrepare_Mutex;
int numWorkers_Max_Concurent_Active = 1;
atomic_int numWorkers_Active;
atomic_bool TPrepareGPUAnalysis_Worker_Active;
vector< Analyser::PrepareGPU_Arguments*>  fWorkerThreads_tPrepare_Jobs;
bool TPrepareGPUAnalysis_Worker_Run(true);

void Analyser::CleanUp()
{

	TPrepareGPUAnalysis_Worker_Run = false;


	while (numWorkers_Active > 0)
		multiThreading->SleepGPU();

#ifdef __linux__

	//for (pthread_t wt : workerThreads)
	//{
	//	//delete wt;
	//}
#else

	for (auto wt : workerThreads)
	{
		wt->join();
		delete wt;
	}

#endif

	workerThreads.clear();
	fWorkerThreads_tPrepare_Jobs.clear();
	fWorkerThreads_tPrepare_Jobs.shrink_to_fit();


}

void* TPrepareGPUAnalysis_Worker(void*)
{
	MultiThreading mt;

	vector<hashmap_streams*>  referenceHashMap_Lazy;
	//referenceHashMap_Lazy.reserve(Hashmap::referenceHashMap_Lazy->size());

	//for (auto rm : *Hashmap::referenceHashMap_Lazy)
	//{
	//	hashmap_streams* newRM = new hashmap_streams(*rm);
	//	referenceHashMap_Lazy.emplace_back(newRM);
	//}


	vector<hashmap_MatchedHashStreamPos*> MatchedHashStreamPos;
	MatchedHashStreamPos.reserve(Hashmap::HashmapFileLocations.size());

	for (size_t i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
	{
		MatchedHashStreamPos[i] = new hashmap_MatchedHashStreamPos();
	}

	while (TPrepareGPUAnalysis_Worker_Run) {

		while (!TPrepareGPUAnalysis_Worker_Active)
			mt.SleepTPrepareWorkerThread();

		fWorkerThreads_tPrepare_Mutex.lock();

		if (fWorkerThreads_tPrepare_Jobs.size() == 0 ||
			numWorkers_Active >= numWorkers_Max_Concurent_Active)
		{
			mt.SleepTPrepareWorkerThread(); // Inside lock, So other threads sleep as well while trying to get a lock
			fWorkerThreads_tPrepare_Mutex.unlock();
			continue;
		}

		// There is work to do
		numWorkers_Active++;


		// Get next arguments to work with
		Analyser::PrepareGPU_Arguments* preArg = fWorkerThreads_tPrepare_Jobs.back();
		fWorkerThreads_tPrepare_Jobs.pop_back();

		fWorkerThreads_tPrepare_Mutex.unlock();

		// Make use of this local threat buffers. This way, these buffers are reused for next worker runs
		preArg->MatchedHashStreamPos = &MatchedHashStreamPos;
		preArg->ReferenceHashMap_Lazy = &referenceHashMap_Lazy;

		// Execute Prepare of analysis
		Analyser::TPrepareGPUAnalysis(preArg);

		preArg->QueryIDs.clear();
		preArg->QueryIDs.shrink_to_fit();

		delete preArg;
		preArg = nullptr;

		// Clean up for the next worker run
		for (size_t i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
			MatchedHashStreamPos[i]->clear();

		//fWorkerThreads_tPrepare_Mutex.lock();
		numWorkers_Active--;
		//fWorkerThreads_tPrepare_Mutex.unlock();
	}

	return NULL;
}

// Main analysis execution function for multi hardware setup
void Analyser::ExecuteAnalysis_MultiHardware(size_t timeStartProgram)
{
	debug_timing_1 = 0;
	debug_timing_2a = 0;
	debug_timing_2 = 0;
	debug_timing_3 = 0;
	debug_timing_4 = 0;
	debug_timing_5 = 0;
	debug_timing_6 = 0;
	debug_timing_7 = 0;
	debug_timing_8 = 0;
	debug_timing_9 = 0;
	debug_timing_10 = 0;
	debug_timing_11 = 0;


	bool AllSequencesAnalysed(false);
	int cntHardwareExecutionJobs = 0;
	size_t numOfQuerySequences = (sequences->inputSequences->size());// / 2);
	if (bUseReverseComplementInQuery)
		numOfQuerySequences /= 2;


	// Fast forward to the correct sequence if multiplatform is used
	if (multiNodePlatform->divideSequenceReadBetweenNodes)
		Query_Processed_LastID = multiNodePlatform->Analyse_Sequences_From_Index;

#if defined(__CUDA__) || defined(__OPENCL__)
	display->logWarningMessage << "To prevent hardware failure, please always use" << endl;
	display->logWarningMessage << "CTRL-C to end (or pause) this program." << endl;
	display->FlushWarning();
#endif

	display->DisplayHeader("Start analysing");
	display->DisplayTitle("* = Auto increase system active.", true);
	display->DisplayTitle("    (Trying to determine the optimal blocksize for the combination of your data and hardware-> Time estimation available during this process is an overestimation.)", true);
	//display->DisplayTitle("    (Trying to determine the optimal blocksize for the combination of your data and hardware-> No time estimation available during this process.)", true);
	display->ShowProgressHeading(false);

	size_t startTimeAnalyse = time(NULL);

	size_t spentSecondsInitialising = time(NULL) - timeStartProgram;


#if defined(__CUDA__) 
	InitialiseCuda(hardware->Devices.size());
#endif
	clock_t begin_time_OverAll = clock();


	size_t numCores = (size_t)max((unsigned int)1, thread::hardware_concurrency());
	size_t TotalPrepareThreads = (size_t)min((size_t)this->multiThreading->maxCPUThreads, (numCores * 40) + 1);
	size_t numWorkers = numCores * 4;
	numWorkers_Max_Concurent_Active = numWorkers; // / 2;
	numWorkers_Active = 0;
	TPrepareGPUAnalysis_Worker_Active = false;

	//cout << "numWorkers: " << numWorkers << endl;
	for (size_t i = 0; i < numWorkers; i++)  // * 4 so we can run TPrepareGPUAnalysis which are clearing memory and analyzing together
	{

#ifdef __linux__
		pthread_t pthreadID;
#else
		thread* t;
#endif

		bool threadStarted(false);
		int threadRetryCount = 0;
		int errorCode = 0;
		// Protect against a failed start of a new thread. Wait some time and try again; resources might be free after a while.
		// Auto increase mode will downsize the number of threads, so we will find the optimal #threads automatically.
		while (!threadStarted)
		{

			bool startSuccess(false);
			bool ErrorCodeAgain(false);
#ifdef __linux__

			int threadStartResult = pthread_create(&pthreadID, NULL, TPrepareGPUAnalysis_Worker, NULL);
			startSuccess = threadStartResult == 0;
			ErrorCodeAgain = threadStartResult == EAGAIN;
			errorCode = threadStartResult;
			workerThreads.emplace_back(pthreadID);
			pthread_detach(pthreadID);  // Detach, so it will just ends when thread is finished
#else


			try
			{
				t = new thread(TPrepareGPUAnalysis_Worker, nullptr);
				workerThreads.emplace_back(t);
				startSuccess = true;
			}
			catch (const std::system_error& e)
			{
				ErrorCodeAgain = e.code().value() == static_cast<int>(std::errc::resource_unavailable_try_again);
			}

#endif
			if (startSuccess)
			{
				threadStarted = true;
#ifdef __linux__
				pthread_detach(pthreadID);  // Detach, so it will just ends when thread is finished
#endif

			}
			else if (threadRetryCount < MultiThreading::THREAD_MAX_RETRY_COUNT && ErrorCodeAgain)
			{
				threadRetryCount++;
				std::this_thread::sleep_for(MultiThreading::THREAD_START_FAILED_WAIT_SLEEP); // Just give the CPU some rest..
			}
			else
			{
				//display->logWarningMessage << "Unable to start a new thread to analyse current sequence!" << endl;
				display->logWarningMessage << "Does your machine have enough resources left?" << endl;
				display->logWarningMessage << "Try to set Hardware_BlockSize_AutoIncrease_Enabled=0 for your CPU in your configuration or lower the Hardware_maxCPUThreads setting until you don't get this error." << endl;
				display->logWarningMessage << "Error phase: Pre process" << endl;
				display->logWarningMessage << "Error code: " << errorCode << endl;
				display->logWarningMessage << "Running threads: " << this->multiThreading->runningThreadsGPUPreAnalysis << endl;
				display->logWarningMessage << "Program will stop." << endl;
				display->FlushWarning(true);
			}

		}



	}
	while (!AllSequencesAnalysed)
	{
#if defined(__CUDA__) || defined(__OPENCL__)
		// Check if the user pressed CTRL-C to pause the program
		if (hardware->GPUAcceleration_Enabled)
			signalHandler->PauseProgramByUserRequest();
#endif

		// Kill finsihed threads
		multiThreading->TerminateFinishedThreads(false);

		// Loop through all available hardware to check if we have 1 free to let it analyse the current sequence
		for (int DeviceID = 0; DeviceID < hardware->AvailableHardware; DeviceID++)
		{

			// Skip if current hardware is disabled
			if (!hardware->DeviceEnabled[DeviceID])
				continue;

			auto fullbegin = high_resolution_clock::now();

			//cout << "(Node:" << multiNodePlatform->Node_Current_Index << ") " << "AllSequencesAnalysed (begin of while): " << Query_Processed_LastID << endl;
			if (!AllSequencesAnalysed && hardware->DeviceQueueSize[DeviceID] < Hardware::MaxDeviceQueueSize)
			{

				float mDebugTime;
				auto begin = high_resolution_clock::now();
				auto begin_prepare = high_resolution_clock::now();


				if (hardware->Device_QuerySize[DeviceID] > 0)
				{
					float spentTime = (float)duration<double, ratio<1, 1>>(std::chrono::high_resolution_clock::now() - hardware->Device_begin_time[DeviceID]).count();

					hardware->TryAutoIncreaseBlocksize(DeviceID, spentTime, 0, hardware->Device_QuerySize[DeviceID], multiThreading->maxCPUThreads);
				}

				hardware->Device_begin_time[DeviceID] = std::chrono::high_resolution_clock::now();


#ifdef show_timings
				mDebugTime = (float(clock() - begin_time_OverAll) / (CLOCKS_PER_SEC / 1000));
				printf(" Start since last time (%f sec) \n", mDebugTime / 1000);
#endif

				//cout << "GO" << endl;

				// First do some cleanup and try to keep memory usage efficient
				// Check how much memory is left and clean up the hashmap lazy loading cache if needed

				/*
				if (Hashmap::Use_Experimental_LazyLoading)
					Hashmap::TidyLazyLoadingCache(Analyser::GetMemFreePercentage(), sequences);*/

				clock_t begin_time_total = clock();



				//// Lock the device, so we protect the cache from being deleted by the 'tidy' method
				//if (Hardware::MaxDeviceQueueSize == 1)
				//	while (multiThreading->mutexAnalysis[DeviceID])
				//		multiThreading->SleepWaitForCPUThreads();


				multiThreading->mutexAnalysis[DeviceID] = true;

				//multiThreading->mutexAnalysis[DeviceID].lock();
				multiThreading->mutGPUResults.lock();


				// We've hardware to work with! Utilize! :)
				hardware->DeviceQueueSize[DeviceID]++;

				// Prereset the device (in parallel so when it takes a long time, we can do de preperation of the data in parallel)
//#if defined(__CUDA__)
//				future<void> futureCudaDeviceReset = std::async(CudaDeviceResetAsync, hardware->DeviceID[DeviceID]);
//#endif

				// Prepare input data
				size_t blockSize = hardware->BlockSize_Current[DeviceID];

				SEQ_ID_t offset = Query_Processed_LastID;
				SEQ_ID_t offsetStart = offset;
				Query_Processed_LastID += blockSize;

				//cout << "GO2" << endl;
				// If Load is divided over platforms, check if we're already at the end of all sequences after this run.
				// Recalculate the number of sequences if that is the case.
				if (multiNodePlatform->divideSequenceReadBetweenNodes)
				{
					if (Query_Processed_LastID >= multiNodePlatform->Analyse_Sequences_To_Index + 1)
					{
						// We have less sequences to work with than we can give this piece of hardware to work on.
						// Recalculate!
						Query_Processed_LastID = multiNodePlatform->Analyse_Sequences_To_Index + 1;
						AllSequencesAnalysed = true;
					}
				}
				else
				{
					if (offset + blockSize >= numOfQuerySequences) // We're at the end
					{
						Query_Processed_LastID = offset + (numOfQuerySequences - offset);
						AllSequencesAnalysed = true;
					}
				}


		/*		cout << "blockSize: " << blockSize << endl;
				cout << "offset: " << offset << endl;
				cout << "Query_Processed_LastID: " << Query_Processed_LastID << endl;*/



				//std::cout << " Exec 1: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
				begin = high_resolution_clock::now();

				// Initialise the working order for current hardware
				Analysis_Arguments* argsForGPU = new Analysis_Arguments(); // Will be deleted in thread itself


				// Pause CPU analysis to give time for preparation fase forTPreloadReferences_LoadFromDisk_LocalBuffer current hardware device.
				// This will prevent stalling the GPU since it will get full CPU support.
				//multiThreading->PauseCPUAnalysis = true;

				if (hardware->Type[DeviceID] == Hardware::HardwareTypes::GPU)
					multiThreading->SetGPUActive(true, hardware->DeviceID[DeviceID]);

				bool addSequencesToArgs(true);



				while (addSequencesToArgs)
				{

					clock_t begin_time_addSequencesToArgs = clock();

					SEQ_AMOUNT_t numQIDs_PerThread = Query_Processed_LastID - offset;


					//start multiple thread to prepare GPU analysis.
					numQIDs_PerThread /= TotalPrepareThreads;
					numQIDs_PerThread = (numQIDs_PerThread < 1 ? 1 : numQIDs_PerThread);

					if (bUseReverseComplementInQuery)
						numQIDs_PerThread *= 2;


					numQIDs_PerThread = numQIDs_PerThread - (numQIDs_PerThread % 2); // Always count per 2 (original + reverse complement).

					// Prepare the qID's which will be send to the threads
					vector<vector<SEQ_ID_t>> qIDs_ForThreads;
					vector<SEQ_ID_t> qIDs;
					qIDs_ForThreads.reserve(TotalPrepareThreads + 1);
					qIDs.reserve(Query_Processed_LastID - offset);
					size_t totalQIDs = 0;
					for (SEQ_ID_t i = offset; i < Query_Processed_LastID; i++)
					{
						// Do not analyse duplicate sequences
						if (sequences->SkipDuplicateSequences && sequences->CountDuplicateList[i].size() == 0)
						{
							if (Query_Processed_LastID != numOfQuerySequences && !(multiNodePlatform->divideSequenceReadBetweenNodes && Query_Processed_LastID > multiNodePlatform->Analyse_Sequences_To_Index))
								Query_Processed_LastID++; // Allow 1 extra query sequence to be added since we do not add this one
							continue;
						}
						totalQIDs++;
						qIDs.emplace_back(i);

						if (bUseReverseComplementInQuery)
						{
							totalQIDs++;
							qIDs.emplace_back(i + (numOfQuerySequences));
						}

						if (qIDs.size() >= numQIDs_PerThread)
						{
							qIDs_ForThreads.emplace_back(qIDs);
							qIDs.clear();
						}
					}

					if (qIDs.size() > 0)
						qIDs_ForThreads.emplace_back(qIDs); // Add remainings


					this->multiThreading->mutexGPUPrepare.lock();
					this->multiThreading->mutMainMultithreads.lock();

					size_t numQIDs_ForThreads = qIDs_ForThreads.size();


					argsForGPU->matchedReferencesPerQuery_CombiPositions_LastPositionInRange.resize(totalQIDs);
					argsForGPU->matchedReferencesPerQuery_HeadTailSequences.resize(totalQIDs);
					argsForGPU->matchedReferencesPerQuery_CombiPositions.resize(totalQIDs);
					argsForGPU->matchedReferencesPerQuery_RefPositions.resize(totalQIDs);
					argsForGPU->matchedReferencesPerQuery_QueryPositions.resize(totalQIDs);
					argsForGPU->matchedReferencesPerQuery.resize(totalQIDs);
					argsForGPU->querySequences.resize(totalQIDs);
					argsForGPU->queryIDs.resize(totalQIDs);
					argsForGPU->runningThreadsGPUPreAnalysis = 0;

					hardware->Device_QuerySize[DeviceID] = totalQIDs;


					TPrepareGPUAnalysis_Worker_Active = false;
					fWorkerThreads_tPrepare_Mutex.lock();
					fWorkerThreads_tPrepare_Jobs.reserve(fWorkerThreads_tPrepare_Jobs.size() + qIDs_ForThreads.size());
					size_t loopSize = qIDs_ForThreads.size();
					for (SEQ_ID_t i = 0; i < loopSize; i++)
					{
						fWorkerThreads_tPrepare_Jobs.emplace_back(new PrepareGPU_Arguments());

						PrepareGPU_Arguments* pgArgs = fWorkerThreads_tPrepare_Jobs.back();// = new PrepareGPU_Arguments();

						pgArgs->AnalysisArguments = argsForGPU;

						pgArgs->QueryIDs.insert(pgArgs->QueryIDs.begin(), qIDs_ForThreads[i].begin(), qIDs_ForThreads[i].end());
						//pgArgs->QueryIDs = move(vector<SEQ_ID_t>(qIDs_ForThreads[i]));
						pgArgs->ThreadResultID = i;
						pgArgs->NumQIDs_PerThread = numQIDs_PerThread;
						pgArgs->ThreadID = this->multiThreading->activeThreads.size();

						pgArgs->sequences = sequences;
						pgArgs->DeviceID = DeviceID;
						if (ConsiderThreshold_HashmapLoaded != -1)
							pgArgs->ConsiderThreshold = ConsiderThreshold_HashmapLoaded;
						else
							pgArgs->ConsiderThreshold = ConsiderThreshold;
						pgArgs->multiThreading = this->multiThreading;
						pgArgs->hardware = this->hardware;
						pgArgs->display = this->display;



						this->multiThreading->activeThreads.push_back(0); // We use worker threads now, so just push a placeholder 0
						argsForGPU->runningThreadsGPUPreAnalysis++;




					}

					for (auto q : qIDs_ForThreads)
						q.clear();
					qIDs_ForThreads.clear();

					//std::cout << " Exec - Add to and start: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
					begin = high_resolution_clock::now();


					fWorkerThreads_tPrepare_Mutex.unlock();
					TPrepareGPUAnalysis_Worker_Active = true;



					this->multiThreading->mutMainMultithreads.unlock();
					this->multiThreading->mutexGPUPrepare.unlock();

					// todo-nice-to-have
					// You could check which threads are already ready and add its return information in the main variables (done below)
					// You should check in order, so only proceed with the next folowing thread (you could do this in other multithread situations as well)
					// Wait until all threads are really finished running
					//while (this->multiThreading->runningThreadsGPUPreAnalysis > 0)
					//{
					//	// Just give the CPU some rest..
					//	this->multiThreading->SleepGPU();
					//}




#ifdef show_timings

					std::cout << " TPrepate all query done : " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
					begin = high_resolution_clock::now();
#endif

					ClearThrashCan(true);
					//fDeletetrashcan.push_back(async(launch::async, ClearThrashCan, false));

					//cout << " voor: " << Analyser::GetMemFreePercentage() << endl;
					//
					//#ifdef show_timings
					//					auto beginPreload = high_resolution_clock::now();
					//
					//#endif
					//
					//
					//#ifdef show_timings
					//
					//					std::cout << "Total loading head tail reference data : " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - beginPreload).count() << "\n";
					//					begin = high_resolution_clock::now();
					//#endif


					//cout << " na: " << Analyser::GetMemFreePercentage() << endl;
					//argsForGPU->matchedReferencesPerQuery_HeadTailSequences.clear();
					//vector<vector<vector<Enum::HeadTailSequence>>>().swap(argsForGPU->matchedReferencesPerQuery_HeadTailSequences);

					//cout << " er: " << Analyser::GetMemFreePercentage() << endl;



					if (argsForGPU->querySequences.size() < blockSize)
					{
						if (Query_Processed_LastID != numOfQuerySequences &&
							!(multiNodePlatform->divideSequenceReadBetweenNodes && Query_Processed_LastID > multiNodePlatform->Analyse_Sequences_To_Index))
						{
							//cout << argsForGPU->querySequences.size() << "Not full" << endl;

							offset = Query_Processed_LastID;

							Query_Processed_LastID += (blockSize - argsForGPU->querySequences.size()); // Calc available room for GPU

							if (multiNodePlatform->divideSequenceReadBetweenNodes)
							{
								if (Query_Processed_LastID >= multiNodePlatform->Analyse_Sequences_To_Index + 1)
								{
									// We have less sequences to work with than we can give this piece of hardware to work on.
									// Recalculate!
									Query_Processed_LastID = multiNodePlatform->Analyse_Sequences_To_Index + 1;
									AllSequencesAnalysed = true;
								}
							}
							else
							{
								if (Query_Processed_LastID >= numOfQuerySequences) // We're at the end
								{
									Query_Processed_LastID = offset + (numOfQuerySequences - offset);
									AllSequencesAnalysed = true;
								}
							}
						}
						else
							addSequencesToArgs = false;
					}
					else
						addSequencesToArgs = false;


					//cout << "addSequencesToArgs:  " << addSequencesToArgs << endl << flush;


					begin = high_resolution_clock::now();
				}


				// If we do not have any sequence to analyse, continue to the next block.
				if (argsForGPU->querySequences.size() == 0) // && argsForGPU->QueryIDs_DirectHits.size() == 0)
				{
					hardware->DeviceQueueSize[DeviceID]--;
					this->multiThreading->mutGPUResults.unlock();
					continue;
				}

				//cout << "Start calc... " << endl << flush;

				//vector<vector<char>> vAgainst(sequences->referenceSequences);
				//argsForGPU->referenceSequences = vAgainst;
				argsForGPU->sequences = sequences;
				argsForGPU->GPUEngine = &hardware->Devices[DeviceID];
				argsForGPU->numOfQueriesPigyBacked = Query_Processed_LastID - offsetStart;
				argsForGPU->GPUHardwareID = hardware->DeviceID[DeviceID];
				argsForGPU->DeviceID = DeviceID;
				argsForGPU->display = display;
				argsForGPU->multiThreading = this->multiThreading;
				argsForGPU->hardware = hardware;
				argsForGPU->ConsiderThreshold = ConsiderThreshold;
				argsForGPU->TotalQuerySequencesAnalysed = &TotalQuerySequencesAnalysed;
				argsForGPU->Output_Mode = &(configuration->Output_Mode);
				argsForGPU->logResultsToScreen = logResultsToScreen;
				argsForGPU->logManagerHighSensitivity = logManagerHighSensitivity;
				argsForGPU->logManagerHighPrecision = logManagerHighPrecision;
				argsForGPU->logManagerBalanced = logManagerBalanced;
				argsForGPU->logManagerRaw = logManagerRaw;
				argsForGPU->Output_HighestScoredResultsOnly = Output_HighestScoredResultsOnly;
				argsForGPU->threadID = (double)(multiThreading->activeThreads.size());
				argsForGPU->PrepareSpentTime = duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin_prepare).count();
				//argsForGPU->threadID = (double)(multiThreading->activeThreads.size() + 1);



				if (hardware->Type[DeviceID] == Hardware::HardwareTypes::GPU)
					multiThreading->SetGPUActive(false, hardware->DeviceID[DeviceID]);

				//multiThreading->GPUsActive[hardwareID] = false;
				//multiThreading->PauseCPUAnalysis = false;


//#if defined(__CUDA__)
//				// Wait until we are sure the device has been reset
//				futureCudaDeviceReset.wait();
//#endif

				multiThreading->mutMainMultithreads.lock();



				// Start threads

#ifdef __linux__
				pthread_t pthreadID;
#else
				thread* t;
#endif

				bool threadStarted(false);
				int threadRetryCount = 0;
				int errorCode = 0;
				// Protect against a failed start of a new thread. Wait some time and try again; resources might be free after a while.
				// Auto increase mode will downsize the number of threads, so we will find the optimal #threads automatically.
				//cout << "Start threads... " << endl << flush;

				while (!threadStarted)
				{
					bool startSuccess(false);
					bool ErrorCodeAgain(false);
#ifdef __linux__

					int threadStartResult = -1;
					if (hardware->Type[DeviceID] == Hardware::HardwareTypes::CPU)
					{
						threadStartResult = pthread_create(&pthreadID, NULL, Analyser::TGetHighestScoreUsingCPU, argsForGPU);
					}
					else if (hardware->Type[DeviceID] == Hardware::HardwareTypes::GPU)
					{
						threadStartResult = pthread_create(&pthreadID, NULL, Analyser::TGetHighestScoreUsingGPU, argsForGPU);
					}
					else
					{
						display->logWarningMessage << "[ExecuteMultiGPUAnalysis] Unknown hardware type used!" << endl;
						display->logWarningMessage << "Program will stop." << endl;
						display->FlushWarning(true);
					}

					startSuccess = threadStartResult == 0;
					ErrorCodeAgain = threadStartResult == EAGAIN;
					errorCode = threadStartResult;
#else



					try
					{
						if (hardware->Type[DeviceID] == Hardware::HardwareTypes::CPU)
						{
							t = new thread(Analyser::TGetHighestScoreUsingCPU, argsForGPU);
						}
						else if (hardware->Type[DeviceID] == Hardware::HardwareTypes::GPU)
						{
							t = new thread(Analyser::TGetHighestScoreUsingGPU, argsForGPU);
						}
						else
						{
							display->logWarningMessage << "[ExecuteMultiGPUAnalysis] Unknown hardware type used!" << endl;
							display->logWarningMessage << "Program will stop." << endl;
							display->FlushWarning(true);
						}

						startSuccess = true;
					}
					catch (const std::system_error& e)
					{
						errorCode = e.code().value();
						ErrorCodeAgain = e.code().value() == static_cast<int>(std::errc::resource_unavailable_try_again);
					}

#endif
					if (startSuccess)
					{
						threadStarted = true;
#ifdef __linux__
						pthread_detach(pthreadID);  // Detach, so it will just ends when thread is finished
						multiThreading->activeThreads.push_back(pthreadID);
#else
						this->multiThreading->activeThreads.push_back(t);
#endif

						multiThreading->runningThreadsAnalysis++;
						multiThreading->runningLogResults++;


					}
					else if (threadRetryCount < MultiThreading::THREAD_MAX_RETRY_COUNT && ErrorCodeAgain)
					{
						threadRetryCount++;
						std::this_thread::sleep_for(MultiThreading::THREAD_START_FAILED_WAIT_SLEEP); // Just give the CPU some rest..
					}
					else
					{
						display->logWarningMessage << "Unable to start a new thread to analyse current sequence!" << endl;
						display->logWarningMessage << "Does your machine have enough resources left?" << endl;
						display->logWarningMessage << "Try to set Hardware_BlockSize_AutoIncrease_Enabled=0 for your CPU in your configuration or lower the Hardware_maxCPUThreads setting until you don't get this error." << endl;
						display->logWarningMessage << "Error phase: Core process" << endl;
						display->logWarningMessage << "Error code: " << errorCode << endl;
						display->logWarningMessage << "Thread ID: " << argsForGPU->threadID << endl;
						display->logWarningMessage << "Running threads: " << this->multiThreading->runningThreadsAnalysis << endl;
						display->logWarningMessage << "Program will stop." << endl;
						display->FlushWarning(true);
					}
				}


				cntHardwareExecutionJobs++;

				// Thread has been started..
				multiThreading->mutGPUResults.unlock();
				multiThreading->mutMainMultithreads.unlock();


				// Show some timing data
				try {

					int percentageFreeMem = Analyser::GetMemFreePercentage();
					if (TotalQuerySequencesAnalysed == 0)
					{
						display->ShowProgress(argsForGPU->querySequences.size() / 2, TotalQuerySequencesAnalysed, numOfQuerySequences, sequences->DuplicateQuerySequencesCount, DeviceID, hardware->TypeName[DeviceID], multiNodePlatform->Node_Current_Index, startTimeAnalyse, percentageFreeMem);
					}
					else
					{
						//de headers tijdens multinode lijken niet weggehaald te worden
						if (hardware->BlockSize_AutoIncrease_IsActive)
							display->ShowProgress(argsForGPU->querySequences.size() / 2, TotalQuerySequencesAnalysed, numOfQuerySequences, sequences->DuplicateQuerySequencesCount, DeviceID, hardware->TypeName[DeviceID], multiNodePlatform->Node_Current_Index, startTimeAnalyse, percentageFreeMem, startTimeAnalyse, hardware->BlockSize_AutoIncrease_AllDevicesStabilized);
						else
							display->ShowProgress(argsForGPU->querySequences.size() / 2, TotalQuerySequencesAnalysed, numOfQuerySequences, sequences->DuplicateQuerySequencesCount, DeviceID, hardware->TypeName[DeviceID], multiNodePlatform->Node_Current_Index, startTimeAnalyse, percentageFreeMem, startTimeAnalyse);
					}
				}
				catch (exception &ex) {
					// Whoops.. close your eyes and move along!
				}
				begin_time_total = clock();
				begin_time_OverAll = clock();

			}


			// Just give the CPU some rest..
			multiThreading->SleepGPU();

		}


	}


	// Set GPU's to active, so CPU can finish his work as it free to utilize CPU power

	// Wait until all threads are really finished running
	int previousRunningThreads = 0;
	while (multiThreading->runningThreadsAnalysis > 0 || multiThreading->runningLogResults > 0)
	{

		if (multiThreading->runningThreadsAnalysis > 0)
		{
			multiThreading->TerminateFinishedThreads(true);

			if (previousRunningThreads != multiThreading->runningThreadsAnalysis)
			{
				multiThreading->SetGPUActive_AllStates(true);
				multiThreading->TerminateFinishedThreads(false);
				previousRunningThreads = multiThreading->runningThreadsAnalysis;
			}
		}
		// Just give the CPU some rest..
		multiThreading->SleepGPU();
	}

	multiThreading->TerminateFinishedThreads(true);
	//ClearThrashCan();
	//fDeletetrashcan.clear();
	stopClearTrashcan = true;

	// Show that we are ready
	int percentageFreeMem = Analyser::GetMemFreePercentage();

	display->ShowProgress(0, numOfQuerySequences, numOfQuerySequences, 0, -1, "-", multiNodePlatform->Node_Current_Index, timeStartProgram, percentageFreeMem);
	display->ShowEndBlock();
}


Analyser::Analyser(LogToScreen* lrts, Sequences* s, MultiThreading* m, LogManager* lmHS, LogManager* lmHP, LogManager* lmB, LogManager* lmR, Display* d, Hardware* h, MultiNodePlatform* mnp, SignalHandler* sh, Configuration *c)
{

	logResultsToScreen = lrts;
	sequences = s;
	multiThreading = m;
	logManagerHighSensitivity = lmHS;
	logManagerHighPrecision = lmHP;
	logManagerBalanced = lmB;
	logManagerRaw = lmR;
	display = d;
	hardware = h;
	multiNodePlatform = mnp;
	signalHandler = sh;
	configuration = c;
}

Analyser::~Analyser()
{

}
