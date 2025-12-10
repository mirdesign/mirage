#pragma once
#ifndef SEQUENCES_H
#define SEQUENCES_H
#include "publics.h"



#include <fstream>
#include <iostream>
#include <stdlib.h>


#include "Enum.h"
#include "Hashmap.h"
#include <stdio.h>
#include <vector>
#include "Configuration.h"
#include "Files.h"
#include "Display.h"
#include "MultiThreading.h"
#include <mutex>
#include <future>
#include "MultiNodePlatform.h"

#include <cmath>
#ifdef __linux__
#include <fcntl.h>// Open
#include <cstring>
#include <numeric> // iota
#include <sys/stat.h> 
#include <unistd.h> // Close
#include <sys/mman.h> // mmadvise
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#endif
class Hashmap; // Forward declaration to use hashmap (since Hashmap uses Sequences)
class MultiNodePlatform;
class Configuration;


using namespace std;







class Sequences
{

private:


	static MultiThreading *multiThreading;

	//bool taxonomyFileRead = false;
	bool *MultiNodePlatform_CombineResult_AllNodes;
	MultiNodePlatform *multiNodePlatform;
	Files *files;
	Configuration *TaxonomyTranslateTable;

	struct TReadFileFromMemArgs {
		vector<vector<char>>* destination;
		size_t ThreadID;
		pair<char*, char*> startEndPosition;
		size_t numSequencesPerThread;
		vector<vector<char>*>* pHeaders;
		size_t argID;
		size_t maxSequenceLength;
		Display *display;
		bool IsGPUDeviceUsed;
		size_t TotalSequencesPerThread;
	};

	struct membuf;
	struct imemstream;

	static void InitOpenFile(ifstream ** s, mutex ** m, string * location);



public:



	static size_t MAX_RETRY_OPEN_FILE;

	static chrono::milliseconds FILE_OPEN_PREPARE_HASHMAP_LOCKED_WAIT;
	static chrono::milliseconds FILE_OPEN_FAILED_WAIT_MS;

	static size_t Reference_Read_Buffer_Size_MB_MaxAllowed_Percentage;
	static size_t Reference_Read_Buffer_Size_GB;
	static int OpenIfstreamsPerHashmap_Async;
	static UINT8 SizeOfSequence_Bytes;
	static UINT8 SizeOfHeader_Bytes;

	static Display *display;

	static HASH_STREAMPOS sp_ReferenceSequence_AfterLastChar;
	static HASH_STREAMPOS sp_ReferenceHeader_AfterLastChar;
	static SEQ_ID_t TSequencesLoaded;
	static pair<vector<vector<char>>, vector<vector<char>>>* TReadFileFromMemArgs_ThreadResults;
	static void toUpperCase(vector<char>::iterator p);
	static void SequenceToUpperCase(vector<char>* vc);
	static void* TReadFastaSequencesFromMem(void* args);
	static void InitLazyLoading();
	static void OpenFile(ifstream * stream, string * path, char* ManualBuffer = nullptr, size_t ManualBufferSize = 0);




	//static void GetLazyContentForCuda(vector<vector<char>>* cache, vector<size_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, size_t refID, char** dstSeq = nullptr, size_t* dstSize = nullptr, bool aSync = false);







	static vector<ifstream*> vIfstream_hashmap;
	static void GetLazyContent_HeadTail(vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, vector<char>* tempRefSequence);
	static void GetLazyContent_HeadTail(ifstream* s, HASH_STREAMPOS* HashStreamPosition, char* dstReference, size_t dstSize);

	static void GetLazyContent_HeadTail(vector<Enum::LazyData>* reference_Lazy, vector<size_t>* reference_lazy_sizes, size_t refID, Enum::Position* head, Enum::Position* tail, Enum::HeadTailSequence* dest, size_t considerThreshold, ifstream* s, vector< HASH_STREAMPOS>* refStreamPos, int hashmapID, hashmap_HeadTailBuffer* HashmapHeadTailBuffers, Enum::LazyData* ptrLazyData);
	static void GetLazyContentWithCache(vector<vector<char>>* cache, vector<SEQ_ID_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, SEQ_ID_t refID, vector<char>* retVal, bool aSync = false);
	static void GetLazyContentWithCache(vector<vector<char>*>* cache, vector<SEQ_ID_t>* cacheSizes, vector<size_t>* cacheCounter, vector<HASH_STREAMPOS>* streamposStart, SEQ_ID_t refID, vector<char>* retVal, bool aSync = false);
	static void GetLazyContent(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* reference_sizes, size_t refID, string* retVal, size_t Offset = 0, size_t Length = 0);
	//static void GetLazyContent(vector<HASH_STREAMPOS>* streamposStart, vector<size_t>* reference_sizes, size_t refID, vector<char>* retVal, size_t Offset = 0, size_t Length = 0);
	static void GetLazyContentASync(vector<HASH_STREAMPOS>* streamposStart, vector<SEQ_ID_t>* cacheSizes, size_t refID, vector<char>* retVal, size_t Offset = 0, size_t Length = 0);
	//static size_t GetSizeOfItem(vector<vector<char>>* cache, vector<Enum::LazyData>* reference_Lazy, size_t itemID);

	bool SortReferenceSequencesOnLoad = true;
	bool SortQuerySequencesOnLoad = true;
	bool NCBIAccession2TaxID_Used = false;
	bool DivideReferenceSequences = false;
	long DivideReferenceSequences_Length = 50000;




	size_t DuplicateQuerySequencesCount = 0;
	vector<vector<int>> CountDuplicateList;
	bool SkipDuplicateSequences = false; // todo-later: put in config and test well

	vector<SEQ_ID_t> referenceHeaderSizes;
	vector<SEQ_ID_t> referenceSequencesSizes;

	unordered_map<string,int> NCBIAccession2TaxID;
	vector<HASH_STREAMPOS> referenceHeadersHashStreamPos;
	
	vector<HASH_STREAMPOS> referenceHashStreamPos;
	vector<Enum::LazyData> referenceSequences_Lazy;
	vector<char*> referenceSequences_Buffer;
	vector<vector<char>> referenceSequences; // t
	vector<vector<char>*> inputHeaders; // t
	vector<Enum::LazyData> referenceHeaders_Lazy;
	vector<vector<char>*> referenceHeaders;
	vector<int32_t> NCBITaxIDs;
	vector<vector<char>>* inputSequences;
	vector<vector<HASH_t>*>* inputSequences_Hashes; 

	size_t Sequences_TotalToAnalyse = 0;


	void TranslateUsingTaxonomyFile(vector<vector<char>>* listToTranslate);
	void PrepareSequencesForMultiNodeEnv(MultiNodePlatform *multiNodePlatform, size_t numOfSequences);


#ifdef _WIN32
	std::wstring s2ws(const std::string& s);
#endif

	void ResetReferenceSequences();

	void SortQuerySequences(vector<vector<char>>* ptrSequences, vector<vector<char>*>* ptrHeaders);
	void SortReferenceSequences(vector<vector<char>>* ptrSequences, vector<vector<char>*>* ptrHeaders);
	
	struct compareLen;
	void IdentifyDuplicates(vector<vector<char>>* list);
	void LoadInputFiles(string Output_Path, Hashmap *hashmap, bool IsGPUDeviceUsed);
	void GetNumberOfHeaders(char * &pFileMapPreSize, size_t &numberOfSequences, std::vector<char *> &newHeaderPositions, char * pFileMap, const size_t &lenFileMap, int &previousProgressLength, const size_t &startTimeLoading, size_t &infoTime, std::string &FileDescription);
	void ReadFastaSequencesFromMem(string FileDescription, char* FileLocation, vector<vector<char>>* pSequences, char* pFileMap, size_t lenFileMap, vector<vector<char>*>* pHeaders, bool IsGPUDeviceUsed, size_t maxSequenceLength = 0, Hashmap* BuildHashmapDuringRead = nullptr);
	void ReadFastaSequencesFromFile(string FileDescription, char* FileLocation, vector<vector<char>>* Sequences, vector<vector<char>*>* Headers, bool IsGPUDeviceUsed, size_t maxSequenceLength = 0, Hashmap* BuildHashmapDuringRead = nullptr);
	void NCBIAccession2TaxID_LookupTaxID(vector<vector<char>*>* referenceSequences, vector<int32_t>* NCBITaxIDs);

	///
	/// Complement DNA
	/// Source: http://arep.med.harvard.edu/labgc/adnan/projects/Utilities/revcomp.html
	///

	static void reverseComplementDNA(vector<char>* src, vector<char>* dest, size_t NumPerThread);
	void AppendReverseComplements(vector<vector<char>> *sequences, vector<vector<char>*> *sequenceHeaders);


	//~Sequences();
	Sequences(Files *f, Display *d, MultiThreading *mt, MultiNodePlatform *mnp, bool *m);
};

#endif