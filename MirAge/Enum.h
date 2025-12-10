#ifndef ENUM_H
#define ENUM_H

#include <stdint.h>
#include <vector>

enum Output_Modes { Default = 0, ProgressBar = 1, Silent = 2, Results = 3, ResultsNoLog = 4 };


#ifndef __Hash64__
typedef uint32_t HASH_t;
using SEQ_ID_t = uint32_t; // ID for a sequence, Should not exceed SEQ_AMOUNT_t
using SEQ_POS_t = uint32_t; // Position inside a sequence (nucleotide)
using SEQ_SIZE_t = uint32_t; // Size of sequence
using SEQ_AMOUNT_t = uint32_t; // Number of sequences, Should be identifiable by SEQ_ID_t
#else
typedef uint64_t HASH_t;
using SEQ_ID_t = uint64_t; // ID for a sequence, Should not exceed SEQ_AMOUNT_t
using SEQ_POS_t = uint64_t; // Position inside a sequence (nucleotide)
using SEQ_SIZE_t = uint64_t; // Size of sequence
using SEQ_AMOUNT_t = uint64_t; // Number of sequences, Should be identifiable by SEQ_ID_t
#endif

typedef  HASH_t HASH_STREAMPOS; // Keep the size of streampos the same accross different OS'es

typedef unsigned char UINT8;
typedef unsigned char byte;
typedef int BYTE_CNT;


using namespace std;

struct BufferHashmapToReadFromFile
{
	byte* bufferPosition;
	size_t BufferSize = 0;
	size_t NumIDs = 0;

	vector<SEQ_ID_t> vSeqIDs;
	vector<SEQ_ID_t> vPos;
	//unordered_map<HASH_STREAMPOS, const BufferHashmapToReadFromFile>::iterator IteratorToMap;
	//HASH_STREAMPOS StreamPos;
	//int Pinned = 0;
};

class Enum
{
public:

	static HASH_STREAMPOS HASH_STREAMPOS_MAX;
	enum Analysis_Modes { Sensitive = 0, Fast = 1, VeryFast = 2, UltraFast = 3 };
	enum Hashmap_MemoryUsageMode { Low = 0, Medium = 1, High = 2, Extreme = 3};

	

	struct HASH_STREAMPOS_INFO // Keep the size of streampos the same accross different OS'es
	{
		HASH_STREAMPOS StreamPos;
		size_t BufferSize;
		BufferHashmapToReadFromFile* DecompressedBuffer;
	};


	struct Position
	{
		Position(SEQ_POS_t r, SEQ_POS_t q) :
			Query(q), Reference(r)
		{ }


		Position()
		{ }

		SEQ_POS_t Reference;
		SEQ_POS_t Query;
	};

	struct LazyData
	{
		LazyData() {}

		LazyData(HASH_STREAMPOS h, int i, BYTE_CNT b) :
			StartPosition(h), HashmapID(i), BytesToReadLength(b)
		{ }

		HASH_STREAMPOS StartPosition;
		int HashmapID;
		BYTE_CNT BytesToReadLength;
	};


	struct HeadTailSequence
	{



		HeadTailSequence() {}

		HeadTailSequence(char* h, char*t, size_t s) :
			Head(h), Tail(t), SequenceLength(s)
		{ }

		//vector<char> Head;
		//vector<char> Tail;

		char* Head;
		char* Tail;
		size_t Head_Size;
		size_t Tail_Size;
		size_t SequenceLength;
	};

	Enum();
	~Enum();
};


#endif