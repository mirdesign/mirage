#include "Display.h"
#include <iomanip> // setw setfill
#include <sstream>  // stringstream

using namespace std;

int Display::ProgressLoading_ShowEvery_Second = 1;

Display::Display(LogToScreen *lts, Output_Modes *o)
{
	Output_Mode = o;
	logToScreen = lts;
	logWarningMessage.MuteScreen(); // Used as a buffer, so don't output anything to the screen
}

void Display::DisplayNewLine()
{
	(*logToScreen) << endl;
}

void Display::DisplayFlush()
{
	logToScreen->Flush();
}


void Display::DisplayTitle(string Title, bool AddNewLine)
{
	(*logToScreen) << setw(setwTitles) << Title << setw(0);

	if (AddNewLine)
		DisplayNewLine();
}
void Display::DisplayValue(string Value, bool AddNewLine)
{
	(*logToScreen) << Value;
	if (AddNewLine)
		DisplayNewLine();
}

void Display::DisplayValue(char Value, bool AddNewLine)
{
	(*logToScreen) << Value;
	if (AddNewLine)
		DisplayNewLine();
}

void Display::DisplayFooter()
{
	(*logToScreen) << right << setw(setwHeaders + 1) << setfill('_') << " " << setw(0) << setfill(' ') << left << endl;
	(*logToScreen) << endl;
}
void Display::DisplayHeader(string Header, bool StartWithFooter)
{
	if (StartWithFooter)
		DisplayFooter();

	int length = setwHeaders;
	length -= Header.size();
	length /= 2;

	int CompensateForUnevenLengths = setwHeaders - (length * 2) - Header.size(); // If we have an uneven length, compensate for it

	(*logToScreen) << right << setw(length) << setfill('=') << " " << setw(0);
	(*logToScreen) << Header;
	(*logToScreen) << left << setw(length + CompensateForUnevenLengths) << setfill('=') << " " << setw(0) << setfill(' ') << endl;


}
void Display::DisplayTitleAndValue(string Title, string Value, bool AddNewLine)
{
	DisplayTitle(Title);
	DisplayValue(Value, AddNewLine);
}
void Display::DisplayTitleAndValue(string Title, int Value, bool AddNewLine)
{
	DisplayTitleAndValue(Title, to_string(Value), AddNewLine);
}

void Display::DisplayAndEndProgram(string Message)
{
	DisplayHeader("Program terminated");
	DisplayTitle(Message, true);
	DisplayFooter();
	exit(EXIT_SUCCESS);
}


///
/// Show start information
///
void Display::DisplaySplashScreen_Start()
{
	DisplayHeader("MirAge v0.10 (c)", false);
	DisplayTitleAndValue("Created by", "Maarten Pater (AMC, January 2015-2025)", true);
	DisplayTitleAndValue("Contact", "mirage@mirdesign.nl | www.mirdesign.nl", true);
	DisplayTitleAndValue("Manual", "www.mirdesign.nl/mirage", true);
}

void Display::DisplaySplashScreen_End(size_t startTime, size_t startTimeAnalyse, string logFilePath, bool divideSequenceReadBetweenNodes, int Node_Current_Index)
{
	// Get end time
	const time_t endTime = time(NULL);
	time_t end_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	DisplayNewLine();

	if (divideSequenceReadBetweenNodes)
		DisplayHeader("End (Node: " + to_string(Node_Current_Index) + ")");
	else
		DisplayHeader("End");

	DisplayTitleAndValue("Analysis took:", GetTimeMessage(endTime - startTimeAnalyse));
	DisplayTitleAndValue("Program took:", GetTimeMessage(endTime - startTime));
	DisplayTitleAndValue("Current time:", ctime(&end_time), false);
	DisplayTitleAndValue("Logfile:", logFilePath);
	DisplayFooter();
}

void Display::DisplayGeneralSettings(Files* files, LogManager* logManager, Sequences* sequences, MultiNodePlatform* multiNodePlatform, int32_t ConsiderThreshold)
{
	DisplayHeader("Settings");

	int referenceMHMFiles = 0;
	int referenceFastaFiles = 0;

	for (int i = 0; i < files->referenceFilePath.size(); i++)
	{
		string multipleHashmapCountMessage = "";

		if (files->referenceFilePath.size() > 1)
			multipleHashmapCountMessage = " (" + to_string(i + 1) + "/" + to_string(files->referenceFilePath.size()) + ")";

		bool isReferenceHashMapDatabase = Hashmap::IsReferenceHashMapDatabase(&(files->referenceFilePath[i])[0]);
		if (isReferenceHashMapDatabase)
		{
			referenceMHMFiles++;
			DisplayTitleAndValue("Reference file (Hashmap)" + multipleHashmapCountMessage + ":", files->referenceFilePath[i]);
		}
		else
		{
			referenceFastaFiles++;
			DisplayTitleAndValue("Reference file" + multipleHashmapCountMessage + ":", files->referenceFilePath[i]);
		}
	}

	if (referenceFastaFiles > 1)
	{
		logWarningMessage << "Multiple reference files provided in fasta format!" << endl;
		logWarningMessage << "This is not supported right now. Only multiple hashmap databases is supported." << endl;
		logWarningMessage << "Program will stop." << endl;
		FlushWarning(true);
	}
	else if (referenceMHMFiles > 1 && referenceFastaFiles > 1)
	{
		logWarningMessage << "Multiple reference files provided in fasta and hashmap database format!" << endl;
		logWarningMessage << "This is not supported right now. Only multiple hashmap databases is supported." << endl;
		logWarningMessage << "Program will stop." << endl;
		FlushWarning(true);
	}

	DisplayTitleAndValue("Query file:", files->queryFilePath);
	//logToScreen << "Output OTU Representives file:     " << logOTUExport.GetLogFilePath() << endl;
	DisplayTitleAndValue("Log file:", logManager->GetLogFilePath());
#ifdef __linux__
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	DisplayTitleAndValue("Hostname:", hostname);
#endif
	DisplayTitleAndValue("Supported bits:", sizeof(HASH_t) * 8);

	// Boost mode
	stringstream boostMessage;
	if (Hashmap::Hashmap_Performance_Boost == 0)
		boostMessage << "No";
	else
	{
		boostMessage << "Yes (x";
		boostMessage << Hashmap::Hashmap_Performance_Boost + 1;
		boostMessage << ")";
	}
	DisplayTitleAndValue("Boost mode active:", boostMessage.str());


	// Speed mode
	string SpeedMode = "";
	switch (Analyser::SpeedMode)
	{
	case Enum::Analysis_Modes::Sensitive:
		SpeedMode = "Sensitive";
		break;

	case Enum::Analysis_Modes::Fast:
		SpeedMode = "Fast";
		break;

	case Enum::Analysis_Modes::VeryFast:
		SpeedMode = "VeryFast";
		break;

	case Enum::Analysis_Modes::UltraFast:
		SpeedMode = "UltraFast";
		break;
	}
	DisplayTitleAndValue("Analysis mode:", SpeedMode);

	// Memory usage mode
	string MemUsageMode = "";
	switch (Hashmap::Hashmap_Load_Reference_Memory_UsageMode)
	{
	case Enum::Hashmap_MemoryUsageMode::Low:
		MemUsageMode = "Low (Slow)";
		break;
	case Enum::Hashmap_MemoryUsageMode::Medium:
		MemUsageMode = "Medium";
		break;
	case Enum::Hashmap_MemoryUsageMode::High:
		MemUsageMode = "High (Fast)";
		break;
	case Enum::Hashmap_MemoryUsageMode::Extreme:
		MemUsageMode = "Extreme (Very fast)";
		break;

	}



	DisplayTitleAndValue("Memory usage mode:", MemUsageMode);



	// Threshold
	DisplayTitleAndValue("ConsiderThreshold:", ConsiderThreshold);
	DisplayTitleAndValue("NCBI Accession2TaxID file:", (sequences->NCBIAccession2TaxID_Used ? files->NCBIAccession2TaxIDFile : "[none provided]"));
	DisplayTitleAndValue("Multi-platform mode:", (multiNodePlatform->divideSequenceReadBetweenNodes ? "Yes" : "No"), false);
	if (multiNodePlatform->divideSequenceReadBetweenNodes)
	{
		DisplayNewLine();
		DisplayTitle("\\", true);
		DisplayTitleAndValue(" |- Total nodes:", multiNodePlatform->Node_Total);
		DisplayTitleAndValue(" |- Current node index:", multiNodePlatform->Node_Current_Index);
		DisplayTitleAndValue(" |- Observal mode:", (multiNodePlatform->combineResult_AllNodes ? "yes" : "no"));

		if (multiNodePlatform->combineResult_AllNodes)
		{
			DisplayTitleAndValue(" |- Current node observer:", (multiNodePlatform->Node_Current_Index == 0 ? "yes" : "no"));

			if (multiNodePlatform->Node_Current_Index == 0)
			{
				DisplayTitleAndValue(" |- % Overview log file:", multiNodePlatform->GetLogFilePath(multiNodePlatform->LOG_FILE_APPEND_PATH_PERCENTAGE_OVERVIEW));
				DisplayTitleAndValue(" \\- Final log file:", multiNodePlatform->GetLogFilePath(multiNodePlatform->LOG_FILE_APPEND_PATH_FINAL));
			}
		}
	}
	else
	{
		DisplayNewLine();
	}

	time_t start_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	DisplayTitleAndValue("Current time:", ctime(&start_time));
	DisplayTitleAndValue("Show file load progress:", (this->ShowFileLoadingTimeEstimates ? "yes" : "no"));
}

/// Erase the last char which has been writen on the screen
void Display::EraseChars(int NumberOfChars)
{
	for (int i = 0; i < NumberOfChars; i++)
	{
		DisplayValue('\b');
		DisplayValue(' ');
		DisplayValue('\b');
	}
}

void Display::FlushWarning(bool ExitProgramAfterFlush)
{
	logToScreen->Enabled = true; // Always show warning messages
	DisplayNewLine();
	DisplayHeader("!!! WARNING !!!");
	DisplayTitle(logWarningMessage.FlushBuffer());
	DisplayFooter();


	if (ExitProgramAfterFlush)
		_Exit(EXIT_FAILURE);
}

int Display::GetEndAndStartBlockSize()
{
	int retVal = setwProgress_Tiny +
		+setwProgress_Tiny
		+ setwProgress_Tiny
		+ setwProgress_Medium
		+ setwProgress_Huge
		+ setwProgress_Big
		+ setwProgress_Big
		+ setwProgress_Big
		+ setwProgress_Tiny // free mem
		+ 13 // for "| "
		- 2  // for the pipes outside the underscores
		- 2;
	return retVal;
}
void Display::ShowEndBlock()
{
	// Only show end block during default mode
	if (*Output_Mode != Output_Modes::Default)
		return;

	int endBlockFillSize = setwProgress_Tiny
		+ setwProgress_Tiny
		+ setwProgress_Tiny
		+ setwProgress_Medium
		+ setwProgress_Huge
		+ setwProgress_Big
		+ setwProgress_Big
		+ setwProgress_Big
		+ 12 // for "| "
		- 2  // for the pipes outside the underscores
		- 2;

	(*logToScreen) << setfill('_') << setw(GetEndAndStartBlockSize()) << "|" << setw(setwProgress_None) << "|" << setfill(' ') << endl;
}
void Display::ShowStartBlock()
{
	(*logToScreen) << endl << " " << right << setfill('_') << setw(GetEndAndStartBlockSize()) << " " << setw(setwProgress_None) << " " << setfill(' ') << left << endl;
}

void Display::ShowProgressHeading(bool showEndBlock)
{
	// Only show headers during advanced mode
	if (*Output_Mode != Output_Modes::Default)
	{
		(*logToScreen) << endl;
		return;
	}
	if (showEndBlock)
		ShowEndBlock();



	ShowStartBlock();

	(*logToScreen) << left << setw(setwProgress_None) << "| "
		<< setw(setwProgress_Tiny) << "Node"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Tiny) << "Device"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Medium) << "Queued"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Huge) << "Processed"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << "Time spent"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << "Time remaining"
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << "Time estimated"
		<< setw(setwProgress_None) << "|"
		<< setw(setwProgress_Tiny) << "Memory"
		<< setw(setwProgress_None) << "|"
		<< endl;
}
///
/// Show progress in advanced mather
///
void Display::ShowProgress_Advanced(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, int hardwareID, string hardwareTypeName, size_t spentSecondsFromInitialising, size_t spentMinutesFromInitialising, size_t spentHoursFromInitialising, int node_Current_Index, size_t remainingSeconds, size_t remainingMinutes, size_t remainingHours, size_t estimateSeconds, size_t estimateMinutes, size_t estimateHours, bool blocksizeStabilised, int percentageFreeMem)
{
	bool timeProvided = !(estimateHours == 0 && estimateMinutes == 0 && estimateSeconds == 0);
	int percentage = 100;

	if (progress_Lines_Shown_After_Heading == 40)
		progress_Lines_Shown_After_Heading = 0;

	if (progress_Lines_Shown_After_Heading == 0)
		ShowProgressHeading(true);

	if (totalSequencesToAnalyse != 0)
		percentage = (100.0 / totalSequencesToAnalyse) * totalSequencedAnalysed;

	stringstream msgOverview;
	msgOverview << totalSequencedAnalysed
		<< " / "
		<< totalSequencesToAnalyse
		<< " ("
		<< percentage
		<< "%)";



	string snode_Current_Index = "";
	if (node_Current_Index == -1)
		snode_Current_Index = "-";
	else
		snode_Current_Index = to_string(node_Current_Index);

	stringstream msgSpent;

	if (spentSecondsFromInitialising == 0 && spentMinutesFromInitialising == 0 && spentHoursFromInitialising == 0)
	{
		msgSpent << "-";
	}
	else
	{
		msgSpent << spentHoursFromInitialising
			<< "h "
			<< spentMinutesFromInitialising
			<< "m "
			<< spentSecondsFromInitialising
			<< "s";
	}

	string sQueriesAnalysedCurrentRun = "";
	if (queriesAnalysedCurrentRun == 0)
		sQueriesAnalysedCurrentRun = "-";
	else
		sQueriesAnalysedCurrentRun = to_string(queriesAnalysedCurrentRun);

	string sEstimateTime = "";
	string sRemainingTime = "";
	if (!timeProvided)
	{
		if (hardwareID == -1)
		{
			sRemainingTime = "-";
			sEstimateTime = "-";
		}
		else
		{
			sRemainingTime = "- *";
			sEstimateTime = "- *";
			sQueriesAnalysedCurrentRun += " *";
		}
	}
	else
	{
		string sInTraining = "";
		if (!blocksizeStabilised)
		{
			sQueriesAnalysedCurrentRun += " *";
			sInTraining = " *";
		}

		stringstream est;
		est << estimateHours
			<< "h "
			<< estimateMinutes
			<< "m "
			<< estimateSeconds
			<< "s"
			<< sInTraining;


		sEstimateTime = est.str();


		stringstream rem;
		rem << remainingHours
			<< "h "
			<< remainingMinutes
			<< "m "
			<< remainingSeconds
			<< "s"
			<< sInTraining;


		sRemainingTime = rem.str();




	}

	//if (hardwareID != -1)

	string sPercentageFreeMem = "";
	sPercentageFreeMem += to_string(percentageFreeMem);
	sPercentageFreeMem += "%";

	(*logToScreen) << left << setw(setwProgress_None) << "| "
		<< setw(setwProgress_Tiny) << snode_Current_Index
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Tiny) << ((hardwareID != -1 ? to_string(hardwareID) : "-") + " " + hardwareTypeName)
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Medium) << sQueriesAnalysedCurrentRun
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Huge) << msgOverview.str()
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << msgSpent.str()
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << sRemainingTime
		<< setw(setwProgress_None) << "| "
		<< setw(setwProgress_Big) << sEstimateTime
		<< setw(setwProgress_None) << "|"
		<< setw(setwProgress_Tiny) << sPercentageFreeMem
		<< setw(setwProgress_None) << "| "
		<< endl;

	progress_Lines_Shown_After_Heading++;


}


///
/// Show progress in OneLiner mather
/// [Node:1] 1m 15s[******] 3422 / 60000 (x%)(-3h 43m 2s / 5h 12m 33s)
///
void Display::ShowProgress_OneLiner(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, int hardwareID, string hardwareTypeName, size_t spentSecondsFromInitialising, size_t spentMinutesFromInitialising, size_t spentHoursFromInitialising, int node_Current_Index, size_t remainingSeconds, size_t remainingMinutes, size_t remainingHours, size_t estimateSeconds, size_t estimateMinutes, size_t estimateHours, bool blocksizeStabilised, int percentageFreeMem)
{

	for (int i = 0; i < previousProgressLength; i++)
	{
		(*logToScreen) << '\b'
			<< ' '
			<< '\b';
	}



	// Node
	string snode_Current_Index = "";
	if (node_Current_Index != -1)
		snode_Current_Index = "[Node: " + to_string(node_Current_Index) + "]";


	// Time spent
	stringstream msgSpent;
	if (spentSecondsFromInitialising == 0 && spentMinutesFromInitialising == 0 && spentHoursFromInitialising == 0)
	{
		msgSpent << "-";
	}
	else
	{
		msgSpent << spentHoursFromInitialising
			<< "h "
			<< spentMinutesFromInitialising
			<< "m "
			<< spentSecondsFromInitialising
			<< "s";
	}


	int percentage = 100;
	if (totalSequencesToAnalyse != 0)
		percentage = (100.0 / totalSequencesToAnalyse) * totalSequencedAnalysed;

	stringstream progressBar;
	string progressBlocks(percentage - 1, '=');
	//string progressBlocks(percentage, 219);
	string progressBlocks_EmptySpace(100 - percentage, ' ');
	//string progressBlocks_EmptySpace(100 - percentage, '-');


	progressBar << "[" << progressBlocks << progressBlocks_EmptySpace << "]";


	// Progress
	stringstream progressNumbers;
	progressNumbers << totalSequencedAnalysed
		<< " / "
		<< totalSequencesToAnalyse;


	stringstream timings;
	string sRemainingTime = "";
	string sEstimateTime = "-";
	bool timeProvided = !(remainingSeconds == 0 && remainingMinutes == 0 && remainingHours == 0);
	if (!timeProvided)
	{
		if (hardwareID != -1)
			//{
			//	sRemainingTime = "-";
			//}
			//else
		{
			sRemainingTime = "?";
		}
	}
	else
	{
		string sInTraining = "";
		if (!blocksizeStabilised)
		{
			sInTraining = " *";
		}

		stringstream est;
		est << estimateHours
			<< "h "
			<< estimateMinutes
			<< "m "
			<< estimateSeconds
			<< "s"
			<< sInTraining;


		sEstimateTime = est.str();


		stringstream rem;
		rem << remainingHours
			<< "h "
			<< remainingMinutes
			<< "m "
			<< remainingSeconds
			<< "s"
			<< sInTraining;


		sRemainingTime = rem.str();

		timings << " ("
			<< msgSpent.str()
			<< " / "
			<< sEstimateTime
			<< ") -"
			<< sRemainingTime;
	}

	//if (hardwareID != -1)


	/// [Node:1] 1m 15s[******] 3422 / 60000 (x%)(-3h 43m 2s / 5h 12m 33s)
	//  snode_Current_Index     msgSpent     progressNumbers  ... /  sRemainingTime
	// -queriesAnalysedCurrentRun

	stringstream ss;
	ss << left
		<< snode_Current_Index
		<< percentage
		<< "%"
		<< " "
		<< progressBar.str()
		<< " "
		<< progressNumbers.str()
		<< timings.str()
		<< " "
		<< percentageFreeMem
		<< "% free mem"
		<< " "
		;

	(*logToScreen) << ss.str();
	previousProgressLength = ss.str().length();

}



void Display::ShowLoadingProgress(size_t currentPosition, size_t TotalPositions, int *previousProgressMessageLength, size_t startTimeLoading, size_t *lastTimeDisplay)
{

	if (ShowFileLoadingTimeEstimates && (time(NULL) - *lastTimeDisplay > Display::ProgressLoading_ShowEvery_Second))
	{
		string progressMessage = "";

		progressMessage += to_string((int)((((float)currentPosition / (float)TotalPositions)) * 100.0));
		progressMessage += "% ";

		if (*previousProgressMessageLength != -1)
		{
			EraseChars(*previousProgressMessageLength);
			progressMessage += GetReadyInTimeMessage(startTimeLoading, TotalPositions, currentPosition);
		}
		DisplayValue(progressMessage);
		*previousProgressMessageLength = progressMessage.length();
		*lastTimeDisplay = time(NULL);
	}
}

///
/// Show progress in Advanced or OneLiner mather
///
void Display::ShowProgress(size_t queriesAnalysedCurrentRun, size_t totalSequencedAnalysed, size_t totalSequencesToAnalyse, size_t duplicateSequences, int hardwareID, string hardwareTypeName, int node_Current_Index, size_t timeStartProgram, int percentageFreeMem, size_t startTimeAnalyse, bool blocksizeStabilised)
{

	size_t estimateSeconds = 0;
	size_t estimateMinutes = 0;
	size_t estimateHours = 0;

	size_t remainingSeconds = 0;
	size_t remainingMinutes = 0;
	size_t remainingHours = 0;


	size_t spentSecondsFromInitialising = time(NULL) - timeStartProgram;
	size_t spentMinutesFromInitialising = spentSecondsFromInitialising / 60;
	size_t spentHoursFromInitialising = spentMinutesFromInitialising / 60;

	spentMinutesFromInitialising -= spentHoursFromInitialising * 60;
	spentSecondsFromInitialising -= 60 * spentHoursFromInitialising * 60;
	spentSecondsFromInitialising -= spentMinutesFromInitialising * 60;


	if (startTimeAnalyse != 0)
	{
		size_t spentSecondsInitialising = startTimeAnalyse - timeStartProgram;

		// Spent time
		size_t spentSecondsAnalysing = time(NULL) - startTimeAnalyse;
		size_t spentMinutesAnalysing = spentSecondsAnalysing / 60;
		size_t spentHoursAnalysing = spentMinutesAnalysing / 60;


		// Estimation of time
		double fEstimatedSeconds = 0;
		if (spentSecondsAnalysing > 0)
		{
			double timePerSequence = double(totalSequencedAnalysed) / double(spentSecondsAnalysing);
			if (timePerSequence > 0)
				fEstimatedSeconds = (double(totalSequencesToAnalyse - duplicateSequences) / timePerSequence) + spentSecondsInitialising;
		}

		double fRemainingSeconds = fEstimatedSeconds - spentSecondsAnalysing;// -spentSecondsInitialising;

		// Estimate time
		estimateSeconds = fEstimatedSeconds;
		estimateMinutes = fEstimatedSeconds / 60.0;
		estimateHours = estimateMinutes / 60.0;


		// Remaining time
		remainingMinutes = fRemainingSeconds / 60.0;
		remainingHours = remainingMinutes / 60.0;
		remainingSeconds = fRemainingSeconds;

		estimateMinutes -= estimateHours * 60;
		estimateSeconds -= 60 * estimateHours * 60;
		estimateSeconds -= estimateMinutes * 60;

		remainingMinutes -= remainingHours * 60;
		remainingSeconds -= 60 * remainingHours * 60;
		remainingSeconds -= remainingMinutes * 60;
	}

	if (*Output_Mode == Output_Modes::Default)
		ShowProgress_Advanced(queriesAnalysedCurrentRun, totalSequencedAnalysed, totalSequencesToAnalyse - duplicateSequences, hardwareID, hardwareTypeName, spentSecondsFromInitialising, spentMinutesFromInitialising, spentHoursFromInitialising, node_Current_Index, remainingSeconds, remainingMinutes, remainingHours, estimateSeconds, estimateMinutes, estimateHours, blocksizeStabilised, percentageFreeMem);
	else if (*Output_Mode == Output_Modes::ProgressBar)
		ShowProgress_OneLiner(queriesAnalysedCurrentRun, totalSequencedAnalysed, totalSequencesToAnalyse - duplicateSequences, hardwareID, hardwareTypeName, spentSecondsFromInitialising, spentMinutesFromInitialising, spentHoursFromInitialising, node_Current_Index, remainingSeconds, remainingMinutes, remainingHours, estimateSeconds, estimateMinutes, estimateHours, blocksizeStabilised, percentageFreeMem);

}


double Display::GetEstimatedSeconds(size_t startTime, size_t totalSize, size_t currentSize)
{
	// Estimation of time
	double fEstimatedSeconds = 0;
	size_t spentSeconds = time(NULL) - startTime;

	if (spentSeconds > 0)
	{
		double timePerSequence = double(currentSize) / double(spentSeconds);
		if (timePerSequence > 0)
			fEstimatedSeconds = (double(totalSize) / timePerSequence);
	}
	return fEstimatedSeconds;
}

string Display::GetEstimatedTimeMessage(size_t startTime, size_t totalSize, size_t currentSize)
{
	string retVal = "";

	// Estimation of time
	double fEstimatedSeconds = GetEstimatedSeconds(startTime, totalSize, currentSize);

	if (fEstimatedSeconds > 0)
	{
		size_t estimatedMinutes = fEstimatedSeconds / 60.0;
		size_t estimatedHours = estimatedMinutes / 60.0;
		size_t estimatedSeconds = fEstimatedSeconds;

		estimatedMinutes -= estimatedHours * 60;
		fEstimatedSeconds -= 60 * estimatedHours * 60;
		fEstimatedSeconds -= estimatedMinutes * 60;

		retVal = " (time estimated: ~" + to_string(fEstimatedSeconds) + "s " + to_string(estimatedMinutes) + "m " + to_string(estimatedHours) + "h) ";
	}

	return retVal;
}

#include <stdio.h>
string Display::GetTimeMessage(size_t elapsedTime)
{
	string retVal = "?h ?m ?s";

	if (elapsedTime >= 0)
	{
		size_t readyinMinutes = elapsedTime / 60.0;
		size_t readyinHours = readyinMinutes / 60.0;
		size_t readyinSeconds = elapsedTime;

		readyinMinutes -= readyinHours * 60;
		readyinSeconds -= 60 * readyinHours * 60;
		readyinSeconds -= readyinMinutes * 60;

		string sHours = (readyinHours != 0 ? to_string(readyinHours) + "h " : "");
		string sMinutes = (readyinMinutes != 0 ? to_string(readyinMinutes) + "m " : "");
		string sSeconds = (readyinSeconds != 0 ? to_string(readyinSeconds) + "s" : "0s");


		retVal = sHours + sMinutes + sSeconds;
	}

	return retVal;
}
string Display::GetReadyInTimeMessage(size_t startTime, size_t totalSize, size_t currentSize)
{
	string retVal = "";

	// Estimation of time
	double fEstimatedSeconds = GetEstimatedSeconds(startTime, totalSize, currentSize);
	size_t spentSeconds = time(NULL) - startTime;
	if (fEstimatedSeconds > 0)
		retVal = " (time left: ~" + GetTimeMessage(fEstimatedSeconds - spentSeconds) + ") ";

	return retVal;
}
