/*
*	MirAge - Matrix Aligner - Identifier
*	Created by: Maarten Pater (AMC) (www.MirDesign.nl) (maarten@mirage.nl)
*	Published on: 2025
*/
using namespace std;


#include <string>
#include <algorithm>
#include <ctime>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <ostream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <map>


/**
* MirAge Classes
**/
#include "Sequences.h" // Sequences management
#include "LogToScreen.h" // Show messages to screen
#include "Display.h" // Messages administration
#include "LogManager.h" // Log messages to files
#include "Configuration.h" // Consume configuration file
#include "MultiNodePlatform.h" // Support for super computer clusters
#include "Files.h" // File management
#include "Hardware.h" // Hardware management (OpenCL/CUDA)
#include "MultiThreading.h" // Multithreading management
#include "Hashmap.h" // Hashmap for speed optimisation (pre-analysis)
#include "SignalHandler.h" // Handles signals from user and system
#include "Screen.h" // Used to configure screensettings
#include "Analyser.h" // Contains all analysis logics


/**
* Debug
**/

//#include <vld.h> // Used for memory leak detection


#ifdef __linux__
#include <sys/time.h>
#include <execinfo.h> // exit signal handling
//struct sysinfo memInfo;
#else
// Memoryleak detection
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>
#endif

#ifdef __CUDA__
//#include <cuda_profiler_api.h> 
//#include <cuda_runtime_api.h>
#include <cuda.h>
#endif



/********************
*
* Output
*
*********************/
string Output_Path; // Path to output folder

/********************
*
* Classes
*
*********************/
MultiThreading multiThreading;
LogToScreen lts;
LogToScreen logResultsToScreen;
LogManager logFileManagerHighSensitivity;
LogManager logFileManagerHighPrecision;
LogManager logFileManagerBalanced;
LogManager logFileManagerRaw;
Screen screen;

Configuration configuration;
Display display(&lts, &configuration.Output_Mode);
Files files(&display);

Hardware hardware(&display, &multiThreading);
MultiNodePlatform multiNodePlatform(&display, &logFileManagerRaw, &files);
SignalHandler signalHandler(&display, &multiThreading);
Sequences sequences(&files, &display, &multiThreading, &multiNodePlatform, &multiNodePlatform.combineResult_AllNodes);
Analyser analyser(&logResultsToScreen, &sequences, &multiThreading, &logFileManagerHighSensitivity, &logFileManagerHighPrecision, &logFileManagerBalanced, &logFileManagerRaw, &display, &hardware, &multiNodePlatform, &signalHandler, &configuration);
Hashmap hashmap(&display, &multiThreading, &multiNodePlatform, &sequences, &analyser.ConsiderThreshold, &logFileManagerRaw, &files, &configuration);


///
/// Parse arguments
///
void ParseArguments(int argc, char* argv[])
{
	if (!(argc > 1))
		return;

	bool bNextIsNodeInputPath(false);
	bool bNextIsNodeIndex(false);
	bool bNextIsNodeTotal(false);
	bool bNextIsNodePrefix(false);
	for (int i = 1; i < argc; i++)
	{
		// Check for setting values
		if (bNextIsNodeInputPath) {
			bNextIsNodeInputPath = false;
			files.queryFilePath = argv[i];
			continue;
		}
		if (bNextIsNodeIndex) {
			bNextIsNodeIndex = false;
			multiNodePlatform.Node_Current_Index = atoi(argv[i]);
			continue;
		}
		if (bNextIsNodeTotal) {
			bNextIsNodeTotal = false;
			multiNodePlatform.Node_Total = atoi(argv[i]);
			continue;
		}
		if (bNextIsNodePrefix) {
			bNextIsNodePrefix = false;
			multiNodePlatform.File_Prefix = argv[i];
			continue;
		}

		// Check for setting names
		if (strcmp(argv[i], "-input") == 0) {
			bNextIsNodeInputPath = true;
			continue;
		}
		if (strcmp(argv[i], "-ni") == 0) {
			bNextIsNodeIndex = true;
			continue;
		}
		if (strcmp(argv[i], "-nt") == 0) {
			bNextIsNodeTotal = true;
			continue;
		}
		if (strcmp(argv[i], "-np") == 0) {
			bNextIsNodePrefix = true;
			continue;
		}
		if (strcmp(argv[i], "-estimatesdisabled") == 0 || strcmp(argv[i], "-ed") == 0) {
			display.ShowFileLoadingTimeEstimates = false;
			continue;
		}

		if (strcmp(argv[i], "--help") == 0
			|| strcmp(argv[i], "--h") == 0
			|| strcmp(argv[i], "-help") == 0
			|| strcmp(argv[i], "-h") == 0)
		{
			display.DisplayValue("Go to www.mirdesign.nl/mirage for more information", true);
			display.DisplayValue("You are allowed to override configuration settings; i.e. override the query file path in configuration file:", true);
			display.DisplayValue("-QuerySequencesPath [path to file]", true);
			display.DisplayNewLine();
			display.DisplayValue("Multi platform support:", true);
			display.DisplayValue("-ni x\t\t\tNumber of current node", true);
			display.DisplayValue("-nt x\t\t\tNumber of total nodes", true);
			display.DisplayValue("-np x\t\t\tPrefix being put before each output file", true);
			display.DisplayNewLine();
			display.DisplayValue("-ed Disable file loading time estimates", true);
			display.DisplayValue("-estimatesdisabled Disable file loading time estimates", true);
			exit(EXIT_SUCCESS);
		}
	}
	if (!multiNodePlatform.IsMultiNodePlatformSettingCorrect())
		exit(EXIT_FAILURE);
}


///
/// Initialise multi node platform settings
/// Return new output path if this is an multinodePlatform situation. Otherwice the current path is provided.
///
string InitialiseMultiNodePlatform()
{
	string retVal(Output_Path);
	// Initialise MultiNodePlatform support (if required)
	if (multiNodePlatform.divideSequenceReadBetweenNodes)
	{
		multiNodePlatform.Log_Path = Output_Path;
		retVal = multiNodePlatform.CreateMultiNodeOutputFolder();

		multiNodePlatform.LogFileNameAddition = "." + to_string(multiNodePlatform.Node_Current_Index) + "_" + to_string(multiNodePlatform.Node_Total);
		if (multiNodePlatform.File_Prefix != "" && multiNodePlatform.File_Prefix[multiNodePlatform.File_Prefix.size()] != '.')
			multiNodePlatform.File_Prefix += '.';

	}
	return retVal;
}

///
/// Parse general configuration settings
///
void ParseConfiguration_General()
{

	analyser.ConsiderThreshold = configuration.GetConfigurationSettingNumber("considerthreshold");
	analyser.Output_HighestScoredResultsOnly = configuration.GetConfigurationSettingBool("output_firstresults_only");
	files.referenceFilePath = configuration.GetConfigurationSettingArray("Reference");
	files.NCBIAccession2TaxIDFile = configuration.GetConfigurationSetting("ReferenceNCBIAccession2TaxIDPath", false);
	if (files.NCBIAccession2TaxIDFile != "")
		sequences.NCBIAccession2TaxID_Used = true;
	Output_Path = &configuration.GetConfigurationSetting("OutputPath")[0];

	bool query_require = Hashmap::IsReferenceHashMapDatabase(&(files.referenceFilePath[0])[0]);
	files.queryFilePath = configuration.GetConfigurationSetting("Query", query_require);

	multiNodePlatform.combineResult_AllNodes = configuration.GetConfigurationSettingBool("MultiNode_CombineResult_AllNodes");
	multiThreading.maxCPUThreads = configuration.GetConfigurationSettingNumber("Hardware_maxCPUThreads");

	sequences.SortReferenceSequencesOnLoad = configuration.GetConfigurationSettingNumber("ReferenceDatabaseSort");
	sequences.DivideReferenceSequences = configuration.TryGetConfigurationSettingBool("ReferenceDatabasePath_Divide", false);
	if (sequences.DivideReferenceSequences)
		sequences.DivideReferenceSequences_Length = configuration.GetConfigurationSettingNumber("ReferenceDatabasePath_DivideBy");

	LogManager::LogThreshold_HighSensitivity = configuration.GetConfigurationSettingFloat("LogThreshold_HighSensitivity");
	LogManager::LogThreshold_HighPrecision = configuration.GetConfigurationSettingFloat("LogThreshold_HighPrecision");
	LogManager::LogThreshold_Balanced = configuration.GetConfigurationSettingFloat("LogThreshold_Balanced");
}


///
/// Parse screen settings
///
void ParseConfiguration_Screen()
{
	// Only for windows users
#ifndef __linux__
	// Screen size
	screen.Screen_Resize = configuration.GetConfigurationSettingBool("Screen_Resize");
	screen.SCREEN_COLS = configuration.GetConfigurationSettingNumber("Screen_COLS");
	screen.SCREEN_ROWS = configuration.GetConfigurationSettingNumber("Screen_ROWS");
#endif
}

///
/// Parse hashmap configuration settings
///
void ParseConfiguration_Hashmap()
{

	hashmap.Save_Hashmap_Database = configuration.GetConfigurationSettingBool("output_save_hashmap_database");

	Hashmap::Hashmap_Save_Hashmap_During_Build_Below_Free_Memory = configuration.GetConfigurationSettingNumber("Hashmap_Save_Hashmap_During_Build_Below_Free_Memory");
	Hashmap::Hashmap_Performance_Boost = configuration.GetConfigurationSettingNumber("Hashmap_Performance_Boost");
	Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy = configuration.GetConfigurationSettingNumber("Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Tidy");
	Hashmap::Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold = configuration.GetConfigurationSettingNumber("Hashmap_LazyLoading_Cache_FreeMemory_Percentage_Threshold");
	Hashmap::Use_Experimental_LazyLoading = configuration.GetConfigurationSettingBool("Hashmap_Use_Experimental_LazyLoading");
	Hashmap::Hashmap_Load_Hashmap_Percentage = configuration.GetConfigurationSettingPercentage("Hashmap_Load_Hashmap_Percentage");
	Hashmap::Hashmap_Load_Hashmap_Skip_On_Failure = configuration.GetConfigurationSettingBool("Hashmap_Load_Hashmap_Skip_On_Failure");

	// Disabled; we don't build hashmap locally anynmore, only in vector and then saved to a MHM file. So we always load our hashmap after build.
	//Hashmap::Hashmap_Load_Hashmap_From_File_After_Build = configuration.GetConfigurationSettingBool("Hashmap_Load_Hashmap_From_File_After_Build");

	string MemUsageMode = configuration.GetConfigurationSetting("Hashmap_Load_Reference_Memory_UsageMode", false);

	// Memory usage mode
	transform(MemUsageMode.begin(), MemUsageMode.end(), MemUsageMode.begin(), ::tolower);
	if (MemUsageMode == "low")
		Hashmap::Hashmap_Load_Reference_Memory_UsageMode = Enum::Hashmap_MemoryUsageMode::Low;
	else if (MemUsageMode == "medium")
		Hashmap::Hashmap_Load_Reference_Memory_UsageMode = Enum::Hashmap_MemoryUsageMode::Medium;
	else if (MemUsageMode == "high")
		Hashmap::Hashmap_Load_Reference_Memory_UsageMode = Enum::Hashmap_MemoryUsageMode::High;
	else if (MemUsageMode == "extreme")
		Hashmap::Hashmap_Load_Reference_Memory_UsageMode = Enum::Hashmap_MemoryUsageMode::Extreme;
	else
	{
		display.logWarningMessage << "Configuration file contains wrong value in 'Hashmap_Load_Reference_Memory_UsageMode' configuration." << endl;
		display.logWarningMessage << "Provided output mode configuration value: " << MemUsageMode << endl;
		display.logWarningMessage << "Expected: low/medium/high/extreme" << endl;
		display.FlushWarning(true);
	}

	sequences.SortQuerySequencesOnLoad = Hashmap::Hashmap_Load_Reference_Memory_UsageMode < Enum::Hashmap_MemoryUsageMode::High;

}

///
/// Parse analysis mode configuration settings
///
void ParseConfiguration_AnalysisMode() {
	string SpeedMode = configuration.GetConfigurationSetting("UltraFastMode", false);
	transform(SpeedMode.begin(), SpeedMode.end(), SpeedMode.begin(), ::tolower);

	if (SpeedMode == "sensitive")
		Analyser::SpeedMode = Enum::Analysis_Modes::Sensitive;
	else if (SpeedMode == "fast")
		Analyser::SpeedMode = Enum::Analysis_Modes::UltraFast;
	else
	{
		display.logWarningMessage << "Configuration file contains wrong value in 'SpeedMode' configuration." << endl;
		display.logWarningMessage << "Provided output mode configuration value: " << SpeedMode << endl;
		display.logWarningMessage << "Expected: sensitive/fast" << endl;
		display.FlushWarning(true);
	}
}

///
/// Parse output mode configuration settings
///
void ParseConfiguration_OutputMode() {
	string outMode = configuration.GetConfigurationSetting("OutputMode", false);
	transform(outMode.begin(), outMode.end(), outMode.begin(), ::tolower);

	if (outMode == "default" || outMode == "" || outMode == "normal")
		configuration.Output_Mode = Output_Modes::Default;
	else if (outMode == "progressbar" || outMode == "bar" || outMode == "progress" || outMode == "pb")
		configuration.Output_Mode = Output_Modes::ProgressBar;
	else if (outMode == "silent")
		configuration.Output_Mode = Output_Modes::Silent;
	else if (outMode == "result" || outMode == "results")
		configuration.Output_Mode = Output_Modes::Results;
	else if (outMode == "resultnolog" || outMode == "resultsnolog")
		configuration.Output_Mode = Output_Modes::ResultsNoLog;
	else
	{
		display.logWarningMessage << "Configuration file contains wrong value in 'OutputMode' configuration." << endl;
		display.logWarningMessage << "Provided output mode configuration value: " << outMode << endl;
		display.logWarningMessage << "Expected: normal/progressbar/silent/results/resultsNoLog" << endl;
		display.FlushWarning(true);
	}

}



///
/// Parse hardware configuration settings
///
void ParseConfiguration_Hardware()
{
	hardware.Auto_Detect_Hardware = configuration.GetConfigurationSettingBool("Hardware_Auto_Detect");
	hardware.BlockSize_Default_CPU = configuration.GetConfigurationSettingNumber("Hardware_BlockSize_Default_CPU");
	hardware.BlockSize_Default_GPU = configuration.GetConfigurationSettingNumber("Hardware_BlockSize_Default_GPU");
	hardware.BlockSize_AutoIncrease_Enabled_Default = configuration.GetConfigurationSettingBool("Hardware_BlockSize_AutoIncrease_Enabled_Default");
	hardware.BlockSize_AutoIncrease_IncreasePercentage = configuration.GetConfigurationSettingNumber("Hardware_BlockSize_AutoIncrease_IncreasePercentage");
	hardware.MaxDeviceQueueSize = configuration.GetConfigurationSettingNumber("Hardware_Device_Queue_MaxSize");

	// Hardware DeviceEnabled
	vector<bool> HARDWARE_Enabled_InConfig = configuration.GetConfigurationSettingBoolArray("Hardware_MANUAL_Enabled");
	hardware.DeviceEnabled.clear();
	hardware.DeviceEnabled.swap(HARDWARE_Enabled_InConfig);

	// Hardware Autoincrease enabled
	vector<bool> HARDWARE_AutoIncrease_Enabled_Config = configuration.GetConfigurationSettingBoolArray("Hardware_MANUAL_BlockSize_AutoIncrease_Enabled");
	hardware.BlockSize_AutoIncrease_Enabled.swap(HARDWARE_AutoIncrease_Enabled_Config);

	// Hardware types
	vector<string> HARDWARE_Type_Configuration_InConfig = configuration.GetConfigurationSettingStringArray("Hardware_MANUAL_Types");
	hardware.Type_Configuration.swap(HARDWARE_Type_Configuration_InConfig);

	// Hardware Blocksize
	vector<size_t> HARDWARE_BlockSize_InConfig = configuration.GetConfigurationSettingSEQ_IDArray("Hardware_MANUAL_BlockSize");
	hardware.BlockSize_Current.swap(HARDWARE_BlockSize_InConfig);

	if (analyser.ConsiderThreshold == 0)
	{
		display.logWarningMessage << "Configuration file contains wrong value in 'considerthreshold' configuration." << endl;
		display.logWarningMessage << "Current value: " << analyser.ConsiderThreshold << endl;
		display.logWarningMessage << "Expected: >0" << endl;
		display.FlushWarning(true);
	}
}

///
/// Parse configuration file
///
void ParseConfigurationFile(int argc, char* argv[])
{
	// Read settings from file
	configuration.SetFiles(&files);
	configuration.ReadConfigurationFile();
	configuration.EnableProgramParameterOverride(argc, argv); // Provide application arguments; they can override a matching configuration setting

	// Parse settings
	ParseConfiguration_General();
	ParseConfiguration_Screen();
	ParseConfiguration_AnalysisMode();
	ParseConfiguration_OutputMode();
	ParseConfiguration_Hardware();
	ParseConfiguration_Hashmap();
}


///
/// Initilialize common settings
///
void Initialise(int argc, char* argv[])
{
	// Parse configuration file
	ParseConfigurationFile(argc, argv);

	// Initialise screen
	screen.InitialiseScreenSettings(&lts, &logResultsToScreen, &(configuration.Output_Mode));

	// Parse command line arguments to potentially override configuration settings
	ParseArguments(argc, argv);

	// Initialise output path
	Output_Path = InitialiseMultiNodePlatform();
	Output_Path = logFileManagerRaw.CreateUniqueOutputFolders(&Output_Path[0]);

	// Start log files
	logFileManagerHighSensitivity.Open(&Output_Path[0], (multiNodePlatform.LogFileNameAddition == "" ? NULL : &multiNodePlatform.LogFileNameAddition[0]), (multiNodePlatform.File_Prefix == "" ? NULL : &multiNodePlatform.File_Prefix[0]), (char*)"HighSensitivity");
	logFileManagerBalanced.Open(&Output_Path[0], (multiNodePlatform.LogFileNameAddition == "" ? NULL : &multiNodePlatform.LogFileNameAddition[0]), (multiNodePlatform.File_Prefix == "" ? NULL : &multiNodePlatform.File_Prefix[0]), (char*)"Balanced");
	logFileManagerRaw.Open(&Output_Path[0], (multiNodePlatform.LogFileNameAddition == "" ? NULL : &multiNodePlatform.LogFileNameAddition[0]), (multiNodePlatform.File_Prefix == "" ? NULL : &multiNodePlatform.File_Prefix[0]), (char*)"Raw");
	logFileManagerHighPrecision.Open(&Output_Path[0], (multiNodePlatform.LogFileNameAddition == "" ? NULL : &multiNodePlatform.LogFileNameAddition[0]), (multiNodePlatform.File_Prefix == "" ? NULL : &multiNodePlatform.File_Prefix[0]), (char*)"HighPrecision");

	// Write headers
	logFileManagerHighSensitivity.WriteOutputHeaders();
	logFileManagerBalanced.WriteOutputHeaders();
	logFileManagerRaw.WriteOutputHeaders();

	if (Hashmap::Hashmap_Load_Hashmap_Percentage == 100)
		logFileManagerHighPrecision.WriteOutputHeaders();
	else
	{
		logFileManagerHighPrecision.LogLine("Hashmap loading is <100% and therefore high precision is not possible. Set loading to 100% to enable high precision (Use Hashmap_Load_Hashmap_Percentage in configuration file).");
		logFileManagerHighPrecision.Disable();
	}
}

void CleanUpMemory()
{
	//delete Hashmap::referenceHashMap; //this takes to long, so let the os free this memory
	//delete  Hashmap::referenceHashMapPositions; //this takes to long, so let the os free this memory
	hardware.Devices.clear();
	hardware.Devices.shrink_to_fit();

	delete sequences.inputSequences;
}

int main(int argc, char* argv[])
{
	// Listen to user break or system kill messages
	signalHandler.ListenToSignalHandlers();

	try {
		// Keep track of elapsed time
		size_t startTime = time(NULL);

		// Load and initialise settings
		Initialise(argc, argv);

		// Show some information
		display.DisplaySplashScreen_Start();
		display.DisplayGeneralSettings(&files, &logFileManagerRaw, &sequences, &multiNodePlatform, analyser.ConsiderThreshold);

		// Detect hardware
		hardware.DetectAvailableHardware();

		// Load data from files
		sequences.LoadInputFiles(Output_Path, &hashmap, hardware.IsGPUDeviceUsed());

		size_t startTimeAnalyse = time(NULL);
		if (sequences.inputSequences->size() > 0)
		{
			// Clean up old temp files
			multiNodePlatform.CleanupBufferFiles();

			// Start analysis
			startTimeAnalyse = time(NULL);
			analyser.ConsiderThreshold_HashmapLoaded = hashmap.GetLoadedThreshold();
			analyser.ExecuteAnalysis_MultiHardware(startTime);

			// If we are on a multinode environment: Wait and combine results from multiple nodes
			multiNodePlatform.HandleEndOfProgram();
		}

		// Clean up
		analyser.CleanUp();

		// Show some information
		display.DisplaySplashScreen_End(startTime, startTimeAnalyse, logFileManagerRaw.GetLogFilePath(), multiNodePlatform.divideSequenceReadBetweenNodes, multiNodePlatform.Node_Current_Index);
	}
	catch (exception &ex)
	{
		// Oh no! :-(
		display.logWarningMessage << "MirAge catched an exception: " << ex.what() << endl;
		display.FlushWarning(true);
	}

	// Household!
	CleanUpMemory();

	// Ready!
	return EXIT_SUCCESS;
}
