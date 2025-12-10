/*
* Keep track and manipulate DNA sequences
*/
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
size_t Sequences::Reference_Read_Buffer_Size_MB_MaxAllowed_Percentage = 33;  // todo: make this system variable (or config?)

size_t Sequences::MAX_RETRY_OPEN_FILE = 500;
chrono::milliseconds Sequences::FILE_OPEN_FAILED_WAIT_MS = chrono::milliseconds(100);
chrono::milliseconds Sequences::FILE_OPEN_PREPARE_HASHMAP_LOCKED_WAIT = chrono::milliseconds(10);


///
/// Get content from lazy loaded hashmap
///
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

///
/// Uppercase whole vector
///
void Sequences::SequenceToUpperCase(vector<char>* seq)
{
	const size_t seq_size = seq->size();
	if (seq_size == 0)
		return;

	for (size_t i = 0; i < seq_size; i++)
		if ((*seq)[i] > 96)
			(*seq)[i] -= 32;
}

///
/// Thread to read part of fasta sequence from memory
///
void* Sequences::TReadFastaSequencesFromMem(void* args)
{
	TReadFileFromMemArgs* argsForReadMem = (TReadFileFromMemArgs*)args;

	// Create output buffer
	vector<vector<char>>* _sequences = &(TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID].first);
	vector<vector<char>>* _headers = &(TReadFileFromMemArgs_ThreadResults[argsForReadMem->argID].second);
	_sequences->reserve(argsForReadMem->numSequencesPerThread);
	_headers->reserve(argsForReadMem->numSequencesPerThread);

	// Start with first sequence
	_sequences->push_back(vector<char>());
	vector<char>* refSeq = &_sequences->back();


	char* pHeaderStartPositions = argsForReadMem->startEndPosition.first;
	char* pFirstStart = pHeaderStartPositions;
	char* line = new char[0];

	int dataCnt = 0;
	int lineCnt = 0;
	int smartReserve_HighestSize = 1;
	int fastaQCounter = 0;

	bool IsFastaQ(false);
	char* loopEnd = argsForReadMem->startEndPosition.second;

	vector<future<void>> futures;
	futures.reserve(argsForReadMem->TotalSequencesPerThread);

	// Parse file data
	for (; *pHeaderStartPositions && pHeaderStartPositions <= loopEnd; pHeaderStartPositions++)
	{
		while (Sequences::multiThreading->WritingToScreen)
			Sequences::multiThreading->SleepWaitForCPUThreads();

		if (*pHeaderStartPositions == '\n')
		{
			dataCnt = pHeaderStartPositions - pFirstStart;
			free(line);
			line = new char[dataCnt];
			memcpy(line, pFirstStart, dataCnt);
			pFirstStart = pHeaderStartPositions;
			++pFirstStart; // Skip the \n char, that's no DNA!

			// Store our previous sequence if we have a new line or are at the end of the file
			if (line[0] == '>' || line[0] == '@') // Is this a new sequence?
			{
				lineCnt = 0;

				// Is FastQ format?
				if (line[0] == '@')
				{
					fastaQCounter = 0;
					IsFastaQ = true;
				}

				if (line[dataCnt - 1] == '\r')
				{
					line[dataCnt - 1] = '\0';
					_headers->emplace_back(line, line + dataCnt);
				}
				else
				{
					_headers->emplace_back(line, line + dataCnt);
					_headers->back().emplace_back('\0');
				}

				if (refSeq->size() > 0) {

					SequenceToUpperCase(refSeq);

					Sequences::multiThreading->mutexReadFileFromMem.lock();
					TSequencesLoaded++;
					Sequences::multiThreading->mutexReadFileFromMem.unlock();

					// Remember the biggest size. This way we lower the amount of resizes during push_backs of future sequences
					if (refSeq->size() > smartReserve_HighestSize)
						smartReserve_HighestSize = refSeq->size();

					_sequences->push_back(vector<char>());
					refSeq = &(*_sequences)[_sequences->size() - 1];
					refSeq->reserve(smartReserve_HighestSize);
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
		}
	}

	delete[] line;

	// Also add the last data row.
	if (pHeaderStartPositions != pFirstStart) {
		dataCnt = pHeaderStartPositions - pFirstStart;
		line = new char[dataCnt];
		memcpy(line, pFirstStart, dataCnt);
		if ((line[0] != '>' && line[0] != '@') || (IsFastaQ && fastaQCounter > 0)) // Do not save a new header line
		{
			if (line[dataCnt - 1] == '\r')
				refSeq->insert(refSeq->end(), line, line + dataCnt - 1);
			else
				refSeq->insert(refSeq->end(), line, line + dataCnt);
		}
		delete[] line;
	}

	if (refSeq->size() > 0)
		futures.push_back(std::async(launch::async, SequenceToUpperCase, refSeq));


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
	Sequences::multiThreading->threadsFinished.push_back(argsForReadMem->ThreadID);
	Sequences::multiThreading->runningThreadsReadingFileFromMem--;

	// Unlock
	Sequences::multiThreading->mutMainMultithreads.unlock();
	Sequences::multiThreading->mutexReadFileFromMem.unlock();

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


	for (auto rh : referenceHeaders)
		delete rh;
	vector<vector<char>*>().swap(referenceHeaders);

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


///
/// Thrashcan method to delete data later
///
static void Trashcan_TReadFileFromMemArgs_Thread(pair<vector<vector<char>>, vector<vector<char>>>* trash)
{
	delete[] trash;
}

void Sequences::GetNumberOfHeaders(char * &pFileMapPreSize, size_t &numberOfSequences, std::vector<char *> &newHeaderPositions, char * pFileMap, const size_t &lenFileMap, int &previousProgressLength, const size_t &startTimeLoading, size_t &infoTime, std::string &FileDescription)
{
	// Calculate number of headers
	if (*pFileMapPreSize == '>')
	{
		numberOfSequences++;
		newHeaderPositions.push_back(pFileMapPreSize);
	}

	if (numberOfSequences == 0)
	{
		Sequences::display->logWarningMessage << "Unable to read input file!" << endl;
		Sequences::display->logWarningMessage << "0 Headers found" << endl;
		Sequences::display->logWarningMessage << "Program will stop." << endl;
		Sequences::display->FlushWarning(true);
	}

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
}

///
/// Read fasta data form memory stream
///
void Sequences::ReadFastaSequencesFromMem(string FileDescription, char* FileLocation, vector<vector<char>>* pSequences, char* pFileMap, size_t lenFileMap, vector<vector<char>*>* pHeaders, bool IsGPUDeviceUsed, size_t maxSequenceLength, Hashmap* BuildHashmapDuringRead)
{
	size_t numSequencesDone = 0;
	size_t numberOfSequences = 0;
	bool firstChar = true;
	char* pFileMapPreSize = pFileMap;

	size_t startTimeLoading = time(NULL);
	size_t infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);
	vector<char*> newHeaderPositions;
	int previousProgressLength = -1;

	// Get the number of headers ni file
	GetNumberOfHeaders(pFileMapPreSize, numberOfSequences, newHeaderPositions, pFileMap, lenFileMap, previousProgressLength, startTimeLoading, infoTime, FileDescription);

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
			HeaderPositionsPerCache = (double)newHeaderPositions.size() / ((double)lenFileMap / (double)maxByteCache);
	}


	// Loop headers
	bool LoopNewHeaderPositions(true);
	size_t IDHeaderPos = 0;
	while (LoopNewHeaderPositions)
	{
		if (IDHeaderPos >= newHeaderPositions.size())
			break;

		// Start with reset the sequences, headers and hashmap; when multiple passes are performed, we already build and saved the hashmap for the previous sequences
		pSequences->clear();
		pSequences->shrink_to_fit();

		for (auto ph : *pHeaders)
			delete ph;

		pHeaders->clear();
		pHeaders->shrink_to_fit();
		Hashmap::ResetHashmapTable();

		// Thread configuration
		size_t numThreads = 200; // Todo: make configurable
		size_t numSequencesPerThread = 1; // Todo: make configurable
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

		// create from,to- headers list
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

		// Create output bucket for current thread
		TReadFileFromMemArgs_ThreadResults = new pair<vector<vector<char>>, vector<vector<char>>>[startEndPositions.size()];
		for (size_t i = 0; i < startEndPositions.size(); i++)
			TReadFileFromMemArgs_ThreadResults[i] = { vector<vector<char>>(),  vector<vector<char>>() };

		// Start the threads
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
			pthread_t pthreadID;

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
		bool loopOnced(false);
		while (!loopOnced || multiThreading->runningThreadsReadingFileFromMem > 0)
		{
			loopOnced = true;
			multiThreading->WritingToScreen = true;
			Sequences::display->ShowLoadingProgress(TSequencesLoaded, LoadSizeTotal, &previousProgressLength, startTimeLoading, &infoTime);
			multiThreading->WritingToScreen = false;
			multiThreading->TerminateFinishedThreads(true);
			multiThreading->SleepWaitForCPUThreads();
		}

		multiThreading->TerminateFinishedThreads(true);


		// Load head
		size_t reserveSize = 0;
		for (size_t i = 0; i < startEndPositions.size(); i++)
			reserveSize += TReadFileFromMemArgs_ThreadResults[i].first.size();
		(*pHeaders).reserve(reserveSize);
		(*pSequences).reserve(reserveSize);

		// Store the loaded headers in the final list.
		// Or divide the headers if this is required for the used hardware (mostly GPU)
		size_t insertIndex = 0;
		for (size_t i = 0; i < startEndPositions.size(); i++)
		{
			Sequences::display->ShowLoadingProgress(i + numberOfSequences, LoadSizeTotal + startEndPositions.size(), &previousProgressLength, startTimeLoading, &infoTime);

			vector<vector<char>>* _sequences = &TReadFileFromMemArgs_ThreadResults[i].first;
			vector<vector<char>>* _headers = &TReadFileFromMemArgs_ThreadResults[i].second;

			if (DivideReferenceSequences)
			{
				// Divide headers
				for (size_t i = 0; i < (*_sequences).size(); i++)
				{
					for (size_t seqFrom = 0; seqFrom < (*_sequences)[i].size(); seqFrom += DivideReferenceSequences_Length) // +divdeSequencesBy so we always cycle over the total length of the sequence
					{
						int seqTo = seqFrom + DivideReferenceSequences_Length;
						if (seqTo > (*_sequences)[i].size())
							seqTo = (*_sequences)[i].size();

						vector<char> dividedSequence((*_sequences)[i].begin() + seqFrom, (*_sequences)[i].begin() + seqTo);
						(*pSequences).push_back(dividedSequence);
						(*pHeaders).push_back(new vector<char>((*_headers)[i]));
					}
				}
			}
			else
			{
				// Just store the headers directly
				for (auto h : *_headers)
					pHeaders->emplace_back(new vector<char>(h));
				(*pSequences).insert((*pSequences).end(), make_move_iterator((*_sequences).begin()), make_move_iterator((*_sequences).end()));
			}

		}

		IDHeaderPos = cnewLinePosition;

	}


	// Should we build a hashmap?
	if (BuildHashmapDuringRead != nullptr)
	{
		//cout << "ReadFastaFromMem, start building hashmap for #seqs:" << pSequences->size() << ", done " << numSequencesDone << " out of " << newHeaderPositions.size() - 1 << endl;
		numSequencesDone += pSequences->size();

		// Add reverse complements of all sequences
		if (!Analyser::bUseReverseComplementInQuery)
			AppendReverseComplements(pSequences, pHeaders);

		// Sort sequences based on length, this may benefit for GPU memory
		if (SortReferenceSequencesOnLoad)
			SortReferenceSequences(pSequences, pHeaders);

		// Add NCBI taxonomy
		NCBIAccession2TaxID_LookupTaxID(pHeaders, &NCBITaxIDs);

		// Build the hashmap
		vector<string> NewHashmapPaths = BuildHashmapDuringRead->BuildHashmapTable(FileLocation);
		files->referenceFilePath.insert(files->referenceFilePath.end(), NewHashmapPaths.begin(), NewHashmapPaths.end());
		BuildHashmapDuringRead->SaveHashMapOutput_Prefix++;

		//cout << "ReadFastaFromMem, clear sequences, headers and hashmap" << endl;
	}

	// Remove any progressbar information
	Sequences::display->EraseChars(previousProgressLength);
}


///
/// Lookup NCBI TaxID's
///
void Sequences::NCBIAccession2TaxID_LookupTaxID(vector<vector<char>*>* referenceHeaders, vector<int32_t>* NCBITaxIDs)
{
	size_t refCnt = referenceHeaders->size();

	if (refCnt == 0)
		return;

	vector<int32_t>(refCnt, -1).swap(*NCBITaxIDs);

	if (!NCBIAccession2TaxID_Used)
		return;

	if (!files->file_exist(&files->NCBIAccession2TaxIDFile[0]))
		files->fileNotExistError(&files->NCBIAccession2TaxIDFile[0], "Could not read NCBI Accession2TaxID file.");

	// Make lookup for the headers
	unordered_map<string, int32_t*> headerLookup = unordered_map<string, int32_t*>();
	headerLookup.reserve(refCnt);
	vector<char>* ptrRef = (*referenceHeaders)[0];
	int32_t* ptrNCBI = &(*NCBITaxIDs)[0];
	for (size_t i = 0; i < refCnt; i++)
	{
		string sAccession = string(ptrRef->begin(), ptrRef->end());
		string::iterator itrSpace = find(sAccession.begin() + 1, sAccession.end(), ' '); // +1 to skip ">"
		if (itrSpace != sAccession.end())
		{
			sAccession = string(sAccession.begin() + 1, itrSpace);
		}
		headerLookup[sAccession] = ptrNCBI;
		ptrRef++;
		ptrNCBI++;
	}

	// Initialiase
	string delimiter = Configuration::__NCBIACCESSION2TAXID_FILE_DELIMITER_COL;
	int sizeDel = Configuration::__NCBIACCESSION2TAXID_FILE_DELIMITER_COL.length();
	int lineCount = 1;
	size_t c = 0;
	size_t startTimeLoading = time(NULL);
	int previousProgressLength = -1;
	size_t infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);
	size_t showProgressEachN = 1000000;

	// Open the file
	size_t parseSize = 4096 * 1024;
	char* lb = (char *)malloc(parseSize);
	FILE* pFile = fopen(&files->NCBIAccession2TaxIDFile[0], "r");
	size_t totalLines = 0;
	while (!feof(pFile))
	{
		fread(lb, 1, parseSize, pFile);
		totalLines += count(lb, lb + parseSize, '\n');
	}
	rewind(pFile);


	// Read file
	string line_header;
	char* lbOffset = lb;
	size_t lbOffsetSize = 0;
	string sAccession = "";
	int iTaxID = 0;
	int foundNumDelimiters = 0;
	int32_t* ptrTaxID = nullptr;
	char* ptrLine;
	char* prevLine;

	size_t currentReadBytes = 0;
	while (!feof(pFile))
	{
		memset(reinterpret_cast<void*>(lbOffset), '\0', parseSize - lbOffsetSize);
		fread(lbOffset, 1, parseSize - lbOffsetSize, pFile);
		currentReadBytes += parseSize - lbOffsetSize;

		char* ptrLB = lb;
		bool parseNextLine = true;
		lbOffset = lb;

		// Parse line
		while (parseNextLine)
		{
			char* endOfLine = strchr(ptrLB, '\n');
			if (endOfLine == NULL)
			{
				lbOffsetSize = parseSize - (ptrLB - lb);

				memcpy(lb, ptrLB, lbOffsetSize);
				lbOffset = lb + lbOffsetSize;
				break;
			}
			ptrLB = endOfLine + 1;
			parseNextLine &= *ptrLB != '\0';

			foundNumDelimiters = 0;

			ptrTaxID = nullptr;
			ptrLine = ptrLB;

			// Proceed while 3 cells are found
			while (foundNumDelimiters < 3)
			{
				ptrLine++;
				prevLine = ptrLine;
				ptrLine = strchr(ptrLine, '\t');
				if (ptrLine == NULL)
					break;

				foundNumDelimiters++;
				if (foundNumDelimiters < 2)
					continue;
				else if (foundNumDelimiters == 2)
				{
					auto HL_Find = headerLookup.find(string(prevLine, ptrLine));

					if (HL_Find != headerLookup.end())
					{
						ptrTaxID = HL_Find->second;
					}
					else
						break; // No header found, so do not proceed with this line
				}
				else if (foundNumDelimiters == 3)
				{
					const string sTaxID = string(prevLine, ptrLine);
					*ptrTaxID = stoi(sTaxID);
					break;
				}
			}

			// Exit if we did not find a setting and value
			if (ptrTaxID != nullptr && foundNumDelimiters < 3)
			{
				cerr << endl << "ERROR in NCBI Accession2TaxID file, incorrect number of columns (4 expected), found: " << foundNumDelimiters << ": [" << string(ptrLB, endOfLine) << "]" << endl;
				exit(1);
			}

			if (c % showProgressEachN == 0)
				Sequences::display->ShowLoadingProgress(lineCount, totalLines, &previousProgressLength, startTimeLoading, &infoTime);

			c++;

			lineCount++;
		}
	}

	free(lb);
}

///
/// Read Fasta(q) Sequences from a file.
///	Fills header list if list is provided.
/// 
void Sequences::ReadFastaSequencesFromFile(string FileDescription, char* FileLocation, vector<vector<char>>* ptrSequences, vector<vector<char>*>* ptrHeaders, bool IsGPUDeviceUsed, size_t maxSequenceLength, Hashmap* BuildHashmapDuringRead)
{
	Sequences::display->DisplayTitle("Initialize " + FileDescription + " file:");
	size_t startTime = time(NULL);
	if (!(files->file_exist(FileLocation)))
		files->fileNotExistError(FileLocation, "Could not find or read fasta file");

	vector<vector<char>> retValue = {};
	try
	{
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
#endif

		ReadFastaSequencesFromMem(FileDescription, FileLocation, ptrSequences, pFileMap, lenFile, ptrHeaders, IsGPUDeviceUsed, maxSequenceLength, BuildHashmapDuringRead);

#ifdef __linux__
		munmap(pFileMap, lenFile);
#else
		//in.clear();
		mmapMemStream.clear();
		UnmapViewOfFile(pFileMap);
		CloseHandle(hMapHnd);
		CloseHandle(hFile);
#endif

	}
	catch (exception &ex) {
		cerr << "WARNING! Couldn't read input file!" << endl;
		cerr << "WARNING! Is file available and does it contain the correct UNIX/windows line break style?" << endl;
		cerr << "File location: " << FileLocation << endl;
		cerr << "Error: " << ex.what() << endl;
		cerr.flush();
		throw exception();
	}

	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);


}

///
/// Complement DNA
///
void Sequences::reverseComplementDNA(vector<char>* src, vector<char>* dest, size_t NumPerThread)
//vector<char> Sequences::reverseComplementDNA(vector<char>* dnaSeq)
{
	//vector<char>* d = (*dest)[dest_i]; // retVal(dnaSeq->size());
	//string retVal = string(dnaSeq->begin(), dnaSeq->end());
	auto complement = [](const char nucl) {
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
			transform(src->cbegin(), src->cend(), dest->begin(), complement);
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

void Sequences::AppendReverseComplements(vector<vector<char>> *sequences, vector<vector<char>*> *sequenceHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Reverse compliment:");

	size_t numSeq = sequences->size();
	sequences->resize(numSeq * 2);
	sequenceHeaders->reserve(numSeq * 2);

	// Duplicate sequences
	for (size_t i = 0; i < numSeq; i++)
		sequenceHeaders->emplace_back(new vector<char>((*sequenceHeaders)[i]->begin(), (*sequenceHeaders)[i]->end()));


	// Start threads to create reverseComplements
	vector<future<void>> futures;
	futures.reserve(numSeq);
	size_t numCores = (size_t)max((unsigned int)1, thread::hardware_concurrency());
	size_t max_thread = numCores * 4;
	size_t numPerThread = max((size_t)1, numSeq / max_thread);

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
extern size_t Get_LOCAL_DATA_rowhit_SIZE();
#endif
void Sequences::LoadInputFiles(string Output_Path, Hashmap *hashmap, bool IsGPUDeviceUsed)
{
	Sequences::display->DisplayHeader("Reading files");

	if (files->queryFilePath.size() > 0)
	{
		inputSequences = new vector<vector<char>>();

#if defined(__CUDA__) || defined(__OPENCL__)
		if (IsGPUDeviceUsed)
#if defined(__CUDA__) // || defined(__OPENCL__)
			ReadFastaSequencesFromFile("query", &files->queryFilePath[0], inputSequences, &inputHeaders, IsGPUDeviceUsed, Get_LOCAL_DATA_rowhit_SIZE());
#else
			ReadFastaSequencesFromFile("query", &files->queryFilePath[0], inputSequences, &inputHeaders, IsGPUDeviceUsed, 6000); // todo-soon : 6000 is harccoded
#endif
		else
			ReadFastaSequencesFromFile("query", &files->queryFilePath[0], inputSequences, &inputHeaders, IsGPUDeviceUsed);
#else
		ReadFastaSequencesFromFile("query", &files->queryFilePath[0], inputSequences, &inputHeaders, IsGPUDeviceUsed);
#endif

		// Sort input data, this way we expect more likely like sequences being processed closed together. Thereby on 1 analysis run, duplicates hashes occur which lowers the number of disk read
		if (SortQuerySequencesOnLoad)
			SortQuerySequences(inputSequences, &inputHeaders);

		// Append reverse complements
		if (Analyser::bUseReverseComplementInQuery)
			AppendReverseComplements(inputSequences, &inputHeaders);

		// check if current node has work to do
		if (multiNodePlatform->divideSequenceReadBetweenNodes)
		{
			if (multiNodePlatform->Node_Current_Index >= inputSequences->size())
			{
				multiNodePlatform->HandleEndOfProgram();
				Sequences::display->DisplayAndEndProgram("Program will stop. Current node (" + to_string(multiNodePlatform->Node_Current_Index) + ") has nothing to do since we have only " + to_string(inputSequences->size()) + " sequences to analyse.");
			}
		}

		// Identify duplicates to be able to skip those during analysis
		if (SkipDuplicateSequences)
			IdentifyDuplicates(inputSequences);
	}

	// Load Reference File
	if (Hashmap::IsReferenceHashMapDatabase(&(files->referenceFilePath[0])[0]))  // [0] is used for reference since we do not support multiple fasta references at the moment. So if [0] is a hashmap, the rest is hashmap as well.
	{
		// Load ASSCII formatted reference fasta file
		Sequences::display->DisplayHeader("Hashmap database");
		Sequences::display->DisplayTitleAndValue("Loading percentage:", to_string(Hashmap::Hashmap_Load_Hashmap_Percentage) + "%");
		Sequences::display->DisplayTitleAndValue("Bit size:", to_string(hashmap->HASH_Supported_Bytes * 8));

		multiThreading->mutHashmapFile = vector<mutex>(files->referenceFilePath.size());

		hashmap->ReadHashmapFile(files->referenceFilePath, &referenceSequences, &referenceHeaders, &referenceSequences_Lazy, &referenceHashStreamPos, &referenceHeadersHashStreamPos, &referenceHeaderSizes, &referenceSequencesSizes, &referenceHeaders_Lazy);
		Sequences::InitLazyLoading();
	}
	else
	{
		// Load hashmap formatted file
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
			for (auto rh : referenceHeaders)
				delete rh;
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

	PrepareSequencesForMultiNodeEnv(multiNodePlatform, inputSequences->size());
}


///
/// Create a file stream, together with a new mutex
///
void Sequences::InitOpenFile(ifstream** s, mutex** m, string* location)
{
	*m = new mutex();
	ifstream* sNew = new ifstream();

	sNew->open(*location, ios::in | ios::binary);

	if (!sNew->is_open())
		throw runtime_error(strerror(errno));

	*s = move(sNew);
}

///
/// Initialise buffers
///
void Sequences::InitLazyLoading()
{
	// resize
	vMutex.resize(Hashmap::HashmapFileLocations.size());
	vIfstream_hashmap.resize(Hashmap::HashmapFileLocations.size());
	vIfstreamBuffer.resize(Hashmap::HashmapFileLocations.size());
	vIfstreamBuffer_async.resize(Hashmap::HashmapFileLocations.size());
	vIfstream_hashmap_async.resize(Hashmap::HashmapFileLocations.size());
	vMutex_async.resize(Hashmap::HashmapFileLocations.size());

	// Resize 2d lists
	for (int i = 0; i < Hashmap::HashmapFileLocations.size(); i++)
	{
		//// Open for async use // todo; do we still use this?
		vIfstream_hashmap_async[i].resize(OpenIfstreamsPerHashmap_Async);
		vMutex_async[i].resize(OpenIfstreamsPerHashmap_Async);
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
		exit(1);
	}
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






///
/// compare length of String. Used to sort lists based on length
struct Sequences::compareLen {
	bool operator()(const vector<char> first, const vector<char> second) {
		return first.size() < second.size();
	}
};


#include <numeric>
void Sequences::SortQuerySequences(vector<vector<char>>* ptrSequences, vector<vector<char>*>* ptrHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Sort query sequences:");

	vector<size_t> p(ptrSequences->size());
	iota(p.begin(), p.end(), 0);
	sort(p.begin(), p.end(), [&](size_t i, size_t j) { return strcmp((*ptrSequences)[i].data(), (*ptrSequences)[j].data()) < 0; });


	vector<vector<char>> sorted_Sequences((*ptrSequences).size());
	vector<vector<char>*> sorted_Header((*ptrSequences).size());
	transform(p.begin(), p.end(), sorted_Sequences.begin(), [&](size_t i) { return (*ptrSequences)[i]; });
	transform(p.begin(), p.end(), sorted_Header.begin(), [&](size_t i) { return (*ptrHeaders)[i]; });

	sorted_Sequences.swap((*ptrSequences));
	sorted_Header.swap((*ptrHeaders));



	Sequences::display->DisplayValue("Done (" + Sequences::display->GetTimeMessage(time(NULL) - startTime) + ")", true);


}


#include <numeric>
void Sequences::SortReferenceSequences(vector<vector<char>>* ptrSequences, vector<vector<char>*>* ptrHeaders)
{

	size_t startTime = time(NULL);
	Sequences::display->DisplayTitle("Sort reference sequences:");

	vector<size_t> p(ptrSequences->size());
	iota(p.begin(), p.end(), 0);
	sort(p.begin(), p.end(), [&](size_t i, size_t j) { return (*ptrSequences)[i].size() < (*ptrSequences)[j].size(); });


	vector<vector<char>> sorted_Sequences((*ptrSequences).size());
	vector<vector<char>*> sorted_Header((*ptrSequences).size());
	transform(p.begin(), p.end(), sorted_Sequences.begin(), [&](size_t i) { return (*ptrSequences)[i]; });
	transform(p.begin(), p.end(), sorted_Header.begin(), [&](size_t i) { return (*ptrHeaders)[i]; });

	sorted_Sequences.swap((*ptrSequences));
	sorted_Header.swap((*ptrHeaders));

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
	// todo-later: how to predict? subtract * 0's.. keep track of ' how many to be ignored'
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

///
/// Create file stream with buffer
///
void Sequences::OpenFile(ifstream* stream, string* path, char* ManualBuffer, size_t ManualBufferSize)
{
	if (stream->is_open())
		return;

	int retryCount = 0;

	// Set buffer
	if (ManualBufferSize > 0)
		stream->rdbuf()->pubsetbuf(ManualBuffer, ManualBufferSize);

	while (!stream->is_open())
	{
		bool OpenFailed(true);
		try
		{
			// Open file stream
			stream->open(path->c_str(), ios::in | ios::binary);
			OpenFailed = false;
		}
		catch (...) {}

		if (OpenFailed || !stream->is_open())
		{
			retryCount++;
			std::this_thread::sleep_for(MultiThreading::THREAD_WAIT_FILE_RETRY_SLEEP); // Just give the CPU some rest..

			// Do nothing, just retry
			if (retryCount >= MultiThreading::THREAD_MAX_RETRY_COUNT)
				std::this_thread::sleep_for(MultiThreading::THREAD_WAIT_FILE_RETRY_SLEEP_LONG); // Just give the CPU some rest..

			if (retryCount >= MultiThreading::THREAD_MAX_RETRY_COUNT_LONG)
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


///
/// Get Content from hashmap Async
///
void Sequences::GetLazyContentASync(vector<HASH_STREAMPOS>* streamposStart, vector<SEQ_ID_t>* cacheSizes, size_t refID, vector<char>* retVal, size_t Offset, size_t Length)
{
	int hashmapID = Hashmap::GetHashmapID(refID);
	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];

	ifstream* s = new ifstream();
	mutex* m = new mutex();
	m->lock();

	HASH_STREAMPOS spToItems = 0;
	size_t numChars = 0;
	vector<char> bufRetVal = vector<char>();

	bool StreamOpen(false);
	// Try to open a stream
	// Be aware: Request more of this method than available Ifstreams opened during initialising may slow down performance
	while (!StreamOpen)
	{
		for (int i = 0; i < OpenIfstreamsPerHashmap_Async; i++)
		{
			if (vMutex_async[hashmapID][i]->try_lock())
			{
				m->unlock();
				delete m;
				m = vMutex_async[hashmapID][i];
				delete s;
				s = vIfstream_hashmap_async[hashmapID][i];
				StreamOpen = true;
				break;
			}
		}

		if (!StreamOpen)
			Sequences::multiThreading->SleepGPU();

	}

	// Go to seek position
	s->seekg((*streamposStart)[refID]);
	numChars = (*cacheSizes)[refID];

	if (Length > 0)
		numChars = std::min<size_t>(numChars, Length);

	// Increase if we have an offset
	if (Offset > 0)
		s->seekg(s->tellg().operator+(Offset));


	// Read into buffer
	bufRetVal.resize(numChars);
	s->read(bufRetVal.data(), numChars * sizeof(char));
	m->unlock();

	// add string end if not present
	if (bufRetVal.back() != '\0')
		bufRetVal.emplace_back('\0');
	retVal->swap(bufRetVal);
}

///
/// Get head an tail from buffer
///
void Sequences::GetLazyContent_HeadTail(vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, vector<char>* tempRefSequence)
{
	HASH_STREAMPOS spToItems = 0;
	dest->SequenceLength = (*reference_lazy_sizes)[refID];

	// Get max length
	size_t numCharsHead = std::min<size_t>(head->Reference, considerThreshold);
	size_t numCharsTail = min(considerThreshold, dest->SequenceLength - tail->Reference);

	// Copy head and tail
	memcpy(dest->Head, tempRefSequence->data() + head->Reference - numCharsHead, numCharsHead);
	memcpy(dest->Tail, tempRefSequence->data() + tail->Reference, numCharsTail);
}

///
/// Get head an tail from hashmap
///
void Sequences::GetLazyContent_HeadTail(ifstream* s, HASH_STREAMPOS* HashStreamPosition, char* dstReference, size_t dstSize)
{
	s->seekg(*HashStreamPosition);
	s->read(dstReference, dstSize);
}


///
/// Get heads and tails
///
void Sequences::GetLazyContent_HeadTail(vector<Enum::LazyData>* reference_Lazy, vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, ifstream* s, vector< HASH_STREAMPOS>* refStreamPos, int hashmapID, hashmap_HeadTailBuffer* HashmapHeadTailBuffers, Enum::LazyData* ptrLazyData)
{

	const bool useCache(false);

	HASH_STREAMPOS spToItems = 0;
	size_t numChars = 0;


	if (!s->is_open())
	{
		display->logWarningMessage << "\nERROR Could not get head/tail from hashmap:[" << Hashmap::HashmapFileLocations[hashmapID] << "]" << endl;
		display->logWarningMessage << strerror(errno) << endl;
		display->FlushWarning(true);
	}

	spToItems = (*refStreamPos)[refID];

	// Get Head
	size_t numCharsHead = std::min<size_t>(head->Reference, considerThreshold);

	spToItems += head->Reference;
	spToItems -= numCharsHead;

	if (useCache)
	{
		vMutex[hashmapID]->lock();
		hashmap_HeadTailBuffer::const_iterator foundInCache = HashmapHeadTailBuffers->find(spToItems);
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


	size_t bufferSize = numCharsHead + tail->Reference - head->Reference + numCharsTail;
	char* bufferChars = new char[bufferSize];
	s->seekg(spToItems);
	s->read(bufferChars, bufferSize);

	memcpy(dest->Head, bufferChars, numCharsHead);
	memcpy(dest->Tail, bufferChars + numCharsHead + tail->Reference - head->Reference, numCharsTail);


	if (useCache)
	{
		vMutex[hashmapID]->lock();
		(*HashmapHeadTailBuffers).insert({ spToItems ,*dest });

		vMutex[hashmapID]->unlock();
	}
}


///
/// Get Content from hashmap file 
///
void Sequences::GetLazyContent(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* reference_sizes, size_t refID, string* retVal, size_t Offset, size_t Length)
{

	int hashmapID = Hashmap::GetHashmapID(refID);
	size_t hashRefID = refID - (*Hashmap::MultipleHashmaps_SequenceOffset)[hashmapID];
	HASH_STREAMPOS spToItems = 0;
	size_t* numChars = 0;


	try {
		// Find and goto item list

		// Lock file
		mutex* mHF = &multiThreading->mutHashmapFile[hashmapID];
		mHF->lock();

		// Get file and seek to start
		ifstream** vIH = &vIfstream_hashmap[hashmapID];
		(*vIH)->seekg((*streamposStart)[refID]);
		numChars = &(*reference_sizes)[refID];

		// If offset is provided, add it to the current position
		if (Offset > 0)
			(*vIH)->seekg((*vIH)->tellg().operator+(Offset));

		// Copy data into buffer
		retVal->resize(*numChars);
		(*vIH)->read(&(*retVal)[0], *numChars * sizeof(char));

		// Free file
		mHF->unlock();
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


///
/// Get Content from hashmap cache
///
void Sequences::GetLazyContentWithCache(vector<vector<char>>* cache, vector<SEQ_ID_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, SEQ_ID_t refID, vector<char>* retVal, bool aSync)
{
	MultiThreading::mutHashmapCache.lock();
	// Get from Cache if available
	if (Hashmap::UseCacheForLazyHashmap && (*cache)[refID].size() > 0)
	{
		(*retVal) = (*cache)[refID];
		MultiThreading::mutHashmapCache.unlock();
		return;
	}
	MultiThreading::mutHashmapCache.unlock();

	// Read from disk
	GetLazyContentASync(streamposStart, cacheSizes, refID, retVal);

	// Add to cache
	MultiThreading::mutHashmapCache.lock();
	(*cache)[refID] = *retVal;
	MultiThreading::mutHashmapCache.unlock();
	return;
}


///
/// Get Content from hashmap cache
///
void Sequences::GetLazyContentWithCache(vector<vector<char>*>* cache, vector<SEQ_ID_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, SEQ_ID_t refID, vector<char>* retVal, bool aSync)
{
	MultiThreading::mutHashmapCache.lock();
	// Get from Cache if available
	if (Hashmap::UseCacheForLazyHashmap && (*cache)[refID] != NULL)
	{
		(*retVal) = *(*cache)[refID];
		MultiThreading::mutHashmapCache.unlock();
		return;
	}

	MultiThreading::mutHashmapCache.unlock();
	// Read from disk
	GetLazyContentASync(streamposStart, cacheSizes, refID, retVal);

	// Add to cache
	MultiThreading::mutHashmapCache.lock();
	(*cache)[refID] = new vector<char>(*retVal);
	MultiThreading::mutHashmapCache.unlock();
	return;
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

