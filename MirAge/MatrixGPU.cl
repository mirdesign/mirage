__kernel void  GetHighestMatrixScore(global const unsigned char*  seqQuerys, global const unsigned char*  seqAgainst,
	global const unsigned int*  seqLengthQuery, global const unsigned int*  seqLengthsAgainst,
	global const unsigned int*  seqStartingPointsAgainst, global const unsigned int*  seqStartingPointsQuerys,
	global const unsigned int*  numAgainst, global float* Scores, global float* PercentageMatches, global const unsigned int*  ConsiderThreshold,
	global const unsigned int*  seqNumAgainstPerQuery, global const unsigned int*  seqPosAgainstPerQuery,
	global const unsigned int*  referenceIDsPerQuery, global const unsigned int*  referenceHashPositionListsPerQuery, global const unsigned int*  referenceHashPositionListsSizesPerQuery,
	global const unsigned int*  referenceHashRefPositionsPerQuery, global const unsigned int*  referenceHashQueryPositionsPerQuery,
	global const unsigned int*  lenQueries)
{

	if (get_global_id(1) >= seqNumAgainstPerQuery[get_global_id(0)])
		return;

	const unsigned int CONSTANT_THRESHOLD = (*ConsiderThreshold);





	unsigned int hashmapPositionsList = *(referenceHashPositionListsPerQuery + seqPosAgainstPerQuery[get_global_id(0)] + get_global_id(1));
	unsigned int hashmapPositionsListSize = *(referenceHashPositionListsSizesPerQuery + seqPosAgainstPerQuery[get_global_id(0)] + get_global_id(1));
	unsigned int iAgainst = *(referenceIDsPerQuery + seqPosAgainstPerQuery[get_global_id(0)] + get_global_id(1));



	const unsigned int _seqLengthQuery = seqLengthQuery[get_global_id(0)];



	if (_seqLengthQuery < CONSTANT_THRESHOLD)
		return;
	const unsigned int _seqLengthAgainst = seqLengthsAgainst[iAgainst];  //place it a bit above, so the GPU has some time to calculate this value before it is used


																		 // To remember which nucleotide hit against a reference nucleotide
																		 // todo-later: make 6000 not hardcoded?
	bool rowHitCurrent[6000] = { false }; // using local mem saves 1000 mb with 2000 sequences of global memory. This way we can upscale the number of sequences drasticly

	__global const unsigned char* dataQuery = seqQuerys + seqStartingPointsQuerys[get_global_id(0)]; //seqStart;
	__global const unsigned char* dataAgainst = seqAgainst + seqStartingPointsAgainst[iAgainst];




	unsigned int diagonalRepeatedTotalScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[2]
	unsigned int diagonalTotalConcidered = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[4]

											  //int ThresholdPlusOne = CONSTANT_THRESHOLD + 1;
											  //int ThresholdAddScore = (CONSTANT_THRESHOLD * (CONSTANT_THRESHOLD + 1)) / 2;
	unsigned int seqLengthQueryMinusThreshold = _seqLengthQuery - (CONSTANT_THRESHOLD + 1);

	__global const unsigned char* ptDataAgainstEnd = dataAgainst + _seqLengthAgainst;

	for (unsigned int cHashmapPositionsList = 0; cHashmapPositionsList < hashmapPositionsListSize; cHashmapPositionsList++)
	{

		unsigned int dataAgainstStart = *(referenceHashRefPositionsPerQuery + hashmapPositionsList + cHashmapPositionsList);

		for (unsigned int rowStart = 0; rowStart <= seqLengthQueryMinusThreshold; rowStart++) // To start from each position of the query 
		{
			__global const unsigned char* ptDataQueryRow = dataQuery + rowStart;
			__global const unsigned char* ptDataAgainst = dataAgainst + dataAgainstStart;


			unsigned int row = rowStart;

			while ((*(ptDataQueryRow) == *(ptDataAgainst)) &&
				(row < _seqLengthQuery) &&
				(ptDataAgainst != ptDataAgainstEnd))
			{
				row++;
				ptDataAgainst++;
				ptDataQueryRow++;
			}

			int numMatched = row - rowStart;
			if (numMatched > CONSTANT_THRESHOLD)
			{


				// from rowStart till end
				//memset(rowHitCurrent + rowStart, true, numMatched);
				for (bool* filler = rowHitCurrent + rowStart; filler < rowHitCurrent + rowStart + numMatched;filler++)
					*filler = true;


				diagonalRepeatedTotalScore += (numMatched * (numMatched + 1)) / 2;
				diagonalTotalConcidered += numMatched;

				unsigned int diagonalRepeatedScore = row - 1; // From before rowStart till start of sequence


															  // Since it is possible to end up in the middle of a matching region (we skip with the half of the threshold during hashmap mapping with the query sequence)
															  // try to add some more information by going back

				for (unsigned int rowReverse = numMatched + 1; rowReverse <= row - 1 && rowReverse <= ptDataAgainst - (dataAgainst + dataAgainstStart); rowReverse++)
				{

					if (*(ptDataQueryRow - rowReverse) == *(ptDataAgainst - rowReverse))
					{
						diagonalRepeatedScore++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;
						diagonalTotalConcidered++;
						rowHitCurrent[row - rowReverse] = true;
					}
					else
					{
						break;
					}
				}
				break; // we found the part which lead to the hashmap discovery
			}
		}
	}


	/*
	gridDim: This variable contains the dimensions of the grid.
	blockIdx : This variable contains the block index within the grid.
	blockDim : This variable and contains the dimensions of the block.
	threadIdx : This variable contains the thread index within the block.*/
	/*
	get_global_id(0)=QueryID
	get_global_id(1)=RefID
	get_global_size(1)=NumOfQueries*/
	const unsigned int scoreID = (get_global_id(0)*get_global_size(1)) + get_global_id(1); //  __mul24(blockDim.x, blockIdx.y) + threadIdx.x = ThreadID


	if (diagonalTotalConcidered > 0)
	{
		unsigned int rowHitsFlat = 0;
		for (unsigned int i = 0; i < _seqLengthQuery; i++)
			if (rowHitCurrent[i])
				rowHitsFlat++;

		float percentageMatch = ((float)100 / (float)_seqLengthQuery) * (float)rowHitsFlat;

		//removed this, since rowHitsFlat can never be > _seqLengthQuery since it is contructed inside a loop of length _seqLengthQuery
		//if (percentageMatch > 100)
		//	percentageMatch = 100;

		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;// __fdividef(diagonalRepeatedTotalScore, diagonalTotalConcidered); // (float)score_concidered[0] / (float)score_concidered[2];
		//printf("%.6f %.6f %d %d\n", ((score / (float)100.0) * percentageMatch), percentageMatch, diagonalRepeatedTotalScore, diagonalTotalConcidered);
		PercentageMatches[scoreID] = percentageMatch;
		Scores[scoreID] = ((score / (float)100) * percentageMatch);// -((score / 100.0)*duplicatePenaltyPercentage);//+((score / 100.0)*percentageDoubles); // (float)score_concidered[0] / (float)score_concidered[2];
	}
	else
	{
		Scores[scoreID] = -2;
	}




}
