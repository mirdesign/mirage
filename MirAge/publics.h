#pragma once


#include "Enum.h"
#include <unordered_map>





typedef   unordered_map<HASH_t, Enum::HASH_STREAMPOS_INFO> hashmap_streams;
typedef   unordered_map<HASH_t, pair<vector<SEQ_ID_t>, vector<SEQ_POS_t>>> hashmap_positions;
typedef   unordered_map<HASH_STREAMPOS, Enum::HeadTailSequence> hashmap_HeadTailBuffer;
typedef   unordered_map<HASH_t, BufferHashmapToReadFromFile> hashmap_MatchedHashStreamPos;

class publics
{
public:
	publics();
	~publics();
};

