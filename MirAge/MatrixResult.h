#pragma once
#ifndef MATRIXRESULT_H
#define MATRIXRESULT_H
#include <vector>
#include <string.h>
#include <math.h>
#include <map>

#include <iostream>
//#include<functional> // For greater
#include "Enum.h"
#include <unordered_map>


using namespace std;


class MatrixResult
{

public:


	struct Results
	{
		vector<SEQ_ID_t> referenceIDs;
		vector<float> percentageMatch;
	};

	struct ResultsScores
	{
		float score;
		SEQ_ID_t referenceID;
		ResultsScores(float s, SEQ_ID_t r)
			: score(s)
			, referenceID(r)
		{}

	};

	struct Result
	{
		float Score;
		float percentageMatch;
	};


	size_t QueryID;
	float HighestScore;
	float HighestRelativeMatrixScore;
	vector<ResultsScores> Scores;




	static float GetRelativeScore(float querySize, float referenceSize, float score);


	MatrixResult();
	MatrixResult(SEQ_ID_t queryID, float highestScore, vector< ResultsScores> s);
	~MatrixResult();
};

#endif