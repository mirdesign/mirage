// Newest, all chars have their own workgroup thing!
void kernel GetHighestMatrixScoreGlobalAccessCharPerChar(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC, global const int* IDQuery, global const int* IDAgainst)
//, global const int* StartPosQuery, global const int* StartPosAgainst)
{
	//todo: second wave for diagonals --> score

	//	!!!!!!!!!!!!!!!!!!!!!!!!
	//	// ok, can we do some trick to get the ID directly? This will save us data and probably a lot of crashes?
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


	int seqQueryID = IDQuery[get_global_id(0)];
	int queryStartPos = get_global_id(0) - seqStartingPointsQuerys[seqQueryID]; //StartPosQuery[get_global_id(0)];

	int seqAgainstID = IDAgainst[get_global_id(1)];
	int AgainstStartPos = get_global_id(1) - seqStartingPointsAgainst[seqAgainstID]; //StartPosAgainst[get_global_id(1)];


																					 /*
																					 int seqQueryID = 0;
																					 int queryStartPos = 0;


																					 //todo: pre-load and provide a list with argument
																					 for (int i = 0; i < 64; i++)
																					 {
																					 queryStartPos = queryStartPos + (seqLengthQuery[i] - 1);

																					 // i = 11
																					 // qsp = 1100
																					 // id = 600
																					 if (get_global_id(0) < queryStartPos)
																					 {
																					 seqQueryID = i;
																					 queryStartPos = queryStartPos - (seqLengthQuery[i] - 1);
																					 queryStartPos = get_global_id(0) - queryStartPos;
																					 break;
																					 }
																					 }


																					 int seqAgainstID = 0;
																					 int AgainstStartPos = 0;


																					 //todo: pre-load and provide a list with argument
																					 for (int i = 0; i < numAgainst[0]; i++)
																					 {
																					 AgainstStartPos = AgainstStartPos + seqLengthsAgainst[i];

																					 // i = 11
																					 // qsp = 1100
																					 // id = 600
																					 if (get_global_id(1) < AgainstStartPos)
																					 {
																					 seqAgainstID = i;
																					 AgainstStartPos = AgainstStartPos - seqLengthsAgainst[i];
																					 AgainstStartPos = get_global_id(1) - AgainstStartPos;
																					 break;
																					 }
																					 }
																					 */
																					 //int diagonalPos = (seqLengthQuery[seqQueryID] - queryStartPos - 1) + AgainstStartPos;

																					 // todo: mem fence?

	int iStop = (seqLengthQuery[seqQueryID] - queryStartPos < seqLengthsAgainst[seqAgainstID] - AgainstStartPos ? seqLengthQuery[seqQueryID] - queryStartPos : seqLengthsAgainst[seqAgainstID] - AgainstStartPos);

	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqQueryID] + queryStartPos;
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqAgainstID] + AgainstStartPos;

	ulong diagonalRepeatedScore = 0;
	int ConsiderThreshold = 15;
	const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	int diagonalTotalConcidered = 0;
	bool ConsiderThresholdFirstPast = false;
	float diagonalRepeatedTotalScore = 0;

	for (int i = 0; i < iStop; i++)
	{
		//todo: add breakOnPrediction , also possible in 1 diagonal
		if (*(ptCharInp) == *(ptCharRef))
		{
			diagonalRepeatedScore++;

			if (diagonalRepeatedScore > ConsiderThreshold)
			{
				diagonalTotalConcidered++;
				diagonalRepeatedTotalScore += diagonalRepeatedScore;

				if (!ConsiderThresholdFirstPast)
				{
					ConsiderThresholdFirstPast = true;
					diagonalTotalConcidered += ConsiderThreshold;
					diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
				}
			}
		}
		else
		{
			diagonalRepeatedScore = 0;
			ConsiderThresholdFirstPast = false;
		}

		ptCharInp++;
		ptCharRef++;
	}

	if (ConsiderThresholdFirstPast)
		diagonalRepeatedTotalScore += diagonalRepeatedScore;

	/* (0), (1)
	0,0
	0,1
	0,2
	0,..
	1,0
	1,1
	..,..
	*/
	ulong aIndex = (get_global_id(0) * get_global_size(1)) + get_global_id(1);
	Scores[aIndex] = diagonalTotalConcidered / diagonalRepeatedTotalScore;


	//todo:
	// save diagonal total
	// save diagonal hits


}


void kernel GetHighestMatrixScoreGlobalAccessT(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC)
{

	//ulong seqIndexQuerys = get_global_id(0);

	ulong seqIndexQuerys = floor((float)(get_global_id(0) / numAgainst[0])); // ok
	ulong seqIndexAgainst = get_global_id(0) - (seqIndexQuerys * numAgainst[0]); // ok


																				 //ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
																				 //Scores[get_global_id(0)] = seqIndexAgainst * seqIndexQuerys;
	Scores[get_global_id(0)] = get_local_id(0);


}

// This one works with Reverse Compliment.. is ok! It uses constant instead of global, no perf difference
void kernel GetHighestMatrixScoreGlobalAccessLocal(__constant char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, __constant char* seqQuerysRC)
{

	//// Global work size in 1
	//ulong seqIndexQuerys = floor((float)(get_global_id(0) / numAgainst[0])); // ok
	//ulong seqIndexAgainst = get_global_id(0) - (seqIndexQuerys * numAgainst[0]); // ok


	// Global work size divided in 2
	ulong seqIndexQuerys = get_global_id(0);
	ulong seqIndexAgainst = get_global_id(1); // todo: test if local ID can be used?


											  //Scores[seqIndexQuerys] = seqIndexQuerys;

											  //return;


											  //ulong seqIndexAgainst = get_global_id(1) * (get_global_id(2) + 1); // todo: test if local ID can be used?
											  //ulong seqIndexAgainst = get_local_id(0);
											  /*
											  if (seqIndexAgainst > 2132)
											  seqIndexAgainst = 0;*/

	ulong lenQuery = seqLengthQuery[seqIndexQuerys];
	ulong lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	__constant  char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	__constant  char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	// Consider threshold
	const int ConsiderThreshold = 18;
	const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;
	bool ConsiderThresholdFirstPastRC = false;

	// Scoring values

	ulong diagonalScoreHighest = 0;
	ulong diagonalScoreHighestRC = 0;
	ulong diagonalRepeatedTotalScore = 0;
	ulong diagonalRepeatedTotalScoreRC = 0;
	ulong diagonalTotalConcidered = 0;
	ulong diagonalTotalConcideredRC = 0;
	ulong diagonalHitScore = 0;
	ulong diagonalHitScoreRC = 0;
	ulong diagonalRepeatedScore = 0;
	ulong diagonalRepeatedScoreRC = 0;
	bool diagonalFullyAnalysed = true;
	bool diagonalFullyAnalysedRC = true;

	// Pre-calculations, provide performance gain
	__constant  char* ptCharInpRowAdded;
	__constant  char* ptCharInpRowAddedRC;
	const global char* ptCharRefRowAdded;
	ulong inpLenMinusRow = 0;
	ulong refLenMinusRow = 0;
	ulong iStop = 0;
	ulong iLeft = 0;
	ulong iLeftRC = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (ulong row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;
		diagonalFullyAnalysed = true;
		diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;
		ptCharInpRowAddedRC = ptCharInpRC + row;


		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;
		iLeftRC = iStop;

		for (ulong i = 0; i < iStop; i++)
		{

			ptCharRef++;

			if (diagonalFullyAnalysed)
			{
				ptCharInpRowAdded++;
				if (*(ptCharInpRowAdded) == *(ptCharRef))
				{
					diagonalRepeatedScore++;

					if (diagonalRepeatedScore > ConsiderThreshold)
					{
						diagonalTotalConcidered++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;

						if (!ConsiderThresholdFirstPast)
						{
							ConsiderThresholdFirstPast = true;
							diagonalTotalConcidered += ConsiderThreshold;
							diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScore++;
				}
				else
				{
					diagonalRepeatedScore = 0;
					ConsiderThresholdFirstPast = false;
					//iLeft--;

					if (--iLeft < diagonalScoreHighest)
					{
						diagonalFullyAnalysed = false;
						if (!diagonalFullyAnalysedRC)
							break;
					}
				}
			}

			if (diagonalFullyAnalysedRC)
			{
				ptCharInpRowAddedRC++;

				if (*(ptCharInpRowAddedRC) == *(ptCharRef))
				{
					diagonalRepeatedScoreRC++;

					if (diagonalRepeatedScoreRC > ConsiderThreshold)
					{
						diagonalTotalConcideredRC++;
						diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

						if (!ConsiderThresholdFirstPastRC)
						{
							ConsiderThresholdFirstPastRC = true;
							diagonalTotalConcideredRC += ConsiderThreshold;
							diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScoreRC++;
				}
				else
				{
					diagonalRepeatedScoreRC = 0;
					ConsiderThresholdFirstPastRC = false;
					//iLeft--;

					if (--iLeftRC < diagonalScoreHighestRC)
					{
						diagonalFullyAnalysedRC = false;
						if (!diagonalFullyAnalysed)
							break;
					}
				}
			}
		}

		ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
		if (diagonalFullyAnalysedRC)
		{
			if (diagonalHitScoreRC > diagonalScoreHighestRC)
				diagonalScoreHighestRC = diagonalHitScoreRC;
		}
	}

	// Right side
	ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	for (ulong row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;
		iLeftRC = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;

		for (ulong i = 0; i < iStop; i++)
		{
			ptCharInp++;

			if (diagonalFullyAnalysed)
			{
				ptCharRefRowAdded++;
				if (*(ptCharRefRowAdded) == *(ptCharInp))
				{
					diagonalRepeatedScore++;

					if (diagonalRepeatedScore > ConsiderThreshold)
					{
						diagonalTotalConcidered++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;

						if (!ConsiderThresholdFirstPast)
						{
							ConsiderThresholdFirstPast = true;
							diagonalTotalConcidered += ConsiderThreshold;
							diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScore++;
				}
				else
				{
					diagonalRepeatedScore = 0;
					ConsiderThresholdFirstPast = false;

					if (--iLeft < diagonalScoreHighest)
					{
						diagonalFullyAnalysed = false;
						if (!diagonalFullyAnalysedRC)
							break;
					}
				}
			}
		}
		if (diagonalFullyAnalysedRC) {
			ptCharInpRC++;

			if (*(ptCharRefRowAdded) == *(ptCharInpRC))
			{
				diagonalRepeatedScoreRC++;

				if (diagonalRepeatedScoreRC > ConsiderThreshold)
				{
					diagonalTotalConcideredRC++;
					diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

					if (!ConsiderThresholdFirstPastRC)
					{
						ConsiderThresholdFirstPastRC = true;
						diagonalTotalConcideredRC += ConsiderThreshold;
						diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScoreRC++;
			}
			else
			{
				diagonalRepeatedScoreRC = 0;
				ConsiderThresholdFirstPastRC = false;

				if (--iLeftRC < diagonalScoreHighestRC)
				{
					diagonalFullyAnalysedRC = false;
					if (!diagonalFullyAnalysed)
						break;
				}
			}
		}

		ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
			{
				diagonalScoreHighest = diagonalHitScore;
			}
		}

		if (diagonalFullyAnalysedRC)
		{
			if (diagonalHitScoreRC > diagonalScoreHighestRC)
			{
				diagonalScoreHighestRC = diagonalHitScoreRC;
			}
		}
	}


	ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;


	if (diagonalTotalConcidered <= 0 && diagonalTotalConcideredRC <= 0)
		Scores[aIndex] = -1;
	else if (diagonalTotalConcidered > 0 && diagonalTotalConcideredRC > 0)
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		if (scoreRC > score)
			Scores[aIndex] = scoreRC;
		else
			Scores[aIndex] = score;

	}
	else if (diagonalTotalConcidered > 0)
	{
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		Scores[aIndex] = score;

	}
	else
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;

		Scores[aIndex] = scoreRC;

	}
}






void SetScore(int idX, int numAgainst, int idY, ulong score_concidered[], global float* Scores)
{

	//ulong aIndex = (idX * numAgainst) + idY;
	if (score_concidered[2] <= 0 && score_concidered[3] <= 0)
		Scores[(idX * numAgainst) + idY] = -1;
	else if (score_concidered[2]  > 0 && score_concidered[3] > 0)
	{
		//float scoreRC = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[3] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[5];
		//float score = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[2] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[4];

		if ((float)score_concidered[1] / (float)score_concidered[3] > (float)score_concidered[0] / (float)score_concidered[2])
			Scores[(idX * numAgainst) + idY] = (float)score_concidered[1] / (float)score_concidered[3];
		else
			Scores[(idX * numAgainst) + idY] = (float)score_concidered[0] / (float)score_concidered[2];

	}
	else if (score_concidered[2]  > 0)
	{
		//float score = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[2] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[4];
		Scores[(idX * numAgainst) + idY] = (float)score_concidered[0] / (float)score_concidered[2];
	}
	else
	{
		//float scoreRC = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[3] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[5];
		Scores[(idX * numAgainst) + idY] = (float)score_concidered[1] / (float)score_concidered[3];
	}

}
// tried to decrease #VGPR usage   _VGPR_OPTIMIZED_BOOLEAN_ARRAY_USAGE
void kernel GetHighestMatrixScore(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC)
{



	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];
	const global char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];

	const global char* ptCharInpRowAdded;
	const global char* ptCharInpRowAddedRC;
	const global char* ptCharRefRowAdded;

	//ulong diagonalRepeatedTotalScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[2]
	//ulong diagonalRepeatedTotalScoreRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[3]
	//ulong diagonalTotalConcidered = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[4]
	//ulong diagonalTotalConcideredRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[5]

	ulong score_concidered[4];
	score_concidered[0] = 0; // diagonalRepeatedTotalScore
	score_concidered[1] = 0; // diagonalRepeatedTotalScoreRC
	score_concidered[2] = 0; // diagonalTotalConcidered
	score_concidered[3] = 0; // diagonalTotalConcideredRC


							 // Analyse Left side of matrix
							 /*
							 * 00, 11, 22
							 * 10, 21, 32
							 */

							 //for (ulong row = 0; row < lenQuery; row++)
	for (ulong row = 0; row < seqLengthQuery[get_global_id(0)]; row++)
	{


		ulong diagonalRepeatedScore = 0;
		ulong diagonalRepeatedScoreRC = 0;

		// Pre-calculate values

		ptCharInpRowAdded = ptCharInp + row;
		ptCharInpRowAddedRC = ptCharInpRC + row;


		for (ulong i = 0; i < (seqLengthsAgainst[get_global_id(1)] < seqLengthQuery[get_global_id(0)] - row ? seqLengthsAgainst[get_global_id(1)] : seqLengthQuery[get_global_id(0)] - row); i++) // recalculate is faster
		{

			ptCharRef++;

			ptCharInpRowAdded++;

			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > 18)
				{
					if (diagonalRepeatedScore == 19)
					{
						score_concidered[2] += 18;
						score_concidered[0] += 171;
					}
					score_concidered[2]++;
					score_concidered[0] += diagonalRepeatedScore;
				}
			}
			else
			{
				diagonalRepeatedScore = 0;
			}
			ptCharInpRowAddedRC++;

			if (*(ptCharInpRowAddedRC) == *(ptCharRef))
			{
				diagonalRepeatedScoreRC++;

				if (diagonalRepeatedScoreRC > 18)
				{
					score_concidered[3]++;
					score_concidered[1] += diagonalRepeatedScoreRC;

					if (diagonalRepeatedScoreRC == 19)
					{
						score_concidered[3] += 18;
						score_concidered[1] += 171;
					}
				}
			}
			else
			{
				diagonalRepeatedScoreRC = 0;
			}
		}
		ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];

	}

	// Right side
	ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
	ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];

	for (ulong row = 1; row < seqLengthsAgainst[get_global_id(1)]; row++)
	{


		ulong diagonalRepeatedScore = 0;
		ulong diagonalRepeatedScoreRC = 0;

		ptCharRefRowAdded = ptCharRef + row;

		for (ulong i = 0; i < (seqLengthQuery[get_global_id(0)] < seqLengthsAgainst[get_global_id(1)] - row ? seqLengthQuery[get_global_id(0)] : seqLengthsAgainst[get_global_id(1)] - row); i++)
		{
			ptCharInp++;

			ptCharRefRowAdded++;
			if (*(ptCharRefRowAdded) == *(ptCharInp))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > 18)
				{
					score_concidered[2]++;
					score_concidered[0] += diagonalRepeatedScore;

					if (diagonalRepeatedScore == 19)
					{
						score_concidered[2] += 18;
						score_concidered[0] += 171;
					}
				}
			}
			else
			{
				diagonalRepeatedScore = 0;
			}
		}
		ptCharInpRC++;

		if (*(ptCharRefRowAdded) == *(ptCharInpRC))
		{
			diagonalRepeatedScoreRC++;

			if (diagonalRepeatedScoreRC > 18)
			{
				score_concidered[3]++;
				score_concidered[1] += diagonalRepeatedScoreRC;

				if (diagonalRepeatedScoreRC == 19)
				{
					score_concidered[3] += 18;
					score_concidered[1] += 171;
				}
			}
		}
		else
		{
			diagonalRepeatedScoreRC = 0;
		}
		ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];

	}


	SetScore(get_global_id(0), get_global_size(1), get_global_id(1), score_concidered, Scores);
}

// tried to decrease #VGPR usage   _VGPR_OPTIMIZED
void kernel GetHighestMatrixScore_VGPR_OPTIMIZED(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC)
{
	//// Global work size in 1
	//ulong seqIndexQuerys = floor((float)(get_global_id(0) / numAgainst[0])); // ok
	//ulong seqIndexAgainst = get_global_id(0) - (seqIndexQuerys * numAgainst[0]); // ok


	// Global work size divided in 2
	//ulong seqIndexQuerys = get_global_id(0);
	//ulong seqIndexAgainst = get_global_id(1); // todo: test if local ID can be used?

	//ulong seqIndexAgainst = get_global_id(1) * (get_global_id(2) + 1); // todo: test if local ID can be used?
	//ulong seqIndexAgainst = get_local_id(0);
	/*
	if (seqIndexAgainst > 2132)
	seqIndexAgainst = 0;*/
	/*
	ulong lenQuery = seqLengthQuery[seqIndexQuerys];
	ulong lenAgainst = seqLengthsAgainst[seqIndexAgainst];*/
	//ulong lenQuery = seqLengthQuery[get_global_id(0)];
	//ulong lenAgainst = seqLengthsAgainst[get_global_id(1)];

	///
	/// Initialise general data
	///

	//// Keep track of sequences
	//const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	//const global char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	//const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];


	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];
	const global char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];

	//// Consider threshold
	//const int ConsiderThreshold = 18;
	//const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	//bool ConsiderThresholdFirstPast = false;
	//bool ConsiderThresholdFirstPastRC = false;
	//__local bool ConsiderThresholdFirstPast;
	//__local bool ConsiderThresholdFirstPastRC;

	//ConsiderThresholdFirstPast = false;
	//ConsiderThresholdFirstPastRC = false;

	// Scoring values

	//ulong diagonalScoreHighest = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[0]
	//ulong diagonalScoreHighestRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[1]
	//ulong diagonalRepeatedTotalScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[2]
	//ulong diagonalRepeatedTotalScoreRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[3]

	//ulong4 diagnonalScoreHighest_RepeatedTotalScore_RC = (0, 0, 0, 0);
	ulong diagnonalScoreHighest_RepeatedTotalScore_RC[7];
	diagnonalScoreHighest_RepeatedTotalScore_RC[0] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[1] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[2] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[3] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[4] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[5] = 0;
	diagnonalScoreHighest_RepeatedTotalScore_RC[6] = 0;

	//ulong diagonalTotalConcidered = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[4]
	//ulong diagonalTotalConcideredRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[5]
	//ulong diagonalHitScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[6]


	ulong diagonalHitScoreRC = 0;
	ulong diagonalRepeatedScore = 0;
	ulong diagonalRepeatedScoreRC = 0;
	//bool diagonalFullyAnalysed = true;
	//bool diagonalFullyAnalysedRC = true;



	//__local ulong diagonalHitScoreRC; // diagnonalScoreHighest_RepeatedTotalScore_RC[7]
	//__local ulong diagonalRepeatedScore; // diagnonalScoreHighest_RepeatedTotalScore_RC[8]
	//__local ulong diagonalRepeatedScoreRC; // diagnonalScoreHighest_RepeatedTotalScore_RC[9]
	//__local bool diagonalFullyAnalysed;
	//__local bool diagonalFullyAnalysedRC;

	//diagonalHitScoreRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[7]
	//diagonalRepeatedScore = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[8]
	//diagonalRepeatedScoreRC = 0; // diagnonalScoreHighest_RepeatedTotalScore_RC[9]
	//diagonalFullyAnalysed = true;
	//diagonalFullyAnalysedRC = true;

	// Pre-calculations, provide performance gain
	const global char* ptCharInpRowAdded;
	const global char* ptCharInpRowAddedRC;
	const global char* ptCharRefRowAdded;
	//ulong inpLenMinusRow = 0;
	ulong refLenMinusRow = 0;
	//ulong iStop = 0; // recalculate is faster
	//ulong iLeft = 0;
	//ulong iLeftRC = 0;

	//__local ulong refLenMinusRow;
	////ulong iStop = 0; // recalculate is faster
	//__local ulong iLeft;
	//__local ulong iLeftRC;

	// refLenMinusRow = 0;
	////ulong iStop = 0; // recalculate is faster
	// iLeft = 0;
	// iLeftRC = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/

	//for (ulong row = 0; row < lenQuery; row++)
	for (ulong row = 0; row < seqLengthQuery[get_global_id(0)]; row++)
	{
		//ConsiderThresholdFirstPast = false;
		//ConsiderThresholdFirstPastRC = false;
		//diagonalFullyAnalysed = true;
		//diagonalFullyAnalysedRC = true;
		//diagnonalScoreHighest_RepeatedTotalScore_RC[6] = 0;
		//diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;

		// Pre-calculate values
		//inpLenMinusRow = lenQuery - row;
		//inpLenMinusRow = seqLengthQuery[get_global_id(0)] - row;
		ptCharInpRowAdded = ptCharInp + row;
		ptCharInpRowAddedRC = ptCharInpRC + row;


		// Determince end of loop
		//iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow); // recalculate is faster
		////iLeft = iStop; // recalculate is faster
		////iLeftRC = iStop; // recalculate is faster
		//iLeft = (seqLengthsAgainst[get_global_id(1)] < seqLengthQuery[get_global_id(0)] - row ? seqLengthsAgainst[get_global_id(1)] : seqLengthQuery[get_global_id(0)] - row); // recalculate is faster
		//iLeftRC = (seqLengthsAgainst[get_global_id(1)] < seqLengthQuery[get_global_id(0)] - row ? seqLengthsAgainst[get_global_id(1)] : seqLengthQuery[get_global_id(0)] - row); // recalculate is faster

		//for (ulong i = 0; i < iStop; i++)
		for (ulong i = 0; i < (seqLengthsAgainst[get_global_id(1)] < seqLengthQuery[get_global_id(0)] - row ? seqLengthsAgainst[get_global_id(1)] : seqLengthQuery[get_global_id(0)] - row); i++) // recalculate is faster
		{

			ptCharRef++;

			//if (diagonalFullyAnalysed)
			//{
			ptCharInpRowAdded++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > 18)
				{
					if (diagonalRepeatedScore == 19)
						//if (!ConsiderThresholdFirstPast)
					{
						//ConsiderThresholdFirstPast = true;
						diagnonalScoreHighest_RepeatedTotalScore_RC[4] += 18;
						//diagonalRepeatedTotalScore += 171;
						diagnonalScoreHighest_RepeatedTotalScore_RC[2] += 171;
					}

					diagnonalScoreHighest_RepeatedTotalScore_RC[4]++;
					//diagonalRepeatedTotalScore += diagonalRepeatedScore;
					diagnonalScoreHighest_RepeatedTotalScore_RC[2] += diagonalRepeatedScore;


				}

				//diagnonalScoreHighest_RepeatedTotalScore_RC[6]++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				//ConsiderThresholdFirstPast = false;
				//iLeft--;

				//if (--iLeft < diagonalScoreHighest)
				//if (--iLeft < diagnonalScoreHighest_RepeatedTotalScore_RC[0])
				//{
				//	diagonalFullyAnalysed = false;
				//	if (!diagonalFullyAnalysedRC)
				//		break;
				//}
			}
			/*		}

			if (diagonalFullyAnalysedRC)
			{*/
			ptCharInpRowAddedRC++;

			if (*(ptCharInpRowAddedRC) == *(ptCharRef))
			{
				diagonalRepeatedScoreRC++;

				if (diagonalRepeatedScoreRC > 18)
				{
					diagnonalScoreHighest_RepeatedTotalScore_RC[5]++;
					//diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;
					diagnonalScoreHighest_RepeatedTotalScore_RC[3] += diagonalRepeatedScoreRC;

					if (diagonalRepeatedScoreRC == 19)
						//if (!ConsiderThresholdFirstPastRC)
					{
						//ConsiderThresholdFirstPastRC = true;
						diagnonalScoreHighest_RepeatedTotalScore_RC[5] += 18;
						//diagonalRepeatedTotalScoreRC += 171;
						diagnonalScoreHighest_RepeatedTotalScore_RC[3] += 171;
					}
				}

				//diagonalHitScoreRC++;
			}
			else
			{
				diagonalRepeatedScoreRC = 0;
				//ConsiderThresholdFirstPastRC = false;
				//iLeft--;

				//if (--iLeftRC < diagonalScoreHighestRC)
				/*		if (--iLeftRC < diagnonalScoreHighest_RepeatedTotalScore_RC[1])
				{
				diagonalFullyAnalysedRC = false;
				if (!diagonalFullyAnalysed)
				break;
				}*/
			}
			//}
		}

		//ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];
		ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];


		//if (diagonalFullyAnalysed)
		//{
		//	if (diagonalHitScore > diagonalScoreHighest)
		//		diagonalScoreHighest = diagonalHitScore;
		//}
		//if (diagonalFullyAnalysedRC)
		//{
		//	if (diagonalHitScoreRC > diagonalScoreHighestRC)
		//		diagonalScoreHighestRC = diagonalHitScoreRC;
		//}
		//if (diagonalFullyAnalysed)
		//{
		//if (diagnonalScoreHighest_RepeatedTotalScore_RC[6] > diagnonalScoreHighest_RepeatedTotalScore_RC[0])
		//	diagnonalScoreHighest_RepeatedTotalScore_RC[0] = diagnonalScoreHighest_RepeatedTotalScore_RC[6];
		//}
		//if (diagonalFullyAnalysedRC)
		//{
		//if (diagonalHitScoreRC > diagnonalScoreHighest_RepeatedTotalScore_RC[1])
		//	diagnonalScoreHighest_RepeatedTotalScore_RC[1] = diagonalHitScoreRC;
		//}
	}


	// Right side
	//ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	//ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	//ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];
	ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
	ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[get_global_id(1)];



	//for (ulong row = 1; row < lenAgainst; row++)
	for (ulong row = 1; row < seqLengthsAgainst[get_global_id(1)]; row++)
	{
		//diagonalFullyAnalysed = true;
		//diagonalFullyAnalysedRC = true;
		//diagnonalScoreHighest_RepeatedTotalScore_RC[6] = 0;
		//diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;
		//refLenMinusRow = lenAgainst - row;
		//refLenMinusRow = seqLengthsAgainst[get_global_id(1)] - row;
		//iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow); // recalculate is faster
		////iLeft = iStop; // recalculate is faster
		////iLeftRC = iStop; // recalculate is faster
		//iLeft = (seqLengthQuery[get_global_id(0)] < seqLengthsAgainst[get_global_id(1)] - row ? seqLengthQuery[get_global_id(0)] : seqLengthsAgainst[get_global_id(1)] - row); // recalculate is faster
		//iLeftRC = (seqLengthQuery[get_global_id(0)] < seqLengthsAgainst[get_global_id(1)] - row ? seqLengthQuery[get_global_id(0)] : seqLengthsAgainst[get_global_id(1)] - row); // recalculate is faster

		ptCharRefRowAdded = ptCharRef + row;

		/*
		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;*/

		//for (ulong i = 0; i < iStop; i++) // recalculate is faster
		for (ulong i = 0; i < (seqLengthQuery[get_global_id(0)] < seqLengthsAgainst[get_global_id(1)] - row ? seqLengthQuery[get_global_id(0)] : seqLengthsAgainst[get_global_id(1)] - row); i++)
		{
			ptCharInp++;

			//if (diagonalFullyAnalysed)
			//{
			ptCharRefRowAdded++;
			if (*(ptCharRefRowAdded) == *(ptCharInp))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > 18)
				{
					diagnonalScoreHighest_RepeatedTotalScore_RC[4]++;
					//diagonalRepeatedTotalScore += diagonalRepeatedScore;
					diagnonalScoreHighest_RepeatedTotalScore_RC[2] += diagonalRepeatedScore;

					if (diagonalRepeatedScore == 19)
						//if (!ConsiderThresholdFirstPast)
					{
						//ConsiderThresholdFirstPast = true;
						diagnonalScoreHighest_RepeatedTotalScore_RC[4] += 18;
						//diagonalRepeatedTotalScore += 171;
						diagnonalScoreHighest_RepeatedTotalScore_RC[2] += 171;
					}
				}

				//diagnonalScoreHighest_RepeatedTotalScore_RC[6]++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				//ConsiderThresholdFirstPast = false;

				//if (--iLeft < diagonalScoreHighest)
				//if (--iLeft < diagnonalScoreHighest_RepeatedTotalScore_RC[0])
				//{
				//	diagonalFullyAnalysed = false;
				//	if (!diagonalFullyAnalysedRC)
				//		break;
				//}
			}
			//}
		}
		//if (diagonalFullyAnalysedRC) {
		ptCharInpRC++;

		if (*(ptCharRefRowAdded) == *(ptCharInpRC))
		{
			diagonalRepeatedScoreRC++;

			if (diagonalRepeatedScoreRC > 18)
			{
				diagnonalScoreHighest_RepeatedTotalScore_RC[5]++;
				//diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;
				diagnonalScoreHighest_RepeatedTotalScore_RC[3] += diagonalRepeatedScoreRC;

				if (diagonalRepeatedScoreRC == 19)
					//if (!ConsiderThresholdFirstPastRC)
				{
					//ConsiderThresholdFirstPastRC = true;
					diagnonalScoreHighest_RepeatedTotalScore_RC[5] += 18;
					//diagonalRepeatedTotalScoreRC += 171;
					diagnonalScoreHighest_RepeatedTotalScore_RC[3] += 171;
				}
			}

			//diagonalHitScoreRC++;
		}
		else
		{
			diagonalRepeatedScoreRC = 0;
			//ConsiderThresholdFirstPastRC = false;

			//if (--iLeftRC < diagonalScoreHighestRC)
			//if (--iLeftRC < diagnonalScoreHighest_RepeatedTotalScore_RC[1])
			//{
			//	diagonalFullyAnalysedRC = false;
			//	if (!diagonalFullyAnalysed)
			//		break;
			//}
		}
		//}

		/*ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];*/
		ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[get_global_id(0)];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[get_global_id(0)];


		//if (diagonalFullyAnalysed)
		//{
		//	if (diagonalHitScore > diagonalScoreHighest)
		//	{
		//		diagonalScoreHighest = diagonalHitScore;
		//	}
		//}

		//if (diagonalFullyAnalysedRC)
		//{
		//	if (diagonalHitScoreRC > diagonalScoreHighestRC)
		//	{
		//		diagonalScoreHighestRC = diagonalHitScoreRC;
		//	}
		//}
		//if (diagonalFullyAnalysed)
		//{
		//if (diagnonalScoreHighest_RepeatedTotalScore_RC[6] > diagnonalScoreHighest_RepeatedTotalScore_RC[0])
		//{
		//	diagnonalScoreHighest_RepeatedTotalScore_RC[0] = diagnonalScoreHighest_RepeatedTotalScore_RC[6];
		//}
		//}

		//if (diagonalFullyAnalysedRC)
		//{
		//if (diagonalHitScoreRC > diagnonalScoreHighest_RepeatedTotalScore_RC[1])
		//{
		//	diagnonalScoreHighest_RepeatedTotalScore_RC[1] = diagonalHitScoreRC;
		//}
		//}
	}


	//ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	ulong aIndex = (get_global_id(0) * numAgainst[0]) + get_global_id(1);


	if (diagnonalScoreHighest_RepeatedTotalScore_RC[4] <= 0 && diagnonalScoreHighest_RepeatedTotalScore_RC[5] <= 0)
		Scores[aIndex] = -1;
	else if (diagnonalScoreHighest_RepeatedTotalScore_RC[4] > 0 && diagnonalScoreHighest_RepeatedTotalScore_RC[5] > 0)
	{
		//float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;
		//float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
		float scoreRC = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[3] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[5];
		float score = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[2] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[4];

		if (scoreRC > score)
			Scores[aIndex] = scoreRC;
		else
			Scores[aIndex] = score;

	}
	else if (diagnonalScoreHighest_RepeatedTotalScore_RC[4] > 0)
	{
		//float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
		float score = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[2] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[4];

		Scores[aIndex] = score;

	}
	else
	{
		//float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;
		float scoreRC = (float)diagnonalScoreHighest_RepeatedTotalScore_RC[3] / (float)diagnonalScoreHighest_RepeatedTotalScore_RC[5];

		Scores[aIndex] = scoreRC;

	}
}

// This one works with Reverse Compliment.. is ok! _OK_RV
void kernel GetHighestMatrixScore_OK_RV(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC)
{

	//// Global work size in 1
	//ulong seqIndexQuerys = floor((float)(get_global_id(0) / numAgainst[0])); // ok
	//ulong seqIndexAgainst = get_global_id(0) - (seqIndexQuerys * numAgainst[0]); // ok


	// Global work size divided in 2
	ulong seqIndexQuerys = get_global_id(0);
	ulong seqIndexAgainst = get_global_id(1); // todo: test if local ID can be used?

											  //ulong seqIndexAgainst = get_global_id(1) * (get_global_id(2) + 1); // todo: test if local ID can be used?
											  //ulong seqIndexAgainst = get_local_id(0);
											  /*
											  if (seqIndexAgainst > 2132)
											  seqIndexAgainst = 0;*/

	ulong lenQuery = seqLengthQuery[seqIndexQuerys];
	ulong lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	const global char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	// Consider threshold
	const int ConsiderThreshold = 18;
	const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;
	bool ConsiderThresholdFirstPastRC = false;

	// Scoring values

	ulong diagonalScoreHighest = 0;
	ulong diagonalScoreHighestRC = 0;
	ulong diagonalRepeatedTotalScore = 0;
	ulong diagonalRepeatedTotalScoreRC = 0;
	ulong diagonalTotalConcidered = 0;
	ulong diagonalTotalConcideredRC = 0;
	ulong diagonalHitScore = 0;
	ulong diagonalHitScoreRC = 0;
	ulong diagonalRepeatedScore = 0;
	ulong diagonalRepeatedScoreRC = 0;
	bool diagonalFullyAnalysed = true;
	bool diagonalFullyAnalysedRC = true;

	// Pre-calculations, provide performance gain
	const global char* ptCharInpRowAdded;
	const global char* ptCharInpRowAddedRC;
	const global char* ptCharRefRowAdded;
	ulong inpLenMinusRow = 0;
	ulong refLenMinusRow = 0;
	ulong iStop = 0;
	ulong iLeft = 0;
	ulong iLeftRC = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (ulong row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;
		diagonalFullyAnalysed = true;
		diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;
		ptCharInpRowAddedRC = ptCharInpRC + row;


		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;
		iLeftRC = iStop;

		for (ulong i = 0; i < iStop; i++)
		{

			ptCharRef++;

			if (diagonalFullyAnalysed)
			{
				ptCharInpRowAdded++;
				if (*(ptCharInpRowAdded) == *(ptCharRef))
				{
					diagonalRepeatedScore++;

					if (diagonalRepeatedScore > ConsiderThreshold)
					{
						diagonalTotalConcidered++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;

						if (!ConsiderThresholdFirstPast)
						{
							ConsiderThresholdFirstPast = true;
							diagonalTotalConcidered += ConsiderThreshold;
							diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScore++;
				}
				else
				{
					diagonalRepeatedScore = 0;
					ConsiderThresholdFirstPast = false;
					//iLeft--;

					if (--iLeft < diagonalScoreHighest)
					{
						diagonalFullyAnalysed = false;
						if (!diagonalFullyAnalysedRC)
							break;
					}
				}
			}

			if (diagonalFullyAnalysedRC)
			{
				ptCharInpRowAddedRC++;

				if (*(ptCharInpRowAddedRC) == *(ptCharRef))
				{
					diagonalRepeatedScoreRC++;

					if (diagonalRepeatedScoreRC > ConsiderThreshold)
					{
						diagonalTotalConcideredRC++;
						diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

						if (!ConsiderThresholdFirstPastRC)
						{
							ConsiderThresholdFirstPastRC = true;
							diagonalTotalConcideredRC += ConsiderThreshold;
							diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScoreRC++;
				}
				else
				{
					diagonalRepeatedScoreRC = 0;
					ConsiderThresholdFirstPastRC = false;
					//iLeft--;

					if (--iLeftRC < diagonalScoreHighestRC)
					{
						diagonalFullyAnalysedRC = false;
						if (!diagonalFullyAnalysed)
							break;
					}
				}
			}
		}

		ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
		if (diagonalFullyAnalysedRC)
		{
			if (diagonalHitScoreRC > diagonalScoreHighestRC)
				diagonalScoreHighestRC = diagonalHitScoreRC;
		}
	}


	// Right side
	ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	for (ulong row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;
		iLeftRC = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;

		for (ulong i = 0; i < iStop; i++)
		{
			ptCharInp++;

			if (diagonalFullyAnalysed)
			{
				ptCharRefRowAdded++;
				if (*(ptCharRefRowAdded) == *(ptCharInp))
				{
					diagonalRepeatedScore++;

					if (diagonalRepeatedScore > ConsiderThreshold)
					{
						diagonalTotalConcidered++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;

						if (!ConsiderThresholdFirstPast)
						{
							ConsiderThresholdFirstPast = true;
							diagonalTotalConcidered += ConsiderThreshold;
							diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
						}
					}

					diagonalHitScore++;
				}
				else
				{
					diagonalRepeatedScore = 0;
					ConsiderThresholdFirstPast = false;

					if (--iLeft < diagonalScoreHighest)
					{
						diagonalFullyAnalysed = false;
						if (!diagonalFullyAnalysedRC)
							break;
					}
				}
			}
		}
		if (diagonalFullyAnalysedRC) {
			ptCharInpRC++;

			if (*(ptCharRefRowAdded) == *(ptCharInpRC))
			{
				diagonalRepeatedScoreRC++;

				if (diagonalRepeatedScoreRC > ConsiderThreshold)
				{
					diagonalTotalConcideredRC++;
					diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

					if (!ConsiderThresholdFirstPastRC)
					{
						ConsiderThresholdFirstPastRC = true;
						diagonalTotalConcideredRC += ConsiderThreshold;
						diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScoreRC++;
			}
			else
			{
				diagonalRepeatedScoreRC = 0;
				ConsiderThresholdFirstPastRC = false;

				if (--iLeftRC < diagonalScoreHighestRC)
				{
					diagonalFullyAnalysedRC = false;
					if (!diagonalFullyAnalysed)
						break;
				}
			}
		}

		ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
			{
				diagonalScoreHighest = diagonalHitScore;
			}
		}

		if (diagonalFullyAnalysedRC)
		{
			if (diagonalHitScoreRC > diagonalScoreHighestRC)
			{
				diagonalScoreHighestRC = diagonalHitScoreRC;
			}
		}
	}


	ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;


	if (diagonalTotalConcidered <= 0 && diagonalTotalConcideredRC <= 0)
		Scores[aIndex] = -1;
	else if (diagonalTotalConcidered > 0 && diagonalTotalConcideredRC > 0)
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		if (scoreRC > score)
			Scores[aIndex] = scoreRC;
		else
			Scores[aIndex] = score;

	}
	else if (diagonalTotalConcidered > 0)
	{
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		Scores[aIndex] = score;

	}
	else
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;

		Scores[aIndex] = scoreRC;

	}
}

void kernel GetHighestMatrixScoreGlobalAccessNoBreakOnPrediction(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores, global const char* seqQuerysRC)
{

	//// Global work size in 1
	//ulong seqIndexQuerys = floor((float)(get_global_id(0) / numAgainst[0])); // ok
	//ulong seqIndexAgainst = get_global_id(0) - (seqIndexQuerys * numAgainst[0]); // ok


	// Global work size divided in 2
	ulong seqIndexQuerys = get_global_id(0);
	ulong seqIndexAgainst = get_global_id(1); // todo: test if local ID can be used?

											  //ulong seqIndexAgainst = get_global_id(1) * (get_global_id(2) + 1); // todo: test if local ID can be used?
											  //ulong seqIndexAgainst = get_local_id(0);
											  /*
											  if (seqIndexAgainst > 2132)
											  seqIndexAgainst = 0;*/

	ulong lenQuery = seqLengthQuery[seqIndexQuerys];
	ulong lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	const global char* ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	// Consider threshold
	const int ConsiderThreshold = 18;
	const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;
	bool ConsiderThresholdFirstPastRC = false;

	// Scoring values

	ulong diagonalScoreHighest = 0;
	ulong diagonalScoreHighestRC = 0;
	ulong diagonalRepeatedTotalScore = 0;
	ulong diagonalRepeatedTotalScoreRC = 0;
	ulong diagonalTotalConcidered = 0;
	ulong diagonalTotalConcideredRC = 0;
	ulong diagonalHitScore = 0;
	ulong diagonalHitScoreRC = 0;
	ulong diagonalRepeatedScore = 0;
	ulong diagonalRepeatedScoreRC = 0;
	//bool diagonalFullyAnalysed = true;
	//bool diagonalFullyAnalysedRC = true;

	// Pre-calculations, provide performance gain
	const global char* ptCharInpRowAdded;
	const global char* ptCharInpRowAddedRC;
	const global char* ptCharRefRowAdded;
	ulong inpLenMinusRow = 0;
	ulong refLenMinusRow = 0;
	ulong iStop = 0;
	//ulong iLeft = 0;
	//ulong iLeftRC = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (ulong row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;
		//diagonalFullyAnalysed = true;
		//diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;
		ptCharInpRowAddedRC = ptCharInpRC + row;


		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		//iLeft = iStop;
		//iLeftRC = iStop;

		for (ulong i = 0; i < iStop; i++)
		{

			ptCharRef++;

			//if (diagonalFullyAnalysed)
			//{
			ptCharInpRowAdded++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				//if (--iLeft < diagonalScoreHighest)
				//{
				//	diagonalFullyAnalysed = false;
				//	if (!diagonalFullyAnalysedRC)
				//		break;
				//}
			}
			/*}

			if (diagonalFullyAnalysedRC)
			{*/
			ptCharInpRowAddedRC++;

			if (*(ptCharInpRowAddedRC) == *(ptCharRef))
			{
				diagonalRepeatedScoreRC++;

				if (diagonalRepeatedScoreRC > ConsiderThreshold)
				{
					diagonalTotalConcideredRC++;
					diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

					if (!ConsiderThresholdFirstPastRC)
					{
						ConsiderThresholdFirstPastRC = true;
						diagonalTotalConcideredRC += ConsiderThreshold;
						diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScoreRC++;
			}
			else
			{
				diagonalRepeatedScoreRC = 0;
				ConsiderThresholdFirstPastRC = false;
				//iLeft--;

				//if (--iLeftRC < diagonalScoreHighestRC)
				//{
				//	diagonalFullyAnalysedRC = false;
				//	if (!diagonalFullyAnalysed)
				//		break;
				//}
			}
		}
		//}

		ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];


		//if (diagonalFullyAnalysed)
		//{
		if (diagonalHitScore > diagonalScoreHighest)
			diagonalScoreHighest = diagonalHitScore;
		//}
		//if (diagonalFullyAnalysedRC)
		//{
		if (diagonalHitScoreRC > diagonalScoreHighestRC)
			diagonalScoreHighestRC = diagonalHitScoreRC;
		//}
	}

	// Right side
	ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];

	for (ulong row = 1; row < lenAgainst; row++)
	{
		//diagonalFullyAnalysed = true;
		//diagonalFullyAnalysedRC = true;
		diagonalHitScore = 0;
		diagonalHitScoreRC = 0;
		diagonalRepeatedScore = 0;
		diagonalRepeatedScoreRC = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		//iLeft = iStop;
		//iLeftRC = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;
		ConsiderThresholdFirstPastRC = false;

		for (ulong i = 0; i < iStop; i++)
		{
			ptCharInp++;

			//if (diagonalFullyAnalysed)
			//{
			ptCharRefRowAdded++;
			if (*(ptCharRefRowAdded) == *(ptCharInp))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				//if (--iLeft < diagonalScoreHighest)
				//{
				//	diagonalFullyAnalysed = false;
				//	if (!diagonalFullyAnalysedRC)
				//		break;
				//}
			}
		}
		/*	}
		if (diagonalFullyAnalysedRC) {*/
		ptCharInpRC++;

		if (*(ptCharRefRowAdded) == *(ptCharInpRC))
		{
			diagonalRepeatedScoreRC++;

			if (diagonalRepeatedScoreRC > ConsiderThreshold)
			{
				diagonalTotalConcideredRC++;
				diagonalRepeatedTotalScoreRC += diagonalRepeatedScoreRC;

				if (!ConsiderThresholdFirstPastRC)
				{
					ConsiderThresholdFirstPastRC = true;
					diagonalTotalConcideredRC += ConsiderThreshold;
					diagonalRepeatedTotalScoreRC += ConsiderThresholdFactorialAdd;
				}
			}

			diagonalHitScoreRC++;
		}
		else
		{
			diagonalRepeatedScoreRC = 0;
			ConsiderThresholdFirstPastRC = false;

			//if (--iLeftRC < diagonalScoreHighestRC)
			//{
			//	diagonalFullyAnalysedRC = false;
			//	if (!diagonalFullyAnalysed)
			//		break;
			//}
		}
		//}

		ptCharInpRC = seqQuerysRC + seqStartingPointsQuerys[seqIndexQuerys];
		ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];


		/*	if (diagonalFullyAnalysed)
		{*/
		if (diagonalHitScore > diagonalScoreHighest)
		{
			diagonalScoreHighest = diagonalHitScore;
		}
		//}

		//if (diagonalFullyAnalysedRC)
		//{
		if (diagonalHitScoreRC > diagonalScoreHighestRC)
		{
			diagonalScoreHighestRC = diagonalHitScoreRC;
		}
		//}
	}


	ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;


	if (diagonalTotalConcidered <= 0 && diagonalTotalConcideredRC <= 0)
		Scores[aIndex] = -1;
	else if (diagonalTotalConcidered > 0 && diagonalTotalConcideredRC > 0)
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		if (scoreRC > score)
			Scores[aIndex] = scoreRC;
		else
			Scores[aIndex] = score;

	}
	else if (diagonalTotalConcidered > 0)
	{
		float score = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;

		Scores[aIndex] = score;

	}
	else
	{
		float scoreRC = (float)diagonalRepeatedTotalScoreRC / (float)diagonalTotalConcideredRC;

		Scores[aIndex] = scoreRC;

	}
}

///THIS ONE IS OK AND WORKING!
void kernel GetHighestMatrixScore_OKANDWORKING(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores)
{
	///
	/// This is the current working version
	///
	int seqIndexQuerys = get_global_id(0);
	int seqIndexAgainst = get_global_id(1);

	int lenQuery = seqLengthQuery[seqIndexQuerys];
	int lenAgainst = seqLengthsAgainst[seqIndexAgainst];


	///
	/// Private Buffer
	///

	/// todo: How to put dataAgainst in local memory, since all data will be accesed by every query sequence
	/// todo: 3000 should be calculated 
	char dataQuery[3000];
	char dataAgainst[3000];

	// Reset all private query and against data to ' '
	for (int i = 0; i < 3000; i++)
	{
		dataQuery[i] = ' ';
		dataAgainst[i] = ' ';
	}

	// Copy Query to private memory
	int startQuery = seqStartingPointsQuerys[seqIndexQuerys];
	int endQuery = startQuery + lenQuery;
	int cntQuery = 0;
	for (int i = startQuery; i < endQuery; i++)
		dataQuery[cntQuery++] = seqQuerys[i];

	// Copy Against to private memory
	int startAgainst = seqStartingPointsAgainst[seqIndexAgainst];
	int endAgainst = startAgainst + lenAgainst;
	int cntAgainst = 0;
	for (int i = startAgainst; i < endAgainst; i++)
		dataAgainst[cntAgainst++] = seqAgainst[i];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	char* ptCharInp = dataQuery;
	char* ptCharRef = dataAgainst;

	// Consider threshold
	const int ConsiderThreshold = 30;
	const int ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;

	// Scoring values
	int diagonalRepeatedScore = 0;
	int diagonalScoreHighest = 0;
	int diagonalRepeatedTotalScore = 0;
	int diagonalTotalConcidered = 0;
	int diagonalHitScore = 0;
	bool diagonalFullyAnalysed = true;

	// Pre-calculations, provide performance gain
	char* ptCharInpRowAdded;
	char* ptCharRefRowAdded;

	int inpLenMinusRow = 0;
	int refLenMinusRow = 0;
	int iStop = 0;
	int iLeft = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (int row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;

		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;

		for (int i = 0; i < iStop; i++)
		{
			ptCharInpRowAdded++;
			ptCharRef++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharRef = dataAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side
	ptCharInp = dataQuery;
	ptCharRef = dataAgainst;

	for (int row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;

		for (int i = 0; i < iStop; i++)
		{
			ptCharRefRowAdded++;
			ptCharInp++;
			if (*(ptCharRefRowAdded) == *(ptCharInp)) // 4% // PERFORAMNCE: SET THIS TO TRUE AND YOu'LL GO FROM 130 MINUTES TO 50 MINUTES (false: 30)
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold) // 18.9%
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore; // 2.8%

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++; // 2.2%
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				if (--iLeft < diagonalScoreHighest) //4.8%
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharInp = dataQuery;


		if (diagonalFullyAnalysed) // DISABLE THIS 130m -> 100m
		{
			if (diagonalHitScore > diagonalScoreHighest) // 8%
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	int aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	if (diagonalTotalConcidered <= 0)
		Scores[aIndex] = -1;
	else
		Scores[aIndex] = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;


}


///using [] instead of *
void kernel GetHighestMatrixScoreArray(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores)
{
	///
	/// This is the current working version
	///
	int seqIndexQuerys = get_global_id(0);
	int seqIndexAgainst = get_global_id(1);

	int lenQuery = seqLengthQuery[seqIndexQuerys];
	int lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Private Buffer
	///

	///// todo: How to put dataAgainst in local memory, since all data will be accesed by every query sequence
	///// todo: 3000 should be calculated 
	//char dataQuery[3000];
	//char dataAgainst[3000];

	//// Reset all private query and against data to ' '
	//for (int i = 0; i < 3000; i++)
	//{
	//	dataQuery[i] = ' ';
	//	dataAgainst[i] = ' ';
	//}

	// Copy Query to private memory
	int startQuery = seqStartingPointsQuerys[seqIndexQuerys];

	//int endQuery = startQuery + lenQuery;
	//int cntQuery = 0;
	//for (int i = startQuery; i < endQuery; i++)
	//	dataQuery[cntQuery++] = seqQuerys[i];

	// Copy Against to private memory
	int startAgainst = seqStartingPointsAgainst[seqIndexAgainst];
	//int endAgainst = startAgainst + lenAgainst;
	//int cntAgainst = 0;
	//for (int i = startAgainst; i < endAgainst; i++)
	//	dataAgainst[cntAgainst++] = seqAgainst[i];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	/*char* ptCharInp = dataQuery;
	char* ptCharRef = dataAgainst;*/
	int ptCharInp = startQuery;
	int ptCharRef = startAgainst;

	// Consider threshold
	const int ConsiderThreshold = 30;
	const int ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;

	// Scoring values
	int diagonalRepeatedScore = 0;
	int diagonalScoreHighest = 0;
	int diagonalRepeatedTotalScore = 0;
	int diagonalTotalConcidered = 0;
	int diagonalHitScore = 0;
	bool diagonalFullyAnalysed = true;

	// Pre-calculations, provide performance gain
	//char* ptCharInpRowAdded;
	//char* ptCharRefRowAdded;
	int ptCharInpRowAdded;
	int ptCharRefRowAdded;

	int inpLenMinusRow = 0;
	int refLenMinusRow = 0;
	int iStop = 0;
	int iLeft = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (int row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;

		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;

		for (int i = 0; i < iStop; i++)
		{
			ptCharInpRowAdded++;
			ptCharRef++;
			//if (*(ptCharInpRowAdded) == *(ptCharRef))
			if (seqQuerys[ptCharInpRowAdded] == seqAgainst[ptCharRef])
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		//ptCharRef = dataAgainst;
		ptCharRef = startAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side
	//ptCharInp = dataQuery;
	//ptCharRef = dataAgainst;
	ptCharInp = startQuery;
	ptCharRef = startAgainst;

	for (int row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;

		for (int i = 0; i < iStop; i++)
		{
			ptCharRefRowAdded++;
			ptCharInp++;
			//if (*(ptCharRefRowAdded) == *(ptCharInp)) // 4% // PERFORAMNCE: SET THIS TO TRUE AND YOu'LL GO FROM 130 MINUTES TO 50 MINUTES (false: 30)
			if (seqQuerys[ptCharInp] == seqAgainst[ptCharRefRowAdded])
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold) // 18.9%
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore; // 2.8%

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++; // 2.2%
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				if (--iLeft < diagonalScoreHighest) //4.8%
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		//ptCharInp = dataQuery;
		ptCharInp = startQuery;


		if (diagonalFullyAnalysed) // DISABLE THIS 130m -> 100m
		{
			if (diagonalHitScore > diagonalScoreHighest) // 8%
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	int aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	if (diagonalTotalConcidered <= 0)
		Scores[aIndex] = -1;
	else
		Scores[aIndex] = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
}

void kernel GetHighestMatrixScoreUsingInt(global const int* seqQuerys, global const int* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores)
{
	///
	/// This is the current working version
	///
	int seqIndexQuerys = get_global_id(0);
	int seqIndexAgainst = get_global_id(1);

	int lenQuery = seqLengthQuery[seqIndexQuerys];
	int lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Private Buffer
	///

	/// todo: How to put dataAgainst in local memory, since all data will be accesed by every query sequence
	/// todo: 3000 should be calculated 
	int dataQuery[3000];
	int dataAgainst[3000];

	// Reset all private query and against data to ' '
	for (int i = 0; i < 3000; i++)
	{
		dataQuery[i] = 0;
		dataAgainst[i] = 0;
	}

	// Copy Query to private memory
	int startQuery = seqStartingPointsQuerys[seqIndexQuerys];
	int endQuery = startQuery + lenQuery;
	int cntQuery = 0;
	for (int i = startQuery; i < endQuery; i++)
		dataQuery[cntQuery++] = seqQuerys[i];

	// Copy Against to private memory
	int startAgainst = seqStartingPointsAgainst[seqIndexAgainst];
	int endAgainst = startAgainst + lenAgainst;
	int cntAgainst = 0;
	for (int i = startAgainst; i < endAgainst; i++)
		dataAgainst[cntAgainst++] = seqAgainst[i];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	int* ptCharInp = dataQuery;
	int* ptCharRef = dataAgainst;

	// Consider threshold
	const int ConsiderThreshold = 30;
	const int ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;

	// Scoring values
	int diagonalScoreHighest = 0;
	int diagonalRepeatedTotalScore = 0;
	int diagonalTotalConcidered = 0;
	int diagonalHitScore = 0;
	int diagonalRepeatedScore = 0;
	bool diagonalFullyAnalysed = true;

	// Pre-calculations, provide performance gain
	int* ptCharInpRowAdded;
	int* ptCharRefRowAdded;
	int inpLenMinusRow = 0;
	int refLenMinusRow = 0;
	int iStop = 0;
	int iLeft = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (int row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;

		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;

		for (int i = 0; i < iStop; i++)
		{
			ptCharInpRowAdded++;
			ptCharRef++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharRef = dataAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side
	ptCharInp = dataQuery;
	ptCharRef = dataAgainst;

	for (int row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;

		for (int i = 0; i < iStop; i++)
		{
			ptCharRefRowAdded++;
			ptCharInp++;
			if (*(ptCharRefRowAdded) == *(ptCharInp)) // 4%
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold) // 18.9%
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore; // 2.8%

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++; // 2.2%
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				if (--iLeft < diagonalScoreHighest) //4.8%
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharInp = dataQuery;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest) // 8%
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	int aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	if (diagonalTotalConcidered <= 0)
		Scores[aIndex] = -1;
	else
		Scores[aIndex] = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
}

void kernel GetHighestMatrixScoreBooleanGate(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores)
{
	///
	/// This is the current working version
	///
	int seqIndexQuerys = get_global_id(0);
	int seqIndexAgainst = get_global_id(1);

	int lenQuery = seqLengthQuery[seqIndexQuerys];
	int lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Private Buffer
	///

	/// todo: How to put dataAgainst in local memory, since all data will be accesed by every query sequence
	/// todo: 3000 should be calculated 
	char dataQuery[3000];
	char dataAgainst[3000];

	// Reset all private query and against data to ' '
	for (int i = 0; i < 3000; i++)
	{
		dataQuery[i] = ' ';
		dataAgainst[i] = ' ';
	}

	// Copy Query to private memory
	int startQuery = seqStartingPointsQuerys[seqIndexQuerys];
	int endQuery = startQuery + lenQuery;
	int cntQuery = 0;
	for (int i = startQuery; i < endQuery; i++)
		dataQuery[cntQuery++] = seqQuerys[i];

	// Copy Against to private memory
	int startAgainst = seqStartingPointsAgainst[seqIndexAgainst];
	int endAgainst = startAgainst + lenAgainst;
	int cntAgainst = 0;
	for (int i = startAgainst; i < endAgainst; i++)
		dataAgainst[cntAgainst++] = seqAgainst[i];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	char* ptCharInp = dataQuery;
	char* ptCharRef = dataAgainst;

	// Consider threshold
	const int ConsiderThreshold = 30 + 1; // +1 if we use the 'boolean' threshold solution
	const int ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;

	// Scoring values
	int diagonalScoreHighest = 0;
	int diagonalRepeatedTotalScore = 0;
	int diagonalTotalConcidered = 0;
	int diagonalHitScore = 0;
	int diagonalRepeatedScore = 0;
	bool diagonalFullyAnalysed = true;

	// Pre-calculations, provide performance gain
	char* ptCharInpRowAdded;
	char* ptCharRefRowAdded;
	int inpLenMinusRow = 0;
	int refLenMinusRow = 0;
	int iStop = 0;
	int iLeft = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (int row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;

		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;

		for (int i = 0; i < iStop; i++)
		{
			ptCharInpRowAdded++;
			ptCharRef++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharRef = dataAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side
	ptCharInp = dataQuery;
	ptCharRef = dataAgainst;

	for (int row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		bool ConsiderThresholdReached = false;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		//ConsiderThresholdFirstPast = false;
		// before: 20 seconds

		for (int i = 0; i < iStop; i++)
		{
			ptCharRefRowAdded++;
			ptCharInp++;
			if (*(ptCharRefRowAdded) == *(ptCharInp)) // 4%
			{
				diagonalRepeatedScore++;

				if (!ConsiderThresholdReached) // Boolean threshold solution
				{
					if (diagonalRepeatedScore == 31)
					{
						ConsiderThresholdReached = true;

						diagonalTotalConcidered++;
						diagonalRepeatedTotalScore += diagonalRepeatedScore;

						diagonalTotalConcidered += 31;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}
				else
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

				}

				//if (diagonalRepeatedScore > ConsiderThreshold) // 18.9%
				//{
				//	diagonalTotalConcidered++;
				//	diagonalRepeatedTotalScore += diagonalRepeatedScore; // 2.8%

				//	if (!ConsiderThresholdFirstPast)
				//	{
				//		ConsiderThresholdFirstPast = true;
				//		diagonalTotalConcidered += ConsiderThreshold;
				//		diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
				//	}
				//}

				diagonalHitScore++; // 2.2%
			}
			else
			{
				diagonalRepeatedScore = 0;
				//ConsiderThresholdFirstPast = false;

				if (--iLeft < diagonalScoreHighest) //4.8%
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharInp = dataQuery;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest) // 8%
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	int aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	if (diagonalTotalConcidered <= 0)
		Scores[aIndex] = -1;
	else
		Scores[aIndex] = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
}

void kernel GetHighestMatrixScoreGlobalAccessNORC(global const char* seqQuerys, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global const int* seqStartingPointsQuerys, global const int* numAgainst, global float* Scores)
{
	///
	/// This is a version which has been changed to access global data directly, after 1st feedback from surfsara
	///

	ulong seqIndexQuerys = get_global_id(0);
	ulong seqIndexAgainst = get_global_id(1);

	ulong lenQuery = seqLengthQuery[seqIndexQuerys];
	ulong lenAgainst = seqLengthsAgainst[seqIndexAgainst];

	///
	/// Private Buffer
	///

	///// todo: How to put dataAgainst in local memory, since all data will be accesed by every query sequence
	///// todo: 3000 should be calculated 
	//char dataQuery[3000];
	//char dataAgainst[3000];
	//
	//// Reset all private query and against data to ' '
	//for (int i = 0; i < 3000; i++)
	//{
	//	dataQuery[i] = ' ';
	//	dataAgainst[i] = ' ';
	//}

	//// Copy Query to private memory
	//int startQuery = seqStartingPointsQuerys[seqIndexQuerys];
	//int endQuery = startQuery + lenQuery;
	//int cntQuery = 0;
	//for (int i = startQuery; i < endQuery; i++)
	//	dataQuery[cntQuery++] = seqQuerys[i];

	//// Copy Against to private memory
	//int startAgainst = seqStartingPointsAgainst[seqIndexAgainst];
	//int endAgainst = startAgainst + lenAgainst;
	//int cntAgainst = 0;
	//for (int i = startAgainst; i < endAgainst; i++)
	//	dataAgainst[cntAgainst++] = seqAgainst[i];

	///
	/// Initialise general data
	///

	// Keep track of sequences
	const global char* ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	//char* ptCharInp = dataQuery;
	const global char* ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];
	//char* ptCharRef = dataAgainst;

	// Consider threshold
	const int ConsiderThreshold = 30;
	const ulong ConsiderThresholdFactorialAdd = (ConsiderThreshold * (ConsiderThreshold + 1)) / 2;
	bool ConsiderThresholdFirstPast = false;

	// Scoring values
	ulong diagonalScoreHighest = 0;
	ulong diagonalRepeatedTotalScore = 0;
	ulong diagonalTotalConcidered = 0;
	ulong diagonalHitScore = 0;
	ulong diagonalRepeatedScore = 0;
	bool diagonalFullyAnalysed = true;

	// Pre-calculations, provide performance gain
	const global char* ptCharInpRowAdded;
	//char* ptCharInpRowAdded;
	const global char* ptCharRefRowAdded;
	//char* ptCharRefRowAdded;
	ulong inpLenMinusRow = 0;
	ulong refLenMinusRow = 0;
	ulong iStop = 0;
	ulong iLeft = 0;

	// Analyse Left side of matrix
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	for (ulong row = 0; row < lenQuery; row++)
	{
		ConsiderThresholdFirstPast = false;
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		// Pre-calculate values
		inpLenMinusRow = lenQuery - row;
		ptCharInpRowAdded = ptCharInp + row;

		// Determince end of loop
		iStop = (lenAgainst < inpLenMinusRow ? lenAgainst : inpLenMinusRow);
		iLeft = iStop;

		for (ulong i = 0; i < iStop; i++)
		{
			ptCharInpRowAdded++;
			ptCharRef++;
			if (*(ptCharInpRowAdded) == *(ptCharRef))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];
		//ptCharRef = dataAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side
	ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
	ptCharRef = seqAgainst + seqStartingPointsAgainst[seqIndexAgainst];
	//ptCharInp = dataQuery;
	//ptCharRef = dataAgainst;

	for (ulong row = 1; row < lenAgainst; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;
		refLenMinusRow = lenAgainst - row;
		iStop = (lenQuery < refLenMinusRow ? lenQuery : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;

		for (ulong i = 0; i < iStop; i++)
		{
			ptCharRefRowAdded++;
			ptCharInp++;
			if (*(ptCharRefRowAdded) == *(ptCharInp))
			{
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharInp = seqQuerys + seqStartingPointsQuerys[seqIndexQuerys];
		//ptCharInp = dataQuery;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	ulong aIndex = (seqIndexQuerys * numAgainst[0]) + seqIndexAgainst;
	if (diagonalTotalConcidered <= 0)
		Scores[aIndex] = -1;
	else
		Scores[aIndex] = (float)diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;
}

void kernel GetHighestMatrixScoreByOne(global const char* seqQuery, global const char* seqAgainst, global const int* seqLengthQuery, global const int* seqLengthsAgainst, global const int* seqStartingPointsAgainst, global float* Scores)
{

	int seqIndexAgainst = get_global_id(0);
	char dataQuery[3000];
	char dataAgainst[3000];


	int inpLen = seqLengthQuery[0];
	int refLen = seqLengthsAgainst[seqIndexAgainst];

	for (int i = 0; i < inpLen; i++)
		dataQuery[i] = seqQuery[i];

	for (int i = 0; i < refLen; i++)
		dataAgainst[i] = seqAgainst[seqStartingPointsAgainst[seqIndexAgainst] + i];



	char* ptCharInp = dataQuery;
	char* ptCharRef = dataAgainst;



	// Left side
	/*
	* 00, 11, 22
	* 10, 21, 32
	*/
	int ConsiderThreshold = 30;
	int diagonalScoreHighest = 0;
	int diagonalRepeatedTotalScore = 0;
	int diagonalTotalConcidered = 0;
	int ConsiderThresholdFactorialAdd = 0;
	//int ConsiderThresholdFirstAdd = 0;

	// todo:
	/*
	Sn = n(n + 1)/2
	in your case n = 100
	sum = 100(100 + 1)/2 = 50 × 101 = 5050
	*/
	for (int fact = 1; fact <= ConsiderThreshold; fact++)
		ConsiderThresholdFactorialAdd += fact;


	bool diagonalFullyAnalysed = true;
	int diagonalHitScore = 0;
	int diagonalRepeatedScore = 0;  // FLOAT!

	int inpLenMinusRow = 0;
	int refLenMinusRow = 0;
	int iStop = 0;
	int iLeft = 0;

	char* ptCharInpRowAdded;
	char* ptCharRefRowAdded;



	bool ConsiderThresholdFirstPast = false;

	for (int row = 0; row < inpLen; row++)
	{

		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;

		inpLenMinusRow = inpLen - row;
		iStop = (refLen < inpLenMinusRow ? refLen : inpLenMinusRow);
		iLeft = iStop;

		ptCharInpRowAdded = ptCharInp + row;
		ConsiderThresholdFirstPast = false;

		for (int i = 0; i < iStop; i++)
		{
			//float AddHight = 0;
			bool AcceptNucleotide = false;
			//if (diagonalRepeatedScoreIncreaseUsingAddHight)
			//{
			//	AddHight = GetAdditionHight(*(++ptCharInpRowAdded), *(++ptCharRef));
			//	if (AddHight != 1)
			//		AddHight *= AddHightMultiPly;

			//	AcceptNucleotide = AddHight != 0;
			//}
			//else
			ptCharInpRowAdded++;
			ptCharRef++;
			AcceptNucleotide = *(ptCharInpRowAdded) == *(ptCharRef);


			if (AcceptNucleotide)
			{
				//if (diagonalRepeatedScoreIncreaseUsingAddHight)
				//	diagonalRepeatedScore += 1.0 / AddHight;
				//else
				diagonalRepeatedScore++;

				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					//++diagonalRepeatedScore;
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;
				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharRef = dataAgainst;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}

	// Right side



	ptCharInp = dataQuery;
	ptCharRef = dataAgainst;

	for (int row = 1; row < refLen; row++)
	{
		diagonalFullyAnalysed = true;
		diagonalHitScore = 0;
		diagonalRepeatedScore = 0;
		refLenMinusRow = refLen - row;
		iStop = (inpLen < refLenMinusRow ? inpLen : refLenMinusRow);
		iLeft = iStop;

		ptCharRefRowAdded = ptCharRef + row;


		ConsiderThresholdFirstPast = false;

		for (int i = 0; i < iStop; i++)
		{

			//float AddHight = 0;
			bool AcceptNucleotide = false;
			//if (diagonalRepeatedScoreIncreaseUsingAddHight)
			//{
			//	AddHight = GetAdditionHight(*(++ptCharRefRowAdded), *(++ptCharInp));
			//	if (AddHight != 1)
			//		AddHight *= AddHightMultiPly;
			//	AcceptNucleotide = AddHight != 0;
			//}
			//else
			ptCharRefRowAdded++;
			ptCharInp++;
			AcceptNucleotide = *(ptCharRefRowAdded) == *(ptCharInp);

			if (AcceptNucleotide)
			{
				//if (diagonalRepeatedScoreIncreaseUsingAddHight)
				//	diagonalRepeatedScore += 1.0 / AddHight;
				//else
				diagonalRepeatedScore++;

				//if (++diagonalRepeatedScore > ConsiderThreshold)
				if (diagonalRepeatedScore > ConsiderThreshold)
				{
					//++diagonalRepeatedScore;
					diagonalTotalConcidered++;
					diagonalRepeatedTotalScore += diagonalRepeatedScore;

					//if (diagonalRepeatedScore == ConsiderThreshold + 1)
					if (!ConsiderThresholdFirstPast)
					{
						ConsiderThresholdFirstPast = true;
						diagonalTotalConcidered += ConsiderThreshold;
						diagonalRepeatedTotalScore += ConsiderThresholdFactorialAdd;
					}
				}

				diagonalHitScore++;
			}
			else
			{
				diagonalRepeatedScore = 0;
				ConsiderThresholdFirstPast = false;

				//iLeft--;

				if (--iLeft < diagonalScoreHighest)
				{
					diagonalFullyAnalysed = false;
					break;
				}
			}
		}

		ptCharInp = dataQuery;


		if (diagonalFullyAnalysed)
		{
			if (diagonalHitScore > diagonalScoreHighest)
				diagonalScoreHighest = diagonalHitScore;
		}
	}


	Scores[seqIndexAgainst] = diagonalRepeatedTotalScore / (float)diagonalTotalConcidered;


}