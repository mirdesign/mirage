#pragma once
#ifndef HASHMAP_H
#define HASHMAP_H

#include "publics.h"
#include <string>
#include <vector>
#include "Enum.h"
#include "asynchronise.h"
#include "Display.h"
#include "Files.h"
#include "Configuration.h"
#include <mutex>
#include <ctime>
#include <fstream> // ifstream
#include "Sequences.h"
#include "MultiNodePlatform.h"
#include "MultiThreading.h"
#include <unordered_set>
#include <iterator>     // std::ostream_iterator
#include <unordered_map>
#include <future>





#include <stdio.h>


#ifdef __LINUX__
#include <sys/stat.h>
#endif

using namespace std;




class Sequences; // Forward declaration to use sequences (since Sequences uses Hashmap)
class Display; // Forward declaration to use sequences (since Sequences uses Hashmap)
class Files; // Forward declaration to use sequences (since Sequences uses Hashmap)
class MultiNodePlatform;
class Configuration;

class Hashmap
{
	///
	/// Read Fasta(q) Sequences from a file.
	///	Fills header list if list is provided.
	/// 

public:

	struct HashmapSettings {
		BYTE_CNT bytes_streampos;
		BYTE_CNT bytes_numRefIDsPerHash;
		BYTE_CNT writeSequenceBitsSize;
		BYTE_CNT writeHeaderBitsSize;
	};


	struct SaveHashmapPreCalculates {
		SEQ_ID_t refIDMin;
		//SEQ_ID_t refID_Dif;
		//SEQ_ID_t refPOS_Dif;
		SEQ_ID_t refPOSMin;
		size_t numSeconds;
		BYTE_CNT bytes_currentRefID;
		BYTE_CNT bytes_currentRefPOS;
		BYTE_CNT bytes_currentRef_PosOffset;
		BYTE_CNT bytes_currentRef_NumSeconds;
		BYTE_CNT bytes_PosNumberRepeation;
		size_t totalBytes;
		streampos spOffsetPrediction;
		vector<pair<SEQ_POS_t, SEQ_POS_t>> vRepeations_offsetPositions;

	};


	struct PosID {
		SEQ_ID_t ID;
		SEQ_POS_t Pos;
	};

	struct HashPosID {

		//void construct(char* c, do_not_initialize_tag) const {
		//	// do nothing
		//}
		//HashPosID() 	{}


		//HashPosID() {
		//	// do nothing
		//}

		HASH_t hash;
		PosID PosIDs;
	};
	struct HashPosIDGroup {
		HASH_t hash;
		vector<PosID> PosIDs;
	};


	static vector<Hashmap::HashmapSettings>* MultipleHashmaps_Settings;
	static mutex mAllowNextThreadFor_BuildHashmap; // static so Thread can access it as well
	static bool bAllowNextThreadFor_BuildHashmap; // static so Thread can access it as well

	static const int HashBits = 28;

	string Output_Path = "";
	int HASH_Supported_Bytes = sizeof(HASH_t);
	static vector<string> HashmapFileLocations;
	static vector<ifstream*> HashmapFileLocations_IFStream;
	//static vector<mutex*> HashmapFileLocations_Lock;

	static SEQ_AMOUNT_t TBuildHashMapAnalysedSequences; // Current amount of sequences which are done during Threading
	bool Save_Hashmap_Database = true;
	int Create_Hashmap_ProportionPercentage = 100;
	static int Hashmap_Save_Hashmap_During_Build_Below_Free_Memory; // Save hashmap during building when memory is below this threshold. Use 0 to disable this feature.
	static size_t loaded_TotalNumberOfReferences;

	static int Hashmap_Performance_Boost;
	static int Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy;
	static int Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold;

	static Enum::Hashmap_MemoryUsageMode Hashmap_Load_Reference_Memory_UsageMode;
	static bool Use_Experimental_LazyLoading;
	static bool UseCacheForLazyHashmap;
	static bool Hashmap_Load_Hashmap_From_File_After_Build; //Hashmap_Load_Hashmap_From_File_After_Build:1 # When a hashmap is build from a fasta file, forget the fasta and builded hashmap information and reload the hashmap from the new build hashmap file
	static int Hashmap_Load_Hashmap_Percentage;
	static bool Hashmap_Load_Hashmap_Skip_On_Failure;

	static vector<vector<BufferHashmapToReadFromFile>> Hashmap_Position_Data_Decompressed;
	static vector<char*> Hashmap_Position_Data;
	static vector<SEQ_ID_t>* MultipleHashmaps_SequenceOffset;
	static vector<HASH_STREAMPOS> referenceSequences_First_HashStreamPos;
	static vector<HASH_STREAMPOS> Hashmap_First_PosData_HashStreamPos;

	static vector<unordered_map<HASH_STREAMPOS, BufferHashmapToReadFromFile>> MatchedHashStreamPos_Cache;

	static hashmap_positions* referenceHashMapPaired;
	static vector< hashmap_streams*>* referenceHashMap_Lazy;
	static vector<hashmap_streams*> referenceHashMap_Lazy_Buffer_Used_For_Current_Analysis;

	void LoadHashMapFromHashmapDB(ifstream* pFileMap, vector<int32_t>* NCBITaxIDs, hashmap_streams* refHashMap_Lazy, Hashmap::HashmapSettings* hashSettings, streampos spEnd, streampos* First_PosData_HashStreamPos);


	//static unordered_map<HASH_t, pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>>* referenceHashMapPaired;

	static unordered_map<HASH_t, vector<SEQ_ID_t>>* referenceHashMap;
	//static vector<unordered_map<HASH_t, HASH_STREAMPOS>*>* referenceHashMap_Lazy;
	//static vector<unordered_map<HASH_t, pair<SEQ_POS_t, HASH_STREAMPOS>>*>* referenceHashMap_Lazy;
	static unordered_map<HASH_t, vector<SEQ_POS_t>>* referenceHashMapPositions;
	static unordered_map<HASH_t, size_t>* referenceHashMap_UsageCount;

	static vector<unordered_map<HASH_t, HASH_STREAMPOS>*>* referenceHashMapPositions_Lazy;
	static vector<size_t> LazyLoadingCacheCounter;
	//static vector<unordered_map<HASH_t, pair<SEQ_POS_t, HASH_STREAMPOS>>*>* referenceHashMapPositions_Lazy;
	static size_t GetSizeOfReferenceSequencesInCache(vector<vector<char>> v);

	template<typename T>
	static size_t GetSizeOfUnorderedMap(unordered_map<HASH_t, vector<T>>* om);

	static void ResetHashmapTable();

	//void* CalculateHashCombinations(void* args); // commented since we didn't find any use

#ifdef _WIN32
	std::wstring s2ws(const std::string& s);
#endif
	void ReadHashmapFile_Future(size_t startTime, vector<string> FileLocation, int i, vector<SEQ_ID_t> * referenceHashStreamPos, vector<Enum::LazyData> * referenceSequences_Lazy, vector<SEQ_ID_t> * referenceSequenceSizes, vector<Enum::LazyData> * referenceHeaders_Lazy, vector<SEQ_ID_t> * referenceHeadersHashStreamPos, vector<SEQ_ID_t> * referenceHeaderSizes, atomic_size_t* sizeLoaded);
	bool ReadHashmapFile(vector<string> FileLocation, vector<vector<char>> *referenceSequences, vector<vector<char>*> *referenceHeaders, vector<Enum::LazyData> *referenceSequences_Lazy, vector<HASH_STREAMPOS> *referenceHashStreamPos, vector<HASH_STREAMPOS> *referenceHeadersHashStreamPos, vector<SEQ_ID_t> *referenceHeaderSizes, vector<SEQ_ID_t>* referenceSequenceSizes, vector<Enum::LazyData> *referenceHeaders_Lazy);
	//bool ReadHashmapFile(vector<string> FileLocation, vector<vector<char>> *referenceSequences, vector<vector<char>> *referenceHeaders, vector<Enum::LazyData> *referenceSequences_Lazy, vector<Enum::LazyData> *referenceHeaders_Lazy);

//bool ReadHashmapFile(vector<string> FileLocation, vector<vector<char>> *referenceSequences, vector<vector<char>> *referenceHeaders);
//void LoadHashMapFromHashmapDB(char* pFileMap);
//void LoadHashMapFromHashmapDB(ifstream* pFileMap, unordered_map<HASH_t, HASH_STREAMPOS>* refHashMap_Lazy, unordered_map<HASH_t, HASH_STREAMPOS>* refHashMapPositions_Lazy);
//void LoadHashMapFromHashmapDBV2(ifstream* pFileMap);
//void LoadHashMapFromHashmapDB(ifstream* pFileMap, unordered_map<HASH_t, pair<SEQ_POS_t, HASH_STREAMPOS>>* refHashMap_Lazy, unordered_map<HASH_t, pair<SEQ_POS_t, HASH_STREAMPOS>>* refHashMapPositions_Lazy);
	bool LoadReferenceDataFromHashmapDB_SuperLazy(ifstream* infile, vector<Enum::LazyData>* dest, vector<HASH_STREAMPOS>* destReferenceStreamPos, vector<SEQ_ID_t>* destReferenceSizes, size_t* numberItems, int hashmapID, BYTE_CNT BytesToReadData);
	//bool LoadReferenceDataFromHashmapDB_SuperLazy(ifstream* infile, vector<Enum::LazyData>* dest, size_t* numberItems, int hashmapID, BYTE_CNT BytesToReadData);

	bool LoadReferenceDataFromHashmapDB_Lazy(ifstream* fPath, vector<Enum::LazyData>* dest, int hashmapID, BYTE_CNT BytesToReadData);
	//bool LoadReferenceDataFromHashmapDB(char* pBuffer, vector<vector<char>>* dest);

	bool LoadReferenceDataFromHashmapDB(ifstream* infile, vector<char*>* dest, size_t numChars);
	bool LoadReferenceDataFromHashmapDB(ifstream* infile, vector<vector<char>>* dest, vector<SEQ_ID_t>* sizes, size_t numItems);
	///
/// Build a hashmap table based on the loaded reference file
///
//void Hashmap::BuildHashmapTable(string outPath, size_t ReferenceID_Offset)
	vector<string> BuildHashmapTable(char* HashmapFileLocation, size_t ReferenceID_Offset = 0);
	string GetHashmapFileName(char* HashmapFileLocation, size_t prefixID = 0);
	void ConvertHashesFromThreadToHashmap(size_t vHashBuildByThreads_size);
	string SaveHashmapTableV3(string outPath, char* HashmapFileLocation, size_t firstRefID_InCurrentHashMap = 0, size_t lastRefID_InCurrentHashMap = 0);

	static void TidyLazyLoadingCache(int percentageFreeMem, Sequences* sequences);

	static void* TBuildHashmapForReference_UsingDiskSpace(void* args);
	static void* TBuildHashmapForReference(void* args);
	// http://eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx [Bernstein hash]
	//static size_t hashNew(const char* s);

	static uint32_t CreateHash32(unsigned char* key, int len);
	static uint64_t CreateHash64(unsigned char * key, int len);
	static uint32_t CreateHash24(unsigned char* key, int len);
	static uint32_t CreateHash16(unsigned char * key, int len);


	static 	uint8_t pearsonLookupTable[256];

	static uint8_t pearsonHash(unsigned char* key, int len, uint8_t* lookupTable);
	static uint16_t pearsonHash16(unsigned char* key, int len);
	//static uint8_t  pearsonHash(std::string message, uint8_t* lookupTable);
	//static uint16_t pearsonHash16(std::string message);
	static uint32_t pearsonHash24(unsigned char* key, int len);



	static bool IsReferenceHashMapDatabase(char* Path);
	static int32_t LoadedBytes;
	//bool IsTaxonomyFileProvided = false;

	HASH_t GetLoadedThreshold();
	static int GetHashmapID(size_t referenceID);

	size_t SaveHashMapOutput_Prefix = 0;

	Hashmap(Display *d, MultiThreading *mt, MultiNodePlatform *mnp, Sequences *s, int32_t *ct, LogManager *lfm, Files *f, Configuration *c);

	void PrepareHashLookupBuffer(Sequences* sequences, Display* display);
	static void BuildHashesForQuery(vector<char> * ptrQuerySeq_from, vector<char> * ptrQuerySeq_till, vector<HASH_t> ** ptrISH , size_t  ptrSteppingQuery, size_t hashSize, vector<SEQ_ID_t>* queryHashes);
	void DecrompressHashmapPositionData(Display * display);
	void LoadHashmapPositionData(Display* display);

private:



	const string hashmapIndicatorMessage = "MirAge - Maarten Pater - www.mirdesign.nl - mirage@mirdesign.nl - Hashmap";
	static MultiThreading *multiThreading;

	int32_t loaded_Threshold = -1;

	int maxThreadsHashmap = 50;
	//
	// 2 = lazy loading with HASH-NumID's-RefID's  
	// 3 = HASH-HASH_STREAMPOS-Hash-HASH_STREAMPOS .... NumID's-RefIDs
	int32_t hashmapVersion = 4; // used to check if we load a hashmap which we can understand (or load differently in the future?)
	LogToScreen warningStream;
	LogManager *logFileManager;
	Files *files;
	Display* display;
	Configuration* configuration;
	struct membuf;
	struct imemstream;
	MultiNodePlatform *multiNodePlatform;
	Sequences *sequences;

	static int32_t *ConsiderThreshold;


	//struct CalculateHashCombinationsArgs {  // commented since we didn't find any use
	//	size_t QueryFrom;
	//	size_t NumberOfQueries;
	//	unordered_map<int32_t, vector<int>>* refHashMap;
	//	unordered_map<int, vector<int>>* ptrQueryReferenceCombination;
	//	unordered_map<int32_t, vector<unordered_set <int>>>* ptrQueryReferenceCombinationPositions;
	//	size_t* ptrfoundNum;
	//	int ptrThresholdMatrix;
	//	int ptrSteppingQuery;
	//	int ptrThresholdSizeRef;
	//};

	struct BuildHashmapTableArgs {
		bool thisAnalysesUsedForTiming;
		Files* files;
		vector<vector<char>> ptrReference;
		size_t	FirstReferenceID;
		double threadID;
		string tmpBuildFolder;
		int Create_Hashmap_ProportionPercentage;
		size_t Index_vHashBuildByThreads;
	};



};
#endif

void Unduplicate(std::vector<SEQ_ID_t> *vHashes);
