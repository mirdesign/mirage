#include "Hashmap.h"
#include <random>


mutex Hashmap::mAllowNextThreadFor_BuildHashmap;
bool Hashmap::bAllowNextThreadFor_BuildHashmap = true;
int32_t *Hashmap::ConsiderThreshold = nullptr;
vector<vector<BufferHashmapToReadFromFile>> Hashmap::Hashmap_Position_Data_Decompressed;
vector<char*> Hashmap::Hashmap_Position_Data;
vector<SEQ_ID_t>* Hashmap::MultipleHashmaps_SequenceOffset = new vector<SEQ_ID_t>();
vector<HASH_STREAMPOS>  Hashmap::referenceSequences_First_HashStreamPos;
vector<HASH_STREAMPOS>  Hashmap::Hashmap_First_PosData_HashStreamPos;
vector<hashmap_streams*>* Hashmap::referenceHashMap_Lazy = new vector<hashmap_streams*>();
vector<hashmap_streams*> Hashmap::referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis;
vector<Hashmap::HashmapSettings>* Hashmap::MultipleHashmaps_Settings = new vector<Hashmap::HashmapSettings>();
hashmap_positions* Hashmap::referenceHashMapPaired = new hashmap_positions();
unordered_map<HASH_t, vector<SEQ_ID_t>>* Hashmap::referenceHashMap = new unordered_map<HASH_t, vector<SEQ_ID_t>>();
unordered_map<HASH_t, vector<SEQ_POS_t>>* Hashmap::referenceHashMapPositions = new unordered_map<HASH_t, vector<SEQ_POS_t>>();
unordered_map<HASH_t, size_t>* Hashmap::referenceHashMap_UsageCount = new unordered_map<HASH_t, size_t>();
vector<unordered_map<HASH_t, HASH_STREAMPOS>*>* Hashmap::referenceHashMapPositions_Lazy = new vector<unordered_map<HASH_t, HASH_STREAMPOS>*>();
vector<unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>> Hashmap::MatchedHashStreamPos_Cache = vector<unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>>();

MultiThreading *Hashmap::multiThreading = nullptr;


Enum::Hashmap_MemoryUsageMode Hashmap::Hashmap_Load_Reference_Memory_UsageMode = Enum::Hashmap_MemoryUsageMode::Low;
int Hashmap::Hashmap_Performance_Boost = 0;
int Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy = 0;
int Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold = 0;
bool Hashmap::Use_Experimental_LazyLoading = true;
bool Hashmap::UseCacheForLazyHashmap = true;
bool Hashmap::Hashmap_Load_Hashmap_From_File_After_Build = true; // Set to true, since we now build hashes in vector and then save them to MHM files. So there is no local hashmap build during initial building
int Hashmap::Hashmap_Save_Hashmap_During_Build_Below_Free_Memory = 0;
int Hashmap::Hashmap_Load_Hashmap_Percentage = 0;
bool Hashmap::Hashmap_Load_Hashmap_Skip_On_Failure = true;

int32_t Hashmap::LoadedBytes = 8;
size_t Hashmap::loaded_TotalNumberOfReferences = 0;

SEQ_AMOUNT_t Hashmap::TBuildHashMapAnalysedSequences = 0;
vector<string> Hashmap::HashmapFileLocations = {};
vector<ifstream*> Hashmap::HashmapFileLocations_IFStream = {};
vector<size_t> Hashmap::LazyLoadingCacheCounter = {};
vector<future<void>> _future_Fill_refHashMap_Lazy;

mutex m;


vector<Hashmap::HashPosID>* vHashBuildByThreads = new vector<Hashmap::HashPosID>();
vector<Hashmap::HashPosIDGroup>* vHashmap = new vector<Hashmap::HashPosIDGroup>();

// Hashmap trashcan
vector<future<void>> fClearHashmapThrashCan;
vector<hashmap_positions*> fToClearHashmapThrashCan;
mutex mClearHashmapThrashCan_AddToVector;
mutex mClearHashmapThrashCan;

// For calculating hashes
bool Auto_BuildHashmapTable_Timer_Busy(false);
size_t Auto_BuildHashmapTable_Timer_Start;
size_t Auto_BuildHashmapTable_Timer_End;
mutex mCalculateNumberOfUniqueHashes;
size_t CalculateHashes_FinalHashmap_Offset = 0;
size_t CalculateHashes_FinalHashmap_Size = 0;
size_t CalculateHashes_NumThreadsContributedTo_FinalHashmap_Size = 0;
bool CalculateHashes_ReadyToFillFinalHashmap = false;



/*
Architecture of the hashmap file:

..
11 HASH 1
12 StreampPos to Num of RefID's 1 (48)
13 HASH 2
14 StreampPos to Num of RefID's 2 (53)
.. Num of TaxIDs
.. TaxID 1
.. TaxID 2
.. TaxID 3
15 HASH_STREAMPOS to end of TOC Sequences (19)
16 HASH_STREAMPOS to sequence 1 (23)
17 HASH_STREAMPOS to sequence 2 (29)
18 HASH_STREAMPOS to sequence 3 (34)
19 HASH_STREAMPOS to end of TOC Headers (23)
20 HASH_STREAMPOS to header 1 (39)
21 HASH_STREAMPOS to header 2 (42)
22 HASH_STREAMPOS to header 3 (45)
23 Sequence 1 # Chars
24 Sequence 1 Char 1
25 Sequence 1 Char 2
26 Sequence 1 Char 3
27 Sequence 1 Char 4
28 Sequence 1 Char 5
29 Sequence 2 # Chars
30 Sequence 2 Char 1
31 Sequence 2 Char 2
32 Sequence 2 Char 3
33 Sequence 2 Char 4
34 Sequence 3 # Chars
35 Sequence 3 Char 1
36 Sequence 3 Char 2
37 Sequence 3 Char 3
38 Header 1 # Chars
39 Header 1 Char 1
40 Header 1 Char 2
41 Header 1 Char 3
42 Header 1 # Chars
43 Header 2 Char 1
44 Header 2 Char 2
45 Header 3 # Chars
46 Header 3 Char 1
47 Header 3 Char 2
48 Num of RefID's 1 (2)   <--------------------- spOffsetPrediction pointing to this position
49 RefID
50 POS
51 RefID
52 POS
53 Num of RefID's 2 (2)
54 RefID
55 POS
56 RefID
57 POS

*/


#ifdef _WIN32
#include "Windows.h" // MultiByteToWideChar
///
/// Convert String to wString
///
std::wstring Hashmap::s2ws(const std::string& s)
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
struct Hashmap::membuf : std::streambuf
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
struct Hashmap::imemstream : virtual membuf, std::istream
{
	imemstream(char const* const buf, std::size_t const size)
		: membuf(buf, size),
		std::istream(static_cast<std::streambuf*>(this))
	{ }
};
#endif

///
/// Read Fasta(q) Sequences from a file.
///	Fills header list if list is provided.
/// 
void Hashmap::ReadHashmapFile_Future(size_t startTime, vector<string> FileLocation, int i, vector<SEQ_ID_t> * referenceHashStreamPos, vector<Enum::LazyData> * referenceSequences_Lazy, vector<SEQ_ID_t> * referenceSequenceSizes, vector<Enum::LazyData> * referenceHeaders_Lazy, vector<SEQ_ID_t> * referenceHeadersHashStreamPos, vector<SEQ_ID_t> * referenceHeaderSizes, atomic_size_t* sizeLoaded)
{
	// Initialise
	ifstream infile;
	
	// Set custom buffer for filestream
	char* LocalIFStreamBuffer;
	size_t LocalIFStreamBufferSize = 4096;
	LocalIFStreamBuffer = new char[LocalIFStreamBufferSize];
	infile.rdbuf()->pubsetbuf(LocalIFStreamBuffer, LocalIFStreamBufferSize);

	try
	{
		startTime = time(NULL);

		// Display progress
		string multipleHashmapCountMessage = "";
		if (FileLocation.size() > 1)
			multipleHashmapCountMessage = " (" + to_string(i + 1) + "/" + to_string(FileLocation.size()) + ")";
		display->DisplayTitleAndValue("Loading hashmap" + multipleHashmapCountMessage + ":", FileLocation[i]);
		
		// Open hashmap file
		infile.open(FileLocation[i], ios::in | ios::binary);
		if (!infile.is_open())
			display->DisplayValue("Loading hashmap failed.");

		// Get start and end of the file
		streampos spStart = infile.tellg();
		infile.seekg(0, std::ios::end);
		streampos spEnd = infile.tellg();
		infile.seekg(spStart);

		// Initialise stream hashmap
		Hashmap::HashmapSettings* currentHashmapSettings = &(*Hashmap::MultipleHashmaps_Settings)[i];
		Hashmap::referenceHashMap_Lazy->at(i) = new hashmap_streams();

		// Load hashmap from file
		streampos First_PosData_HashStreamPos = 0;
		try
		{
			LoadHashMapFromHashmapDB(&infile, &sequences->NCBITaxIDs, (*Hashmap::referenceHashMap_Lazy)[i], currentHashmapSettings, spEnd, &First_PosData_HashStreamPos);

			// Remember the first position of data for current hashmap
			Hashmap::Hashmap_First_PosData_HashStreamPos.emplace_back(First_PosData_HashStreamPos);
		}
		catch (exception e)
		{
			// Clean up and show error
			infile.close();
			delete[] LocalIFStreamBuffer;
			display->DisplayValue("Loading hashmap failed.");

			// Continue program if required by the configuration
			if (Hashmap_Load_Hashmap_Skip_On_Failure)
			{
				display->DisplayValue("Skip and continue. (This can be turned off in the settings)");
				return;
			}

			display->logWarningMessage << "Couldn't read hashmap file!" << endl;
			display->FlushWarning(true);
		}

		// Remember reference ID offset of the current database. If we use multiple databases, reference ID's are stacked. Now we know each start per database.
		if (i > 0)
			(*MultipleHashmaps_SequenceOffset)[i] = loaded_TotalNumberOfReferences;


		//cout << "Hashmap::referenceHashMap_Lazy->at(i)->size(): " << Hashmap::referenceHashMap_Lazy->at(i)->size() << endl;
		display->DisplayTitle("Pre-Load reference sequences:");

		// Preload reference sequences
		size_t referenceHashStreamPos_size_before = referenceHashStreamPos->size();
		size_t loadedReferences = 0;
		if (Use_Experimental_LazyLoading)
		{
			LoadReferenceDataFromHashmapDB_SuperLazy(&infile, referenceSequences_Lazy, referenceHashStreamPos, referenceSequenceSizes, &loadedReferences, i, currentHashmapSettings->writeSequenceBitsSize);
			loaded_TotalNumberOfReferences += loadedReferences;
		}

		display->DisplayValue("Done", true);


		// Load complete reference sequences into memory if required by configuration
		if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::Low && Analyser::SpeedMode != Enum::Analysis_Modes::UltraFast)
		{
			size_t startGetRefSeqs = time(NULL);
			display->DisplayTitle("Load reference sequences:");

			// Initialise
			streampos spBefore = infile.tellg();
			Hashmap::referenceSequences_First_HashStreamPos.emplace_back((*referenceHashStreamPos)[referenceHashStreamPos_size_before]);
			infile.seekg(Hashmap::referenceSequences_First_HashStreamPos.back());
			size_t numChars = (referenceHashStreamPos->back() + referenceSequenceSizes->back()) - Hashmap::referenceSequences_First_HashStreamPos.back();

			// Load from disk
			LoadReferenceDataFromHashmapDB(&infile, &sequences->referenceSequences_Buffer, numChars);
			infile.seekg(spBefore); // Reset position to beginning of the data
			display->DisplayValue("Done", true);
			//cout << endl << "\t\t startGetRefSeqs took:.. " << time(NULL) - startGetRefSeqs << endl;
		}
	
		sequences->referenceSequences.resize(loaded_TotalNumberOfReferences, vector<char>());


		// Load reference headers from database
		sequences->referenceHeaders.resize(loaded_TotalNumberOfReferences, nullptr);
		for (auto rh : sequences->referenceHeaders)
			rh = new vector<char>();

		display->DisplayTitle("Load reference headers:");
		if (Use_Experimental_LazyLoading)
		{
			size_t loadedReferences = 0;
			LoadReferenceDataFromHashmapDB_SuperLazy(&infile, referenceHeaders_Lazy, referenceHeadersHashStreamPos, referenceHeaderSizes, &loadedReferences, i, currentHashmapSettings->writeHeaderBitsSize);

			Sequences::sp_ReferenceHeader_AfterLastChar = infile.tellg();
		}
		display->DisplayValue("Done", true);


		*sizeLoaded += infile.tellg();
		*sizeLoaded -= spStart;


		// Load complete positioning data into memory
		if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode == Enum::Hashmap_MemoryUsageMode::Extreme)
		{
			size_t byte_size = spEnd - First_PosData_HashStreamPos;
			infile.seekg(First_PosData_HashStreamPos);
			Hashmap::Hashmap_Position_Data[i] = new char[byte_size];
			infile.read(Hashmap::Hashmap_Position_Data[i], byte_size);
		}

		// Clean up
		infile.close();
		delete[] LocalIFStreamBuffer;

		display->DisplayTitleAndValue("Total load time:", display->GetTimeMessage(time(NULL) - startTime));
	}
	catch (exception &ex) {

		infile.close();
		delete[] LocalIFStreamBuffer;

		display->logWarningMessage << "Couldn't read hashmap file!" << endl;
		display->logWarningMessage << "File location: " << FileLocation[i] << endl;
		display->logWarningMessage << "Error: " << ex.what() << endl;
		display->FlushWarning(true);
	}
}

///
/// Read hashmap files
///
bool Hashmap::ReadHashmapFile(vector<string> FileLocation, vector<vector<char>> *referenceSequences, vector<vector<char>*> *referenceHeaders, vector<Enum::LazyData> *referenceSequences_Lazy, vector<HASH_STREAMPOS> *referenceHashStreamPos, vector<HASH_STREAMPOS> *referenceHeadersHashStreamPos, vector<SEQ_ID_t>* referenceHeaderSizes, vector<SEQ_ID_t>* referenceSequenceSizes, vector<Enum::LazyData> *referenceHeaders_Lazy)
{
	bool sdebug(true);
	size_t startTime = time(NULL);

	Use_Experimental_LazyLoading |= FileLocation.size() > 1; // Set lazy loading ON if we use multiple hashmap databases

	// Check if all files exist
	for (int i = 0; i < FileLocation.size(); i++)
		if (!(files->file_exist(&FileLocation[i][0])))
			files->fileNotExistError(&FileLocation[i][0], "Could not read sequences hashmap file");

	// Initialise
	Hashmap::HashmapFileLocations = FileLocation;
	Hashmap::HashmapFileLocations_IFStream.resize(FileLocation.size(), new ifstream());
	Hashmap::MultipleHashmaps_Settings->resize(FileLocation.size());
	Hashmap::referenceHashMap_Lazy->resize(FileLocation.size());
	Hashmap::referenceHashMapPositions_Lazy->resize(FileLocation.size());
	Hashmap::MatchedHashStreamPos_Cache.resize(FileLocation.size());
	Hashmap_Position_Data.resize(FileLocation.size());
	MultipleHashmaps_SequenceOffset->resize(FileLocation.size());
	size_t numReferences = 0;

	// Add total from potential previously loaded hashmaps
	if (Hashmap::Use_Experimental_LazyLoading)
		numReferences = loaded_TotalNumberOfReferences;
	else
		numReferences = referenceSequences->size();

	(*MultipleHashmaps_SequenceOffset)[0] = numReferences;

	// Read each hashmap file 
	atomic_size_t sizeLoaded(0);
	for (int i = 0; i < FileLocation.size(); i++)
		ReadHashmapFile_Future(startTime, FileLocation, i, referenceHashStreamPos, referenceSequences_Lazy, referenceSequenceSizes, referenceHeaders_Lazy, referenceHeadersHashStreamPos, referenceHeaderSizes, &sizeLoaded);

	// Did we at least load 1 file?
	if (sizeLoaded == 0)
	{
		display->logWarningMessage << "Couldn't read at least 1 hashmap file." << endl;
		display->logWarningMessage << "Program is closing." << endl;
		display->FlushWarning(true);
	}

	display->DisplayTitle("Finish loading:");
	Asynchronise::wait_for_futures(&_future_Fill_refHashMap_Lazy);
	display->DisplayValue("Done", true);
	display->DisplayTitleAndValue("Grand Total load time:", display->GetTimeMessage(time(NULL) - startTime));
	return true;
}


///
/// Resize precalculation result vector
///
void vector_Resize_vSaveHashmapPreCalculates(vector<Hashmap::SaveHashmapPreCalculates>* toResize, size_t size)
{
	toResize->resize(size);
}

///
/// Transform thread results into 1 vector
///
void Fill_refHashMap_Lazy(hashmap_streams* refHashMap_Lazy, pair<HASH_t, Enum::HASH_STREAMPOS_INFO>* fromStart, pair<HASH_t, Enum::HASH_STREAMPOS_INFO>*  toEnd)
{
	refHashMap_Lazy->rehash(toEnd - fromStart);
	refHashMap_Lazy->insert(fromStart, toEnd);
	delete[] fromStart;
}

///
/// Load hashmap from database
///
void Hashmap::LoadHashMapFromHashmapDB(ifstream* infile, vector<int32_t>* NCBITaxIDs, hashmap_streams* refHashMap_Lazy, Hashmap::HashmapSettings* hashmapSettings, streampos spEnd, streampos* First_PosData_HashStreamPos)
{
	// Get length
	HASH_STREAMPOS lenFile = infile->seekg(0, infile->end).tellg();
	infile->seekg(0, infile->beg);

	// Read out special hashmap identicator
	string inHashmapIndicatorMessage(hashmapIndicatorMessage.length(), ' ');
	try
	{
		infile->read((char*)&inHashmapIndicatorMessage[0], sizeof(char) * hashmapIndicatorMessage.length());
	}
	catch (exception e)
	{
		// Do nothing, below check will provide a nice error message.
	}

	if (inHashmapIndicatorMessage != hashmapIndicatorMessage)
	{
		display->logWarningMessage << "Unable to load hashmap data. File does not seem to be a valid MirAge hashmap database." << endl;
		display->logWarningMessage << "Indicator message expected: " << hashmapIndicatorMessage << endl;
		display->logWarningMessage << "Indicator message found: " << inHashmapIndicatorMessage << endl;
		display->FlushWarning(true);
	}

	// Read out number of hashes
	size_t loaded_Hashes;
	infile->read((char*)&loaded_Hashes, sizeof(size_t));

	// Read out version number of used hashmap save method
	int32_t loaded_HashmapVersion;
	infile->read((char*)&loaded_HashmapVersion, sizeof(int32_t));

	if (loaded_HashmapVersion != hashmapVersion)
	{
		display->logWarningMessage << "Unable to load hashmap data." << endl;
		display->logWarningMessage << "Unsupported hashmap version used." << endl;
		display->logWarningMessage << "Loaded hashmap version : '" << loaded_HashmapVersion << "'." << endl;
		display->logWarningMessage << "Supported: '" << hashmapVersion << "'" << endl;
		display->FlushWarning(true);
	}

	infile->read((char*)&LoadedBytes, sizeof(int32_t));
	if (LoadedBytes != HASH_Supported_Bytes)
	{
		display->logWarningMessage << "Unable to load hashmap data." << endl;
		display->logWarningMessage << "Unsupported hashmap bit size used." << endl;
		display->logWarningMessage << "Loaded hashmap bit size : '" << LoadedBytes * 8 << "'." << endl;
		display->logWarningMessage << "Supported: '" << HASH_Supported_Bytes * 8 << "'" << endl;
		display->logWarningMessage << "Use another database or try to compile for the correct bit size using '-D__HASHxx__' where 'xx' is the bitsize required." << endl;
		display->FlushWarning(true);
	}

	// Read out threshold used building the hashmap
	infile->read((char*)&loaded_Threshold, sizeof(int32_t));
	if (*ConsiderThreshold < loaded_Threshold)
	{
		display->logWarningMessage << "Threshold in configurationfile (" << *ConsiderThreshold << ") is less as used during the build of the hashmap (" << loaded_Threshold << ")." << endl;
		display->logWarningMessage << "You may find unexpected results. Therefore, this program will exit." << endl;
		display->logWarningMessage << "Increase your threshold if you want to use this hashmap database." << endl;
		display->FlushWarning(true);
	}
	else if (*ConsiderThreshold > loaded_Threshold)
	{
		display->logWarningMessage << "Threshold in configurationfile is higher (" << *ConsiderThreshold << ") as used during the build of the hashmap (" << loaded_Threshold << ")." << endl;
		display->logWarningMessage << "Create a hashmap using a threshold closer or equal to your current threshold will speed up the search." << endl;
		display->FlushWarning();
	}

	// Read used byte sizes
	infile->read((char*)&hashmapSettings->bytes_streampos, 1);
	infile->read((char*)&hashmapSettings->bytes_numRefIDsPerHash, 1);
	infile->read((char*)&hashmapSettings->writeSequenceBitsSize, 1);
	infile->read((char*)&hashmapSettings->writeHeaderBitsSize, 1);

	Sequences::SizeOfSequence_Bytes = hashmapSettings->writeSequenceBitsSize;
	Sequences::SizeOfHeader_Bytes = hashmapSettings->writeHeaderBitsSize;

	int previousProgressLength = -1;
	size_t startTimeLoading = time(NULL);
	size_t infoTime = time(NULL) - (Display::ProgressLoading_ShowEvery_Second + 1);

	//cout << endl << "Before Load hashmap, free mem: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;
	//cout << endl << "loaded_Hashes: " << loaded_Hashes << endl << flush;

	display->DisplayTitle("Load hashmap (Lazy): ", false);
	display->ShowLoadingProgress(0, loaded_Hashes, &previousProgressLength, startTimeLoading, &infoTime);

	int savedToDbase = 0;

	// Load only partial by randomization, if required by configuration
	mt19937 rng(random_device{}());
	uniform_int_distribution<std::mt19937::result_type> generateRandom(0, 99);
	bool useRandomGenerator = Hashmap::Hashmap_Load_Hashmap_Percentage != 100;
	size_t loaded_Hashes_ByPercentage = loaded_Hashes;

	if (useRandomGenerator)
		loaded_Hashes_ByPercentage = (loaded_Hashes / 100.0) * (Hashmap::Hashmap_Load_Hashmap_Percentage + 5);

	size_t LoadingProcess_FinalAmount = loaded_Hashes * 2;
	display->ShowLoadingProgress(0, LoadingProcess_FinalAmount, &previousProgressLength, startTimeLoading, &infoTime);

	//cout << "loaded_Hashes: " << loaded_Hashes << endl;
	//cout << "loaded_Hashes_ByPercentage: " << loaded_Hashes_ByPercentage << endl;
	//cout << "loaded_Hashes_ByPercentage: " << Hashmap::Hashmap_Load_Hashmap_Percentage << endl;

	// Initialise streampositions
	pair<HASH_t, Enum::HASH_STREAMPOS_INFO>* bufferedPairs = new pair<HASH_t, Enum::HASH_STREAMPOS_INFO>[loaded_Hashes_ByPercentage];
	pair<HASH_t, Enum::HASH_STREAMPOS_INFO>* bufferedPairsStart = bufferedPairs;
	pair<HASH_t, Enum::HASH_STREAMPOS_INFO>* bufferedPairsPrev = bufferedPairs;

	// Calculate required buffer size
	size_t counter = 0;
	size_t bufferSize = 0;
	bufferSize += loaded_Hashes * LoadedBytes;
	bufferSize += loaded_Hashes * hashmapSettings->bytes_streampos;

	// Create buffer
	byte* buffer = new byte[bufferSize];
	byte* bufferStart = buffer;

	// Read all data into buffer
	infile->read((char*)buffer, bufferSize);

	// Convert buffer to hashes
	for (size_t i = 0; i < loaded_Hashes; i++)
	{
		// Load only partial by randomization, if required by configuration
		if (useRandomGenerator && generateRandom(rng) >= Hashmap::Hashmap_Load_Hashmap_Percentage)
			continue;

		display->ShowLoadingProgress(i, LoadingProcess_FinalAmount, &previousProgressLength, startTimeLoading, &infoTime);

		// Copy hash from buffer
		memcpy(&bufferedPairs->first, buffer, LoadedBytes);
		buffer += LoadedBytes;
		memcpy(&bufferedPairs->second.StreamPos, buffer, hashmapSettings->bytes_streampos);
		buffer += hashmapSettings->bytes_streampos;

		// Calculate buffersize
		bufferedPairsPrev->second.BufferSize = bufferedPairs->second.StreamPos - bufferedPairsPrev->second.StreamPos;

		// Remember first stream position
		if (i == 0)
			*First_PosData_HashStreamPos = bufferedPairs->second.StreamPos;

		// Walk to next part of the buffer
		bufferedPairsPrev = bufferedPairs;
		bufferedPairs++;

		if (i == loaded_Hashes - 1)
			bufferedPairs->second.BufferSize = ((HASH_STREAMPOS)spEnd) - bufferedPairs->second.StreamPos;
	}


	delete[] bufferStart;

	// Save all buffered pairs into hashmap, async
	_future_Fill_refHashMap_Lazy.push_back(std::async(launch::async, Fill_refHashMap_Lazy, refHashMap_Lazy, bufferedPairsStart, bufferedPairs));

	// Read all NCBITaxIDs
	size_t numberItems;
	infile->read((char*)&numberItems, LoadedBytes);
	size_t offsetRefStrPos;
	offsetRefStrPos = NCBITaxIDs->size();
	NCBITaxIDs->resize(offsetRefStrPos + (numberItems));
	infile->read((char*)(NCBITaxIDs->data() + (offsetRefStrPos)), sizeof(int32_t) * (numberItems));

	//cout << endl << display->GetTimeMessage(time(NULL) - startTimeLoading);

	display->EraseChars(previousProgressLength);

	display->DisplayValue("Done", true);
	display->DisplayTitleAndValue("\t- Hashmap sequences:", to_string(loaded_Hashes));
	display->DisplayTitleAndValue("\t- Hashmap version:", to_string(loaded_HashmapVersion));
	display->DisplayTitleAndValue("\t- Hashmap BIT size:", to_string(LoadedBytes * 8));
	display->DisplayTitleAndValue("\t- Hashmap threshold:", to_string(loaded_Threshold));
	display->DisplayTitleAndValue("\t- Hashmap % loaded:", to_string(Hashmap::Hashmap_Load_Hashmap_Percentage) + (Hashmap::Hashmap_Load_Hashmap_Percentage < 100 ? " !!" : ""));
}

///
/// Load reference data from disk into new buffer
///
bool Hashmap::LoadReferenceDataFromHashmapDB(ifstream* infile, vector<char*>* dest, size_t numChars)
{
	dest->emplace_back(new char[numChars]);
	infile->read(dest->back(), numChars);
	return true;
}

///
/// Load reference data from disk into new buffer
///
bool Hashmap::LoadReferenceDataFromHashmapDB(ifstream* infile, vector<vector<char>>* dest, vector<SEQ_ID_t>* sizes, size_t numItems)
{
	// Initialise
	size_t startTime = time(NULL);
	int previousProgressLength = -1;
	size_t startTimeLoading = time(NULL);
	size_t infoTime = time(NULL) - (Display::ProgressLoading_ShowEvery_Second + 1);

	// If multiple hashamp are loaded, append to the previous loaded
	size_t offset = dest->size();
	(*dest).reserve(dest->size() + numItems);

	// Loop reference data
	for (int32_t i = 0; i < numItems; i++)
	{
		display->ShowLoadingProgress(i, numItems, &previousProgressLength, startTimeLoading, &infoTime);
		std::vector<char> data;

		// Read out number of items
		int32_t numSubItems = (*sizes)[i + offset];

		// Read data from disk
		data.resize(numSubItems);
		infile->read(&data[0], numSubItems);
		(*dest).push_back(move(data));
	}

	display->EraseChars(previousProgressLength);

	return true;
}

///
/// Load reference data, positions only
bool Hashmap::LoadReferenceDataFromHashmapDB_SuperLazy(ifstream* infile, vector<Enum::LazyData>* dest, vector<HASH_STREAMPOS>* destReferenceStreamPos, vector<SEQ_ID_t>* destSizes, size_t* numberItems, int hashmapID, BYTE_CNT BytesToReadData)
{
	// Skip sequences list
	HASH_STREAMPOS spGoto;
	infile->read((char*)&spGoto, LoadedBytes);
	infile->read((char*)numberItems, LoadedBytes);

	size_t offsetRefStrPos;

	// Read all sizes per reference
	offsetRefStrPos = destSizes->size();
	destSizes->resize(offsetRefStrPos + (*numberItems));
	infile->read((char*)(destSizes->data() + (offsetRefStrPos)), LoadedBytes * (*numberItems));

	// Save current position
	Enum::LazyData newVal;
	newVal.StartPosition = infile->tellg();
	newVal.HashmapID = hashmapID;
	newVal.BytesToReadLength = BytesToReadData;
	dest->push_back(move(newVal));


	// Read all streampositions per reference
	offsetRefStrPos = destReferenceStreamPos->size();
	destReferenceStreamPos->resize(offsetRefStrPos + (*numberItems));
	infile->read((char*)(destReferenceStreamPos->data() + (offsetRefStrPos)), LoadedBytes * (*numberItems));

	//  Reset position to where we were at start of this method
	infile->seekg(spGoto);

	return true;
}

///
/// Empty the hashmap trashcan
///
void ClearHashmapThrashCan()
{
	// Only 1 instance allowed
	if (!mClearHashmapThrashCan.try_lock())
		return;

	vector<hashmap_positions*> tmp_fToClearHashmapThrashCan;

	// Securely pull into local variable
	mClearHashmapThrashCan_AddToVector.lock();
	tmp_fToClearHashmapThrashCan.swap(fToClearHashmapThrashCan);
	mClearHashmapThrashCan_AddToVector.unlock();

	// Delete all content
	for (auto t : tmp_fToClearHashmapThrashCan)
		delete t;

	mClearHashmapThrashCan.unlock();
}

///
/// Clean current and create new hashmap table
void Hashmap::ResetHashmapTable()
{
	// Clean
	vector<Hashmap::HashPosIDGroup>().swap(*vHashmap);
	Hashmap::referenceHashMapPaired->clear();
	Hashmap::referenceHashMapPaired->rehash(0);
	delete Hashmap::referenceHashMapPaired;

	// Create new
	Hashmap::referenceHashMapPaired = new hashmap_positions();
}


///
/// Multithread sort hashPosID list
///
void mtSort_HashPosID(Hashmap::HashPosID  *data, size_t len, int numThreads)
{
	if (numThreads < 2 || len <= numThreads) {
		sort(data, (&(data[len])), [&](Hashmap::HashPosID i, Hashmap::HashPosID j) { return i.hash < j.hash; });
	}
	else {

		std::future<void> b = std::async(std::launch::async, mtSort_HashPosID, data, len / 2, numThreads / 2);
		std::future<void> a = std::async(std::launch::async, mtSort_HashPosID, &data[len / 2], len - len / 2, numThreads / 2);

		a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len], [&](Hashmap::HashPosID i, Hashmap::HashPosID j) { return i.hash < j.hash; });
	}
}


///
/// Multithread sort Hash_t list
///
void mtSort_HASHT(HASH_t  *data, size_t len, int numThreads)
{
	if (numThreads < 2 || len <= numThreads) {
		sort(data, (&(data[len])));
	}
	else {

		std::future<void> b = std::async(std::launch::async, mtSort_HASHT, data, len / 2, numThreads / 2);
		std::future<void> a = std::async(std::launch::async, mtSort_HASHT, &data[len / 2], len - len / 2, numThreads / 2);

		a.wait();
		b.wait();

		std::inplace_merge(data, &data[len / 2], &data[len]);
	}
}


///
/// Copy hashes from threads to one container
///
void Hashmap::ConvertHashesFromThreadToHashmap(size_t vHashBuildByThreads_size)
{
	// Now we have a vector full of struct HashPosID's, sort them
	mtSort_HashPosID(&vHashBuildByThreads->front(), vHashBuildByThreads_size, 4);


	if (vHashBuildByThreads_size > 0)
	{
		// Get number of unique items
		const size_t sizeLoop = vHashBuildByThreads_size;
		HASH_t currentHash = (*vHashBuildByThreads)[0].hash;
		size_t numUnique = 1;
		Hashmap::HashPosID* ptrHPID = &(*vHashBuildByThreads)[1];

		for (size_t i = 1; i < sizeLoop; i++)
		{
			if (ptrHPID->hash != currentHash)
			{
				numUnique++;
				currentHash = ptrHPID->hash;
			}
			ptrHPID++;
		}


		// Insert first
		vHashmap->resize(numUnique);
		size_t vHPIDG_Cnt = 0;

		// Get reference to first
		ptrHPID = &(*vHashBuildByThreads)[0];
		currentHash = ptrHPID->hash;
		ptrHPID++;
		HashPosIDGroup* cHashPosIDGroup = &((*vHashmap)[vHPIDG_Cnt++]);
		const size_t sizeMinusOne = sizeLoop - 1;

		size_t firstIndexOfRange = 0;

		// Loop hashes and group them together with all the positions
		for (size_t i = 1; i <= sizeLoop; i++)
		{
			// Are we at the end of a group or total list?
			if (i == sizeLoop || ptrHPID->hash != currentHash)
			{
				// Get number of positions
				size_t numInsert = i - firstIndexOfRange;

				// Save hash and related positions
				cHashPosIDGroup->hash = currentHash;
				cHashPosIDGroup->PosIDs.reserve(numInsert);

				// Loop the positions
				Hashmap::HashPosID* ptrHPID_Copy = &(*vHashBuildByThreads)[firstIndexOfRange];
				for (size_t j = firstIndexOfRange; j < i; j++)
				{
					cHashPosIDGroup->PosIDs.emplace_back(move(ptrHPID_Copy->PosIDs));
					ptrHPID_Copy++;
				}

				// Are we done?
				if (i == sizeLoop)
					break;

				// Go to next container
				firstIndexOfRange = i;
				currentHash = ptrHPID->hash;
				cHashPosIDGroup = &(*vHashmap)[vHPIDG_Cnt++];
			}
			ptrHPID++;
		}

	}

	// Clean up
	vHashBuildByThreads->clear();
	vHashBuildByThreads->shrink_to_fit();
	vector<Hashmap::HashPosID>().swap(*vHashBuildByThreads);
}

///
/// Build a hashmap table based on the loaded reference file
/// Return by reference: Vector of paths to the new saved hashmap files
///
void CalculateHashes(vector<vector<char>*> sequences, int32_t* ConsiderThreshold, Display* display, vector<HASH_t>* finalHashmap, int index, int* numThreads)
{
	// Initialise
	vector<HASH_t> vHashes;
	HASH_t steppingQuery = (*ConsiderThreshold) / 2;
	const HASH_t hashSizeRef = *ConsiderThreshold - steppingQuery + 1;
	size_t toReserve = 0;

	int perc = 0;
	string ignoreString = string(hashSizeRef, 'N');
	HASH_t ignoreHash = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&ignoreString[0]), hashSizeRef);

	// Determine total number of chars
	for (size_t iRefID = 0; iRefID < sequences.size(); iRefID++)
	{
		vector<char>* refSeq = sequences[iRefID];
		SEQ_POS_t iStop = refSeq->size();
		if (hashSizeRef <= iStop)
			iStop -= hashSizeRef;

		toReserve += iStop;
	}

	// Try to reserve space for the chars
	try {
		vHashes.reserve(toReserve);
	}
	catch (...)
	{
		throw new invalid_argument("OUT OF MEMORY - Too large precalc in building database thread.");
	}

	// Loop sequences
	for (size_t iRefID = 0; iRefID < sequences.size(); iRefID++)
	{
		vector<char>* refSeq = sequences[iRefID];
		SEQ_POS_t iStop = refSeq->size();

		// Skip if we have nothing to work with
		if (iStop == 0)
			continue;

		// Substract hashsize if it does not fall below 0
		if (hashSizeRef <= iStop)
			iStop -= hashSizeRef;

		// Get all non-ignored hashes
		for (SEQ_POS_t i = 0; i < iStop; i++)
		{
			HASH_t newHash = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*refSeq)[i]), hashSizeRef);
			if (newHash != ignoreHash)
				vHashes.emplace_back(newHash);
		}
	}


	
	// Remove duplicates
	Unduplicate(&vHashes);

	// Remember number of found hashes
	mCalculateNumberOfUniqueHashes.lock();
	CalculateHashes_FinalHashmap_Size += vHashes.size();
	CalculateHashes_NumThreadsContributedTo_FinalHashmap_Size++;
	mCalculateNumberOfUniqueHashes.unlock();

	// Wait for all threads to finish
	while (CalculateHashes_NumThreadsContributedTo_FinalHashmap_Size != *numThreads)
		std::this_thread::sleep_for(chrono::milliseconds(500));

	// Insert hashes into main list
	mCalculateNumberOfUniqueHashes.lock();
	finalHashmap->reserve(finalHashmap->size() + vHashes.size());
	std::move(std::begin(vHashes), std::end(vHashes), std::back_inserter(*finalHashmap));
	
	// Unduplicate final hashes, to restrict memory usage
	Unduplicate(finalHashmap);
	mCalculateNumberOfUniqueHashes.unlock();
}

///
/// Unduplicate has list
///
void Unduplicate(std::vector<SEQ_ID_t> *vHashes)
{
	mtSort_HASHT(&vHashes->front(), vHashes->size(), 4);
	auto ptrUnique = unique(vHashes->begin(), vHashes->end());
	vHashes->resize(ptrUnique - vHashes->begin());
	vHashes->shrink_to_fit();
}

///
/// Get the number of unique hashes from all sequences
///
void CalculateNumberOfUniqueHashes(vector<vector<char>*> sequences, int32_t* ConsiderThreshold, Display* display, hashmap_streams* finalHashmap)
{
	// Initialise
	hashmap_streams uniqueHashSet;
	HASH_t steppingQuery = (*ConsiderThreshold) / 2;
	const HASH_t hashSizeRef = *ConsiderThreshold - steppingQuery + 1;

	// Determine max number of hashes so we quit looking for more
	size_t maxSize = pow(2, Hashmap::HashBits);

	// Loop sequences
	for (int iRefID = 0; iRefID < sequences.size(); iRefID++)
	{
		// A hash can never generate more numbers than 2^UsedBits
		if (uniqueHashSet.size() == maxSize)
			break;

		SEQ_POS_t iStop = sequences[iRefID]->size();
		if (hashSizeRef < iStop)
			iStop -= hashSizeRef;

		// Create hashes
		vector<char>* refSeq = sequences[iRefID];
		for (SEQ_POS_t i = 0; i < iStop; i++)
		{
			HASH_t h = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*refSeq)[i]), hashSizeRef);
			if (uniqueHashSet.find(h) == uniqueHashSet.end())
			{
				uniqueHashSet[h] = Enum::HASH_STREAMPOS_INFO();
				uniqueHashSet[h].BufferSize = 0;
				uniqueHashSet[h].StreamPos = 0;
			}
		}
	}

	// Append unique hashes
	mCalculateNumberOfUniqueHashes.lock();
	if (finalHashmap->size() < maxSize)
		finalHashmap->insert(uniqueHashSet.begin(), uniqueHashSet.end());
	mCalculateNumberOfUniqueHashes.unlock();

}
///
/// Build hashmap table
///
vector<string> Hashmap::BuildHashmapTable(char* SourceFileLocation, size_t ReferenceID_Offset)
{
	// Do we have an output path set?
	if (Output_Path == "")
	{
		display->logWarningMessage << "No output path provided during hashmap build." << endl;
		display->FlushWarning(true);
	}

	// Initialise
	vector<string> retVal;
	size_t startTime = time(NULL);
	int32_t cReferenceCnt = 0;
	int refStepping = 2; //todo: was 100
	maxThreadsHashmap = 50;


	display->DisplayTitle("Building hash table v" + to_string(hashmapVersion) + ((Create_Hashmap_ProportionPercentage < 100) ? " (" + to_string(Create_Hashmap_ProportionPercentage) + "%)" : "") + ":", false);


	///
	/// Auto thread size
	///
	int threadPercentageIncrease = 110;
	Auto_BuildHashmapTable_Timer_Start = 0;
	Auto_BuildHashmapTable_Timer_End = 0;
	size_t previousThreadTimeElapsed = 0;
	bool presaveHashmapOccured(false);
	bool autoIncreaseThreadSize(false);
	bool autoIncreaseThreadSizeSet(false);

	int previousProgressLength = -1;
	int MemoryMinimumRequiredForBuildingHashmap = 60;

	size_t numReferenceChars = 0;
	for (int firstRefID = 0; firstRefID < sequences->referenceSequences.size(); firstRefID++)
		numReferenceChars += sequences->referenceSequences[firstRefID].size();

	size_t numHashesPerDatabaseFile = numReferenceChars;
	size_t numHashesPerDatabaseFile_Initial = numHashesPerDatabaseFile;

	//cout << endl << "Before start building.. after rehash: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

	size_t startTimeLoading = time(NULL);
	size_t infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);
	size_t firstRefID_InCurrentHashMap = 0;
	size_t lastRefID_InCurrentHashMap = 0;

	TBuildHashMapAnalysedSequences = 0; // Reset counter if building hashmap is done multiple times (due to out of cache situations)

	size_t startTimeThreadsTotal = time(NULL);
	size_t startTimeThreads = time(NULL);


	HASH_t steppingQuery = (*ConsiderThreshold) / 2;
	const HASH_t hashSizeRef = *ConsiderThreshold - steppingQuery + 1;


	vector<HashPosID>().swap(*vHashBuildByThreads);
	vHashBuildByThreads->resize(numHashesPerDatabaseFile);



	size_t index_vHashBuildByThreads = 0;
	size_t maxBuildHashThreads = 0;

	//cout << " Start loop" << numHashesPerDatabaseFile << endl;

	for (int firstRefID = 0; firstRefID < sequences->referenceSequences.size(); firstRefID += refStepping)
	{

		while (!Hashmap::bAllowNextThreadFor_BuildHashmap
			)
			multiThreading->SleepWaitForCPUThreads();

		Hashmap::mAllowNextThreadFor_BuildHashmap.lock();
		Hashmap::bAllowNextThreadFor_BuildHashmap = false;
		Hashmap::mAllowNextThreadFor_BuildHashmap.unlock();

		// Do we have enough memory to work with?
		// If not, save and start a new hashmap
		size_t freeMem = Analyser::GetMemFreePercentage();

		// Determine the range
		int referenceCount = refStepping;
		if (firstRefID + referenceCount > sequences->referenceSequences.size())
			referenceCount = sequences->referenceSequences.size() - firstRefID;


		size_t numHashesPerDatabaseFile_InCurrentBuild = 0;
		for (int j = firstRefID; j < sequences->referenceSequences.size() && j < firstRefID + referenceCount; j++)
			numHashesPerDatabaseFile_InCurrentBuild += sequences->referenceSequences[j].size();

	
		if (index_vHashBuildByThreads + numHashesPerDatabaseFile_InCurrentBuild > numHashesPerDatabaseFile)
		{
			//cout << endl << " Building hashmap: Out of mem, go for save file during build: " << index_vHashBuildByThreads + numHashesPerDatabaseFile_InCurrentBuild << endl;
			//cout << endl << " Before inter save (free mem)..: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;
			//cout << "multiThreading->runningThreadsBuildHashMap: " << multiThreading->runningThreadsBuildHashMap << endl;


			while (multiThreading->runningThreadsBuildHashMap > 0)
			{
				multiThreading->SleepWaitForCPUThreads();
				display->ShowLoadingProgress(TBuildHashMapAnalysedSequences, numHashesPerDatabaseFile_Initial, &previousProgressLength, startTimeLoading, &infoTime);
			}


			if (!Save_Hashmap_Database)
			{
				display->logWarningMessage << "Unable to start a new thread to build hashmap!" << endl;
				display->logWarningMessage << "Out of memory." << endl;
				display->logWarningMessage << "Program will stop." << endl;
				display->FlushWarning(true);
			}

			// Save current hashmap and start building a new one

			size_t timeConvert = time(NULL);
			// Now we have a vector full oh struct HashPosID's;
			//cout << endl << " Before Convert (free mem)..: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;
			ConvertHashesFromThreadToHashmap(TBuildHashMapAnalysedSequences);
			TBuildHashMapAnalysedSequences = 0;

			//cout << endl << " After Convert (free mem)..: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

			size_t startTimeLoading = time(NULL);
			string hashmapPath = SaveHashmapTableV3(Output_Path, SourceFileLocation, firstRefID_InCurrentHashMap, lastRefID_InCurrentHashMap);
			//cout << endl << " After save (free mem)..: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

			retVal.push_back(hashmapPath);

			SaveHashMapOutput_Prefix++;
			// reset hashmap
			ResetHashmapTable();
			firstRefID_InCurrentHashMap = firstRefID;
			presaveHashmapOccured = true;
			multiThreading->TerminateFinishedThreads(true);

			//cout << " numHashesPerDatabaseFile: " << numHashesPerDatabaseFile << endl;

			vHashBuildByThreads->clear();
			vHashBuildByThreads->shrink_to_fit();
			vHashBuildByThreads->resize(numHashesPerDatabaseFile);
			index_vHashBuildByThreads = 0;
			//cout << endl << " After inter (free mem)..: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

		}




		// Remember the last ID
		lastRefID_InCurrentHashMap = firstRefID + referenceCount;

		display->ShowLoadingProgress(TBuildHashMapAnalysedSequences, numHashesPerDatabaseFile_Initial, &previousProgressLength, startTimeLoading, &infoTime);

		multiThreading->TerminateFinishedThreads(true);

		while (multiThreading->runningThreadsAnalysing >= maxThreadsHashmap)
		{
			display->ShowLoadingProgress(TBuildHashMapAnalysedSequences, numHashesPerDatabaseFile_Initial, &previousProgressLength, startTimeLoading, &infoTime);
			multiThreading->SleepWaitForCPUThreads();
		}


		multiThreading->mutHashMap.lock();
		multiThreading->runningThreadsAnalysing++;
		multiThreading->runningThreadsBuildHashMap++;
		multiThreading->mutHashMap.unlock();


		vector<vector<char>> buildForReferences(sequences->referenceSequences.begin() + firstRefID, sequences->referenceSequences.begin() + firstRefID + referenceCount);

		BuildHashmapTableArgs* argsForPrepareGPU = new BuildHashmapTableArgs();
		argsForPrepareGPU->thisAnalysesUsedForTiming = !autoIncreaseThreadSizeSet;
		autoIncreaseThreadSizeSet = true;


		argsForPrepareGPU->files = files;
		argsForPrepareGPU->ptrReference = buildForReferences;
		argsForPrepareGPU->FirstReferenceID = firstRefID + ReferenceID_Offset;
		argsForPrepareGPU->threadID = (double)multiThreading->activeThreads.size();
		argsForPrepareGPU->tmpBuildFolder = Output_Path;
		argsForPrepareGPU->Create_Hashmap_ProportionPercentage = Create_Hashmap_ProportionPercentage;
		argsForPrepareGPU->Index_vHashBuildByThreads = index_vHashBuildByThreads;
		buildForReferences.clear();
		buildForReferences.shrink_to_fit();

		startTimeThreads = time(NULL);




#ifdef __linux__
		pthread_t pthreadID;
#else
		thread* t;
#endif

		bool threadStarted(false);
		int threadRetryCount = 0;
		int errorCode = 0;

		while (!threadStarted)
		{

			bool startSuccess(false);
			bool ErrorCodeAgain(false);
#ifdef __linux__

			int threadStartResult = pthread_create(&pthreadID, NULL, TBuildHashmapForReference, argsForPrepareGPU);
			startSuccess = threadStartResult == 0;
			ErrorCodeAgain = threadStartResult == EAGAIN;
			errorCode = threadStartResult;
#else


			try
			{
				t = new thread(TBuildHashmapForReference, argsForPrepareGPU);

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
				multiThreading->activeThreads.push_back(pthreadID);
#else
				this->multiThreading->activeThreads.push_back(t);
#endif

			}
			else if (threadRetryCount < MultiThreading::THREAD_MAX_RETRY_COUNT && ErrorCodeAgain)
			{
				threadRetryCount++;
				std::this_thread::sleep_for(MultiThreading::THREAD_START_FAILED_WAIT_SLEEP); // Just give the CPU some rest..
			}
			else
			{
				display->logWarningMessage << "Unable to start a new thread to build hashmap!" << endl;
				display->logWarningMessage << "Does your machine have enough resources left?" << endl;
				display->logWarningMessage << "Try to set Hardware_BlockSize_AutoIncrease_Enabled=0 for your CPU in your configuration or lower the Hardware_maxCPUThreads setting until you don't get this error." << endl;
				display->logWarningMessage << "Error phase: Build hashmap" << endl;
				display->logWarningMessage << "Thread ID: " << argsForPrepareGPU->FirstReferenceID << endl;
				display->logWarningMessage << "Running threads: " << multiThreading->runningThreadsBuildHashMap << endl;
				display->logWarningMessage << "Program will stop." << endl;
				display->FlushWarning(true);
			}
		}





		// Get number of chars to be used for index in vHashBuildByThreads
		for (int i = firstRefID; i < firstRefID + referenceCount; i++)
		{
			SEQ_POS_t iStop = sequences->referenceSequences[i].size();
			if (hashSizeRef < iStop)
				iStop -= hashSizeRef;
			index_vHashBuildByThreads += iStop;
		}

	}




	// Wait until all threads are really finished running
	int previousRunningThreads = 0;
	while (multiThreading->runningThreadsBuildHashMap > 0)
	{
		display->ShowLoadingProgress(TBuildHashMapAnalysedSequences, numHashesPerDatabaseFile_Initial, &previousProgressLength, startTimeLoading, &infoTime);

		if (previousRunningThreads != multiThreading->runningThreadsBuildHashMap)
		{
			multiThreading->TerminateFinishedThreads(true);
			previousRunningThreads = multiThreading->runningThreadsBuildHashMap;
		}
		// Just give the CPU some rest..
		multiThreading->SleepWaitForCPUThreads();
	}

	size_t timeConvert = time(NULL);
	// Now we have a vector full oh struct HashPosID's;
	//cout << endl << "Convert.. " << time(NULL) - timeConvert << endl;
	ConvertHashesFromThreadToHashmap(index_vHashBuildByThreads);
	//cout << endl << "Last Convert took:.. " << time(NULL) - timeConvert << endl;






	display->EraseChars(previousProgressLength);
	display->DisplayValue("Done", true);


	multiThreading->TerminateFinishedThreads(true);



	//cout << endl << "Building in total took:.. " << time(NULL) - startTimeThreadsTotal << endl;


	if (!(multiNodePlatform->divideSequenceReadBetweenNodes && multiNodePlatform->Node_Total > 1))
		if (Save_Hashmap_Database)
		{
			//cout << "firstRefID_InCurrentHashMap: " << firstRefID_InCurrentHashMap << endl;
			//cout << "lastRefID_InCurrentHashMap: " << lastRefID_InCurrentHashMap << endl;
			string hashmapPath = SaveHashmapTableV3(Output_Path, SourceFileLocation, firstRefID_InCurrentHashMap, lastRefID_InCurrentHashMap);
			retVal.push_back(hashmapPath);
		}

	return retVal;

}


string Hashmap::GetHashmapFileName(char* HashmapFileLocation, size_t prefixID)
{
	string retVal = "hashmap_";
	retVal += files->GetFileNameFromPath(string(HashmapFileLocation));

	if (prefixID > 0)
		retVal += to_string(prefixID);
	retVal += ".mhm";
	return retVal;
}


// Save hashmap and reference information directly to storage.
// Do not buffer, since large hashmap takes up twice the memory.
//void Hashmap::SaveHashmapTable(string outPath)
//{
//	string hashmapFileName(outPath + (outPath[outPath.size() - 1] == '\\' || outPath[outPath.size() - 1] == '/' ? "" : "/") + logFileManager->GetLogFilePrefix() + GetHashmapFileName());
//
//	if ((files->file_exist(&hashmapFileName[0])))
//		files->fileExistError(&hashmapFileName[0], "Could not save hashmap file. Destination file already exist.");
//
//
//	display->DisplayTitle("Saving hashmap database:", false);
//
//
//
//
//
//	std::ofstream outFileHashmap;
//	outFileHashmap.open(hashmapFileName, std::ios::out | ios::binary);
//
//
//
//	int32_t numHash = (referenceHashMap->size());
//	size_t numRefSeq = sequences->referenceSequences.size();
//
//	outFileHashmap.write((char*)(&hashmapIndicatorMessage[0]), sizeof(char) * hashmapIndicatorMessage.length());
//	outFileHashmap.write((char*)(&numHash), sizeof(int32_t));
//	outFileHashmap.write((char*)(&hashmapVersion), sizeof(int32_t)); // Version
//	outFileHashmap.write((char*)(&HASH_Supported_Bytes), sizeof(int32_t)); // Bits
//	outFileHashmap.write((char*)(&*ConsiderThreshold), sizeof(int32_t)); // Threshold
//
//
//
//
//
//
//
//	HASH_STREAMPOS spOffsetPrediction = outFileHashmap.tellp();
//
//	spOffsetPrediction += (numHash * sizeof(HASH_t)); // TOC HASH
//	spOffsetPrediction += (numHash * sizeof(HASH_STREAMPOS)); // TOC HASH_STREAMPOS to List of RefIDs
//	spOffsetPrediction += sizeof(HASH_STREAMPOS); // TOC Sequences Size
//	spOffsetPrediction += (numRefSeq * sizeof(HASH_STREAMPOS)); // TOC HASH_STREAMPOS to List of Reference sequences
//	spOffsetPrediction += sizeof(HASH_STREAMPOS); // TOC Headers Size
//	spOffsetPrediction += (numRefSeq * sizeof(HASH_STREAMPOS)); // TOC HASH_STREAMPOS to List of Reference headers
//
//
//
//	//spOffsetPrediction += sizeof(HASH_STREAMPOS); // Sequences position
//
//	/*
//	..
//	11 HASH 1
//	12 StreampPos to Num of RefID's (23)
//	13 HASH 2
//	14 StreampPos to Num of RefID's (28)
//	15 HASH_STREAMPOS to end of TOC Sequences (19)
//	16 HASH_STREAMPOS to sequence 1 (33)
//	17 HASH_STREAMPOS to sequence 2 (39)
//	18 HASH_STREAMPOS to sequence 3 (44)
//	19 HASH_STREAMPOS to end of TOC Headers (23)
//	20 HASH_STREAMPOS to header 1 (48)
//	21 HASH_STREAMPOS to header 2 (52)
//	22 HASH_STREAMPOS to header 3 (54)
//	23 Num of RefID's 1 (2)   <--------------------- spOffsetPrediction pointing to this position
//	24 RefID
//	25 POS
//	26 RefID
//	27 POS
//	28 Num of RefID's 2 (2)
//	29 RefID
//	30 POS
//	31 RefID
//	32 POS
//	33 Sequence 1 # Chars    <--------------------- (After TOC refID:) spOffsetPrediction pointing to this position
//	34 Sequence 1 Char 1
//	35 Sequence 1 Char 2
//	36 Sequence 1 Char 3
//	37 Sequence 1 Char 4
//	38 Sequence 1 Char 5
//	39 Sequence 2 # Chars
//	40 Sequence 2 Char 1
//	41 Sequence 2 Char 2
//	42 Sequence 2 Char 3
//	43 Sequence 2 Char 4
//	44 Sequence 3 # Chars
//	45 Sequence 3 Char 1
//	46 Sequence 3 Char 2
//	47 Sequence 3 Char 3
//	48 Header 1 # Chars
//	49 Header 1 Char 1
//	50 Header 1 Char 2
//	51 Header 1 Char 3
//	52 Header 1 # Chars
//	53 Header 2 Char 1
//	54 Header 2 Char 2
//	55 Header 3 # Chars
//	56 Header 3 Char 1
//	57 Header 3 Char 2
//
//	*/
//
//
//	///
//	/// TOC HASHES + RefID and Position TOC
//	///
//	// Write Hashes + stream position of the actual reference IDs
//
//
//
//
//	for (auto itr = referenceHashMap->begin(); itr != referenceHashMap->end(); ++itr)
//	{
//
//
//		if (spOffsetPrediction > Enum::HASH_STREAMPOS_MAX)
//		{
//			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
//			display->logWarningMessage << "File size to large. " << endl;
//			display->logWarningMessage << "Please switch to 64 bit version." << endl;
//			display->logWarningMessage << "spOffsetPrediction: " << spOffsetPrediction << endl;
//			display->logWarningMessage << "Enum::HASH_STREAMPOS_MAX: " << Enum::HASH_STREAMPOS_MAX << endl;
//			display->FlushWarning(true);
//		}
//		outFileHashmap.write((char*)(&itr->first), sizeof(HASH_t));
//		outFileHashmap.write((char*)(&spOffsetPrediction), sizeof(HASH_STREAMPOS));
//		//cout << spOffsetPrediction << endl;
//		spOffsetPrediction += sizeof(int32_t); // number of reference ID's
//		spOffsetPrediction += sizeof(SEQ_ID_t) * itr->second.size(); // the reference ID's 
//		spOffsetPrediction += sizeof(SEQ_POS_t) * itr->second.size(); // the Positions
//
//
//	}
//
//
//
//	///
//	/// Reference sequences + header TOC
//	/// 
//
//	// Write end of reference sequences TOC
//	HASH_STREAMPOS spAfterToc = outFileHashmap.tellp();
//	spAfterToc += numRefSeq * sizeof(HASH_STREAMPOS); // TOC sequences
//	spAfterToc += 1 * sizeof(HASH_STREAMPOS); // for the spAfterToc write itself
//	outFileHashmap.write((char*)(&spAfterToc), sizeof(HASH_STREAMPOS)); // Write the first position after the TOC, instead of the number of references. This due to 32 or 64 bit of size_t.
//	for (auto itr = sequences->referenceSequences.begin(); itr != sequences->referenceSequences.end(); ++itr)
//	{
//		if (spOffsetPrediction > Enum::HASH_STREAMPOS_MAX)
//		{
//			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
//			display->logWarningMessage << "File size to large. " << endl;
//			display->logWarningMessage << "Please switch to 64 bit version." << endl;
//			display->FlushWarning(true);
//		}
//		outFileHashmap.write((char*)(&spOffsetPrediction), sizeof(HASH_STREAMPOS));
//		spOffsetPrediction += sizeof(int32_t); // The number of chars of current sequence
//		spOffsetPrediction += itr->size() * sizeof(char); // The chars of current sequence
//
//	}
//
//
//
//	// Write end of reference header TOC
//	spAfterToc = outFileHashmap.tellp();
//	spAfterToc += numRefSeq * sizeof(HASH_STREAMPOS); // TOC header
//	spAfterToc += 1 * sizeof(HASH_STREAMPOS); // for the spAfterToc write itself
//	outFileHashmap.write((char*)(&spAfterToc), sizeof(HASH_STREAMPOS)); // Write the first position after the TOC, instead of the number of references. This due to 32 or 64 bit of size_t.
//	for (auto itr = sequences->referenceHeaders.begin(); itr != sequences->referenceHeaders.end(); ++itr)
//	{
//		if (spOffsetPrediction > Enum::HASH_STREAMPOS_MAX)
//		{
//			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
//			display->logWarningMessage << "File size to large. " << endl;
//			display->logWarningMessage << "Please switch to 64 bit version." << endl;
//			display->FlushWarning(true);
//		}
//		outFileHashmap.write((char*)(&spOffsetPrediction), sizeof(HASH_STREAMPOS));
//		spOffsetPrediction += sizeof(int32_t); // The number of chars of current sequence
//		spOffsetPrediction += itr->size() * sizeof(char); // The chars of current sequence
//
//	}
//
//
//
//	///
//	// Write the real data
//	///
//
//	// Write RefID's and positions
//	auto itrPositions = referenceHashMapPositions->begin();
//	for (auto itr = referenceHashMap->begin(); itr != referenceHashMap->end(); ++itr)
//	{
//		if (itrPositions != referenceHashMapPositions->begin())
//			itrPositions++;
//
//		int32_t numSeconds = (itr->second.size());
//		outFileHashmap.write((char*)(&numSeconds), sizeof(int32_t));
//		outFileHashmap.write((char*)(&itr->second.at(0)), sizeof(SEQ_ID_t) * numSeconds); // RefID's
//		outFileHashmap.write((char*)(&itrPositions->second.at(0)), sizeof(SEQ_POS_t) * numSeconds); // Positions
//
//
//		//itrPositions++;
//	}
//
//
//
//	// Write sequences
//	for (auto itr = sequences->referenceSequences.begin(); itr != sequences->referenceSequences.end(); ++itr)
//	{
//		int32_t numItems = itr->size();
//		outFileHashmap.write((char*)(&numItems), sizeof(int32_t));
//		copy(itr->begin(), itr->end(), std::ostream_iterator<char>(outFileHashmap));
//	}
//
//	// Write headers
//	for (vector<char> writeItem : sequences->referenceHeaders)
//	{
//		int32_t numItems = writeItem.size();
//		outFileHashmap.write((char*)(&numItems), sizeof(int32_t));
//		copy(writeItem.begin(), writeItem.end(), ostream_iterator<char>(outFileHashmap));
//	}
//
//
//	outFileHashmap.close();
//	display->DisplayValue("Done", true);
//}
//

int countBits(size_t n)
{
	int count = 0;
	// While loop will run until we get n = 0
	while (n)
	{
		count++;
		// We are shifting n to right by 1 
		// place as explained above
		n = n >> 1;
	}
	return count;
}



void transform_and_swap_async(vector<size_t>::iterator _First, vector<size_t>::iterator _Last, vector<SEQ_ID_t>* _Dest, vector<SEQ_ID_t>* v)
{
	transform(_First, _Last, _Dest->begin(), [&](size_t i) { return  (*v)[i]; });
	_Dest->swap(*v);
}



#include<numeric>

//void SortAndCorrectPairVector(vector<pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>*> ToSort, size_t firstRefID)
void SortAndCorrectPairVector(vector<vector<Hashmap::PosID>*> ToSort, size_t firstRefID)
{
	size_t index = 0;
	for (vector<Hashmap::PosID>* v : ToSort)
	{
		sort(v->begin(), v->end(), [&](Hashmap::PosID i, Hashmap::PosID j) { return i.Pos < j.Pos; });

		// Lower the ID if we use a high firstID value
		// This is used when saving parts of the hashmap due to memory limits
		if (firstRefID > 0)
			for (size_t i = 0; i < v[index].size(); i++)
				v[index][i].ID = v[index][i].ID - firstRefID;
	}
}


void GetMaxMin_Element(vector<SEQ_ID_t>* v, SEQ_ID_t* max, SEQ_ID_t* min)
{
	auto result = std::minmax_element(v->begin(), v->end());

	*min = *result.first;
	*max = *result.second;
}



pair<SEQ_POS_t, SEQ_POS_t> GetMinMax_FromPOS(vector<Hashmap::PosID>* v)
{
	if (v->size() == 0)
		throw invalid_argument("GetMinMax_FromPOS requires at least 1 item");

	SEQ_POS_t minV = (*v)[0].Pos;
	SEQ_POS_t maxV = minV;
	for (auto i : *v)
	{
		minV = min(minV, i.Pos);
		maxV = max(maxV, i.Pos);
	}

	return { minV, maxV };
}

pair<SEQ_ID_t, SEQ_ID_t> GetMinMax_FromID(vector<Hashmap::PosID>* v)
{
	if (v->size() == 0)
		throw invalid_argument("GetMinMax_FromID requires at least 1 item");

	SEQ_POS_t minV = (*v)[0].ID;
	SEQ_POS_t maxV = minV;
	for (auto i : *v)
	{
		minV = min(minV, i.ID);
		maxV = max(maxV, i.ID);
	}

	return { minV, maxV };
}

void GetSaveHashmapPreCalculates(vector<pair<Hashmap::HashPosIDGroup*, Hashmap::SaveHashmapPreCalculates*>> toPreCalculate)
{
	for (pair<Hashmap::HashPosIDGroup*, Hashmap::SaveHashmapPreCalculates*> tPC : toPreCalculate)
	{
		Hashmap::HashPosIDGroup* itr = get<0>(tPC);
		Hashmap::SaveHashmapPreCalculates* vSHPC = get<1>(tPC);
		vSHPC->numSeconds = ((itr)->PosIDs.size());
		auto resultRefID = GetMinMax_FromID(&itr->PosIDs);
		vSHPC->refIDMin = resultRefID.first;
		SEQ_ID_t refIDMax = resultRefID.second;


		vSHPC->refPOSMin = (itr)->PosIDs[0].Pos;
		SEQ_ID_t refPOSMax = (itr)->PosIDs[vSHPC->numSeconds - 1].Pos;


		SEQ_ID_t refID_Dif = refIDMax - vSHPC->refIDMin;
		SEQ_ID_t refPOS_Dif = refPOSMax - vSHPC->refPOSMin;

		for (auto& element : (itr)->PosIDs)
			element.Pos -= vSHPC->refPOSMin;

		vSHPC->bytes_currentRefID = ((countBits(refID_Dif) - 1) / 8) + 1;
		vSHPC->bytes_currentRefPOS = ((countBits(refPOS_Dif) - 1) / 8) + 1;





		vSHPC->bytes_currentRef_PosOffset = ((countBits(max(vSHPC->refIDMin, vSHPC->refPOSMin)) - 1) / 8) + 1;


		vSHPC->bytes_currentRef_NumSeconds = ((countBits(vSHPC->numSeconds) - 1) / 8) + 1;

		vSHPC->spOffsetPrediction = 0;
		vSHPC->spOffsetPrediction += 1; // bytes_currentRef_PosOffset
		vSHPC->spOffsetPrediction += 1; // bytes_currentRefID
		vSHPC->spOffsetPrediction += 1; // bytes_currentRefPOS
		vSHPC->spOffsetPrediction += 1; // bytes_currentRef_NumSeconds
		vSHPC->spOffsetPrediction += vSHPC->bytes_currentRef_PosOffset; // refIDMin
		vSHPC->spOffsetPrediction += vSHPC->bytes_currentRef_PosOffset; // refIDMin
		vSHPC->spOffsetPrediction += vSHPC->bytes_currentRef_NumSeconds; // numSeconds
		vSHPC->spOffsetPrediction += (vSHPC->bytes_currentRefID * (itr)->PosIDs.size()); // refIDMin
		vSHPC->spOffsetPrediction += 1; // bytes_PosNumberRepeation


		// Precalculate compression and required space
		SEQ_POS_t vPosSize = (itr)->PosIDs.size();
		SEQ_POS_t prevPos = ((itr)->PosIDs)[0].Pos;
		SEQ_ID_t prevIPos = 0;
		SEQ_ID_t cntRepeation = 0;

		vSHPC->bytes_PosNumberRepeation = ((countBits(vPosSize) - 1) / 8) + 1;



		vSHPC->vRepeations_offsetPositions.reserve(vPosSize);

		for (SEQ_ID_t iPos = 1; iPos <= vPosSize; iPos++)
		{
			if (iPos == vPosSize || prevPos != ((itr)->PosIDs)[iPos].Pos)
			{
				cntRepeation++;
				vSHPC->vRepeations_offsetPositions.emplace_back(iPos - prevIPos, prevPos);

				if (iPos == vPosSize)
					break;

				prevPos = ((itr)->PosIDs)[iPos].Pos;
				prevIPos = iPos;
			}
		}

		vSHPC->vRepeations_offsetPositions.shrink_to_fit(); // clean up unused reserved space

		vSHPC->spOffsetPrediction += vSHPC->bytes_currentRefPOS; // num
		vSHPC->spOffsetPrediction += cntRepeation * vSHPC->bytes_PosNumberRepeation; // prevPos
		vSHPC->spOffsetPrediction += cntRepeation * vSHPC->bytes_currentRefPOS; // prevPos
	}
}
// Save hashmap and reference information directly to storage.
// Do not buffer, since large hashmap takes up twice the memory.
string Hashmap::SaveHashmapTableV3(string outPath, char* SourceFileLocation, size_t firstRefID_InCurrentHashMap, size_t lastRefID_InCurrentHashMap)
{
	if (lastRefID_InCurrentHashMap < lastRefID_InCurrentHashMap)
		exit(1); // todo; provide good error

	//cout << endl << "Before start building.. Before save: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

	size_t startTimeLoading = time(NULL);
	size_t startIntermediates = time(NULL);



	string hashmapFileName(outPath +
		(outPath[outPath.size() - 1] == '\\' || outPath[outPath.size() - 1] == '/' ? "" : "/") +
		logFileManager->GetLogFilePrefix() +
		(SaveHashMapOutput_Prefix > 0 ? GetHashmapFileName(SourceFileLocation, SaveHashMapOutput_Prefix) : GetHashmapFileName(SourceFileLocation)));



	if ((files->file_exist(&hashmapFileName[0])))
		files->fileExistError(&hashmapFileName[0], "Could not save hashmap file. Destination file already exist.");
	display->DisplayTitle("Saving hashmap database:", false);

	// Is no range given?
	// Provide the full range instead
	if (lastRefID_InCurrentHashMap == 0)
		lastRefID_InCurrentHashMap = sequences->referenceSequences.size();

	// Precalculate dynamic byte sized variables
	//
	// Determine required bits for ref sequences + headers
	//
	// Sequences
	SEQ_POS_t maxReferenceSequenceSize = 0;
	size_t referencesSequences_TotalChars = 0;

	for (auto itr = sequences->referenceSequences.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceSequences.begin() + lastRefID_InCurrentHashMap; ++itr) // lastRefID_InCurrentHashMap + 1 so we match up with the !=
	{
		referencesSequences_TotalChars += itr->size();
		if (itr->size() > maxReferenceSequenceSize)
			maxReferenceSequenceSize = itr->size();
	}


	// Headers
	SEQ_POS_t maxReferenceHeaderSize = 0;
	size_t referencesHeaders_TotalChars = 0;
	for (auto itr = sequences->referenceHeaders.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceHeaders.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		referencesHeaders_TotalChars += (*itr)->size();
		if ((*itr)->size() > maxReferenceHeaderSize)
			maxReferenceHeaderSize = (*itr)->size();
	}
	BYTE_CNT writeSequenceBitsSize = ((countBits(maxReferenceSequenceSize) - 1) / 8) + 1;
	BYTE_CNT writeHeaderBitsSize = ((countBits(maxReferenceHeaderSize) - 1) / 8) + 1;



	bool EnableFragmentedDatabase(false);
	EnableFragmentedDatabase = (*ConsiderThreshold < 20);
	size_t maxNumPosIDPerHash = 0;
	size_t thresholdNumPosIDPerHash = 0;

	vector<size_t> posIDSizes;
	posIDSizes.reserve(vHashmap->size());
	int percentFragment = 30; // 8;
	if (EnableFragmentedDatabase)
	{
		//cout << endl;
		//cout << endl;
		//cout << endl;
		//cout << " EnableFragmentedDatabase Enabled.." << endl;
		for (auto itr = vHashmap->begin(); itr != vHashmap->end(); ++itr)
		{
			// todo; remove this IF
			//if (itr->PosIDs.size() > 700000)
			//{
			//	cout << "itr->PosIDs.size(): " << itr->PosIDs.size() << endl;
			//	cout << "itr->hash: " << itr->hash << endl;
			//}
			posIDSizes.emplace_back(itr->PosIDs.size());
			maxNumPosIDPerHash = max(maxNumPosIDPerHash, itr->PosIDs.size());
		}
		//cout << "posIDSizes.size(): " << posIDSizes.size() << endl;
		//cout << "maxNumPosIDPerHash: " << maxNumPosIDPerHash << endl;



		//thresholdNumPosIDPerHash = (maxNumPosIDPerHash / 100.0) * 10.0;
		sort(posIDSizes.begin(), posIDSizes.end());
		//auto posIDSizes_Last = unique(posIDSizes.begin(), posIDSizes.end());
		//size_t numPosIDSizes_Unique = posIDSizes_Last - posIDSizes.begin();
		size_t numPosIDSizes_Unique = posIDSizes.size();
		//cout << "numPosIDSizes_Unique: " << numPosIDSizes_Unique << endl;


		//cout << endl;
		//cout << endl;

		size_t fragmentID = (numPosIDSizes_Unique / 100.0) * percentFragment;
		thresholdNumPosIDPerHash = posIDSizes[fragmentID];

		//cout << "fragmentID: " << fragmentID << endl;
		//cout << "thresholdNumPosIDPerHash: " << thresholdNumPosIDPerHash << endl;


		thresholdNumPosIDPerHash = min((size_t)20, thresholdNumPosIDPerHash);
		//cout << "thresholdNumPosIDPerHash: " << thresholdNumPosIDPerHash << endl;
	}
	vector<size_t>().swap(posIDSizes);


	//size_t numHash = (referenceHashMapPaired->size());
	size_t numHash = (vHashmap->size());

	size_t maxNumReferenceIDsPerHash = 0;


	vector<SaveHashmapPreCalculates> vSaveHashmapPreCalculates;
	future<void> future_Resize_vSaveHashmapPreCalculates = std::async(launch::async, vector_Resize_vSaveHashmapPreCalculates, &(vSaveHashmapPreCalculates), numHash);

	//vSaveHashmapPreCalculates.resize(numHash);



	//for (auto itr = referenceHashMapPaired->begin(); itr != referenceHashMapPaired->end(); ++itr)
	vector<vector<Hashmap::HashPosIDGroup>::iterator>* vReferenceHashMapPaired_Iterators = new vector<vector<Hashmap::HashPosIDGroup>::iterator>(); //todo , delete this (...why?)
	vReferenceHashMapPaired_Iterators->reserve(numHash);
	for (auto itr = vHashmap->begin(); itr != vHashmap->end(); ++itr)
	{
		if (EnableFragmentedDatabase)
			if (itr->PosIDs.size() > thresholdNumPosIDPerHash)
				continue;
		vReferenceHashMapPaired_Iterators->emplace_back(itr);
		maxNumReferenceIDsPerHash = max(maxNumReferenceIDsPerHash, itr->PosIDs.size());
		//maxNumReferenceIDsPerHash = max(maxNumReferenceIDsPerHash, itr->second.first.size());
	}

	if (EnableFragmentedDatabase)
		numHash = vReferenceHashMapPaired_Iterators->size();

	BYTE_CNT bytes_numRefIDsPerHash = ((countBits(maxNumReferenceIDsPerHash) - 1) / 8) + 1;



	//
	// END Determine required bits for ref sequences + headers
	//



	//cout << endl << "\t\t start took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);


	std::ofstream outFileHashmap;

	//const int buf_size = 204800;
	//char buf[buf_size];
	//outFileHashmap.rdbuf()->pubsetbuf(buf, buf_size);

	outFileHashmap.open(hashmapFileName, std::ios::out | ios::binary);

	if (!outFileHashmap)
		files->fileNotExistError(&hashmapFileName[0], "Could not create hashmap file.");


	size_t numRefSeq = (lastRefID_InCurrentHashMap - firstRefID_InCurrentHashMap);// sequences->referenceSequences.size();
	BYTE_CNT bytes_streampos = sizeof(HASH_STREAMPOS);
	//BYTE_CNT bytes_sizeofstreaminfo = sizeof(size_t); // Used when Buffer Size is saved in MHM itself.
	//uint8_t bytes_referenceIDs = ((countBits(numRefSeq) - 1) / 8) + 1;

	outFileHashmap.write((char*)(&hashmapIndicatorMessage[0]), sizeof(char) * hashmapIndicatorMessage.length());
	outFileHashmap.write((char*)(&numHash), sizeof(size_t));
	outFileHashmap.write((char*)(&hashmapVersion), sizeof(int32_t)); // Version
	outFileHashmap.write((char*)(&HASH_Supported_Bytes), sizeof(int32_t)); // Bits
	outFileHashmap.write((char*)(&*ConsiderThreshold), sizeof(int32_t)); // Threshold
	outFileHashmap.write((char*)(&bytes_streampos), 1); // Bits
	outFileHashmap.write((char*)(&bytes_numRefIDsPerHash), 1);
	outFileHashmap.write((char*)(&writeSequenceBitsSize), 1);
	outFileHashmap.write((char*)(&writeHeaderBitsSize), 1);




	HASH_STREAMPOS spOffsetPrediction = outFileHashmap.tellp();

	spOffsetPrediction += (numHash * HASH_Supported_Bytes); // TOC HASH
	spOffsetPrediction += (numHash * bytes_streampos); // TOC HASH_STREAMPOS to List of RefIDs
	//spOffsetPrediction += (numHash * bytes_sizeofstreaminfo); // TOC Info total buffer size // Used when Buffer Size is saved in MHM itself.
	spOffsetPrediction += bytes_streampos; // TOC Sequences Size
	spOffsetPrediction += HASH_Supported_Bytes; // All reference sequences number
	spOffsetPrediction += (numRefSeq * bytes_streampos); // TOC HASH_STREAMPOS to List of Reference sequences (RefID + RefPos)
	spOffsetPrediction += bytes_streampos; // TOC Headers Size
	spOffsetPrediction += (numRefSeq * bytes_streampos); // TOC HASH_STREAMPOS to List of Reference headers
	spOffsetPrediction += HASH_Supported_Bytes; // All reference sequences number

	spOffsetPrediction += HASH_Supported_Bytes; // Number of TaxID's writtin once
	spOffsetPrediction += numRefSeq * sizeof(int32_t); // All NCBITaxIDs 
	spOffsetPrediction += numRefSeq * HASH_Supported_Bytes; // All reference sequences sizes
	spOffsetPrediction += numRefSeq * HASH_Supported_Bytes; // All reference headers sizes




	HASH_STREAMPOS spOffsetPrediction_StartSequences = spOffsetPrediction;


	spOffsetPrediction += referencesSequences_TotalChars * sizeof(char); // All reference sequences 
	spOffsetPrediction += referencesHeaders_TotalChars * sizeof(char); // All reference headers 

	/*
	..
	11 HASH 1
	12 StreampPos to Num of RefID's 1 (48)
	13 HASH 2
	14 StreampPos to Num of RefID's 2 (53)
	.. Num of TaxIDs
	.. TaxID 1
	.. TaxID 2
	.. TaxID 3
	15 HASH_STREAMPOS to end of TOC Sequences (19)
	16 HASH_STREAMPOS to sequence 1 (23)
	17 HASH_STREAMPOS to sequence 2 (29)
	18 HASH_STREAMPOS to sequence 3 (34)
	19 HASH_STREAMPOS to end of TOC Headers (23)
	20 HASH_STREAMPOS to header 1 (39)
	21 HASH_STREAMPOS to header 2 (42)
	22 HASH_STREAMPOS to header 3 (45)
	23 Sequence 1 # Chars
	24 Sequence 1 Char 1
	25 Sequence 1 Char 2
	26 Sequence 1 Char 3
	27 Sequence 1 Char 4
	28 Sequence 1 Char 5
	29 Sequence 2 # Chars
	30 Sequence 2 Char 1
	31 Sequence 2 Char 2
	32 Sequence 2 Char 3
	33 Sequence 2 Char 4
	34 Sequence 3 # Chars
	35 Sequence 3 Char 1
	36 Sequence 3 Char 2
	37 Sequence 3 Char 3
	38 Header 1 # Chars
	39 Header 1 Char 1
	40 Header 1 Char 2
	41 Header 1 Char 3
	42 Header 1 # Chars
	43 Header 2 Char 1
	44 Header 2 Char 2
	45 Header 3 # Chars
	46 Header 3 Char 1
	47 Header 3 Char 2
	48 Num of RefID's 1 (2)   <--------------------- spOffsetPrediction pointing to this position
	49 RefID
	50 POS
	51 RefID
	52 POS
	53 Num of RefID's 2 (2)
	54 RefID
	55 POS
	56 RefID
	57 POS

	*/


	///
	/// TOC HASHES + RefID and Position TOC
	///
	// Write Hashes + stream position of the actual reference IDs



	//cout << "multisort.." << endl;

	// Multithread sort the hashmap
	vector<future<void>> futures;
	//vector<pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>*> ToSort;
	vector<vector<PosID>*> ToSort;
	size_t numCores = (size_t)max((unsigned int)1, thread::hardware_concurrency());
	int max_thread = numCores;
	int max_tosort = 400;
	ToSort.reserve(max_tosort);
	futures.reserve(numHash / max_tosort);

	//for (auto itr = referenceHashMapPaired->begin(); itr != referenceHashMapPaired->end(); ++itr)
	//for (hashmap_positions::iterator itr : vReferenceHashMapPaired_Iterators)
	for (vector<Hashmap::HashPosIDGroup>::iterator itr : *vReferenceHashMapPaired_Iterators)
	{

		Asynchronise::wait_for_futures_on_full_vector(&futures, max_thread);
		ToSort.emplace_back(&(itr->PosIDs));

		if (ToSort.size() > max_tosort)
		{
			futures.push_back(std::async(launch::async, SortAndCorrectPairVector, ToSort, firstRefID_InCurrentHashMap));
			vector<vector<PosID>*>().swap(ToSort);
			//vector<pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>*>().swap(ToSort);
			ToSort.reserve(max_tosort);
		}
	}

	futures.push_back(std::async(launch::async, SortAndCorrectPairVector, ToSort, firstRefID_InCurrentHashMap));
	Asynchronise::wait_for_futures(&futures);

	vector<future<void>>().swap(futures);
	vector<vector<PosID>*>().swap(ToSort);
	//vector<pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>*>().swap(ToSort);


	//cout << endl << "\t\t multisort took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);

	// Write the hashes and location to the ID's + Positions

	vector<pair<Hashmap::HashPosIDGroup*, SaveHashmapPreCalculates*>> ToPreCalculate;
	/*
		vector<SaveHashmapPreCalculates> vSaveHashmapPreCalculates;
		vSaveHashmapPreCalculates.resize(numHash);*/
	future_Resize_vSaveHashmapPreCalculates.wait();
	size_t vSHPC_Cnt = 0;



	//cout << endl << "\t\t Start prepare:.. " << vReferenceHashMapPaired_Iterators->size() << endl;
	//cout << endl << "\t\t numHash:.. " << numHash << endl;
	size_t i = 0;
	while (true)
	{
		if (i >= vReferenceHashMapPaired_Iterators->size())
			break;

		SaveHashmapPreCalculates* vSHPC = &vSaveHashmapPreCalculates[vSHPC_Cnt++];
		ToPreCalculate.emplace_back(&(*(*vReferenceHashMapPaired_Iterators)[i]), vSHPC);
		i++;


		if (ToPreCalculate.size() >= max_tosort)
		{
			Asynchronise::wait_for_futures_on_full_vector(&futures, max_thread);

			futures.push_back(std::async(launch::async, GetSaveHashmapPreCalculates, ToPreCalculate));

			size_t sizeToPreCalculate = ToPreCalculate.size();

			ToPreCalculate.clear();
			ToPreCalculate.shrink_to_fit();
			ToPreCalculate.reserve(sizeToPreCalculate);
		}
	}
	//cout << "last futures.size: " << futures.size() << endl;
	//cout << "last ToPreCalculate.size: " << ToPreCalculate.size() << endl;

	futures.push_back(std::async(launch::async, GetSaveHashmapPreCalculates, ToPreCalculate));
	//cout << "Now wait .size: " << ToPreCalculate.size() << endl;

	Asynchronise::wait_for_futures(&futures);
	vector<future<void>>().swap(futures);
	ToPreCalculate.clear();
	ToPreCalculate.shrink_to_fit();










	////////////////
	/////////////// START WRITING DATA
	////////////////



	//cout << endl << "\t\t prepare took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);
	//cout << "write hashes.." << endl;
	//cout << "#hashes: " << numHash << endl;




	// Write Hashes + Streampos to ID's + POS
	size_t byteBufferHash_Size = vReferenceHashMapPaired_Iterators->size();
	byteBufferHash_Size = byteBufferHash_Size * (HASH_Supported_Bytes + bytes_streampos);
	//byteBufferHash_Size = byteBufferHash_Size * (HASH_Supported_Bytes + bytes_streampos + bytes_sizeofstreaminfo); // Used when Buffer Size is saved in MHM itself.



	byte* byteBufferHash = new byte[byteBufferHash_Size];
	byte* byteBufferHashStart = byteBufferHash;
	//streampos previous_RequiredBufferForRead = vSaveHashmapPreCalculates[0].spOffsetPrediction;
	vSHPC_Cnt = 0;

	for (vector<Hashmap::HashPosIDGroup>::iterator itr : *vReferenceHashMapPaired_Iterators)
	{
		SaveHashmapPreCalculates* vSHPC = &vSaveHashmapPreCalculates[vSHPC_Cnt++];

		if (spOffsetPrediction > Enum::HASH_STREAMPOS_MAX)
		{
			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
			display->logWarningMessage << "Required file size to large for 32 bit. " << endl;
			display->logWarningMessage << "Please switch to 64 bit version." << endl;
			display->FlushWarning(true);
		}

		memcpy(byteBufferHash, &itr->hash, HASH_Supported_Bytes);
		byteBufferHash += HASH_Supported_Bytes;

		memcpy(byteBufferHash, &spOffsetPrediction, bytes_streampos);
		byteBufferHash += bytes_streampos;

		//previous_RequiredBufferForRead = vSHPC->spOffsetPrediction;

		//memcpy(byteBufferHash, &previous_RequiredBufferForRead, bytes_sizeofstreaminfo); // Used when Buffer Size is saved in MHM itself.
		//byteBufferHash += bytes_sizeofstreaminfo; // Used when Buffer Size is saved in MHM itself.



		/*
				outFileHashmap.write((char*)(&itr->hash), HASH_Supported_Bytes);
				outFileHashmap.write((char*)(&spOffsetPrediction), bytes_streampos);*/
		spOffsetPrediction += vSHPC->spOffsetPrediction;
	}
	outFileHashmap.write((char*)byteBufferHashStart, byteBufferHash_Size);

	delete[] byteBufferHashStart;




	//cout << "write NCBI Tax" << endl;
	// Write NCBI Tax
	outFileHashmap.write((char*)(&numRefSeq), HASH_Supported_Bytes); // Write the number of references.
	byteBufferHash_Size = numRefSeq * sizeof(int32_t);
	byteBufferHash = new byte[byteBufferHash_Size];
	byteBufferHashStart = byteBufferHash;
	for (vector<int32_t>::const_iterator itr = sequences->NCBITaxIDs.begin() + firstRefID_InCurrentHashMap; itr != sequences->NCBITaxIDs.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		int32_t v = *itr;
		memcpy(byteBufferHash, &v, sizeof(int32_t));
		byteBufferHash += sizeof(int32_t);
	}

	outFileHashmap.write((char*)byteBufferHashStart, byteBufferHash_Size);

	delete[] byteBufferHashStart;


	///
	/// Reference sequences + header TOC
	/// 

	// Write end of reference sequences TOC
	HASH_STREAMPOS spAfterToc = outFileHashmap.tellp();
	spAfterToc += 1 * bytes_streampos; // for the spAfterToc write itself
	spAfterToc += 1 * HASH_Supported_Bytes; // for the spAfterToc write itself
	spAfterToc += numRefSeq * HASH_Supported_Bytes; // TOC sequences sizes
	spAfterToc += numRefSeq * bytes_streampos; // TOC sequences

	outFileHashmap.write((char*)(&spAfterToc), bytes_streampos); // Write the first position after the TOC, instead of the number of references. This due to 32 or 64 bit of size_t.
	outFileHashmap.write((char*)(&numRefSeq), HASH_Supported_Bytes); // Write the number of references.



	// Write Size per sequence
	for (auto itr = sequences->referenceSequences.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceSequences.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		size_t numItems = itr->size();
		outFileHashmap.write((char*)(&numItems), HASH_Supported_Bytes);
	}



	// Write Streampos to Sequence
	for (auto itr = sequences->referenceSequences.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceSequences.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		if (spOffsetPrediction_StartSequences > Enum::HASH_STREAMPOS_MAX)
		{
			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
			display->logWarningMessage << "File size to large. " << endl;
			display->logWarningMessage << "Please switch to 64 bit version." << endl;
			display->FlushWarning(true);
		}
		outFileHashmap.write((char*)(&spOffsetPrediction_StartSequences), bytes_streampos);
		//spOffsetPrediction_StartSequences += writeSequenceBitsSize; // The number of chars of current sequence
		spOffsetPrediction_StartSequences += itr->size() * sizeof(char); // The chars of current sequence

	}









	// Write end of reference header TOC
	spAfterToc = outFileHashmap.tellp();
	spAfterToc += 1 * bytes_streampos; // for the spAfterToc write itself
	spAfterToc += 1 * HASH_Supported_Bytes; // for the spAfterToc write itself
	spAfterToc += numRefSeq * HASH_Supported_Bytes; // TOC header sizes
	spAfterToc += numRefSeq * bytes_streampos; // TOC header
	outFileHashmap.write((char*)(&spAfterToc), bytes_streampos); // Write the first position after the TOC, instead of the number of references. This due to 32 or 64 bit of size_t.
	outFileHashmap.write((char*)(&numRefSeq), HASH_Supported_Bytes); // Write the first position after the TOC, instead of the number of references. This due to 32 or 64 bit of size_t.


	// Write size per header
	 // todo: memcpy the sizes first to a buffer and then directly write the buffer
	for (auto itr = sequences->referenceHeaders.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceHeaders.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		size_t numItems = (*itr)->size();
		outFileHashmap.write((char*)(&numItems), HASH_Supported_Bytes);
	}

	// Write Streampos to Headers
	for (auto itr = sequences->referenceHeaders.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceHeaders.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		if (spOffsetPrediction_StartSequences > Enum::HASH_STREAMPOS_MAX)
		{
			display->logWarningMessage << "Couldn't save hashmap file!" << endl;
			display->logWarningMessage << "File size to large. " << endl;
			display->logWarningMessage << "Please switch to 64 bit version." << endl;
			display->FlushWarning(true);
		}

		// todo: memcpy the spOffsetPrediction_StartSequences first to a buffer and then directly write the buffer
		outFileHashmap.write((char*)(&spOffsetPrediction_StartSequences), bytes_streampos);
		//spOffsetPrediction_StartSequences += writeHeaderBitsSize; // The number of chars of current header
		spOffsetPrediction_StartSequences += (*itr)->size() * sizeof(char); // The chars of current header

	}








	//cout << endl << "\t\t write meta took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);




	///
	// Write the real data
	///


	//cout << "write ref seqs" << endl;
	// Write sequences
	for (auto itr = sequences->referenceSequences.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceSequences.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		size_t numItems = itr->size();
		//outFileHashmap.write((char*)(&numItems), writeSequenceBitsSize);
		// todo: buffer
		outFileHashmap.write((char *)itr->data(), numItems);
	}

	//cout << "write ref head" << endl;

	// Write headers
	for (auto itr = sequences->referenceHeaders.begin() + firstRefID_InCurrentHashMap; itr != sequences->referenceHeaders.begin() + lastRefID_InCurrentHashMap; ++itr)
	{
		size_t numItems = (*itr)->size();
		//outFileHashmap.write((char*)(&numItems), writeHeaderBitsSize);
		// todo: buffer
		outFileHashmap.write((char *)(*itr)->data(), (*itr)->size());
	}


	//cout << endl << "\t\t write sequences and headers took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);




	int cnt = 0;
	//cout << "write positions" << endl;

	size_t infoTime = startTimeLoading - (Display::ProgressLoading_ShowEvery_Second + 1);
	// Write RefID's and positions
	int previousProgressLength;


	//for (auto itr = referenceHashMapPaired->begin(); itr != referenceHashMapPaired->end(); ++itr)
	//for (auto itr = referenceHashMapPaired->begin(); itr != referenceHashMapPaired->end(); ++itr)
	//for (hashmap_positions::iterator itr : vReferenceHashMapPaired_Iterators)


	size_t byteBufferSize_Previous = 0;
	byte* byteBuffer = new byte[byteBufferSize_Previous];
	byte* byteBufferStart = byteBuffer;


	for (vector<Hashmap::HashPosIDGroup>::iterator itr : *vReferenceHashMapPaired_Iterators)
	{

		//display->ShowLoadingProgress(cnt, referenceHashMapPaired->size(), &previousProgressLength, startTimeLoading, &infoTime);

		SaveHashmapPreCalculates* vSHPC = &vSaveHashmapPreCalculates[cnt];



		cnt++;

		//pair< vector<SEQ_ID_t>, vector<SEQ_POS_t>> vSeq_Pos = itr->second;
		//vector<SEQ_ID_t>* vSeq = &itr->second.first;
		//vector<SEQ_POS_t>* vPos = &itr->second.second;


		vector<Hashmap::PosID>* cHashPosID = &itr->PosIDs;
		const SEQ_ID_t refIDMin = vSHPC->refIDMin;


		transform(cHashPosID->begin(), cHashPosID->end(), cHashPosID->begin(), [&](Hashmap::PosID& item) { item.ID -= refIDMin; return item;	});

		//Hashmap::PosID* ptr_cHashPosID = &cHashPosID->front();
		//size_t cHashPosID_size = cHashPosID->size();
		//for (size_t i = 0; i < cHashPosID_size; i++)
		//{
		//	ptr_cHashPosID->ID -= refIDMin;
		//	ptr_cHashPosID++;
		//}


		//for (auto& element : *cHashPosID)
		//	element.ID -= vSHPC->refIDMin;





		if (vSHPC->bytes_currentRef_PosOffset > UINT32_MAX ||
			vSHPC->bytes_currentRefID > UINT32_MAX ||
			vSHPC->bytes_currentRefPOS > UINT32_MAX ||
			bytes_numRefIDsPerHash > UINT32_MAX ||
			vSHPC->bytes_currentRef_NumSeconds > UINT32_MAX)

		{
			display->logWarningMessage << "Too many bits required for writing the hashmap database.";
			display->FlushWarning(true);
		}

		if (vSHPC->bytes_currentRef_PosOffset == 0 ||
			vSHPC->bytes_currentRefID == 0 ||
			vSHPC->bytes_currentRefPOS == 0 ||
			bytes_numRefIDsPerHash == 0 ||
			vSHPC->bytes_currentRef_NumSeconds == 0)

		{
			display->logWarningMessage << "No bits are used, unable to continue.";
			display->FlushWarning(true);
		}


		///
		/// Precalculate repeations (so we can reserve byteBuffer)
		///
		//SEQ_POS_t prevPos = (*vPos)[0];
		//SEQ_POS_t prevIPos = 0;
		////vector<pair<SEQ_POS_t, SEQ_POS_t>> vRepeations_offsetPositions;
		////vector<SEQ_POS_t> offsetPositions;
		//size_t vPosSize = vPos->size();
		size_t numRep = vSHPC->vRepeations_offsetPositions.size();


		/*

				vRepeations_offsetPositions.reserve(vPosSize);

				for (SEQ_POS_t iPos = 1; iPos <= vPosSize; iPos++)
				{
					if (iPos == vPosSize || prevPos != (*vPos)[iPos])
					{
						numRep++;
						vRepeations_offsetPositions.emplace_back(iPos - prevIPos, prevPos);

						if (iPos == vPosSize)
							break;

						prevPos = (*vPos)[iPos];
						prevIPos = iPos;
					}
				}*/




		size_t byteBufferSize = 0;
		byteBufferSize += 4; // bytesSizes
		byteBufferSize += vSHPC->bytes_currentRef_PosOffset; // bytesSizes
		byteBufferSize += vSHPC->bytes_currentRef_PosOffset; // refIDMin
		byteBufferSize += vSHPC->bytes_currentRef_NumSeconds; // refPOSMin
		byteBufferSize += vSHPC->bytes_currentRefID * vSHPC->numSeconds; // numSeconds
		byteBufferSize += 1; // bytes_PosNumberRepeation
		byteBufferSize += vSHPC->bytes_currentRefPOS; // numRep
		byteBufferSize += numRep * vSHPC->bytes_PosNumberRepeation; // numRep
		byteBufferSize += numRep * vSHPC->bytes_currentRefPOS; // numRep



		if (byteBufferSize > byteBufferSize_Previous)
		{
			delete[] byteBufferStart;
			byteBuffer = new byte[byteBufferSize];
			byteBufferStart = byteBuffer;
			byteBufferSize_Previous = byteBufferSize;

			if (!byteBuffer)
			{
				display->logWarningMessage << "Buffer could not be created (out of memory), buffer size requested: " << byteBufferSize;
				display->FlushWarning(true);
			}

		}
		else
			byteBuffer = byteBufferStart; // reuse buffer


		memcpy(byteBuffer, (&vSHPC->bytes_currentRef_PosOffset), 1);
		byteBuffer += 1;
		memcpy(byteBuffer, (&vSHPC->bytes_currentRefID), 1);
		byteBuffer += 1;
		memcpy(byteBuffer, (&vSHPC->bytes_currentRefPOS), 1);
		byteBuffer += 1;
		memcpy(byteBuffer, (&vSHPC->bytes_currentRef_NumSeconds), 1);
		byteBuffer += 1;

		//// todo: could bring these 4 to 1 byte.. only need 2 bits for 32 bit sizes
		//outFileHashmap.write((char*)(&bytes_currentRef_PosOffset), 1);
		//outFileHashmap.write((char*)(&bytes_currentRefID), 1);
		//outFileHashmap.write((char*)(&bytes_currentRefPOS), 1);
		//outFileHashmap.write((char*)(&bytes_currentRef_NumSeconds), 1);



		memcpy(byteBuffer, (&vSHPC->refIDMin), vSHPC->bytes_currentRef_PosOffset); // offset
		byteBuffer += vSHPC->bytes_currentRef_PosOffset;
		memcpy(byteBuffer, (&vSHPC->refPOSMin), vSHPC->bytes_currentRef_PosOffset);
		byteBuffer += vSHPC->bytes_currentRef_PosOffset;
		memcpy(byteBuffer, (&vSHPC->numSeconds), vSHPC->bytes_currentRef_NumSeconds);
		byteBuffer += vSHPC->bytes_currentRef_NumSeconds;


		//outFileHashmap.write((char*)(&refIDMin), bytes_currentRef_PosOffset); // offset
		//outFileHashmap.write((char*)(&refPOSMin), bytes_currentRef_PosOffset);
		//outFileHashmap.write((char*)(&numSeconds), bytes_currentRef_NumSeconds);




		Hashmap::PosID* ptrPID = &cHashPosID->front();
		for (size_t iRefID = 0; iRefID < vSHPC->numSeconds; iRefID++)
		{

			memcpy(byteBuffer, (&ptrPID->ID), vSHPC->bytes_currentRefID); // RefID's
			byteBuffer += vSHPC->bytes_currentRefID;
			ptrPID++;
		}



		//memcpy(byteBuffer, &(*vSeq)[0], vSHPC->bytes_currentRefID * vSHPC->numSeconds); // RefID's
		//byteBuffer += vSHPC->bytes_currentRefID* vSHPC->numSeconds;



		//outFileHashmap.write((char*)(&vSeq[iRefID]), bytes_currentRefID); // RefID's


		memcpy(byteBuffer, (&vSHPC->bytes_PosNumberRepeation), 1);
		byteBuffer += 1;
		memcpy(byteBuffer, (&numRep), vSHPC->bytes_currentRefPOS);
		byteBuffer += vSHPC->bytes_currentRefPOS;

		//outFileHashmap.write((char*)(&bytes_PosNumberRepeation), 1);
		//outFileHashmap.write((char*)(&numRep), bytes_currentRefPOS);


		pair<SEQ_ID_t, SEQ_ID_t>* p = &vSHPC->vRepeations_offsetPositions.front();

		for (size_t iPos = 0; iPos < numRep; iPos++)
		{

			memcpy(byteBuffer, (&p->first), vSHPC->bytes_PosNumberRepeation);
			byteBuffer += vSHPC->bytes_PosNumberRepeation;
			memcpy(byteBuffer, (&p->second), vSHPC->bytes_currentRefPOS);
			byteBuffer += vSHPC->bytes_currentRefPOS;

			p++;
			//outFileHashmap.write((char*)(&vRepeations[iPos]), bytes_PosNumberRepeation);
			//outFileHashmap.write((char*)(&offsetPositions[iPos]), bytes_currentRefPOS);

		}

		//cout << " Second mem bench: " << Analyser::GetMemMachineFreeB() << endl;


		outFileHashmap.write((char*)byteBufferStart, byteBufferSize);




	}

	delete vReferenceHashMapPaired_Iterators;
	delete[] byteBufferStart;
	vSaveHashmapPreCalculates.clear();
	vSaveHashmapPreCalculates.shrink_to_fit();

	//cout << endl << "Before start building.. After save: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;


	//cout << endl << "\t\t write pos took:.. " << time(NULL) - startIntermediates << endl;
	//startIntermediates = time(NULL);
	vector<Hashmap::HashPosIDGroup>().swap(*vHashmap);
	//cout << endl << "\t\t empty vHashPosIDGroup took:.. " << time(NULL) - startIntermediates << endl;
	startIntermediates = time(NULL);
	//cout << endl << "Saving took:.. " << time(NULL) - startTimeLoading << endl;

	outFileHashmap.close();
	display->DisplayValue("Done", true);

	return hashmapFileName;
}

size_t Hashmap::GetSizeOfReferenceSequencesInCache(vector<vector<char>> v)
{
	size_t retVal = 0;

	for (auto i : v)
		if (i.size() > 0)
			retVal++;

	return retVal;
}

template<typename T>
size_t Hashmap::GetSizeOfUnorderedMap(unordered_map<HASH_t, vector<T>>* om)
{
	return Hashmap::referenceHashMap->size();
	//if (Hashmap::referenceHashMap->max_load_factor() > 1.0)
	//	return  Hashmap::referenceHashMap->bucket_count() * Hashmap::referenceHashMap->max_load_factor();
	//else
	//	return Hashmap::referenceHashMap->bucket_count();
}

bool FirstTimeTidyLoadingCache(true);
bool FirstTimeTidyLoadingCache_UseLoadFactor(false);
size_t SizeWhenMemoryExceedLimit = 0;
size_t SizeWhenMemoryExceedLimit_ReferenceSequencesCache = 0;
void Hashmap::TidyLazyLoadingCache(int percentageFreeMem, Sequences* sequences)
{

	if (FirstTimeTidyLoadingCache)
	{
		if (percentageFreeMem >= Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold)
			return;

	}
	else
	{
		if (SizeWhenMemoryExceedLimit > GetSizeOfUnorderedMap(Hashmap::referenceHashMap)
			&&
			SizeWhenMemoryExceedLimit_ReferenceSequencesCache > GetSizeOfReferenceSequencesInCache(sequences->referenceSequences))
			return;
	}


	//cout << SizeWhenMemoryExceedLimit << " - " << GetSizeOfUnorderedMap(Hashmap::referenceHashMap) << endl;



	// Get lock on all analysis
	//cout << "Get Lock on all.." << endl;
	//cout.flush();
	int iNumberLocksRequired = multiThreading->mutexAnalysis.size();
	int iNumberLocksReceived = 0;
	//vector <bool> isLocked(iNumberLocksRequired, false);

	while (iNumberLocksRequired != iNumberLocksReceived)
		for (int i = 0; i < iNumberLocksRequired; i++)
		{
			//if (!isLocked[i])
				//if (multiThreading->mutexAnalysis[i].try_lock())
			if (!multiThreading->mutexAnalysis[i])
			{
				multiThreading->mutexAnalysis[i] = true;

				iNumberLocksReceived++;
				//isLocked[i] = true;
				//cout << "Got " << i << endl;
				//cout.flush();
			}
		}



	MultiThreading::mutHashmapCache.lock();



	if (FirstTimeTidyLoadingCache)
	{
		FirstTimeTidyLoadingCache = false;
		SizeWhenMemoryExceedLimit = GetSizeOfUnorderedMap(Hashmap::referenceHashMap);
		SizeWhenMemoryExceedLimit_ReferenceSequencesCache = GetSizeOfReferenceSequencesInCache(sequences->referenceSequences);
	}

	// Since we use pointers to our data, wait until all gpu's are ready using the pointed-to-data
	//int previousRunningThreads = 0;
	//while (multiThreading->runningThreadsAnalysis > 0)
	//{
	//	if (previousRunningThreads != multiThreading->runningThreadsAnalysis)
	//	{
	//		cout << "multiThreading->runningThreadsAnalysis: " << multiThreading->runningThreadsAnalysis << endl;
	//		multiThreading->SetGPUActive_AllStates(true);
	//		multiThreading->TerminateFinishedThreads(false);
	//		previousRunningThreads = multiThreading->runningThreadsAnalysis;
	//	}
	//	// Just give the CPU some rest..
	//	multiThreading->SleepWaitForCPUThreads();
	//}


	//swap lijkt nu goed te werken; wellicht ook bij de CLEAR GPU/CPU gebruiken?
		//toch lijkt het weer niet goed gefree-ed te worden??.. even de check binnen "tidy"uitzetten?
	//exit(1);



	time_t startTime = time(NULL);
	size_t highestUsage = 0;
	//for (int i = 0; i < Hashmap::LazyLoadingCacheCounter.size(); i++)
	//	if (Hashmap::LazyLoadingCacheCounter[i] > highestUsage)
	//		highestUsage = Hashmap::LazyLoadingCacheCounter[i];



	//cout << "Tijd q: " << time(NULL) - startTime << endl;
	startTime = time(NULL);




	// Reset all reference headers and sequence data
	//if (Hashmap::UseCacheForLazyHashmap)
	//	memset(&LazyLoadingCacheCounter[0], 0, LazyLoadingCacheCounter.size() * sizeof LazyLoadingCacheCounter[0]);

	for (auto rh : sequences->referenceHeaders)
		delete rh;
	vector<vector<char>*>(sequences->referenceHeaders.size()).swap(sequences->referenceHeaders);
	vector<vector<char>>(sequences->referenceSequences.size()).swap(sequences->referenceSequences);

	// Reset hashmaps
	Hashmap::ResetHashmapTable();
	//unordered_map < HASH_t, pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>>().swap((*Hashmap::referenceHashMapPaired));

	/*
	unordered_map < HASH_t, vector<SEQ_ID_t>>().swap((*Hashmap::referenceHashMap));
	unordered_map < HASH_t, vector<SEQ_POS_t>>().swap((*Hashmap::referenceHashMapPositions));*/


	// Take up all the memory directly, so we know we will go out of memory before reaching SizeWhenMemoryExceedLimit due to other processes eating up more memory
	Hashmap::referenceHashMapPaired->reserve(SizeWhenMemoryExceedLimit);

	//Hashmap::referenceHashMap->reserve(SizeWhenMemoryExceedLimit);
	//Hashmap::referenceHashMapPositions->reserve(SizeWhenMemoryExceedLimit);

	//unordered_map < HASH_t, vector<SEQ_ID_t>>((*Hashmap::referenceHashMap).size()).swap((*Hashmap::referenceHashMap));
	//unordered_map < HASH_t, vector<SEQ_POS_t>>((*Hashmap::referenceHashMapPositions).size()).swap((*Hashmap::referenceHashMapPositions));


	//if (highestUsage > 0)
	//{
	//	size_t tidyBelow = (highestUsage / 100.0) *  Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy;
	//	for (int i = 0; i < Hashmap::LazyLoadingCacheCounter.size(); i++)
	//	{
	//		if (Hashmap::LazyLoadingCacheCounter[i] != 0 && Hashmap::LazyLoadingCacheCounter[i] <= tidyBelow)
	//		{
	//			Hashmap::LazyLoadingCacheCounter[i] = 0;
	//			sequences->referenceHeaders[i].clear();
	//			sequences->referenceHeaders[i].shrink_to_fit();
	//			sequences->referenceHeaders[i] = vector<char>();
	//			vector<char>().swap(sequences->referenceHeaders[i]);
	//			sequences->referenceSequences[i].clear();
	//			sequences->referenceSequences[i].shrink_to_fit();
	//			sequences->referenceSequences[i] = vector<char>();
	//			vector<char>().swap(sequences->referenceSequences[i]);
	//		}
	//	}
	//}
	//sequences->referenceHeaders.shrink_to_fit();
	//sequences->referenceSequences.shrink_to_fit();
	//cout << "Tijd a: " << time(NULL) - startTime << endl;
	//startTime = time(NULL);




	//// Hashmap
	//highestUsage = 0;
	//for (auto& it : *Hashmap::referenceHashMap_UsageCount)
	//	if (it.second > highestUsage)
	//		highestUsage = it.second;
	//cout << "Tijd b: " << time(NULL) - startTime << endl;
	//startTime = time(NULL);
	//if (highestUsage > 0)
	//{
	//	size_t tidyBelow = (highestUsage / 100.0) *  Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy;
	//	for (auto& it : *Hashmap::referenceHashMap_UsageCount)
	//	{
	//		//if (it.second < tidyBelow)
	//		//{
	//		//	
	//			/*(*Hashmap::referenceHashMap)[it.first].clear();
	//			(*Hashmap::referenceHashMap)[it.first].shrink_to_fit();
	//			Hashmap::referenceHashMap->erase(it.first);


	//			(*Hashmap::referenceHashMapPositions)[it.first].clear();
	//			(*Hashmap::referenceHashMapPositions)[it.first].shrink_to_fit();
	//			Hashmap::referenceHashMapPositions->erase(it.first);*/


	//		(*Hashmap::referenceHashMap_UsageCount)[it.first] = 0;
	//		//}
	//	}

		//cout << "Tijd c: " << time(NULL) - startTime << endl;
		//startTime = time(NULL);
		//unordered_map<HASH_t, vector<SEQ_ID_t>>((*Hashmap::referenceHashMap)).swap((*Hashmap::referenceHashMap));
		//unordered_map<HASH_t, vector<SEQ_POS_t>>((*Hashmap::referenceHashMapPositions)).swap((*Hashmap::referenceHashMapPositions));


		//cout << "Tijd d : " << time(NULL) - startTime << endl;
		//startTime = time(NULL);
	//}


	MultiThreading::mutHashmapCache.unlock();


	// Release lock on all analysis
	for (int i = 0; i < multiThreading->mutexAnalysis.size(); i++)
		multiThreading->mutexAnalysis[i] = false;
	//multiThreading->mutexAnalysis[i].unlock();

	//cout << "Swap done" << endl;

}

template<typename T>
inline void freeMem(T& memObject)
{
	T emptyObject;
	swap(memObject, emptyObject);
}

#include<set> // for set operations 


void* Hashmap::TBuildHashmapForReference(void* args)
{

	//cout << " Building hashmap: Inside new thread..starting.." << endl;

	//cout << endl << "Mem buildhashmap before TBuildHashmapForReference: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;

	//test 
	//HASH_t steppingRef = 1;// (*ConsiderThreshold) / 4;
	//const HASH_t hashSizeRef = 6; //*ConsiderThreshold - steppingRef + 0.5;

	////test werkt
	//HASH_t steppingRef = 1;// (*ConsiderThreshold) / 4;
	//const HASH_t hashSizeRef = 13; //*ConsiderThreshold - steppingRef + 0.5;


	// werkend
	HASH_t steppingRef = 1;
	HASH_t steppingQuery = (*ConsiderThreshold) / 2;
	const HASH_t hashSizeRef = *ConsiderThreshold - steppingQuery + 1;

	BuildHashmapTableArgs* argsBuildHashMap = (BuildHashmapTableArgs*)args;
	vector<vector<char>>* references = &argsBuildHashMap->ptrReference;
	SEQ_POS_t FirstReferenceID = argsBuildHashMap->FirstReferenceID;
	double threadID = argsBuildHashMap->threadID;




	// Use random generator for building smaller hashmap databases
	//random_device dev;
	//mt19937 rng(dev());
	//uniform_int_distribution<std::mt19937::result_type> generateRandom(0, 99); // distribution in range [1, 6]
	//bool useRandomGenerator = argsBuildHashMap->Create_Hashmap_ProportionPercentage != 100;

	HASH_t refCount = 0;


	string ignoreString = string(hashSizeRef, 'N');

	//
	//
	//#ifndef __Hash64__
	//	HASH_t eNNN = CreateHash32(reinterpret_cast<unsigned char*>(&ignoreString), hashSizeRef);
	//#else
	//	HASH_t eNNN = CreateHash64(reinterpret_cast<unsigned char*>(&ignoreString), hashSizeRef);
	//#endif	
	//

	HASH_t eNNN = CreateHash24(reinterpret_cast<unsigned char*>(&ignoreString[0]), hashSizeRef);

	//// todo: remove this
	//cout << " ignoreString: " << ignoreString << endl;
	//cout << " eNNN: " << eNNN << endl;


	//HASH_t eNNN = CreateHash32(reinterpret_cast<unsigned char*>(&ignoreString), hashSizeRef);
	//HASH_t eNNN = CreateHash64(reinterpret_cast<unsigned char*>(&ignoreString), hashSizeRef);

//HASH_t eNNN = pearsonHash24(reinterpret_cast<unsigned char*>(&ignoreString), hashSizeRef);

	size_t bufSeqPosID_CurrentIndex = 0;

	vector<Hashmap::HashPosID> bufSeqPosID;
	size_t reserveSize = 0;
	for (SEQ_ID_t iRefID = 0; iRefID < (*references).size(); iRefID++)
	{

		SEQ_POS_t iStop = (*references)[iRefID].size();
		if (hashSizeRef < iStop)
			iStop -= hashSizeRef;
		reserveSize += iStop;

	}
	//try
	//{
	bufSeqPosID.resize(reserveSize);
	//}
	//catch (...)
	//{
	//	cerr << "Out of memory.";
	//	exit(1);
	//}


	// Building hashmap: Inside new thread.. ready allocating memory, allow next new thread to start (or save when out of mem)..
	Hashmap::mAllowNextThreadFor_BuildHashmap.lock();
	Hashmap::bAllowNextThreadFor_BuildHashmap = true;
	Hashmap::mAllowNextThreadFor_BuildHashmap.unlock();


	vector<char>* currentReference = &references->front();
	size_t numRef = references->size();
	for (SEQ_ID_t iRefID = 0; iRefID < numRef; iRefID++)
	{
		SEQ_POS_t iStop = currentReference->size();
		if (hashSizeRef < iStop)
			iStop -= hashSizeRef;

		size_t currentRefID = FirstReferenceID + refCount;

		char* ptr_inRef = currentReference->data();
		for (SEQ_POS_t i = 0; i < iStop; i += steppingRef)
		{
			//if (useRandomGenerator)
			//{
			//	uniform_int_distribution<std::mt19937::result_type> generateRandom(0, 99); // distribution in range [1, 6]
			//	if (generateRandom(rng) >= argsBuildHashMap->Create_Hashmap_ProportionPercentage)
			//		continue;
			//}

			Hashmap::HashPosID* cHashPosID = &bufSeqPosID[bufSeqPosID_CurrentIndex++];
			cHashPosID->hash = CreateHash24(reinterpret_cast<unsigned char*>(ptr_inRef), hashSizeRef);



			if (cHashPosID->hash == eNNN)
			{
				bufSeqPosID_CurrentIndex--;
				continue;
			}

			cHashPosID->PosIDs.ID = currentRefID;
			cHashPosID->PosIDs.Pos = i;
			ptr_inRef++;
		}


		refCount++;
		currentReference++;

	}

	//sort(bufSeqPosID.begin(), bufSeqPosID.end(), [&](Hashmap::HashPosID i, Hashmap::HashPosID j) { return i.hash < j.hash; });
	//mtSort_HashPosID(&bufSeqPosID.front(), bufSeqPosID.size(), 4);


	multiThreading->mutHashMapEndResult.lock();

	move(bufSeqPosID.begin(), bufSeqPosID.begin() + bufSeqPosID_CurrentIndex, vHashBuildByThreads->begin() + TBuildHashMapAnalysedSequences);
	//move(bufSeqPosID.begin(), bufSeqPosID.begin() + bufSeqPosID_CurrentIndex, vHashBuildByThreads->begin() + argsBuildHashMap->Index_vHashBuildByThreads);


	multiThreading->mutMainMultithreads.lock();
	TBuildHashMapAnalysedSequences += bufSeqPosID_CurrentIndex;
	multiThreading->mutMainMultithreads.unlock(); // used for FinishThreads method



	multiThreading->mutHashMapEndResult.unlock();



	// Keep track of elapsed time fur current thread, used for auto increase thread size during build of hashmap
	if (argsBuildHashMap->thisAnalysesUsedForTiming)
	{
		Auto_BuildHashmapTable_Timer_End = time(NULL);
	}


	multiThreading->mutMainMultithreads.lock();
	multiThreading->mutHashMap.lock();

	argsBuildHashMap->ptrReference.clear();
	argsBuildHashMap->ptrReference.shrink_to_fit();
	delete argsBuildHashMap;
	bufSeqPosID.clear();
	bufSeqPosID.shrink_to_fit();

	//TBuildHashMapAnalysedSequences += references->size();
	multiThreading->runningThreadsAnalysing--;
	multiThreading->threadsFinished.push_back(threadID);
	multiThreading->runningThreadsBuildHashMap--;


	// Mem todo-perform-memcheck
	// Clean up
	//hashMap.clear();
	//hashMapPositions.clear();
	//hashMap.rehash(0);
	//hashMapPositions.rehash(0);

	//for (int i = 0; i < references.size();i++)
	//{
	//	references[i].clear();
	//	references[i].shrink_to_fit();
	//}

	//references.clear();
	//references.shrink_to_fit();

	/*
		hashMapPaired->clear();
		hashMapPaired->rehash(0);*/
		//delete hashMapPaired;


	multiThreading->mutHashMap.unlock();
	multiThreading->mutMainMultithreads.unlock(); // used for FinishThreads method


	//cout << endl << "Mem buildhashmap after TBuildHashmapForReference: " << Analyser::GetMemFreePercentage() << "%" << endl << flush;


	return NULL;
}




bool Hashmap::IsReferenceHashMapDatabase(char* Path)
{


	//#ifdef __LINUX__
	//	bool isFile(false);
	//
	//	struct stat s;
	//	int r = stat(Path, &s);
	//	if (r == 0)
	//	{
	//		if (s.st_mode & S_IFDIR)
	//		{
	//			isFile = false;
	//		}
	//		else if (s.st_mode & S_IFREG)
	//		{
	//			isFile = true;
	//		}
	//		else
	//		{
	//			throw new invalid_argument("Couldn't detect if input path is a hashmap or not!");
	//		}
	//	}
	//	else
	//		throw new invalid_argument("Couldn't detect if input path is a hashmap or not!");
	//
	//
	//	// A hashmap database is a folder, so if we find the path to be a file; consider it not being a hashmap database
	//	return (!isFile);
	//	//return (!files->file_exist(Path));
	//
	//#else
	string fileExtention = Files::TryGetFileExtention(Path);
	return fileExtention == ".mhm";
	//if ((fileExtention == ".fasta")
	//	|| (fileExtention == ".fastq")
	//	|| (fileExtention == ".fq")
	//	|| (fileExtention == ".fa")
	//	|| (fileExtention == ".fsa")
	//	|| (fileExtention == ".fqa")
	//	|| (fileExtention == ".fna"))
	//	return false;
	//return true;

	//#endif
}



HASH_t Hashmap::GetLoadedThreshold()
{
	return loaded_Threshold;
}


int Hashmap::GetHashmapID(size_t referenceID)
{
	SEQ_ID_t* ptrSeqIDt = &(*Hashmap::MultipleHashmaps_SequenceOffset)[0];
	SEQ_ID_t* ptrSeqIDt_next = ptrSeqIDt;
	size_t ms_size_min_one = Hashmap::MultipleHashmaps_SequenceOffset->size() - 1;

	if (ms_size_min_one > 0)
		ptrSeqIDt_next++;
	for (int i = 0; i < ms_size_min_one; i++)
	{
		if (referenceID >= *ptrSeqIDt && referenceID < *ptrSeqIDt_next)
			return i;
		ptrSeqIDt++;
		ptrSeqIDt_next++;
	}

	return ms_size_min_one;

}


//const HASH_t MOD_ADLER = 65521;
//HASH_t Hashmap::CreateHash(unsigned char *key, int len)
//{
//	HASH_t a = 1, b = 0;
//	int index;
//
//	// Process each byte of the data in order
//	for (index = 0; index < len; ++index)
//	{
//		a = (a + key[index]) % MOD_ADLER;
//		b = (b + a) % MOD_ADLER;
//	}
//
//	return (b << 16) | a;
//}


//https://gist.github.com/gregkrsak/9046561
uint8_t Hashmap::pearsonLookupTable[256] = {
  98, 6, 85, 150, 36, 23, 112, 164, 135, 207, 169, 5, 26, 64, 165, 219,
  61, 20, 68, 89, 130, 63, 52, 102, 24, 229, 132, 245, 80, 216, 195, 115,
  90, 168, 156, 203, 177, 120, 2, 190, 188, 7, 100, 185, 174, 243, 162, 10,
  237, 18, 253, 225, 8, 208, 172, 244, 255, 126, 101, 79, 145, 235, 228, 121,
  123, 251, 67, 250, 161, 0, 107, 97, 241, 111, 181, 82, 249, 33, 69, 55,
  59, 153, 29, 9, 213, 167, 84, 93, 30, 46, 94, 75, 151, 114, 73, 222,
  197, 96, 210, 45, 16, 227, 248, 202, 51, 152, 252, 125, 81, 206, 215, 186,
  39, 158, 178, 187, 131, 136, 1, 49, 50, 17, 141, 91, 47, 129, 60, 99,
  154, 35, 86, 171, 105, 34, 38, 200, 147, 58, 77, 118, 173, 246, 76, 254,
  133, 232, 196, 144, 98, 124, 53, 4, 108, 74, 223, 234, 134, 230, 157, 139,
  189, 205, 199, 128, 176, 19, 211, 236, 127, 192, 231, 70, 233, 88, 146, 44,
  183, 201, 22, 83, 13, 214, 116, 109, 159, 32, 95, 226, 140, 220, 57, 12,
  221, 31, 209, 182, 143, 92, 149, 184, 148, 62, 113, 65, 37, 27, 106, 166,
  3, 14, 204, 72, 21, 41, 56, 66, 28, 193, 40, 217, 25, 54, 179, 117,
  238, 87, 240, 155, 180, 170, 242, 212, 191, 163, 78, 218, 137, 194, 175, 110,
  43, 119, 224, 71, 122, 142, 42, 160, 104, 48, 247, 103, 15, 11, 138, 239
};



uint8_t Hashmap::pearsonHash(unsigned char* key, int len, uint8_t* lookupTable)
{
	uint8_t result = 0;
	for (size_t i = 0; i < len; i++)
	{
		result = lookupTable[result ^ *key];
		key++;
	}
	return result;
}


uint16_t Hashmap::pearsonHash16(unsigned char* key, int len)
{
	uint16_t result = 0;

	unsigned char h;
	for (int iteration = 0; iteration < 2; ++iteration)
	{
		h = pearsonLookupTable[(key[0] + iteration) % 256];
		for (size_t i = 1; i < len; ++i) {
			h = pearsonLookupTable[h ^ key[i]];
		}
		result = (result << 8) ^ h;
	}
	return result;


	//uint16_t result = 0;
	//unsigned char* start = key;
	//uint8_t correctedFirstMessageByte = *start;
	//for (int iteration = 0; iteration < 2; ++iteration)
	//{
	//	uint8_t iterationResult = pearsonHash(key, len, pearsonLookupTable);
	//	result += (iterationResult << (iteration << 1));
	//	*start += 1;
	//	key = start;
	//}
	//*start = correctedFirstMessageByte;
	//return result+1;
}
//
//int Hashmap::pearsonHash_GetBitsPerHash()
//{
//	int b = 4;
//	return ((b - 1) * 8) + (4);
//}
uint32_t Hashmap::pearsonHash24(unsigned char* key, int len)
{
	uint32_t result = 0;

	unsigned char h;
	const int numIt = 4;
	const int numItMinOne = numIt - 1;
	for (int iteration = 0; iteration < numIt; ++iteration)
	{
		h = pearsonLookupTable[(key[0] + iteration) % 256];
		for (size_t i = 1; i < len; ++i) {
			h = pearsonLookupTable[h ^ key[i]];
		}
		if (iteration == numItMinOne)
			result = (result << 4) ^ h;
		else
			result = (result << 8) ^ h;

	}
	return result + 1;


}

//uint32_t Hashmap::pearsonHash32(std::string message)
//{
//	uint32_t result = 0;
//	uint8_t correctedFirstMessageByte = message.at(0);
//	for (int iteration = 0; iteration < 4; ++iteration)
//	{
//		uint8_t iterationResult = pearsonHash(message, pearsonLookupTable);
//		result += (iterationResult << (iteration << 2));
//		message.at(0) += 1;
//	}
//	message.at(0) = correctedFirstMessageByte;
//	return result;
//}
//uint64_t Hashmap::pearsonHash64(std::string message)
//{
//	uint64_t result = 0;
//	uint8_t correctedFirstMessageByte = message.at(0);
//	for (int iteration = 0; iteration < 8; ++iteration)
//	{
//		uint8_t iterationResult = pearsonHash(message, pearsonLookupTable);
//		result += (iterationResult << (iteration << 3));
//		message.at(0) += 1;
//	}
//	message.at(0) = correctedFirstMessageByte;
//	return result;
//}


//https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
uint32_t Hashmap::CreateHash32(unsigned char* key, int len)
{
	unsigned char* p = key;

	// 32 bit
	unsigned int h = 2166136261;
	const unsigned int prime = 16777619;

	size_t i;
	for (i = 0; i < len; i++)
		h = (h * prime) ^ p[i];


	return h;
}

uint64_t Hashmap::CreateHash64(unsigned char* key, int len)
{
	unsigned char* p = key;

	//64 bit
	uint64_t h = 14695981039346656037u;
	const uint64_t prime = 1099511628211u;

	size_t i;
	for (i = 0; i < len; i++)
		h = (h * prime) ^ p[i];

	return h;
}

//http://www.isthe.com/chongo/tech/comp/fnv/
//To translate the hash to any other size (ie 24 bit, 56 bit, etc), right shift the top of the hash and xor with the bottom:
// uint32_t hash24 = (hash32 >> 24) ^ (hash32 & 0xFFFFFF);
/* NOTE: non-standard use of a larger hash */
//#define MASK_24 (((uint64_t)1<<24)-1) /* i.e., (u_int64_t)0xffffff */
uint32_t Hashmap::CreateHash24(unsigned char* key, int len)
{
	const size_t mask = pow(2, HashBits) - (size_t)1;

	if (HashBits == 32)
	{
		return Hashmap::CreateHash32(key, len);;
	}
	else
	{
		// 28 bit
		//uint32_t hash;

		//hash = Hashmap::CreateHash32(key, len);
		//return (((hash >> 28) ^ hash) & 0xFFFFFFF);

		// 24:

		/*uint32_t hash;

		hash = Hashmap::CreateHash32(key, len);
		return (((hash >> 24) ^ hash) & 0xFFFFFF);*/

		// 16 bit:


		// Use const
		uint32_t hash = Hashmap::CreateHash32(key, len);
		return (((hash >> HashBits) ^ hash) & mask);
	}
}


//http://www.isthe.com/chongo/tech/comp/fnv/
//To translate the hash to any other size (ie 24 bit, 56 bit, etc), right shift the top of the hash and xor with the bottom:
// uint32_t hash24 = (hash32 >> 24) ^ (hash32 & 0xFFFFFF);
/* NOTE: non-standard use of a larger hash */
//#define MASK_24 (((uint64_t)1<<24)-1) /* i.e., (u_int64_t)0xffffff */
uint32_t Hashmap::CreateHash16(unsigned char* key, int len)
{
	const size_t mask = pow(2, 16) - (size_t)1;

	// 16 bit:

	uint32_t hash = Hashmap::CreateHash32(key, len);
	return (((hash >> 16) ^ hash) & mask);
}



Hashmap::Hashmap(Display *d, MultiThreading *mt, MultiNodePlatform *mnp, Sequences *s, int32_t *ct, LogManager *lfm, Files *f, Configuration *c)
{
	display = d;
	multiThreading = mt;
	multiNodePlatform = mnp;
	sequences = s;
	ConsiderThreshold = ct;
	logFileManager = lfm;
	files = f;
	configuration = c;
}

void Hashmap::PrepareHashLookupBuffer(Sequences * sequences, Display* display)
{

	auto begin = high_resolution_clock::now();



	display->DisplayTitle("Prepare hash lookup buffer: ");


	const size_t hashmapHighestSteppingSize = (*ConsiderThreshold / 2);
	const size_t ptrSteppingQuery = *ConsiderThreshold / 2;
	const size_t hashSize = *ConsiderThreshold - ptrSteppingQuery + 1;

	// Get max num Hashes
	size_t numHashes = 0;
	for (SEQ_ID_t qID = 0; qID < sequences->inputSequences->size(); qID++)
	{
		vector<char>* ptrQuerySeq = &(*sequences->inputSequences)[qID];

		// Do not calculate this. Takes time and to reserve a little bit more than required is not a problem
		//SEQ_POS_t iStop = ptrQuerySeq->size();
		//// Make sure we don't overshoot with the threshold
		//if (*ConsiderThreshold < iStop)
		//	iStop -= *ConsiderThreshold;

		numHashes += (ptrQuerySeq->size() / ptrSteppingQuery) + 1;
	}


	//std::cout << "get num Hashes took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	//cout.flush();
	//begin = high_resolution_clock::now();


	// Build hashes
	const size_t numQ = sequences->inputSequences->size();
	vector<char>* ptrQuerySeq = &(*sequences->inputSequences)[0];
	vector<HASH_t> queryHashes;
	queryHashes.reserve(numHashes);

	// To store generated hashes for later use in tPrepare in Analyser
	sequences->inputSequences_Hashes = new vector<vector<HASH_t>*>();
	sequences->inputSequences_Hashes->resize(numQ);
	vector<HASH_t>** ptrISH = &sequences->inputSequences_Hashes->front();




	size_t startGetRefSeqs = time(NULL);
	vector<future<void>> future_BuildHashesForQuery;
	size_t numCores = (size_t)max((unsigned int)1, thread::hardware_concurrency());
	size_t numThreads = numCores * 4;
	int queryPerThread = numQ / numThreads;

	if (true)
	{
		for (SEQ_ID_t qID = 0; qID < numQ; qID += queryPerThread)
		{
			size_t qID_Until = qID + queryPerThread;
			if (qID_Until > numQ)
				qID_Until = numQ;
			//cout << qID << " " << qID_Until << " " << numQ << endl;
			future_BuildHashesForQuery.emplace_back(std::async(launch::async, BuildHashesForQuery, ptrQuerySeq + qID, ptrQuerySeq + qID_Until, ptrISH, ptrSteppingQuery, hashSize, &queryHashes));

			//BuildHashesForQuery(ptrQuerySeq + qID, ptrQuerySeq + qpt, ptrISH, ptrSteppingQuery, hashSize, &queryHashes);

			ptrISH = ptrISH + queryPerThread;
		}
	}
	else
	{
		for (SEQ_ID_t qID = 0; qID < numQ; qID++)
		{
			SEQ_POS_t iStop = (ptrQuerySeq)->size();

			//Make sure we don't overshoot with the threshold
			if (*ConsiderThreshold < iStop)
				iStop -= *ConsiderThreshold;

			(*ptrISH)->reserve((iStop / ptrSteppingQuery) + 1);


			for (SEQ_POS_t currentCharPosition_InQuery = 0; currentCharPosition_InQuery < iStop; currentCharPosition_InQuery += ptrSteppingQuery)
			{
				const HASH_t cHash = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*ptrQuerySeq)[currentCharPosition_InQuery]), hashSize);
				queryHashes.emplace_back(cHash);
				(*ptrISH)->emplace_back(move(cHash));
			}
			(*ptrISH)->shrink_to_fit();

			ptrQuerySeq++;
			ptrISH++;

		}
	}

	Asynchronise::wait_for_futures(&future_BuildHashesForQuery);
	//cout << endl << "\t\t BuildHashesForQuery took:.. " << time(NULL) - startGetRefSeqs << endl;


	//std::cout << "get Hashes took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	//cout.flush();
	//begin = high_resolution_clock::now();

	sort(queryHashes.begin(), queryHashes.end());
	vector<HASH_t>::iterator unqHashEnd = unique(queryHashes.begin(), queryHashes.end());
	size_t numUnique = unqHashEnd - queryHashes.begin();



	//std::cout << "get Hashes sorted took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
	//cout.flush();
	//begin = high_resolution_clock::now();

	display->DisplayValue(to_string((100.0 / queryHashes.size()) * numUnique) + "% unique");

	referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis.resize(Hashmap::HashmapFileLocations.size());
	for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	{
		Hashmap::referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis[iFileID] = new hashmap_streams();

		hashmap_streams* ptrreferenceHashMap_Lazy_Buffer = Hashmap::referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis[iFileID];
		
		hashmap_streams* ptrreferenceHashMap_Lazy = (*Hashmap::referenceHashMap_Lazy)[iFileID];
		hashmap_streams::const_iterator ptrLG_End = ptrreferenceHashMap_Lazy->end();

		//size_t maxBufferSize = 0;
		//for (auto &el : *ptrreferenceHashMap_Lazy)
		//{
		//	maxBufferSize = max(maxBufferSize, el.second.BufferSize);
		//}
		//size_t thresholdBufferSize = (maxBufferSize/ 100.0) * 40.0;


		vector<HASH_t>::iterator unqHashCurrent = queryHashes.begin();

		ptrreferenceHashMap_Lazy_Buffer->rehash(numUnique);
		for (; unqHashCurrent != unqHashEnd; unqHashCurrent++)
		{
			hashmap_streams::const_iterator found = ptrreferenceHashMap_Lazy->find(*unqHashCurrent);
			if (found != ptrLG_End)
			{
				//if (found->second.BufferSize <= thresholdBufferSize)
				ptrreferenceHashMap_Lazy_Buffer->insert(*found);
			}
		}

		ptrreferenceHashMap_Lazy->rehash(0);
		ptrreferenceHashMap_Lazy->clear();
		delete ptrreferenceHashMap_Lazy;

		//std::cout << "get Hashes for 1 file took: " << duration<double, std::ratio<1, 1>>(high_resolution_clock::now() - begin).count() << "\n";
		//cout.flush();
		//begin = high_resolution_clock::now();
	}

	vector<HASH_t>().swap(queryHashes);


	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode == Enum::Hashmap_MemoryUsageMode::High)
	{
		display->DisplayTitle("Load hash buffer:");

		LoadHashmapPositionData(display);

		display->DisplayValue("Done.");
	}


	//if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode >= Enum::Hashmap_MemoryUsageMode::High)
	//{
	//	display->DisplayTitle("Decompress hash buffer:");

	//	DecrompressHashmapPositionData(display);

	//	display->DisplayValue("Done.");
	//}



}


mutex mBuildHashesForQuery;
void Hashmap::BuildHashesForQuery(vector<char> * ptrQuerySeq_from, vector<char> * ptrQuerySeq_till, vector<HASH_t> ** ptrISH, size_t  ptrSteppingQuery, size_t hashSize, vector<SEQ_ID_t>* queryHashes)
{
	vector<char> * ptrQuerySeq = ptrQuerySeq_from;
	vector<HASH_t> createdHashes;
	size_t maxNumHashes = 0;
	while (ptrQuerySeq < ptrQuerySeq_till)
	{
		maxNumHashes += ptrQuerySeq->size() / ptrSteppingQuery;
		ptrQuerySeq++;

	}
	//cout << 1 << endl;

	createdHashes.reserve(maxNumHashes);
	ptrQuerySeq = ptrQuerySeq_from;
	while (ptrQuerySeq < ptrQuerySeq_till)
	{
		SEQ_POS_t iStop = (ptrQuerySeq)->size();

		//Make sure we don't overshoot with the threshold
		if (*ConsiderThreshold < iStop)
			iStop -= *ConsiderThreshold;

		*ptrISH = new vector<HASH_t>();
		//mBuildHashesForQuery.lock();

		//cout << "(iStop / ptrSteppingQuery) + 1:  " << (iStop / ptrSteppingQuery) + 1 << endl;
		//cout << "(ptrQuerySeq)->size(): " << (ptrQuerySeq)->size() << endl;
		//cout << "ptrQuerySeq: " << ptrQuerySeq << endl;
		//cout << "ptrQuerySeq_till: " << ptrQuerySeq_till << endl;
		//mBuildHashesForQuery.unlock();
		(*ptrISH)->reserve((iStop / ptrSteppingQuery) + 1);


		for (SEQ_POS_t currentCharPosition_InQuery = 0; currentCharPosition_InQuery < iStop; currentCharPosition_InQuery += ptrSteppingQuery)
		{
			const HASH_t cHash = Hashmap::CreateHash24(reinterpret_cast<unsigned char*>(&(*ptrQuerySeq)[currentCharPosition_InQuery]), hashSize);
			createdHashes.emplace_back(cHash);
			(*ptrISH)->emplace_back(move(cHash));
		}
		(*ptrISH)->shrink_to_fit();

		ptrISH++;
		ptrQuerySeq++;
	}
	//cout << 2 << endl;


	sort(createdHashes.begin(), createdHashes.end());
	//cout << 3 << endl;

	mBuildHashesForQuery.lock();
	queryHashes->insert(queryHashes->end(), createdHashes.begin(), unique(createdHashes.begin(), createdHashes.end()));
	mBuildHashesForQuery.unlock();
	//cout << 4 << endl;

}



void LoadHashlistFromDisk(size_t iFileID, char** buffer_total)
{


	hashmap_streams* ptrreferenceHashMap_Lazy = (Hashmap::referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis)[iFileID];
	vector<pair< HASH_STREAMPOS, Enum::HASH_STREAMPOS_INFO*>> vMatchedHashStreamPos;// (elems->begin(), elems->end());
	vMatchedHashStreamPos.reserve(ptrreferenceHashMap_Lazy->size());
	for (auto& e : *ptrreferenceHashMap_Lazy)
	{
		vMatchedHashStreamPos.emplace_back(e.first, &e.second);
	}

	sort(vMatchedHashStreamPos.begin(), vMatchedHashStreamPos.end(), [](const pair< HASH_STREAMPOS, Enum::HASH_STREAMPOS_INFO*>& i, const  pair< HASH_STREAMPOS, Enum::HASH_STREAMPOS_INFO*>& j) { return i.first < j.first; });


	char* LocalIFStreamBuffer;
	size_t LocalIFStreamBufferSize = 4096;
	ifstream* ptrvIFStream = new ifstream();

	LocalIFStreamBuffer = new char[LocalIFStreamBufferSize];

	Sequences::OpenFile(ptrvIFStream, &Hashmap::HashmapFileLocations[iFileID], LocalIFStreamBuffer, LocalIFStreamBufferSize);


	size_t buffer_total_size = 0;
	for (auto& el : vMatchedHashStreamPos)
	{
		buffer_total_size += el.second->BufferSize;
	}
	//cout << "buffer_total_size: " << buffer_total_size << endl;
	*buffer_total = new char[buffer_total_size];

	for (auto& el : vMatchedHashStreamPos)
	{
		ptrvIFStream->seekg(el.second->StreamPos);
		ptrvIFStream->read((char*)*buffer_total, el.second->BufferSize);
		el.second->StreamPos = (HASH_STREAMPOS)*buffer_total; // Change pointer from file pointer to this new local buffer
		*buffer_total += el.second->BufferSize;
	}


	ptrvIFStream->close();
	delete ptrvIFStream;
	delete[] LocalIFStreamBuffer;
}


void Hashmap::DecrompressHashmapPositionData(Display * display)
{
	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode < Enum::Hashmap_MemoryUsageMode::High)
		throw new invalid_argument("LoadHashmapPositionData only supported in high or extreme hashmap loading mode");


	Hashmap_Position_Data_Decompressed.resize(Hashmap::HashmapFileLocations.size());


	for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	{
		size_t raise_StartValue = (*Hashmap::MultipleHashmaps_SequenceOffset)[iFileID];
		size_t hashStreampOffset = Hashmap::Hashmap_First_PosData_HashStreamPos[iFileID];
		char* ptrhashStreampStartData = Hashmap::Hashmap_Position_Data[iFileID];

		hashmap_streams* ptrreferenceHashMap_Lazy = (Hashmap::referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis)[iFileID];

		size_t count = 0;
		for (auto& el : *ptrreferenceHashMap_Lazy)
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
			const HASH_STREAMPOS gotoHashStreamPos = el.second.StreamPos;
			const size_t HashStreamPos_BufferSize = el.second.BufferSize;






			char* buffer_all = (ptrhashStreampStartData + (gotoHashStreamPos - hashStreampOffset));
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


			el.second.DecompressedBuffer = new BufferHashmapToReadFromFile();
			el.second.DecompressedBuffer->NumIDs = numIDs;


			// Precalculate values
			size_t listOffset = el.second.DecompressedBuffer->vSeqIDs.size(); // If we have multiple hashmaps files, stack the ID's
			size_t numIDs_ListOffset = numIDs + listOffset;

			// Reserve new or extended memory space
			//if (el.second.DecompressedBuffer->vSeqIDs.size() > 0)
			//	cout << el.second.DecompressedBuffer->vSeqIDs.size() << endl;
			el.second.DecompressedBuffer->vSeqIDs.resize(numIDs_ListOffset, 0); // Since we can write ID's to the hashmap with less bits than sizeof(size_t), set all bits to 0 so we only set the bits to 1 which will be really read from file below (otherwise you might end up with bits to 1 at random places outside the bytes_currentRefID window)
			//el.second.vPos.reserve(numIDs_ListOffset);

			SEQ_ID_t raise = raise_StartValue; // Now we have the reference ID's, but if we have multiple hashmaps, we stack all reference in 1 list, so we need to add the offset per hashmap to the newly loaded reference ID's
			raise += refIDMin; // raise due to the memory compression method used in saving the hashmap database

			// Buffer file data
			bufferSize = bytes_currentRefID * numIDs;

			vector<SEQ_ID_t>* ptrVSIT = &el.second.DecompressedBuffer->vSeqIDs;
			SEQ_ID_t* sit = &(*ptrVSIT)[listOffset];

			for (size_t iRefID = 0; iRefID < numIDs; iRefID++)
			{
				memcpy(sit, buffer_all, bytes_currentRefID);
				//memcpy(sit, buffer_data_3 + (iRefID *bytes_currentRefID), bytes_currentRefID);
				buffer_all += bytes_currentRefID;
				sit++;
			}

			//if (ptrVSIT->size() == 0)
			//	cout << 1;

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


			size_t cPos = 0;
			size_t offset = el.second.DecompressedBuffer->vPos.size();
			el.second.DecompressedBuffer->vPos.resize(el.second.DecompressedBuffer->vPos.size() + totalNumbers);

			ptrNumRep = &vRepeations.front();
			ptroffsetPositions = &offsetPositions.front();
			for (size_t iPos = 0; iPos < numRepeations; iPos++)
			{
				fill(el.second.DecompressedBuffer->vPos.begin() + cPos + offset, el.second.DecompressedBuffer->vPos.begin() + cPos + offset + *ptrNumRep, *ptroffsetPositions);
				cPos += *ptrNumRep;

				ptrNumRep++;
				ptroffsetPositions++;
			}

			buffer_all++;
		}


	}


}



void Hashmap::LoadHashmapPositionData(Display * display)
{


	if (Hashmap::Hashmap_Load_Reference_Memory_UsageMode != Enum::Hashmap_MemoryUsageMode::High)
		throw new invalid_argument("LoadHashmapPositionData not supported in non-high hashmap loading mode");


	bool useFutures(true);

	vector<future<void>> future_LoadHashlistFromDisk;


	vector < vector<HASH_STREAMPOS>> vMatchedHashStreamPos;
	vMatchedHashStreamPos.resize(Hashmap::HashmapFileLocations.size());

	for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	{
		if (useFutures)
			future_LoadHashlistFromDisk.emplace_back(std::async(launch::async, LoadHashlistFromDisk, iFileID, &Hashmap::Hashmap_Position_Data[iFileID]));
		else
			LoadHashlistFromDisk(iFileID, &Hashmap::Hashmap_Position_Data[iFileID]);
	}


	if (useFutures)
		Asynchronise::wait_for_futures(&future_LoadHashlistFromDisk);


	for (size_t iFileID = 0; iFileID < Hashmap::HashmapFileLocations.size(); iFileID++)
	{
		Hashmap::Hashmap_Position_Data[iFileID] = (char*)0;
		Hashmap::Hashmap_First_PosData_HashStreamPos[iFileID] = 0;
	}
}
