#pragma once
#ifndef MULTINODEPLATFORM_H
#define MULTINODEPLATFORM_H
#include <vector>
#include <string>
#include <cstring>
#include "fstream"
#include <iostream>
#include <thread>
#include "Files.h"
#include "Display.h"
#include "LogManager.h"


#ifdef __linux__
#include <unistd.h>
#endif
using namespace std;
class Display;
class Files;
class LogManager;
class MultiNodePlatform
{
private:
	Files *files;
	Display *display;
	LogManager *logFileManager;
	int MultiNodePlatform_WaitingAllNodesReady_Sleep = 1000; // Used to pause waiting for all nodes to be ready
	int MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep = 50; // Used to pause waiting for all nodes to be ready

	vector<string> MultiNodePlatform_WaitForFiles;
	const string __MULTIPLATFORM_NODESREADY_CONTENT_DELIMITER = ":";



	//string GetNodeReadyLockFileFilePath();
	//string GetNodeReadyFilePath();
	void EnsureWriteDataToFile(string data, string filepath, ios_base::openmode fileMode);
	int GetLastNodeIndexWroteStatus();
	vector<string> GetFileContent(string filepath);
	string GetHeaderNameFromLogFileLine(string LogFileLine);

	const string LOG_FILE_APPEND_PATH_LAST_NODE_READY = "nodes-ready-last-index.log";
	const string LOG_FILE_APPEND_PATH_NODES_READY = "nodes-ready-logpaths.log";
	
	
	bool FileExist(const string& name);

public:

	char Log_Seperator = ';';
	string Log_Path = "";
	string File_Prefix = "";
	bool combineResult_AllNodes = false;
	bool divideSequenceReadBetweenNodes = false;
	int Node_Current_Index = -1;
	int Node_Total = -1;
	string LogFileNameAddition = "";
	int Analyse_Sequences_From_Index = -1;
	int Analyse_Sequences_To_Index = -1;
	string GetLogFilePath(string Append);


	string CreateMultiNodeOutputFolder();

	void CleanupBufferFiles();
	void HandleEndOfProgram();
	void RemoveNodeReadyFile();
	void WriteNodeReadyFile(string LogPath);
	void WaitForAllNodesToBeReady();
	void SummarizePercentageOfAllNodes();
	void SummarizeAllNodes();

	const string LOG_FILE_APPEND_PATH_PERCENTAGE_OVERVIEW = "final-percentage.overview.log";
	const string LOG_FILE_APPEND_PATH_FINAL = "final-log.log";


	bool IsMultiNodePlatformSettingCorrect();

	MultiNodePlatform(Display *d, LogManager *lfm, Files *f);
	~MultiNodePlatform();
};

#endif