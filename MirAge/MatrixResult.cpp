#include "MatrixResult.h"

MatrixResult::MatrixResult()
{
}

///
/// Store scores
///
MatrixResult::MatrixResult(SEQ_ID_t queryID, float highestScore, vector<ResultsScores> scores) //, vector<char>* querySequence, vector<vector<char>>* againstSequences)
{
	QueryID = queryID;
	HighestScore = highestScore;
	Scores.swap(scores);

}

///
/// Retrieve relative score
///
float MatrixResult::GetRelativeScore(float querySize, float referenceSize, float score)
{
	const float lenMax = (float)(querySize < referenceSize ? querySize : referenceSize);
	const float scoreMax =( ((lenMax * (lenMax + 1.0)) / 2.0) / lenMax) + 1;
	
	score += 1; // To overcome 'Log(1) = 0' and Log(<1) = -x'

	return log(score) / log(scoreMax);// ((100.0 / log(scoreMax)) * log(score)) / 100.0;
}

MatrixResult::~MatrixResult()
{
	vector<ResultsScores>().swap(Scores);
}
