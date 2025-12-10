#include "Sequences.h"

UINT8 Sequences::SizeOfSequence_Bytes = 1;
UINT8 Sequences::SizeOfHeader_Bytes = 1;

SEQ_ID_t Sequences::TSequencesLoaded = 0;
MultiThreading *Sequences::multiThreading = nullptr;
pair<vector<vector<char>>, vector<vector<char>>> *Sequences::TReadFileFromMemArgs_ThreadResults = nullptr;
HASH_STREAMPOS Sequences::sp_ReferenceSequence_AfterLastChar = 0;
HASH_STREAMPOS Sequences::sp_ReferenceHeader_AfterLastChar = 0;
Display *Sequences::display;
vector<ifstream*> Sequences::vIfstream_hashmap = {};
size_t Sequences::Reference_Read_Buffer_Size_GB = 5;  // todo: make this system variable (or config?)
size_t Sequences::Reference_Read_Buffer_Size_MB_MaxAllowed_Percentage = 33;  // 3000=3 gb, todo: make this system variable (or config?)

size_t Sequences::MAX_RETRY_OPEN_FILE = 500;
chrono::milliseconds Sequences::FILE_OPEN_FAILED_WAIT_MS = chrono::milliseconds(100);
chrono::milliseconds Sequences::FILE_OPEN_PREPARE_HASHMAP_LOCKED_WAIT = chrono::milliseconds(10);


void Sequences::toUpperCase(vector<char>::iterator p)
{
	switch (*p)
	{
	case 'a':*p = 'A'; return;
	case 'c':*p = 'C'; return;
	case 'g':*p = 'G'; return;
	case 't':*p = 'T'; return;

	case 'w':*p = 'W'; return;
	case 's':*p = 'S'; return;
	case 'm':*p = 'M'; return;
	case 'k':*p = 'K'; return;
	case 'r':*p = 'R'; return;
	case 'y':*p = 'Y'; return;

	case 'b':*p = 'B'; return;
	case 'd':*p = 'D'; return;
	case 'h':*p = 'H'; return;
	case 'v':*p = 'V'; return;
	case 'n':*p = 'N'; return;
	case 'z':*p = 'Z'; return;

		//case 'e':*p = 'E'; return;
		//case 'f':*p = 'F'; return;
		//case 'i':*p = 'I'; return;
		//case 'j':*p = 'J'; return;
		//case 'l':*p = 'L'; return;
		//case 'o':*p = 'O'; return;
		//case 'p':*p = 'P'; return;
		//case 'q':*p = 'Q'; return;
		//case 'u':*p = 'U'; return;
		//case 'x':*p = 'X'; return;
	};
	return;
}



// https://stackoverflow.com/questions/10688831/fastest-way-to-capitalize-words
void Sequences::SequenceToUpperCase(vector<char>* vc)
{
	size_t s = vc->size();
	if (s == 0)
		return;

	for (size_t i = 0; i < s; i++)
		if ((*vc)[i] > 96)
			(*vc)[i] -= 32;

	//vector<char>::iterator vEnd = vc->end();
	//for (vector<char>::iterator it = vc->begin(); it != vEnd; ++it)
	//	if (*it > 96)
	//		*it -= 32;
		//toUpperCase(it);
}


void* Sequences::TReadFastaSequencesFromMem(void* args)
{
	TReadFileFromMemArgs* argsForReadMem = (TReadFileFromMemArgs*)args;


	// Lock
	Sequences::multiThreading->mutexReadFileFromMem.lock();
	Sequences::multiThreading->mutMainMultithreads.lock();

	// Finish
	TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID] = { vector<vector<char>>(),  vector<vector<char>>() };

	vector<vector<char>>* _sequences = &(TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID].first);
	vector<vector<char>>* _headers = &(TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID].second);

	// Unlock
	Sequences::multiThreading->mutMainMultithreads.unlock();
	Sequences::multiThreading->mutexReadFileFromMem.unlock();



	//vector<vector<char>> _sequences = vector<vector<char>>();
	//vector<vector<char>> _headers = vector<vector<char>>();


	_sequences->reserve(argsForReadMem->numSequencesPerThread);
	_headers->reserve(argsForReadMem->numSequencesPerThread);

	_sequences->push_back(vector<char>());
	vector<char>* refSeq = &_sequences->back();


	// Parse file data
	char* pHeaderStartPositions = argsForReadMem->startEndPosition.first;
	char* pFirstStart = pHeaderStartPositions;
	int dataCnt = 0;
	int lineCnt = 0;
	int smartReserve_HighestSize = 1;

	char* line = new char[0];//(char*)emalloc(0, __LINE__);

	bool IsFastaQ(false);
	int fastaQCounter = 0;
	vector<future<void>> futures;
	futures.reserve(argsForReadMem->TotalSequencesPerThread);
	char* loopEnd = argsForReadMem->startEndPosition.second;
	for (; *pHeaderStartPositions && pHeaderStartPositions <= loopEnd; pHeaderStartPositions++)
	{
		while (Sequences::multiThreading->WritingToScreen)
			Sequences::multiThreading->SleepWaitForCPUThreads();

		if (*pHeaderStartPositions == '\n')
		{
			dataCnt = pHeaderStartPositions - pFirstStart;
			/*
			char* newLine = (char*)emalloc(sizeof(char) * dataCnt, __LINE__);
			line = newLine;*/
			//delete[] line;
			free(line);
			line = new char[dataCnt];//(char*)emalloc(sizeof(char) * dataCnt, __LINE__);
			memcpy(line, pFirstStart, dataCnt);
			pFirstStart = pHeaderStartPositions;
			++pFirstStart; // Skip the \n char, that's no DNA!

						   //logToScreen << string(line, line + dataCnt) << endl;

						   // Store our previous sequence if we have a new line or are at the end of the file
			if (line[0] == '>' || line[0] == '@') // Is this a new sequence?
			{

				lineCnt = 0;

				if (line[0] == '@')
				{
					fastaQCounter = 0;
					IsFastaQ = true;
				}

				if (line[dataCnt - 1] == '\r')
				{
					//vector<char> lineData(line, line + dataCnt);
					//lineData[lineData.size() - 1] = '\0';
					//_headers.push_back(move(lineData));
					line[dataCnt - 1] = '\0';
					_headers->emplace_back(line, line + dataCnt);
					//_headers[headCnt] = (move(lineData));
					//headCnt++;
				}
				else
				{
					_headers->emplace_back(line, line + dataCnt);
					//vector<char> lineData(line, line + dataCnt); // duur 5%
					//_headers.push_back(move(lineData));
					//_headers[headCnt] = move(lineData); // duur 25%
					//headCnt++;
				}

				if (refSeq->size() > 0) {
					////SequenceToUpperCase(refSeq);
					//Asynchronise::wait_for_futures_on_full_vector(&futures, 200);

					//futures.push_back(std::async(launch::async, SequenceToUpperCase, refSeq));



					SequenceToUpperCase(refSeq);


					Sequences::multiThreading->mutexReadFileFromMem.lock();
					TSequencesLoaded++;
					Sequences::multiThreading->mutexReadFileFromMem.unlock();





					if (refSeq->size() > smartReserve_HighestSize)
						smartReserve_HighestSize = refSeq->size(); // Remember the biggest size. This way we lower the amount of resizes during push_backs of future sequences

					_sequences->push_back(vector<char>());
					refSeq = &(*_sequences)[_sequences->size() - 1];
					refSeq->reserve(smartReserve_HighestSize);



					//_sequences.push_back(refSeq); // duur 25%
					//							  //_sequences[seqCnt] = refSeq; // duur 25%
					//							  //seqCnt++;
						//refSeq.resize(0);
					//refSeq.shrink_to_fit();
					//refSeq.reserve(smartReserve_HighestSize);

				}
			}
			else
			{
				if (!IsFastaQ || (fastaQCounter == 0))
				{
					if (line[dataCnt - 1] == '\r')
						refSeq->insert(refSeq->end(), line, line + dataCnt - 1);
					else
						refSeq->insert(refSeq->end(), line, line + dataCnt);
				}
				if (IsFastaQ)
					fastaQCounter++;
			}
			//delete[] newLine;
		}
	}

	delete[] line;
	//free(line);

	// Also add the last data row.
	if (pHeaderStartPositions != pFirstStart) {
		dataCnt = pHeaderStartPositions - pFirstStart;
		line = new char[dataCnt];//(char*)emalloc(sizeof(char) * dataCnt, __LINE__);
								 //char* newLine = (char*)emalloc(sizeof(char) * dataCnt, __LINE__);
								 //line = newLine;
		memcpy(line, pFirstStart, dataCnt);
		if ((line[0] != '>' && line[0] != '@') || (IsFastaQ && fastaQCounter > 0)) // Do not save a new header line
		{
			if (line[dataCnt - 1] == '\r')
				refSeq->insert(refSeq->end(), line, line + dataCnt - 1);
			else
				refSeq->insert(refSeq->end(), line, line + dataCnt);
		}
		delete[] line;

		//free(newLine);
	}

	if (refSeq->size() > 0)
	{
		futures.push_back(std::async(launch::async, SequenceToUpperCase, refSeq));

		//SequenceToUpperCase(refSeq);
	}


	Asynchronise::wait_for_futures(&futures);


	// Check if a sequence exceeds maximum length
	// Todo-later: Create a fix so we do not need the array in CU code
	if (argsForReadMem->IsGPUDeviceUsed)
		for (int i = 0; i < _sequences->size(); i++)
			if ((*_sequences)[i].size() > argsForReadMem->maxSequenceLength)
			{
				argsForReadMem->display->logWarningMessage << "Unable to analyse query file!" << endl;
				argsForReadMem->display->logWarningMessage << "A sequence exceeds the size which will fit on the GPU." << endl;
				argsForReadMem->display->logWarningMessage << "Sequence size: " << (*_sequences)[i].size() << endl;
				argsForReadMem->display->logWarningMessage << "Header: " << string((*_headers)[i].begin(), (*_headers)[i].end()) << endl;
				argsForReadMem->display->logWarningMessage << "Maximum size: " << argsForReadMem->maxSequenceLength << endl;
				argsForReadMem->display->logWarningMessage << "Program will stop." << endl;
				argsForReadMem->display->FlushWarning(true);
			}



	// Lock
	Sequences::multiThreading->mutexReadFileFromMem.lock();
	Sequences::multiThreading->mutMainMultithreads.lock();

	// Finish
	//TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID] = { _sequences, _headers };
	Sequences::multiThreading->threadsFinished.push_back(argsForReadMem->ThreadID);
	Sequences::multiThreading->runningThreadsReadingFileFromMem--;

	// Unlock
	Sequences::multiThreading->mutMainMultithreads.unlock();
	Sequences::multiThreading->mutexReadFileFromMem.unlock();

	// Mem todo-perform-memchck

	//for (int i = 0; i < _sequences.size();i++)
	//{
	//	_sequences[i].clear();
	//	_sequences[i].shrink_to_fit();
	//}


	//for (int i = 0; i < _headers.size();i++)
	//{
	//	_headers[i].clear();
	//	_headers[i].shrink_to_fit();
	//}

	//_sequences.clear();
	//_headers.clear();
	//refSeq.clear();

	//_sequences.shrink_to_fit();
	//_headers.shrink_to_fit();
	//refSeq.shrink_to_fit();

	delete argsForReadMem;

	return NULL;
}

///
/// Empty sequence data and headers
///
void Sequences::ResetReferenceSequences()
{
	size_t preSize = referenceHeaders.size();

	vector<vector<char>>().swap(referenceSequences);
	vector<vector<char>>().swap(referenceHeaders);

	referenceSequences.reserve(preSize);
	referenceHeaders.reserve(preSize);
}



#ifdef _WIN32
#include "Windows.h" // MultiByteToWideChar
///
/// Convert String to wString
///
std::wstring Sequences::s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

///
/// Streambuffer used to read input files
///
struct Sequences::membuf : std::streambuf
{
	membuf(char const* const buf, std::size_t const size)
	{
		char* const p = const_cast<char*>(buf);
		this->setg(p, p, p + size);
	}
};

///
/// IStream used to read input files
///
struct Sequences::imemstream : virtual membuf, std::istream
{
	imemstream(char const* const buf, std::size_t const size)
		: membuf(buf, size),
		std::istream(static_cast<std::streambuf*>(this))
	{ }
};
#endif



//
//void AssignToVector(vector<vector<char>>* dest, size_t insertIndex, vector<vector<char>>* src)
//{
//	dest->insert(dest->begin() + insertIndex, src->begin(), src->end());
//}

static void Trashcan_TReadFileFromMemArgs_Thread(pair<vector<vector<char>>, vector<vector<char>>>* trash)
{
	delete[] trash;
}

void Sequences::ReadFastaSequencesFromMem(string FileDescription, char* FileLocation, vector<vector<char>>* pSequences, char* pFileMap, size_t lenFileMap, vector<vector<char>>* pHeaders, bool IsGPUDeviceUsed, size_t maxSequenceLength, Hashmap* BuildHashmapDuringRead)
{

	size_t numSequencesDone = 0;
	size_t numberOfSequences = 0;
	bool firstChar = true;
	char* pFileMapPreSize = pFileMap;



	vector<char*> newHeaderPositions;
	if (*pFileMapPreSize == '>')
	{
		numberOfSequences++;
		newHeaderPositions.push_back(pFileMapPreSize);
	}
	int previousProgressLength = -1;
	size_t startTimeLoading = time(NULL);
	size_t infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);


	// todo: multithread initialise
	for (; *pFileMapPreSize; pFileMapPreSize++)
	{
		if (*pFileMapPreSize == '\n' || pFileMapPreSize == pFileMap + lenFileMap)
		{
			Sequences::display->ShowLoadingProgress((pFileMapPreSize - pFileMap), lenFileMap, &previousProgressLength, startTimeLoading, &infoTime);

			pFileMapPreSize++;
			if (pFileMapPreSize - pFileMap < lenFileMap && *pFileMapPreSize == '>')
			{
				numberOfSequences++;
				newHeaderPositions.push_back(pFileMapPreSize);
			}
		}
	}

	newHeaderPositions.push_back(pFileMapPreSize);

	Sequences::display->EraseChars(previousProgressLength);
	Sequences::display->DisplayValue(to_string(numberOfSequences) + " sequences found", true);
	Sequences::display->DisplayTitle("Reading " + FileDescription + " file:");

	//logToScreen << "[Sequences: " << numberOfSequences << "].. ";
	//logToScreen.flush(); // not required since logToScreen provides this

	if (newHeaderPositions.size() == 0)
		return;

	// Guard for enough buffer memory space
	unsigned long maxMachineMemoryUsedGB = (size_t)((Analyser::GetMemMachineFreeB() / 1024 / 1024 / 1024) * (Sequences::Reference_Read_Buffer_Size_MB_MaxAllowed_Percentage / 100.0));
	Sequences::Reference_Read_Buffer_Size_GB = max(Sequences::Reference_Read_Buffer_Size_GB, maxMachineMemoryUsedGB);




	size_t maxByteCache = Sequences::Reference_Read_Buffer_Size_GB * 1024 * 1024 * 1024;

	size_t HeaderPositionsPerCache = newHeaderPositions.size();
	if (BuildHashmapDuringRead != nullptr)
	{
		if (maxByteCache < lenFileMap)
			HeaderPositionsPerCache = (double)newHeaderPositions.size() / ((double)lenFileMap / (double)maxByteCache); // Read all at once, don't use buffer
	}


	bool LoopNewHeaderPositions(true);
	size_t IDHeaderPos = 0;
	while (LoopNewHeaderPositions)
	{

		if (IDHeaderPos >= newHeaderPositions.size())
		{
			break;
			//IDHeaderPos = newHeaderPositions.size() - 1;
			//LoopNewHeaderPositions = false;
		}


		// Start with reset the sequences, headers and hashmap; when multiple passes are performed, we already build and saved the hashmap for the previous sequences
		pSequences->clear();
		pSequences->shrink_to_fit();
		pHeaders->clear();
		pHeaders->shrink_to_fit();
		Hashmap::ResetHashmapTable();


		// test:
		//size_t numThreads = 10;
		//size_t numSequencesPerThread = 200;
		//HeaderPositionsPerCache = 500;
		//
		size_t numThreads = 200;
		size_t numSequencesPerThread = 1;
		if (HeaderPositionsPerCache >= numThreads)
			numSequencesPerThread = (HeaderPositionsPerCache + 1) / numThreads;




		// Prepare the qID's which will be send to the threads
		vector<tuple<char*, char*, size_t>> startEndPositions; // Start *, To *, Sequence ID of first item
		pFileMapPreSize = pFileMap;
		char* pFileMapPrevious = pFileMapPreSize;

		size_t loopUntil = IDHeaderPos + HeaderPositionsPerCache;
		if (loopUntil > newHeaderPositions.size())
			loopUntil = newHeaderPositions.size();
		if (IDHeaderPos + numSequencesPerThread > newHeaderPositions.size() - 1)
			numSequencesPerThread = newHeaderPositions.size() - IDHeaderPos - 1;


		size_t cnewLinePosition = IDHeaderPos;
		size_t offSetPos = IDHeaderPos;


		if (offSetPos + HeaderPositionsPerCache > newHeaderPositions.size())
			HeaderPositionsPerCache = newHeaderPositions.size() - offSetPos;

		if (numSequencesPerThread > 0)
			for (size_t cPos = 0; cPos < HeaderPositionsPerCache; cPos += numSequencesPerThread)
			{
				size_t cPosPlusOffset = cPos + offSetPos;

				tuple<char*, char*, size_t> newTuple;

				if (cPosPlusOffset + numSequencesPerThread >= newHeaderPositions.size())
					newTuple = (make_tuple(newHeaderPositions[cPosPlusOffset], newHeaderPositions[newHeaderPositions.size() - 1], newHeaderPositions.size() - cPosPlusOffset));
				else
					newTuple = (make_tuple(newHeaderPositions[cPosPlusOffset], newHeaderPositions[cPosPlusOffset + numSequencesPerThread], numSequencesPerThread));

				if (get<0>(newTuple) != get<1>(newTuple))
					startEndPositions.push_back(newTuple);
			}


		cnewLinePosition += HeaderPositionsPerCache;

		Sequences::multiThreading->mutMainMultithreads.lock();
		TSequencesLoaded = 0;

		TReadFileFromMemArgs_ThreadResults = new pair<vector<vector<char>>, vector<vector<char>>>[startEndPositions.size()];
		for (size_t i = 0; i < startEndPositions.size(); i++)
		{
			TReadFileFromMemArgs* rfArgs = new TReadFileFromMemArgs();
			rfArgs->startEndPosition = { (char*)get<0>(startEndPositions[i]),(char*)get<1>(startEndPositions[i]) };
			rfArgs->ThreadID = Sequences::multiThreading->activeThreads.size();
			rfArgs->destination = pSequences;
			rfArgs->maxSequenceLength = maxSequenceLength;
			rfArgs->numSequencesPerThread = get<2>(startEndPositions[i]);
			rfArgs->IsGPUDeviceUsed = IsGPUDeviceUsed;

			rfArgs->pHeaders = pHeaders;
			rfArgs->argID = i;
			rfArgs->display = Sequences::display;
			rfArgs->TotalSequencesPerThread = numSequencesPerThread;


#ifdef __linux__
			pthread_t pthreadID;// = (pthread_t)emalloc(sizeof(pthread_t), __LINE__);

			int threadStartResult = pthread_create(&pthreadID, NULL, TReadFastaSequencesFromMem, rfArgs);
			if (threadStartResult != 0)
			{
				Sequences::display->logWarningMessage << "Unable to start a new thread to read file!" << endl;
				Sequences::display->logWarningMessage << "Does your machine have enough resources left?" << endl;
				Sequences::display->logWarningMessage << "Error phase: Read fasta from mem" << endl;
				Sequences::display->logWarningMessage << "Error code: " << threadStartResult << endl;
				Sequences::display->logWarningMessage << "Thread ID: " << rfArgs->ThreadID << endl;
				Sequences::display->logWarningMessage << "Running threads: " << Sequences::multiThreading->runningThreadsReadingFileFromMem << endl;
				Sequences::display->logWarningMessage << "Program will stop." << endl;
				Sequences::display->FlushWarning(true);
			}
			pthread_detach(pthreadID);


			Sequences::multiThreading->activeThreads.push_back(pthreadID);
#else
			Sequences::multiThreading->activeThreads.push_back(new thread(TReadFastaSequencesFromMem, rfArgs));
#endif

			multiThreading->runningThreadsReadingFileFromMem++;
		}

		Sequences::multiThreading->mutMainMultithreads.unlock();



		// Wait until all threads are really finished running
		startTimeLoading = time(NULL);
		infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);
		previousProgressLength = -1;
		size_t LoadSizeTotal = numberOfSequences + startEndPositions.size();

		while (multiThreading->runningThreadsReadingFileFromMem > 0)
		{
			multiThreading->WritingToScreen = true;
			Sequences::display->ShowLoadingProgress(TSequencesLoaded, LoadSizeTotal, &previousProgressLength, startTimeLoading, &infoTime);
			multiThreading->WritingToScreen = false;
			multiThreading->TerminateFinishedThreads(true);
			multiThreading->SleepWaitForCPUThreads();
		}



		size_t reserveSize = 0;
		for (size_t i = 0; i < startEndPositions.size(); i++)
			reserveSize += TReadFileFromMemArgs_ThreadResults[i].first.size();
		(*pHeaders).reserve(reserveSize);

		(*pSequences).reserve(reserveSize);

		size_t insertIndex = 0;
		for (size_t i = 0; i < startEndPositions.size(); i++)
		{
			Sequences::display->ShowLoadingProgress(i + numberOfSequences, LoadSizeTotal + startEndPositions.size(), &previousProgressLength, startTimeLoading, &infoTime);

			vector<vector<char>>* _sequences = &TReadFileFromMemArgs_ThreadResults[i].first;
			vector<vector<char>>* _headers = &TReadFileFromMemArgs_ThreadResults[i].second;

			if (DivideReferenceSequences)
			{
				for (size_t i = 0; i < (*_sequences).size(); i++)
				{
					for (size_t seqFrom = 0; seqFrom < (*_sequences)[i].size(); seqFrom += DivideReferenceSequences_Length) // +divdeSequencesBy so we always cycle over the total length of the sequence
					{
						int seqTo = seqFrom + DivideReferenceSequences_Length;
						if (seqTo > (*_sequences)[i].size())
							seqTo = (*_sequences)[i].size();

						vector<char> dividedSequence((*_sequences)[i].begin() + seqFrom, (*_sequences)[i].begin() + seqTo);
						(*pSequences).push_back(dividedSequence);
						(*pHeaders).push_back((*_headers)[i]);
					}
				}
			}
			else
			{
				(*pHeaders).insert((*pHeaders).end(), (*_headers).begin(), (*_headers).end());
				(*pSequences).insert((*pSequences).end(), (*_sequences).begin(), (*_sequences).end());
			}

		}


		future<void> fTrash = std::async(launch::async, Trashcan_TReadFileFromMemArgs_Thread, TReadFileFromMemArgs_ThreadResults);
		//delete[] TReadFileFromMemArgs_ThreadResults;

		//cout << endl << "Mem ReadFastaFromMem 2: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

		if (BuildHashmapDuringRead != nullptr)
		{

			if (taxonomyAutoTranslate) {
				Sequences::display->DisplayTitle("Reading taxonomy file:");
				ReadTaxonomyFile(files->taxonomyFile);
				TranslateUsingTaxonomyFile(pHeaders);
				Sequences::display->DisplayValue("Done", true);
			}

			cout << "ReadFastaFromMem, start building hashmap for #seqs:" << pSequences->size() << ", done " << numSequencesDone << " out of " << newHeaderPositions.size() - 1 << endl;
			numSequencesDone += pSequences->size();
			if (!Analyser::bUseReverseComplementInQuery)
				AppendReverseComplements(pSequences, pHeaders);

			if (SortReferenceSequencesOnLoad)
				SortReferenceSequences(pSequences, pHeaders);

			vector<string> NewHashmapPaths = BuildHashmapDuringRead->BuildHashmapTable(FileLocation);
			files->referenceFilePath.insert(files->referenceFilePath.end(), NewHashmapPaths.begin(), NewHashmapPaths.end());
			BuildHashmapDuringRead->SaveHashMapOutput_Prefix++;

			cout << "ReadFastaFromMem, clear sequences, headers and hashmap" << endl;


		}

		//cout << endl << "Mem ReadFastaFromMem 3: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;


		IDHeaderPos = cnewLinePosition;
		//IDHeaderPos += HeaderPositionsPerCache;

	}


	//Asynchronise::wait_for_futures(&futures);

	Sequences::display->EraseChars(previousProgressLength);

}

///
/// Read Fasta(q) Sequences from a file.
///	Fills header list if list is provided.
/// 
void Sequences::ReadFastaSequencesFromFile(string FileDescription, char* FileLocation, vector<vector<char>>* ptrSequences, vector<vector<char>>* ptrHeaders, bool IsGPUDeviceUsed, size_t maxSequenceLength, Hashmap* BuildHashmapDuringRead)
{
	Sequences::display->DisplayTitle("Initialize " + FileDescription + " file:");
	size_t startTime = time(NULL);
	if (!(files->file_exist(FileLocation)))
		files->fileNotExistError(FileLocation, "Could not find or read fasta file");

	vector<vector<char>> retValue = {};
	//try
	//{
		// Get length of file
	ifstream inf(FileLocation, ios::in | ios::binary);
	size_t lenFile = inf.seekg(0, inf.end).tellg();
	inf.close();



	if (lenFile == 0)
	{
		Sequences::display->DisplayValue("[Sequences: 0].. ", false);
		return;
	}

	// Create file stream
#ifdef __linux__
	int answer = 0;
	int fd = open(FileLocation, O_RDONLY);
	char *  pFileMap = reinterpret_cast<char *>(mmap(NULL, lenFile, PROT_READ, MAP_FILE | MAP_SHARED | MAP_POPULATE, fd, 0));
	madvise(pFileMap, lenFile, MADV_SEQUENTIAL | MADV_WILLNEED);
	close(fd);
#else
		// Convert string to L_STRING
	string locBuf(FileLocation);
	std::wstring stemp = s2ws(locBuf);
	LPCWSTR sFileName = stemp.c_str();

	// Get stream
	HANDLE hFile = CreateFile(sFileName, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); // !! Make sure Visual studio setting: Set Project -> Properties -> General -> Character Set option to Unicode
	HANDLE hMapHnd = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, L"MySharedMapping");
	char* pFileMap = (char *)MapViewOfFile(hMapHnd, FILE_MAP_READ, 0, 0, 0); // Last value is max buffer

	if (pFileMap == NULL)
		throw exception("File could not be opened.");

	imemstream mmapMemStream(pFileMap, lenFile);
	istream in(&mmapMemStream);
#endif

	ReadFastaSequencesFromMem(FileDescription, FileLocation, ptrSequences, pFileMap, lenFile, ptrHeaders, IsGPUDeviceUsed, maxSequenceLength, BuildHashmapDuringRead);

#ifdef __linux__
	//free(pFileMap);
#else
	in.clear();
	mmapMemStream.clear();
	CloseHandle(hMapHnd);
	CloseHandle(hFile);
#endif

	//}
	//catch (exception &ex) {
	//	cerr << "WARNING! Couldn't read input file!" << endl;
	//	cerr << "WARNING! Is file available and does it contain the correct UNIX/windows line break style?" << endl;
	//	cerr << "File location: " << FileLocation << endl;
	//	cerr << "Error: " << ex.what() << endl;
	//	cerr.flush();
	//	throw exception();
	//}

	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);


}

///
/// Complement DNA
/// Source: http://arep.med.harvard.edu/labgc/adnan/projects/Utilities/revcomp.html
///
void Sequences::reverseComplementDNA(vector<char>* src, vector<char>* dest, size_t NumPerThread)
//vector<char> Sequences::reverseComplementDNA(vector<char>* dnaSeq)
{
	//vector<char>* d = (*dest)[dest_i]; // retVal(dnaSeq->size());
	//string retVal = string(dnaSeq->begin(), dnaSeq->end());
	auto alter = [](const char nucl) {
		switch (nucl) {
		case 'A': return 'T';
		case 'T': return 'A';
		case 'U': return 'A';
		case 'G': return 'C';
		case 'C': return 'G';
		case 'Y': return 'R';
		case 'R': return 'Y';
		case 'S': return 'S';
		case 'W': return 'W';
		case 'K': return 'M';
		case 'M': return 'K';
		case 'B': return 'V';
		case 'D': return 'H';
		case 'H': return 'D';
		case 'V': return 'B';
		case 'N': return 'N';
		case '-': return '-';
		case '~': return '~';
		default:
			throw invalid_argument("Unknown Nucleotide: " + string(&nucl));
		}
	};

	for (size_t i = 0; i < NumPerThread; i++)
	{
		try
		{
			dest->resize(src->size());
			transform(src->cbegin(), src->cend(), dest->begin(), alter);
			reverse(dest->begin(), dest->end());
		}
		catch (invalid_argument ex)
		{
			string seq = string(src->begin(), src->end());
			throw invalid_argument("Unable to create a reverse complement sequence.\n" + string(ex.what()) + "\nSequence:\n" + seq);
		}
		src++;
		dest++;
	}
}

void Sequences::AppendReverseComplements(vector<vector<char>> *sequences, vector<vector<char>> *sequenceHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Reverse compliment:");

	size_t numSeq = sequences->size();
	sequences->resize(numSeq * 2);
	sequenceHeaders->reserve(numSeq * 2);

	// Just duplicate the header list
	sequenceHeaders->insert(sequenceHeaders->end(), sequenceHeaders->begin(), sequenceHeaders->end());



	vector<future<void>> futures;
	futures.reserve(numSeq);
	size_t numCores = (size_t)max((unsigned int)1, thread::hardware_concurrency());
	size_t max_thread = numCores * 4;
	size_t numPerThread = max( (size_t)1,numSeq / max_thread);

	for (size_t i = 0; i < numSeq; i += numPerThread)
	{
		size_t current_numPerThread = numPerThread;

		if (i + current_numPerThread > numSeq)
			current_numPerThread = numSeq - i;

		futures.push_back(
			std::async(
				launch::async,
				Sequences::reverseComplementDNA,
				&((*sequences)[i]),
				&((*sequences)[numSeq + i]),
				current_numPerThread
			)
		);
	}

	Asynchronise::wait_for_futures(&futures);

	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);

}

///
/// Read reference and input files
///
#if defined(__CUDA__) // || defined(__OPENCL__)
//extern int Get_LOCAL_DATA_AGAINST_SIZE();
extern size_t Get_LOCAL_DATA_rowhit_SIZE();
#endif
void Sequences::LoadInputFiles(string Output_Path, Hashmap *hashmap, bool IsGPUDeviceUsed)
{
	Sequences::display->DisplayHeader("Reading files");

	if (taxonomyAutoTranslate)
		if (!files->file_exist(&files->taxonomyFile[0]))
			files->fileNotExistError(&files->taxonomyFile[0], "Could not read taxonomy file.");


#if defined(__CUDA__) || defined(__OPENCL__)
	if (IsGPUDeviceUsed)
#if defined(__CUDA__) // || defined(__OPENCL__)
		ReadFastaSequencesFromFile("query", &files->queryFilePath[0], &inputSequences, &inputHeaders, IsGPUDeviceUsed, Get_LOCAL_DATA_rowhit_SIZE());
#else
		ReadFastaSequencesFromFile("query", &files->queryFilePath[0], &inputSequences, &inputHeaders, IsGPUDeviceUsed, 6000); // todo-soon : 6000 is harccoded
#endif
	else
		ReadFastaSequencesFromFile("query", &files->queryFilePath[0], &inputSequences, &inputHeaders, IsGPUDeviceUsed);
#else
	ReadFastaSequencesFromFile("query", &files->queryFilePath[0], &inputSequences, &inputHeaders, IsGPUDeviceUsed);
#endif



	// Sort input data, this way we expect more likely like sequences being processed closed together. Thereby on 1 analysis run, duplicates hashes occur which lowers the number of disk read
	SortQuerySequences(&inputSequences, &inputHeaders);



	//for (auto e : inputSequences)
	//	cout << string(e.begin(), e.end()) << endl;

	if (Analyser::bUseReverseComplementInQuery)
		AppendReverseComplements(&inputSequences, &inputHeaders);




	// check if current node has work to do
	if (multiNodePlatform->divideSequenceReadBetweenNodes)
	{
		if (multiNodePlatform->Node_Current_Index >= inputSequences.size())
		{
			multiNodePlatform->HandleEndOfProgram();
			Sequences::display->DisplayAndEndProgram("Program will stop. Current node (" + to_string(multiNodePlatform->Node_Current_Index) + ") has nothing to do since we have only " + to_string(inputSequences.size()) + " sequences to analyse.");
		}
	}



	if (SkipDuplicateSequences)
		IdentifyDuplicates(&inputSequences);



	// Reference File
	if (Hashmap::IsReferenceHashMapDatabase(&(files->referenceFilePath[0])[0]))  // [0] is used for reference since we do not support multiple fasta references at the moment. So if [0] is a hashmap, the rest is hashmap as well.
	{
		Sequences::display->DisplayHeader("Hashmap database");
		Sequences::display->DisplayTitleAndValue("Loading percentage:", to_string(Hashmap::Hashmap_Load_Hashmap_Percentage) + "%");
		Sequences::display->DisplayTitleAndValue("Bit size:", to_string(hashmap->HASH_Supported_Bytes * 8));

		multiThreading->mutHashmapFile = vector<mutex>(files->referenceFilePath.size());

		hashmap->ReadHashmapFile(files->referenceFilePath, &referenceSequences, &referenceHeaders, &referenceSequences_Lazy, &referenceHashStreamPos, &referenceHeadersHashStreamPos, &referenceHeaderSizes, &referenceSequencesSizes, &referenceHeaders_Lazy);
		Sequences::InitLazyLoading();
	}
	else
	{
		hashmap->Output_Path = Output_Path;
		string referenceFilePath = files->referenceFilePath[0];
		files->referenceFilePath.clear();
		files->referenceFilePath.shrink_to_fit();


		if (!Hashmap::Hashmap_Load_Hashmap_From_File_After_Build)
			Hashmap::Use_Experimental_LazyLoading = false;


		Sequences::display->DisplayTitleAndValue("Split reference sequences:", (DivideReferenceSequences ? "Yes (per " + to_string(DivideReferenceSequences_Length) + ")" : "No"));
		ReadFastaSequencesFromFile("reference", &(referenceFilePath)[0], &referenceSequences, &referenceHeaders, IsGPUDeviceUsed, -1, hashmap); // [0] is used for reference since we do not support multiple fasta references at the moment.



		if (Hashmap::Hashmap_Load_Hashmap_From_File_After_Build)
		{
			// Reset all loaded fasta information
			referenceSequences.clear();
			referenceSequences.shrink_to_fit();
			referenceHeaders.clear();
			referenceHeaders.shrink_to_fit();

			// Reset hashamp
			Hashmap::ResetHashmapTable();

			// Load new build hashmap files
			multiThreading->mutHashmapFile = vector<mutex>(files->referenceFilePath.size());
			hashmap->ReadHashmapFile(files->referenceFilePath, &referenceSequences, &referenceHeaders, &referenceSequences_Lazy, &referenceHashStreamPos, &referenceHeadersHashStreamPos, &referenceHeaderSizes, &referenceSequencesSizes, &referenceHeaders_Lazy);
			Sequences::InitLazyLoading();

		}
	}


	PrepareSequencesForMultiNodeEnv(multiNodePlatform, inputSequences.size());
}


///
/// Get content from lazy loaded hashmap
//mutex m;
vector<mutex*> vMutex;
vector<char*> vIfstreamBuffer;
bool mInitialised(false);
size_t IFStreamBufferSize = 10;
bool useIFStreamBuffer(false);

// Async
vector<vector<mutex*>> vMutex_async;
vector<vector<ifstream*>> vIfstream_hashmap_async;
vector < vector<char*>> vIfstreamBuffer_async;

int Sequences::OpenIfstreamsPerHashmap_Async = 10;


void Sequences::InitOpenFile(ifstream** s, mutex** m, string* location)
{

	*m = new mutex();

	ifstream* sNew = new ifstream();

	sNew->open(*location, ios::in | ios::binary);

	if (!sNew->is_open())
	{
		//display->logWarningMessage << "\nERROR during initialise hashmap:[" << location << "]" << endl;
		//display->logWarningMessage << strerror(errno) << endl;
		//display->FlushWarning(true);
		throw runtime_error(strerror(errno));
	}

	*s = move(sNew);
}
void Sequences::InitLazyLoading()
{

	vMutex.resize(Hashmap::HashmapFileLocations.size());
	vIfstream_hashmap.resize(Hashmap::HashmapFileLocations.size());
	vIfstreamBuffer.resize(Hashmap::HashmapFileLocations.size());
	vIfstreamBuffer_async.resize(Hashmap::HashmapFileLocations.size());
	vIfstream_hashmap_async.resize(Hashmap::HashmapFileLocations.size());
	vMutex_async.resize(Hashmap::HashmapFileLocations.size());



	for (int i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
	{
		//// Open for async use // todo; do we still use this?
		vIfstream_hashmap_async[i].resize(OpenIfstreamsPerHashmap_Async);
		vMutex_async[i].resize(OpenIfstreamsPerHashmap_Async);
		//vIfstreamBuffer_async[i].resize(OpenIfstreamsPerHashmap_Async);
		InitOpenFile(&vIfstream_hashmap[i], &vMutex[i], &Hashmap::HashmapFileLocations[i]);
	}


	int jMaxOpen = OpenIfstreamsPerHashmap_Async;
	OpenIfstreamsPerHashmap_Async = 0; // now reset, and add 1 everytime we succeeded to initiliase a new ifstream for all hashmaps


	for (int j = 0; j < jMaxOpen; j++)
	{
		try
		{
			for (int i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
			{

				InitOpenFile(&vIfstream_hashmap_async[i][j], &vMutex_async[i][j], &Hashmap::HashmapFileLocations[i]);
				if (useIFStreamBuffer)
				{
					vIfstreamBuffer_async[i][j] = (char*)malloc(IFStreamBufferSize);
					vIfstream_hashmap_async[i][j]->rdbuf()->pubsetbuf(vIfstreamBuffer_async[i][j], IFStreamBufferSize);
				}
			}
			OpenIfstreamsPerHashmap_Async++;

		}
		catch (...)
		{
			break;
		}
	}

	if (OpenIfstreamsPerHashmap_Async == 0)
	{
		//todo
		exit(1);
	}
}

///
/// Read the taxonomy file
///
void Sequences::ReadTaxonomyFile(ifstream* inFile)
{
	//if (taxonomyFileRead)
	//	throw (runtime_error("Taxonomy already read."));
	//taxonomyFileRead = true;
	//TaxonomyTranslateTable = new Configuration(files);

	TaxonomyTranslateTable->ReadConfigurationFile(inFile, "\t");
}

///
/// Read the taxonomy file
///
void Sequences::ReadTaxonomyFile(string filePath)
{
	//if (taxonomyFileRead)
	//	throw (runtime_error("Taxonomy already read."));
	//taxonomyFileRead = true;
	//TaxonomyTranslateTable = new Configuration(files);

	TaxonomyTranslateTable->ReadConfigurationFile(filePath, "\t");
}




///
/// Translate the sequences by using the taxonomy data
///
void Sequences::TranslateUsingTaxonomyFile(vector<vector<char>>* listToTranslate)
{
	for (size_t i = 0; i < listToTranslate->size(); i++)
	{
		string currentHeaderName = string((*listToTranslate)[i].begin() + 1, (*listToTranslate)[i].end()); // +1 to cut ">" away from fasta files
		string newHeaderName = TaxonomyTranslateTable->GetConfigurationSetting(currentHeaderName, false);
		replace(newHeaderName.begin(), newHeaderName.end(), *LogManager::Output_ColumnSeperator, '_'); // todo; check if this still works (changed to a static value)

		if (newHeaderName != "")
		{
			(*listToTranslate)[i].clear();
			(*listToTranslate)[i].shrink_to_fit();
			(*listToTranslate)[i].insert((*listToTranslate)[i].end(), newHeaderName.begin(), newHeaderName.end());
		}
	}
}




///
/// Calculate correct sequence block sizes
///
void Sequences::PrepareSequencesForMultiNodeEnv(MultiNodePlatform *multiNodePlatform, size_t numOfSequences)
{
	///
	/// Divide load if multi platform is used
	///
	Sequences::display->DisplayHeader("Multi platform");

	if (multiNodePlatform->divideSequenceReadBetweenNodes)
	{
		int totalNodes = multiNodePlatform->Node_Total;
		if (totalNodes > numOfSequences)// If we have more nodes to work with as sequences to analyse; limit the number of nodes which will do their work
			totalNodes = numOfSequences;

		// Blocksize
		int blockSize = ceil(numOfSequences / totalNodes);

		// From
		multiNodePlatform->Analyse_Sequences_From_Index = multiNodePlatform->Node_Current_Index * blockSize;

		// To
		if (multiNodePlatform->Node_Current_Index == multiNodePlatform->Node_Total - 1)
		{
			multiNodePlatform->Analyse_Sequences_To_Index = numOfSequences - 1; // The rest!
		}
		else {
			multiNodePlatform->Analyse_Sequences_To_Index = multiNodePlatform->Analyse_Sequences_From_Index + blockSize - 1;
			if (multiNodePlatform->Analyse_Sequences_To_Index > numOfSequences - 1)
				multiNodePlatform->Analyse_Sequences_To_Index = numOfSequences - 1;
		}
		Sequences_TotalToAnalyse = multiNodePlatform->Analyse_Sequences_To_Index - multiNodePlatform->Analyse_Sequences_From_Index + 1;

		Sequences::display->DisplayTitleAndValue("Multiplatform detected:", "Yes");
		Sequences::display->DisplayTitleAndValue("Assigned sequences:", to_string(multiNodePlatform->Analyse_Sequences_From_Index + 1) + " to " + to_string(multiNodePlatform->Analyse_Sequences_To_Index + 1));

	}
	else
	{
		Sequences_TotalToAnalyse = numOfSequences;
		Sequences::display->DisplayTitleAndValue("Multiplatform detected:", "No (provide -ni and -nt arguments to enable this feature)");
		Sequences::display->DisplayTitleAndValue("Assigned sequences:", "1 to " + to_string(numOfSequences));
	}
}



#ifdef __linux__

template <typename T, typename Compare>
std::vector<std::size_t> Sequences::sort_permutation(
	const std::vector<T>& vec,
	Compare& compare)
{
	std::vector<std::size_t> p(vec.size());
	std::iota(p.begin(), p.end(), 0);
	std::sort(p.begin(), p.end(),
		[&](std::size_t i, std::size_t j) { return compare(vec[i], vec[j]); });
	return p;
}
template <typename T>
std::vector<T> Sequences::apply_permutation(
	const std::vector<T>& vec,
	const std::vector<std::size_t>& p)
{
	std::vector<T> sorted_vec(p.size());
	std::transform(p.begin(), p.end(), sorted_vec.begin(),
		[&](std::size_t i) { return vec[i]; });
	return sorted_vec;
}
#endif


///
/// compare length of String. Used to sort lists based on length
struct Sequences::compareLen {
	bool operator()(const vector<char> first, const vector<char> second) {
		return first.size() < second.size();
	}
};


#include <numeric>
void Sequences::SortQuerySequences(vector<vector<char>>* ptrSequences, vector<vector<char>>* ptrHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Sort query sequences:");

	vector<size_t> p(ptrSequences->size());
	iota(p.begin(), p.end(), 0);
	sort(p.begin(), p.end(), [&](size_t i, size_t j) { return strcmp((*ptrSequences)[i].data(), (*ptrSequences)[j].data()) < 0; });


	vector<vector<char>> sorted_Sequences((*ptrSequences).size());
	vector<vector<char>> sorted_Header((*ptrSequences).size());
	transform(p.begin(), p.end(), sorted_Sequences.begin(), [&](size_t i) { return (*ptrSequences)[i]; });
	transform(p.begin(), p.end(), sorted_Header.begin(), [&](size_t i) { return (*ptrHeaders)[i]; });

	sorted_Sequences.swap((*ptrSequences));
	sorted_Header.swap((*ptrHeaders));




	/*

			compareLen c;
			auto p = sort_permutation(referenceSequences, c);
			referenceSequences = apply_permutation(referenceSequences, p);
			referenceHeaders = apply_permutation(referenceHeaders, p);*/
	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);


}


#include <numeric>
void Sequences::SortReferenceSequences(vector<vector<char>>* ptrSequences, vector<vector<char>>* ptrHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Sort reference sequences:");

	vector<size_t> p(ptrSequences->size());
	iota(p.begin(), p.end(), 0);
	sort(p.begin(), p.end(), [&](size_t i, size_t j) { return (*ptrSequences)[i].size() < (*ptrSequences)[j].size(); });


	vector<vector<char>> sorted_Sequences((*ptrSequences).size());
	vector<vector<char>> sorted_Header((*ptrSequences).size());
	transform(p.begin(), p.end(), sorted_Sequences.begin(), [&](size_t i) { return (*ptrSequences)[i]; });
	transform(p.begin(), p.end(), sorted_Header.begin(), [&](size_t i) { return (*ptrHeaders)[i]; });

	sorted_Sequences.swap((*ptrSequences));
	sorted_Header.swap((*ptrHeaders));




	/*

			compareLen c;
			auto p = sort_permutation(referenceSequences, c);
			referenceSequences = apply_permutation(referenceSequences, p);
			referenceHeaders = apply_permutation(referenceHeaders, p);*/
	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);


}




///
/// Remove duplicates from list
///
void Sequences::IdentifyDuplicates(vector<vector<char>>* list)
{
	vector<bool> IsDuplicateChild;
	IsDuplicateChild.assign((*list).size(), false);
	CountDuplicateList.assign((*list).size(), vector<int>());

	Sequences::display->DisplayTitle("Start identifying duplicates.. ");
	//todo-later: how to predict? subtract * 0's.. keep track of ' how many to be ignored'
	// todo-later: use hashmap to increase speed.. But if you find a hash, check for real sequence comparison also, since 2 different sequence may end up in the same hash

	for (int i = 0; i < (*list).size() - 1; i++)
	{
		if (IsDuplicateChild[i] == false)
		{
			vector<int> ids(1);
			ids[0] = i;
			//(*DuplicateIDs)[i].push_back(i);

			// don't check for duplicates on the last sequence
			for (int j = i + 1; j < (*list).size(); j++)
				if (IsDuplicateChild[j] == false)
					if ((*list)[i].size() == (*list)[j].size())
						if (equal((*list)[i].begin(), (*list)[i].end(), (*list)[j].begin()))
						{
							ids.push_back(j);
							IsDuplicateChild[j] = true;
							DuplicateQuerySequencesCount++;
						}
			CountDuplicateList[i].swap(ids);
		}
	}

	// add the last sequence
	vector<int> ids(1);
	ids[0] = (*list).size() - 1;
	CountDuplicateList[(*list).size() - 1].swap(ids);


	Sequences::display->DisplayValue(to_string(DuplicateQuerySequencesCount) + " found.");
}

//
//vector<char> Sequences::IsEqual(const char* str1, vector<pair<HASH_STREAMPOS, int>>* reference_Lazy, size_t refID, int32_t RefOffset, int32_t Length)
//{
//
//
//	int hashmapID = Hashmap::GetHashmapID(refID);
//	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
//
//	vMutex[0]->lock();
//
//	try {
//		vIfstream[hashmapID]->seekg((*reference_Lazy)[hashmapID].first);
//
//		// Find and goto item list
//		vIfstream[hashmapID]->ignore((hashRefID + 1) * Hashmap::LoadedBytes); // skip to correct list
//
//		HASH_STREAMPOS spToItems = 0;
//		vIfstream[hashmapID]->read((char*)&spToItems, Hashmap::LoadedBytes);
//		vIfstream[hashmapID]->seekg(spToItems);
//
//
//		int32_t numChars = 0;
//		vIfstream[hashmapID]->read((char*)&numChars, sizeof(int32_t));
//
//		if (Length > 0 && numChars > Length)
//			numChars = Length;
//
//		if (RefOffset > 0)
//			vIfstream[hashmapID]->ignore(RefOffset);
//
//
//		return strncmp(str1, vIfstream[hashmapID]->tellg(), numChars);
//		//vIfstream[hashmapID]->read(&retVal[0], numChars*sizeof(char));
//	}
//	catch (exception e)
//	{
//		cout << "Failed to load lazy data!" << endl;
//		cout << "Hashmap ID: " << hashmapID << endl;
//		cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
//		cout << "Stream position: " << (*reference_Lazy)[hashmapID].first << endl;
//		cout << "Program will stop." << endl;
//		exit(1);
//	}
//
//	vMutex[0]->unlock();
//
//
//	return retVal;
//}
//
//







void Sequences::OpenFile(ifstream* stream, string* path, char* ManualBuffer, size_t ManualBufferSize)
{
	if (stream->is_open())
		return;

	int retryCount = 0;

	if (ManualBufferSize > 0)
		stream->rdbuf()->pubsetbuf(ManualBuffer, ManualBufferSize);

	while (!stream->is_open())
	{
		bool OpenFailed(true);
		try
		{
			stream->open(path->c_str(), ios::in | ios::binary);
			OpenFailed = false;
		}
		catch (exception e) {
		}

		if (OpenFailed || !stream->is_open())
		{

			retryCount++;
			std::this_thread::sleep_for(MultiThreading::THREAD_WAIT_FILE_RETRY_SLEEP); // Just give the CPU some rest..
																			// Do nothing, just retry

			if (retryCount >= MultiThreading::THREAD_MAX_RETRY_COUNT)
			{
				Sequences::display->logWarningMessage << "Unable to read a file!" << endl;
				Sequences::display->logWarningMessage << "File location: " << *path << endl;
				Sequences::display->logWarningMessage << strerror(errno) << endl;
				Sequences::display->logWarningMessage << "Program will stop." << endl;
				Sequences::display->FlushWarning(true);
			}

		}
	}
}




//
//void Sequences::GetLazyContentASync(vector<pair<HASH_STREAMPOS, int>>* reference_Lazy, size_t refID, vector<char>* retVal, int32_t Offset, int32_t Length)
//{
//
//	bool useMultiPreOpenedIfstreams(true);
//	int hashmapID = Hashmap::GetHashmapID(refID);
//	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
//
//	try {
//		ifstream* s;
//		mutex* m;
//		char* buf;
//
//
//		HASH_STREAMPOS spToItems = 0;
//		int32_t numChars = 0;
//		vector<char> bufRetVal = vector<char>();
//
//
//		FILE *fp = fopen(Hashmap::HashmapFileLocations[hashmapID].c_str(), "rb");
//
//
//		fseek(fp, (*reference_Lazy)[hashmapID].first + (hashRefID + 1) * Hashmap::LoadedBytes, SEEK_SET);
//		fread((char*)&spToItems, 1, Hashmap::LoadedBytes, fp);
//
//		fseek(fp, spToItems, SEEK_SET);
//		fread((char*)&numChars, 1, sizeof(int32_t), fp);
//
//
//
//
//		// Set custom buffer
//		bufRetVal.resize(numChars);
//
//		fread(bufRetVal.data(), sizeof(char), numChars, fp);
//
//
//
//		fclose(fp);
//
//
//
//
//		MultiThreading::mutHashmapCache.lock();
//		(*retVal) = move(bufRetVal);
//		MultiThreading::mutHashmapCache.unlock();
//	}
//	catch (exception e)
//	{
//		cout << "Failed to load async lazy data!" << endl;
//		cout << "Hashmap ID: " << hashmapID << endl;
//		cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
//		cout << "Stream position: " << (*reference_Lazy)[hashmapID].first << endl;
//		cout << "Program will stop." << endl;
//		exit(1);
//	}
//}


void Sequences::GetLazyContentASync(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* cacheSizes, size_t refID, vector<char>* retVal, size_t Offset, size_t Length)
{

	//bool useMultiPreOpenedIfstreams(true);
	int hashmapID = Hashmap::GetHashmapID(refID);
	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];

	//try {
	//bool usedLocalStream(true);
	ifstream* s = new ifstream();
	mutex* m = new mutex();
	m->lock();
	//char* buf;

	HASH_STREAMPOS spToItems = 0;
	size_t numChars = 0;
	vector<char> bufRetVal = vector<char>();

	//if (useMultiPreOpenedIfstreams)
	//{
	bool StreamOpen(false);
	while (!StreamOpen)
	{
		for (int i = 0; i < OpenIfstreamsPerHashmap_Async; i++)
		{
			if (vMutex_async[hashmapID][i]->try_lock())
			{
				//usedLocalStream = false;
				m->unlock();
				delete m;
				m = vMutex_async[hashmapID][i];
				delete s;
				s = vIfstream_hashmap_async[hashmapID][i];
				//buf = vIfstreamBuffer_async[hashmapID][i];
				StreamOpen = true;
				break;
			}
		}

		// Be aware: Request more of this method than available Ifstreams opened during initialising may slow down performance
		if (!StreamOpen)
			Sequences::multiThreading->SleepGPU();

	}
	//}
	//else
	//{
	//	s = new ifstream();
	//	Sequences::OpenFile(s, &Hashmap::HashmapFileLocations[hashmapID]);
	//}

	//s->seekg((*reference_Lazy)[hashmapID].StartPosition + (hashRefID)* Hashmap::LoadedBytes);
	//s->read((char*)&spToItems, Hashmap::LoadedBytes); // Find and goto item list
	//s->seekg(spToItems);
	//s->read((char*)&numChars, (*reference_Lazy)[hashmapID].BytesToReadLength);

	s->seekg((*streamposStart)[refID]);

	numChars = (*cacheSizes)[refID];

	if (Length > 0)
		numChars = min(numChars, Length);

	if (Offset > 0)
		s->seekg(s->tellg().operator+(Offset));


	// Set custom buffer
	bufRetVal.resize(numChars);
	s->read(bufRetVal.data(), numChars * sizeof(char));

	//if (!useMultiPreOpenedIfstreams)
		//s->close();

	//if (usedLocalStream)
	//{
	//	delete m;
	//	delete s;
	//}
	//else
	m->unlock();

	//MultiThreading::mutHashmapCache.lock();

	// add string end if not present
	if (bufRetVal.back() != '\0')
		bufRetVal.emplace_back('\0');
	retVal->swap(bufRetVal);
	//(*retVal) = move(bufRetVal);
	//MultiThreading::mutHashmapCache.unlock();
//}
//catch (exception e)
//{
//	cout << "Failed to load async lazy data!" << endl;
//	cout << "Hashmap ID: " << hashmapID << endl;
//	cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
//	cout << "Stream position: " << (*reference_Lazy)[hashmapID].StartPosition << endl;
//	cout << "BytesToReadLength: " << (*reference_Lazy)[hashmapID].BytesToReadLength << endl;
//	cout << "Program will stop." << endl;
//	exit(1);
//}
}

void Sequences::GetLazyContent_HeadTail(vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, vector<char>* tempRefSequence)
{


	HASH_STREAMPOS spToItems = 0;
	dest->SequenceLength = (*reference_lazy_sizes)[refID];
	size_t numCharsHead = min(head->Reference, considerThreshold);
	size_t numCharsTail = min(considerThreshold, dest->SequenceLength - tail->Reference);



	memcpy(dest->Head, tempRefSequence->data() + head->Reference - numCharsHead, numCharsHead);
	memcpy(dest->Tail, tempRefSequence->data() + tail->Reference, numCharsTail);


}
void Sequences::GetLazyContent_HeadTail(ifstream* s, HASH_STREAMPOS* HashStreamPosition, char* dstReference, size_t dstSize)
{
	s->seekg(*HashStreamPosition);
	s->read(dstReference, dstSize);
}

void Sequences::GetLazyContent_HeadTail(vector<Enum::LazyData>* reference_Lazy, vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, ifstream* s, vector< HASH_STREAMPOS>* refStreamPos, int hashmapID, google_hashmap_HeadTailBuffer* HashmapHeadTailBuffers, Enum::LazyData* ptrLazyData)
{

	const bool useCache(false);
	//size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];

	HASH_STREAMPOS spToItems = 0;
	size_t numChars = 0;


	if (!s->is_open())
	{
		display->logWarningMessage << "\nERROR Could not get head/tail from hashmap:[" << Hashmap::HashmapFileLocations[hashmapID] << "]" << endl;
		display->logWarningMessage << strerror(errno) << endl;
		display->FlushWarning(true);
	}





	//s->seekg((*reference_Lazy)[hashmapID].StartPosition + (hashRefID)* Hashmap::LoadedBytes);
	//s->read((char*)&spToItems, Hashmap::LoadedBytes); // Find and goto item list
	//s->seekg(spToItems);
	//s->read((char*)&numChars, (*reference_Lazy)[hashmapID].BytesToReadLength);

	//if (Length > 0)
	//	numChars = min(numChars, Length);

	//if (Offset > 0)
	//	s->seekg(s->tellg().operator+(Offset));


	//// Set custom buffer
	//bufRetVal.resize(numChars);
	//s->read(bufRetVal.data(), numChars * sizeof(char));

	//Enum::LazyData* ptrLazyData = &(*reference_Lazy)[hashmapID];
	//BYTE_CNT BytesToReadLength = ptrLazyData->BytesToReadLength;

	//s->seekg(ptrLazyData->StartPosition + (hashRefID) * Hashmap::LoadedBytes);
	//s->read((char*)&spToItems, Hashmap::LoadedBytes); // Find and goto item list

	spToItems = (*refStreamPos)[refID];

	//s->seekg(spToItems);
	//s->read((char*)&numChars, BytesToReadLength);

	// Get Head
	size_t numCharsHead = min(head->Reference, considerThreshold);


	spToItems += head->Reference;
	spToItems -= numCharsHead;



	if (useCache)
	{
		vMutex[hashmapID]->lock();
		google_hashmap_HeadTailBuffer::const_iterator foundInCache = HashmapHeadTailBuffers->find(spToItems);
		if (foundInCache != HashmapHeadTailBuffers->end())
		{
			vMutex[hashmapID]->unlock();
			*dest = foundInCache->second;
			return;
		}
		vMutex[hashmapID]->unlock();
	}



	dest->SequenceLength = (*reference_lazy_sizes)[refID];// numChars;
	size_t numCharsTail = min(considerThreshold, dest->SequenceLength - tail->Reference);


	//dest->Head.resize(numCharsHead);
	//dest->Tail.resize(numCharsTail);

	//spToItems += BytesToReadLength;



	size_t bufferSize = numCharsHead + tail->Reference - head->Reference + numCharsTail;
	char* bufferChars = new char[bufferSize];
	s->seekg(spToItems);
	s->read(bufferChars, bufferSize);
	//dest->Head = vector<char>(bufferChars, bufferChars + numCharsHead);
	//dest->Tail = vector<char>(bufferChars + numCharsHead + tail->Reference - head->Reference, bufferChars + numCharsHead + tail->Reference - head->Reference + numCharsTail);
	memcpy(dest->Head, bufferChars, numCharsHead);
	memcpy(dest->Tail, bufferChars + numCharsHead + tail->Reference - head->Reference, numCharsTail);

	//bool StreamOpen(false);
	//while (!StreamOpen)
	//{
	//	for (int i = 0; i < OpenIfstreamsPerHashmap_Async; i++)
	//	{
	//		if (vMutex_async[hashmapID][i]->try_lock())
	//		{
	//			m = vMutex_async[hashmapID][i];
	//			_s = vIfstream_hashmap_async[hashmapID][i];
	//			//buf = vIfstreamBuffer_async[hashmapID][i];
	//			StreamOpen = true;
	//			break;
	//		}
	//	}

	//	// Be aware: Request more of this method than available Ifstreams opened during initialising may slow down performance
	//	if (!StreamOpen)
	//		Sequences::multiThreading->SleepGPU();

	//}

	//vMutex[hashmapID]->lock()






	//char* h = (char*)malloc(numCharsHead);
	//char* t = (char*)malloc(numCharsTail);


	//s->seekg(spToItems);
	//s->read(h, numCharsHead);
	//s->ignore(tail->Reference - head->Reference);
	//s->read(t, numCharsTail);



	//memcpy(dest->Head.data(), h, numCharsHead);
	//memcpy(dest->Tail.data(), t, numCharsTail);

	//free(h);
	//free(t);




	//Enum::HeadTailSequence h;
	//h.Head = dest->Head;
	//h.SequenceLength = dest->SequenceLength;
	//h.Tail = dest->Tail;

	if (useCache)

	{
		vMutex[hashmapID]->lock();
		(*HashmapHeadTailBuffers).insert({ spToItems ,*dest });

		vMutex[hashmapID]->unlock();
	}

	//(*HashmapHeadTailBuffers)[spToItems] = *dest;
	//vMutex[hashmapID]->unlock();

	//memcpy(dest->Head.data(), bufferChars, numCharsHead);
	//memcpy(dest->Tail.data(), bufferChars + numCharsHead + tail->Reference - head->Reference, numCharsTail);

	//delete[] bufferChars;


	/////////////



	//dest->Head.resize(numCharsHead);

	//s->read(dest->Head.data(), numCharsHead * sizeof(char));


	//// Get Tail
	//spToItems += numCharsHead; // Go back to head start position
	//spToItems += tail->Reference - head->Reference;
	//s->seekg(spToItems);

	//size_t numCharsTail = min(considerThreshold, dest->SequenceLength - tail->Reference);
	//dest->Tail.resize(numCharsTail);
	//s->read(dest->Tail.data(), numCharsTail * sizeof(char));


	//m->unlock();


}
//void Sequences::GetLazyContent(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* reference_sizes, size_t refID, vector<char>* retVal, size_t Offset, size_t Length)
//{
//
//	int hashmapID = Hashmap::GetHashmapID(refID);
//	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
//	HASH_STREAMPOS spToItems = 0;
//	int32_t numChars = 0;
//
//
//
//
//	try {
//
//		// Find and goto item list
//		//vMutex[hashmapID]->lock();
//		multiThreading->mutHashmapFile[hashmapID].lock();
//
//		//vIfstream_hashmap[hashmapID]->seekg((*reference_Lazy)[hashmapID].StartPosition + (hashRefID)* Hashmap::LoadedBytes);
//		//vIfstream_hashmap[hashmapID]->read((char*)&spToItems, Hashmap::LoadedBytes);
//
//		//cout << "spToItems: " << spToItems << endl;
//		//cout << "(*reference_Lazy)[hashmapID].BytesToReadLength: " << (*reference_Lazy)[hashmapID].BytesToReadLength << endl;
//
//		vIfstream_hashmap[hashmapID]->seekg((*streamposStart)[refID]);
//		numChars = (*reference_sizes)[refID];
//		//vIfstream_hashmap[hashmapID]->seekg(spToItems);
//		//vIfstream_hashmap[hashmapID]->read((char*)&numChars, (*reference_Lazy)[hashmapID].BytesToReadLength);
//
//
//		//todo ; make SizeOfSequence_Bytes different fo rheader!! (we already have this number from the mhm..just use it here
//		if (Length > 0 && numChars > Length)
//			numChars = Length;
//
//		if (Offset > 0)
//			vIfstream_hashmap[hashmapID]->seekg(vIfstream_hashmap[hashmapID]->tellg().operator+(Offset));
//
//		//cout << "numChars: " << numChars << endl;
//
//		//MultiThreading::mutHashmapCache.lock();
//		retVal->resize(numChars);
//		vIfstream_hashmap[hashmapID]->read(&(*retVal)[0], numChars * sizeof(char));
//		multiThreading->mutHashmapFile[hashmapID].unlock();
//		//MultiThreading::mutHashmapCache.unlock();
//		//vMutex[hashmapID]->unlock();
//
//	}
//	catch (exception e)
//	{
//		cout << "Failed to load lazy data!" << endl;
//		cout << "Hashmap ID: " << hashmapID << endl;
//		cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
//		cout << "Program will stop." << endl;
//		exit(1);
//	}
//}


void Sequences::GetLazyContent(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* reference_sizes, size_t refID, string* retVal, size_t Offset, size_t Length)
{

	int hashmapID = Hashmap::GetHashmapID(refID);
	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
	HASH_STREAMPOS spToItems = 0;
	size_t* numChars = 0;




	try {



		// Find and goto item list
		//vMutex[hashmapID]->lock();
		mutex* mHF = &multiThreading->mutHashmapFile[hashmapID];
		mHF->lock();

		ifstream** vIH = &vIfstream_hashmap[hashmapID];
		(*vIH)->seekg((*streamposStart)[refID]);
		numChars = &(*reference_sizes)[refID];

		//todo ; make SizeOfSequence_Bytes different fo rheader!! (we already have this number from the mhm..just use it here
		//if (Length > 0 && numChars > Length)
		//	numChars = Length;

		if (Offset > 0)
			(*vIH)->seekg((*vIH)->tellg().operator+(Offset));

		//MultiThreading::mutHashmapCache.lock();
		retVal->resize(*numChars);
		(*vIH)->read(&(*retVal)[0], *numChars * sizeof(char));
		mHF->unlock();
		//MultiThreading::mutHashmapCache.unlock();

		//vMutex[hashmapID]->unlock();

	}
	catch (exception e)
	{
		cout << "Failed to load lazy data!" << endl;
		cout << "Hashmap ID: " << hashmapID << endl;
		cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
		cout << "Program will stop." << endl;
		exit(1);
	}
}


void Sequences::GetLazyContentWithCache(vector<vector<char>>* cache, vector<size_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, SEQ_ID_t refID, vector<char>* retVal, bool aSync)
{
	bool UseCache(true);

	if (UseCache)
	{
		MultiThreading::mutHashmapCache.lock();
		// Get from Cache
		if (Hashmap::UseCacheForLazyHashmap && (*cache)[refID].size() > 0)
		{
			//(*cacheCounter)[refID]++;
			(*retVal) = (*cache)[refID];
			MultiThreading::mutHashmapCache.unlock();
			return;
		}

		MultiThreading::mutHashmapCache.unlock();
		// Read from disk

		//if (aSync)
		GetLazyContentASync(streamposStart, cacheSizes, refID, retVal);
		//else
			//GetLazyContent(streamposStart, cacheSizes, refID, *retVal);

		MultiThreading::mutHashmapCache.lock();
		(*cache)[refID] = *retVal;
		MultiThreading::mutHashmapCache.unlock();
		return;
	}
}





//void Sequences::GetLazyContentForCuda(vector<vector<char>>* cache, vector<size_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, size_t refID, char** dstSeq, size_t* dstSize, bool aSync)
//{
//	bool UseCache(true);
//	vector<char>* vR = &(*cache)[refID];
//
//	if (UseCache)
//	{
//		//MultiThreading::mutHashmapCache.lock();
//		if (!(Hashmap::UseCacheForLazyHashmap && vR->size() > 0))
//		{
//			if (aSync)
//				GetLazyContentASync(streamposStart, cacheSizes, refID, vR);
//			else
//				GetLazyContent(streamposStart, cacheSizes, refID, vR);
//		}
//
//		if (dstSeq != nullptr || dstSize != nullptr) {
//			MultiThreading::mutHashmapCache.lock();
//			// Set cache for current referenceID
//
//			if (dstSeq != nullptr)
//				*dstSeq = &(*vR)[0];
//			if (dstSize != nullptr)
//				*dstSize = vR->size();
//
//			MultiThreading::mutHashmapCache.unlock();
//		}
//		return;
//
//		//}
//	}
//
//	//throw(new exception("GetLazyContentForCuda does not support non-cache"));
//	// we do not support non-cache in this method
//	exit(1); // todo
//}


//
//
//size_t Sequences::GetSizeOfItem(vector<vector<char>>* cache, vector<Enum::LazyData>* reference_Lazy, size_t itemID)
//{
//	if (Hashmap::UseCacheForLazyHashmap)
//	{
//		MultiThreading::mutHashmapCache.lock();
//		size_t rV = (*cache)[itemID].size();
//		MultiThreading::mutHashmapCache.unlock();
//
//		if (rV > 0)
//			return rV;
//	}
//
//	int hashmapID = Hashmap::GetHashmapID(itemID);
//	int32_t retVal = -1;
//	size_t hashRefID = itemID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
//
//	vMutex[hashmapID]->lock();
//
//
//	//vIfstream[hashmapID]->rdbuf()->pubsetbuf(NULL, 65536);
//
//
//	try {
//		//vIfstream[hashmapID]->seekg((*reference_Lazy)[hashmapID].first);
//		vIfstream_hashmap[hashmapID]->seekg((*reference_Lazy)[hashmapID].StartPosition + (hashRefID + 1) *Hashmap::LoadedBytes);
//		// Find and goto item list
//		//vIfstream[hashmapID]->ignore((hashRefID + 1) *Hashmap::LoadedBytes); // skip to correct list
//		HASH_STREAMPOS spToItems = 0;
//		vIfstream_hashmap[hashmapID]->read((char*)&spToItems, Hashmap::LoadedBytes);
//		vIfstream_hashmap[hashmapID]->seekg(spToItems);
//		vIfstream_hashmap[hashmapID]->read((char*)&retVal, (*reference_Lazy)[hashmapID].BytesToReadLength);
//	}
//	catch (exception e)
//	{
//		cout << "Failed to load lazy data!" << endl;
//		cout << "Hashmap ID: " << hashmapID << endl;
//		cout << "Hashmap file: " << Hashmap::HashmapFileLocations[hashmapID] << endl;
//		cout << "Stream position: " << (*reference_Lazy)[hashmapID].StartPosition << endl;
//		cout << "BytesToReadLength: " << (*reference_Lazy)[hashmapID].BytesToReadLength << endl;
//		cout << "Program will stop." << endl;
//		exit(1);
//	}
//	vMutex[hashmapID]->unlock();
//	return retVal;
//}


Sequences::~Sequences()
{

	for (int i = 0; i < referenceSequences.size(); i++)
	{
		referenceSequences[i].clear();
		referenceSequences[i].shrink_to_fit();
	}
	referenceSequences.clear();
	referenceSequences.shrink_to_fit();

	referenceSequences_Lazy.clear();
	referenceSequences_Lazy.shrink_to_fit();


	for (int i = 0; i < inputSequences.size(); i++)
	{
		inputSequences[i].clear();
		inputSequences[i].shrink_to_fit();
	}
	inputSequences.clear();
	inputSequences.shrink_to_fit();


	for (int i = 0; i < CountDuplicateList.size(); i++)
	{
		CountDuplicateList[i].clear();
		CountDuplicateList[i].shrink_to_fit();
	}

	CountDuplicateList.clear();
	CountDuplicateList.shrink_to_fit();
	for (int i = 0; i < inputHeaders.size(); i++)
	{
		inputHeaders[i].clear();
		inputHeaders[i].shrink_to_fit();
	}
	inputHeaders.shrink_to_fit();
	inputHeaders.clear();

	for (int i = 0; i < referenceHeaders.size(); i++)
	{
		referenceHeaders[i].clear();
		referenceHeaders[i].shrink_to_fit();
	}
	referenceHeaders.clear();
	referenceHeaders.shrink_to_fit();
	referenceHeaders_Lazy.clear();
	referenceHeaders_Lazy.shrink_to_fit();
	delete TaxonomyTranslateTable;
}
Sequences::Sequences(Files *f, Display *d, MultiThreading *mt, MultiNodePlatform *mnp, bool *mca)
{
	TaxonomyTranslateTable = new Configuration();
	TaxonomyTranslateTable->SetFiles(f);
	files = f;
	Sequences::display = d;
	Sequences::multiThreading = mt;
	multiNodePlatform = mnp;
	MultiNodePlatform_CombineResult_AllNodes = mca;
}

