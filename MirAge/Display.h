#pragma once
#ifndef DISPLAY_H
#define DISPLAY_H
#include "Enum.h"
#include <string>
#include <vector>
#include "LogToScreen.h"
#include "LogManager.h"
#include "Files.h"
#include "MultiNodePlatform.h"
#include "Sequences.h"
#include "Hashmap.h"
#include "Analyser.h"
#include "Configuration.h"


class MultiNodePlatform;
class Configuration;

typedef   unordered_map<HASH_STREAMPOS, Enum::HeadTailSequence> hashmap_HeadTailBuffer;
class Display
{
private:
	Configuration *configuration;
	LogToScreen *logToScreen;
	int previousProgressLength = 0;
	int setwProgress_None = 0;
	int setwProgress_Small = 2;
	int setwProgress_Tiny = 6;
	int setwProgress_Medium = 10;
	int setwProgress_Big = 15;
	int setwProgress_Huge = 28;
	int setwSplashTitle = 30;
	int setwTitles = 27;
	int setwHeaders = 46;
	int progress_Lines_Shown_After_Heading = 1; // 1, so we can start showing the header outside the showprogress method
	int GetEndAndStartBlockSize();
	void ShowProgress_Advanced(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, int hardwareID, string hardwareTypeName, size_t spentSecondsFromInitialising, size_t spentMinutesFromInitialising, size_t spentHoursFromInitialising, int node_Current_Index, size_t remainingSeconds, size_t remainingMinutes, size_t remainingHours, size_t estimateSeconds, size_t estimateMinutes, size_t estimateHours, bool blocksizeStabilised, int percentageFreeMem);
	void ShowProgress_OneLiner(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, int hardwareID, string hardwareTypeName, size_t spentSecondsFromInitialising, size_t spentMinutesFromInitialising, size_t spentHoursFromInitialising, int node_Current_Index, size_t remainingSeconds, size_t remainingMinutes, size_t remainingHours, size_t estimateSeconds, size_t estimateMinutes, size_t estimateHours, bool blocksizeStabilised, int percentageFreeMem);



public:
	static int ProgressLoading_ShowEvery_Second; // Used for percentage progress, show progress every X seconds

	Output_Modes* Output_Mode;

	bool ShowFileLoadingTimeEstimates = true;

	LogToScreen logWarningMessage;

	void DisplayNewLine();
	void DisplayFlush();
	void DisplayTitle(string Title, bool AddNewLine = false);
	void DisplayValue(string Value, bool AddNewLine = false);
	void DisplayValue(char Value, bool AddNewLine = false);
	void DisplayFooter();
	void DisplayHeader(string Header, bool StartWithFooter = true);
	void DisplayTitleAndValue(string Title, string Value, bool AddNewLine = true);
	void DisplayTitleAndValue(string Title, int Value, bool AddNewLine = true);
	void DisplayAndEndProgram(string Message);
	void DisplaySplashScreen_Start();
	void DisplaySplashScreen_End(size_t startTime, size_t startTimeAnalyse, string logFilePath, bool divideSequenceReadBetweenNodes, int Node_Current_Index);
	void DisplayGeneralSettings(Files* files, LogManager* logManager, Sequences* sequences, MultiNodePlatform* multiNodePlatform, int32_t ConsiderThreshold);
	void EraseChars(int NumberOfChars);
	void FlushWarning(bool ExitProgramAfterFlush = false);

	void ShowEndBlock();
	void ShowStartBlock();

	void ShowProgressHeading(bool showEndBlock);
	void ShowLoadingProgress(size_t currentPosition, size_t TotalPositions, int *previousProgressMessageLength, size_t startTimeLoading, size_t *lastTimeDisplay);
	void ShowProgress(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, size_t duplicateSequences, int hardwareID, string hardwareTypeName, int node_Current_Index, size_t timeStartProgram, int percentageFreeMem, size_t startTimeAnalyse = 0, bool blocksizeStabilised = false);
	//void ShowProgress(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t toalSequencesToAnalyse, int hardwareID, string hardwareTypeName, size_t spentSecondsFromInitialising, size_t spentMinutesFromInitialising, size_t spentHoursFromInitialising, int node_Current_Index, size_t estimatedSeconds = 0, size_t estimatedMinutes = 0, size_t estimatedHours = 0, bool blocksizeStabilised = false);


	double GetEstimatedSeconds(size_t startTime, size_t totalSize, size_t currentSize);
	string GetEstimatedTimeMessage(size_t startTime, size_t totalSize, size_t currentSize);
	string GetTimeMessage(size_t elapsedTime);
	string GetReadyInTimeMessage(size_t startTime, size_t totalSize, size_t currentSize);

	Display(LogToScreen *lts, Output_Modes *o);
};
#endif