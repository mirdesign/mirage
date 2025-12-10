#pragma once
#ifndef LOGMANAGER_H
#define LOGMANAGER_H
#include <iostream>
#include <fstream>
#include "Files.h"

#if defined(_WIN32)
#include <direct.h> // for _mkdir
#else
#include <sys/stat.h> // for _mkdir
#include <sys/types.h>
#endif
using namespace std;

class Hashmap;

class LogManager
{
private:
	ofstream out;
	string logFile;
	char* logFilePath;

	// Buffer
	stringstream bufferStream;
	bool disabled = false;
	bool buffered = false;

	template<typename T>
	void LogObject(T logMessage, bool newLine);
	template<typename T>
	void LogObject(T* logMessage, size_t size, bool newLine);

public:
	void BufferOn();
	void Flush();

	static float LogThreshold_HighSensitivity;
	static float LogThreshold_HighPrecision;
	static float LogThreshold_Balanced;
	static const char* Output_ColumnSeperator;
	static const char* Output_DuplicateSeperator;
	void Disable();
	bool IsDisabled();
	void LogLine(int LogMessage);
	void LogLine(float LogMessage);
	void LogLine(double LogMessage);
	void ClearFile();
	void LogLine(string LogMessage = "");
	void Log(string LogMessage, bool newLine = false);
	void Log(char LogMessage, bool newLine = false);
	void Log(uint32_t LogMessage, bool newLine = false);
	void Log(uint64_t LogMessage, bool newLine = false);
	void Log(int LogMessage, bool newLine = false);
	void Log(float LogMessage, bool newLine = false);
	void Log(double LogMessage, bool newLine = false);
	void Log(char* LogMessage, size_t size, bool newLine = false);

	string CreateUniqueOutputFolders(char * Path, string HashmapFolderPath = "");
	void Open(char* LogFilePath, char* FileNamePostfix = nullptr, char* FileNamePrefix = nullptr, char* FileNameResultType = nullptr, bool FileNameIsExactLocation = false);
	string GetLogFilePrefix();
	char* GetLogFilePath();
	void WriteOutputHeaders();
	LogManager();
	~LogManager();


};

#endif