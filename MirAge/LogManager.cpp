#include "LogManager.h"
#include <stdio.h>
#include <time.h>
#include <string>
#include <cstring>

const char* LogManager::Output_ColumnSeperator = ";";
const char* LogManager::Output_DuplicateSeperator = ",";

float LogManager::LogThreshold_HighSensitivity = 0.4;
float LogManager::LogThreshold_HighPrecision = 1;
float LogManager::LogThreshold_Balanced = 0.7;

ofstream out;
string logFile;
char* logFilePath;
string logFilePrefix;

mutex mutexLogObject;

///
/// Set buffer on
///
void LogManager::BufferOn()
{
	buffered = true;
}

///
/// Log buffered data
///
void LogManager::Flush()
{
	buffered = false;
	LogObject(bufferStream.str(), false);
	stringstream().swap(bufferStream);
}

///
/// Disable this logmanager to prevent writing output
///
void LogManager::Disable()
{
	disabled = true;
}

///
/// Check if this logmanager id disabled to prevent writing output
///
bool LogManager::IsDisabled()
{
	return disabled;
}

///
/// Clear content of the current logfile
///
void LogManager::ClearFile()
{
	if (IsDisabled())
		return;
	mutexLogObject.lock();

	out.close();
	out.open(logFile, ios::trunc);
	out.imbue(locale(""));

	if (!out.is_open())
	{
		cerr << "\nERROR with file destination:[" << logFile << "]" << endl;
		for (int i = 0; i < 5; i++)
		{
			cerr << "********************************************************" << endl;
		}
		cerr << "******************PROGRAM TERMINATED********************\n";
		exit(1);
	}
	mutexLogObject.unlock();

}

///
/// Log line
///
void LogManager::LogLine(string LogMessage)
{
	this->Log(LogMessage, true);
}

///
/// Log line
///
void LogManager::LogLine(int LogMessage)
{
	this->Log(LogMessage, true);
}

///
/// Log line
///
void LogManager::LogLine(float LogMessage)
{
	this->Log(LogMessage, true);
}

///
/// Log line
///
void LogManager::LogLine(double LogMessage)
{
	this->Log(LogMessage, true);
}


///
/// Log message
///
template<typename T>
void LogManager::LogObject(T logMessage, bool newLine)
{
	// Do not continue if current logmanager is disabled
	if (IsDisabled())
		return;

	mutexLogObject.lock();

	if (buffered)
	{
		// Store message in buffer
		if (newLine)
			bufferStream << logMessage << endl;
		else
			bufferStream << logMessage;
	}
	else
	{
		// Save message to stream
		int retryCount = 0;
		int retryMax = 100;
		bool logSucces(false);
		while (!logSucces)
		{
			retryCount++;
			try
			{
				if (newLine)
					out << logMessage << endl;
				else
					out << logMessage;

				logSucces = true;
			}
			catch (exception e) {
				if (retryCount >= retryMax)
				{
					mutexLogObject.unlock();
					throw e;
				}
			}
		}
	}
	mutexLogObject.unlock();

}

///
/// Log message
///
template<typename T>
void LogManager::LogObject(T* logMessage, size_t size, bool newLine)
{
	// Do not continue if current logmanager is disabled
	if (IsDisabled())
		return;
	mutexLogObject.lock();

	if (buffered)
	{
		// Store message in buffer
		if (newLine)
			bufferStream << logMessage << endl;
		else
			bufferStream << logMessage;
	}
	else
	{
		// Save message to stream
		int retryCount = 0;
		int retryMax = 100;
		bool logSucces(false);
		while (!logSucces)
		{
			retryCount++;
			try
			{
				out.write(logMessage, size);
				if (newLine)
					out << endl;

				logSucces = true;
			}
			catch (exception e) {
				if (retryCount >= retryMax)
				{
					mutexLogObject.unlock();
					throw e;
				}
			}
		}
	}
	mutexLogObject.unlock();
}


///
/// Log message
///
#pragma warning( disable : 4996 )
void LogManager::Log(string LogMessage, bool newLine)
{
	if (IsDisabled())
		return;

	LogObject(LogMessage.c_str(), newLine);
}


///
/// Log message
///
void LogManager::Log(char* LogMessage, size_t size, bool newLine)
{
	LogObject(LogMessage, size, newLine);
}

///
/// Log message
///
void LogManager::Log(char LogMessage, bool newLine)
{
	string s(1, LogMessage);

	LogObject(s, newLine);
}


///
/// Log message
///
void LogManager::Log(uint32_t LogMessage, bool newLine)
{
	LogObject(LogMessage, newLine);
}
///
/// Log message
///
void LogManager::Log(uint64_t LogMessage, bool newLine)
{
	LogObject(LogMessage, newLine);
}
///
/// Log message
///
void LogManager::Log(int LogMessage, bool newLine)
{
	LogObject(LogMessage, newLine);
}

///
/// Log message
///
void LogManager::Log(float LogMessage, bool newLine)
{
	out << LogMessage;
}
///
/// Log message
///
void LogManager::Log(double LogMessage, bool newLine)
{
	out << LogMessage;
}

///
/// Creates:
///			/output/results_[unique number]
///			/output/results_[unique number]/hashmap
string LogManager::CreateUniqueOutputFolders(char* Path, string HashmapFolderPath)
{
	size_t seconds_past_epoch = time(0);

	string newPath(Path);
	newPath += "/results_";
	newPath += to_string(seconds_past_epoch);

	string hashMapPath;
	if (HashmapFolderPath != "")
	{
		hashMapPath += newPath;
		hashMapPath += "/";
		hashMapPath += HashmapFolderPath;
	}

#if defined(_WIN32)
	_mkdir(newPath.c_str());
	if (HashmapFolderPath != "")
		_mkdir(hashMapPath.c_str());
#else 
	mkdir(newPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP);
	if (HashmapFolderPath != "")
		mkdir(hashMapPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP);
#endif
	return newPath;
}

///
/// Open logfile
///
void LogManager::Open(char* LogFilePath, char* FileNamePostfix, char* FileNamePrefix, char* FileNameResultType, bool FileNameIsExactLocation)
{
	if (IsDisabled())
		return;

	// Do we need to compose file name?
	if (FileNameIsExactLocation)
	{
		// No, just use the postfix directly
		logFile += *FileNamePostfix;
	}
	else
	{
		// Yes, compose filename
		string ext = ".log";
		logFile = "results";

		// Add prefix
		if (FileNamePrefix != nullptr)
			logFile = string(FileNamePrefix) + "_" + logFile;

		// Add result type
		if (FileNameResultType != nullptr)
			logFile += "_" + string(FileNameResultType);

		// Add postfix
		if (FileNamePostfix != nullptr)
			logFile += "_" + string(FileNamePostfix);

		// Add extension
		logFile += ext;
	}

	// Append to path
	logFile = string(LogFilePath) + "/" + logFile;

	// Open file
	out.open(logFile, ios::app);
	out.imbue(locale(""));

	if (!out.is_open())
	{
		cerr << "\nERROR with file destination:[" << logFile << "]" << endl;
		cerr << strerror(errno) << endl;
		for (int i = 0; i < 5; i++)
			cerr << "********************************************************" << endl;
		cerr << "******************PROGRAM TERMINATED********************\n";
		exit(1);
	}

}

///
/// Get log file prefix
///
string LogManager::GetLogFilePrefix()
{
	return logFilePrefix;
}

///
/// Get log file path
///
char* LogManager::GetLogFilePath()
{
	return &logFile[0u];
}

///
/// Write headers for output file
///
void LogManager::WriteOutputHeaders()
{
	Log("InputID");
	Log(Output_ColumnSeperator);
	Log("InputSequence");
	Log(Output_ColumnSeperator);
	Log("Confident");
	Log(Output_ColumnSeperator);
	Log("TaxIDs");
	Log(Output_ColumnSeperator);
	Log("Occurence_Per_TaxID");
	Log(Output_ColumnSeperator);
	Log("Headers");
	LogLine();
}

LogManager::LogManager()
{}

LogManager::~LogManager()
{
	out.close();
}
