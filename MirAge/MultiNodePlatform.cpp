/*
* Enable HPC environment usage
*/

#include "MultiNodePlatform.h"

///
/// Create a folder to work with
///
string MultiNodePlatform::CreateMultiNodeOutputFolder()
{
	string mnOutputPath(this->Log_Path);
	mnOutputPath += "/";
	mnOutputPath += this->File_Prefix;

#if defined(_WIN32)
	_mkdir(mnOutputPath.c_str());
#else 
	mkdir(mnOutputPath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP);
#endif

	this->Log_Path = mnOutputPath;

	return mnOutputPath;
}

///
/// Removes multiNodePlatform buffer files
///
void MultiNodePlatform::CleanupBufferFiles()
{
	if (divideSequenceReadBetweenNodes)
		if (Node_Current_Index == 0)
			RemoveNodeReadyFile();
}

///
/// If current run is in multiNodePlatform mode;
/// Write ready state of current node
/// Wait for all nodes to be finished if current node is 0
///
void MultiNodePlatform::HandleEndOfProgram() {

	if (divideSequenceReadBetweenNodes)
	{
		display->DisplayHeader("Multinode");
		display->DisplayTitle("[" + to_string(Node_Current_Index) + "] We're in a multi node environment.", true);
		WriteNodeReadyFile(logFileManager->GetLogFilePath());

		// Wait for all other nodes to be finished if current node is 0
		if (Node_Current_Index == 0)
		{
			display->DisplayTitle("[" + to_string(Node_Current_Index) + "] We're node 0.. waiting for all other nodes to finish..", true);
			WaitForAllNodesToBeReady();

			if (combineResult_AllNodes)
			{
				display->DisplayTitle("[" + to_string(Node_Current_Index) + "] All other nodes are ready, gather all other node data..", true);
				SummarizeAllNodes();
				//SummarizePercentageOfAllNodes();
			}
			else
			{
				display->DisplayTitle("[" + to_string(Node_Current_Index) + "] All other nodes are ready, skip gather all other node data (due to configuration)..", true);
			}
		}
	}
}

///
/// Get the log file path combined with provided file name
///
string MultiNodePlatform::GetLogFilePath(string fileName)
{
	string retVal = "";

	// Add multinode platform prefix
	if (File_Prefix != "")
		retVal = File_Prefix + fileName;


	string _logPath = this->Log_Path;
	if (_logPath[_logPath.size()] != '/')
		_logPath += '/';

	retVal = _logPath + retVal;

	return retVal;
}

///
/// Check if a file already exists
///
bool MultiNodePlatform::FileExist(const string& fileName)
{
	ifstream file(fileName);
	return (bool)file;
}

///
/// Write data to a file. Make sure it succeeds
///
void MultiNodePlatform::EnsureWriteDataToFile(string data, string filepath, ios_base::openmode fileMode)
{
	ofstream outfile;
	while (!outfile.is_open())
	{
		outfile.open(filepath, fileMode);
		if (!outfile.is_open())
			this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
	}

	int retryCount = 0;
	int retryMax = 100;
	bool logSucces(false);
	while (!logSucces)
	{
		retryCount++;
		try
		{
			// Write data
			outfile << data << endl;

			logSucces = true;
		}
		catch (exception e) {
			if (retryCount >= retryMax)
				throw e;
		}

	}

	outfile.flush();
	outfile.close();
}

///
/// Get the last node index which wrote in the ready file
///
int MultiNodePlatform::GetLastNodeIndexWroteStatus()
{
	int retVal = -1;

	try {
		bool outFileExists = FileExist(GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY));
		if (outFileExists)
		{
			ifstream infile;
			while (!infile.is_open())
			{
				infile.open(GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY), fstream::in);
				if (!infile.is_open())
					this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
			}

			infile >> retVal;
			infile.close();
		}
	}
	catch (exception ex)
	{
		cout << "[" << Node_Current_Index << "] ERROR! Failed to read to the 'node ready' file: " << GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY) << " - ex: " << ex.what() << endl;
	}
	return retVal;
}

/// Write the current node index into the ready file
///
void MultiNodePlatform::WriteNodeReadyFile(string LogPath)
{
	string filepath = GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY);
	cout << "[" << Node_Current_Index << "] Attempt to write the current node index in 'Node ready file'." << endl;
	cout.flush();
	try {

		bool NodeAllowedToWrite(false);
		bool WaitMessageShown(false);
		if (Node_Current_Index == 0)
			NodeAllowedToWrite = true;

		// Loop, since blocking files do not work properly
		while (!NodeAllowedToWrite)
		{
			int lastNodeIndexWroteStatus = GetLastNodeIndexWroteStatus();
			NodeAllowedToWrite = (lastNodeIndexWroteStatus + 1 == Node_Current_Index);

			if (!NodeAllowedToWrite)
			{
				if (!WaitMessageShown)
				{
					cout << "[" << Node_Current_Index << "] Previous nodes are not ready yet, so wait until we are allowed to write the current node index in 'Node ready file'." << endl;
					WaitMessageShown = true;
				}

				// Wait a moment before we retry
				this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
			}
		}

		// write current node number in lock file
		EnsureWriteDataToFile(to_string(Node_Current_Index), GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY), ofstream::trunc);

		// write current logpath in read file
		EnsureWriteDataToFile(LogPath, GetLogFilePath(LOG_FILE_APPEND_PATH_NODES_READY), ofstream::app);

	}
	catch (exception ex)
	{
		cout << "[" << Node_Current_Index << "] ERROR! Failed to write to the 'node ready' file: " << filepath << " - ex: " << ex.what() << endl;
	}

	cout << "[" << Node_Current_Index << "] Done writing ready file! " << endl;
	cout.flush();
}

///
/// Read all lines from a file into a string array
///
vector<string> MultiNodePlatform::GetFileContent(string filepath)
{

	if (!(files->file_exist(&filepath[0])))
		files->fileNotExistError(&filepath[0], "Could not get file content.");

	string newLine;
	ifstream fileReader;
	vector<string> retVal;
	while (!fileReader.is_open())
	{
		fileReader.open(filepath);

		if (!fileReader.is_open())
			this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
	}

	while (!fileReader.eof())
	{
		getline(fileReader, newLine);
		if (newLine != "")
			retVal.push_back(newLine);
	}
	fileReader.close();

	return retVal;
}

///
/// Wait until all other nodes are ready analysing the data
///
void MultiNodePlatform::WaitForAllNodesToBeReady()
{
	bool AllNodesReady(false);

	time_t now = time(0);
	char* dt_start = ctime(&now);
	dt_start[strlen(dt_start) - 1] = '\0';
	cout << "[" << Node_Current_Index << "] Node " << Node_Current_Index << " is now waiting for all other nodes to be finished (" << dt_start << ")..." << endl;

	int previousLastNodeIndexWroteStatus = -1;
	while (!AllNodesReady)
	{
		int lastNodeIndexWroteStatus = -1;
		ifstream infile;
		while (!infile.is_open())
		{
			infile.open(GetLogFilePath(LOG_FILE_APPEND_PATH_LAST_NODE_READY), fstream::in);

			if (!infile.is_open())
				this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
		}


		infile >> lastNodeIndexWroteStatus;

		if (previousLastNodeIndexWroteStatus != lastNodeIndexWroteStatus)
		{
			previousLastNodeIndexWroteStatus = lastNodeIndexWroteStatus;
			cout << "[" << Node_Current_Index << "] Last node seen to have written its ready state changed to: " << lastNodeIndexWroteStatus << endl;
		}

		if (lastNodeIndexWroteStatus == (Node_Total - 1))
			AllNodesReady = true;
		else
			this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_Sleep));
	}
	char* dt_end = ctime(&now);
	dt_end[strlen(dt_end) - 1] = '\0';
	cout << "[" << Node_Current_Index << "] All nodes are finsihed (" << dt_end << ")!" << endl;
}

///
/// Delete any old node ready file
///
void MultiNodePlatform::RemoveNodeReadyFile()
{
	display->DisplayHeader("Clean Up");

	// Remove program-ready file
	string file = GetLogFilePath(LOG_FILE_APPEND_PATH_NODES_READY);
	if (FileExist(file))
	{
		display->DisplayValue("[" + to_string(Node_Current_Index) + "] Removing previous used 'program-ready' state file (" + file + ")... ");

		try {
			if (FileExist(file))
				remove(&file[0]);
		}
		catch (exception ex)
		{
			display->DisplayValue("Failed!", true);
			display->logWarningMessage << "Unable to clean up old 'program-ready' state file!" << endl;
			display->logWarningMessage << "Is the file opened in another program?" << endl;
			display->logWarningMessage << "File: " << file << endl;
			display->logWarningMessage << "Node_Current_Index: " << Node_Current_Index << endl;
			display->logWarningMessage << "Program will stop." << endl;
			display->FlushWarning(true);
		}
		display->DisplayValue("Done!", true);
	}
	else
		display->DisplayValue("All tidy, nothing to do.", true);
}

///
/// Get header name from a log line
///
string MultiNodePlatform::GetHeaderNameFromLogFileLine(string LogFileLine)
{
	// Strip header name from log file line
	// 320;50;0.997463;538;>blaMIR-1_1_M37839;541;>

	int indexStartHeaderSeperator = 4;
	int index_First_Seperator = 0;
	int index_Second_Seperator = 0;
	int previous_Index = -1;
	for (int i = 0; i < indexStartHeaderSeperator + 1; i++)
	{
		previous_Index = LogFileLine.find(Log_Seperator, previous_Index + 1);

		if (i == indexStartHeaderSeperator - 1)
			index_First_Seperator = previous_Index + 1;
		if (i == indexStartHeaderSeperator)
			index_Second_Seperator = previous_Index;
	}

	return LogFileLine.substr(index_First_Seperator, index_Second_Seperator - index_First_Seperator);
}

///
/// Calculate overall percentage scores using the final data file
///
void MultiNodePlatform::SummarizePercentageOfAllNodes()
{
	cout << "[" << Node_Current_Index << "] Calculate percentages of all log files from all " << Node_Total << " nodes..." << endl;
	cout << "[" << Node_Current_Index << "] Get data from: " << GetLogFilePath(this->LOG_FILE_APPEND_PATH_FINAL) << endl;

	vector<string> fileContent = GetFileContent(GetLogFilePath(this->LOG_FILE_APPEND_PATH_FINAL));
	vector<string> InFile;
	vector<int> counts;

	for (string &refFound : fileContent)
	{
		string headerName = GetHeaderNameFromLogFileLine(refFound);

		bool bFound(false);
		int index = 0;
		for (string &refInFile : InFile)
		{
			if (headerName == refInFile)
			{
				bFound = true;
				break;
			}
			index++;
		}

		if (!bFound)
		{
			InFile.push_back(headerName);
			counts.push_back(1);
		}
		else
		{
			counts[index] = counts[index] + 1;
		}
	}

	cout << "[" << Node_Current_Index << "] Calculate percentage and write data to: " << GetLogFilePath(this->LOG_FILE_APPEND_PATH_PERCENTAGE_OVERVIEW) << endl;

	try
	{
		ofstream outfile;
		while (!outfile.is_open())
		{
			outfile.open(GetLogFilePath(this->LOG_FILE_APPEND_PATH_PERCENTAGE_OVERVIEW));
			this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
		}

		int index = 0;
		for (string &refInFile : InFile)
		{
			double percentage = (100.0 / fileContent.size()) * counts[index];
			outfile << refInFile;
			outfile << Log_Seperator;
			outfile << percentage;
			outfile << "%";
			outfile << endl;
			index++;
		}

		outfile.close();
	}
	catch (exception ex)
	{
		display->logWarningMessage << "[" << Node_Current_Index << "] Failed to write 'node ready' file: " << GetLogFilePath(this->LOG_FILE_APPEND_PATH_PERCENTAGE_OVERVIEW) << endl;
		display->FlushWarning();
	}
}

///
/// Retrieve all data from all node log files and combine them in 1 final file
///
void MultiNodePlatform::SummarizeAllNodes()
{
	cout << "[" << Node_Current_Index << "] Processing log files from all " << Node_Total << " nodes..." << endl;
	cout << "[" << Node_Current_Index << "] Reading all log files.. ";

	vector<string> fileContent = GetFileContent(GetLogFilePath(LOG_FILE_APPEND_PATH_NODES_READY));
	vector<string> AllLogFileContent;
	bool keepHeader(true);
	for (string & s : fileContent)
	{
		vector<string> logContent = GetFileContent(s);

		// Remove header
		if (!keepHeader)
		{
			logContent.erase(logContent.begin());
			keepHeader = false;
		}
		AllLogFileContent.insert(AllLogFileContent.end(), make_move_iterator(logContent.begin()), make_move_iterator(logContent.end()));
	}

	cout << "Done!" << endl;
	cout << "[" << Node_Current_Index << "] Write 'Final log' file.. ";

	string outPath = GetLogFilePath(this->LOG_FILE_APPEND_PATH_FINAL);

	try {

		// Write headers
		ofstream outfile;
		while (!outfile.is_open())
		{
			outfile.open(outPath);
			this_thread::sleep_for(chrono::milliseconds(MultiNodePlatform_WaitingAllNodesReady_OpenFileTimeoutSleep));
		}
		outfile << "InputID";
		outfile << Log_Seperator;
		outfile << "Score";
		outfile << Log_Seperator;
		outfile << "relativeMatrixScore";
		outfile << Log_Seperator;
		outfile << "ReferenceID[]";
		outfile << endl;

		for (string & s : AllLogFileContent)
			outfile << s << endl;

		outfile.close();

	}
	catch (exception ex)
	{
		display->logWarningMessage << "[" << Node_Current_Index << "] Failed to write summary file: " << outPath << endl;
		display->FlushWarning();
	}

	cout << "Done!" << endl;
	cout << "[" << Node_Current_Index << "] Processing log files of all " << Node_Total << " nodes is finsihed!" << endl;
	cout.flush();

}

///
/// Check if multinode settings are used and if they are correct.
///
bool MultiNodePlatform::IsMultiNodePlatformSettingCorrect()
{
	// Check if both Index as Total has been provided
	if (File_Prefix != "" && Node_Current_Index != -1 && Node_Total != -1)
	{
		if (Node_Total <= 0)
		{
			display->logWarningMessage << "Multiplatform 'total nodes (-nt)' can't be 0." << endl;
			display->logWarningMessage << "Provided: " << Node_Total << endl;
			display->logWarningMessage << "Please provide a legal value." << endl;
			display->FlushWarning();
			return false;
		}
		else if (Node_Current_Index >= Node_Total || Node_Current_Index < 0)
		{
			display->logWarningMessage << "Multiplatform 'current index node (-ni)' argument can't be equal or higher than 'total nodes argument (-nt)'." << endl;
			display->logWarningMessage << "Index node provided: " << Node_Current_Index << endl;
			display->logWarningMessage << "Total nodes provided: " << Node_Total << endl;
			display->logWarningMessage << "Please provide a legal value." << endl;
			display->FlushWarning();
			return false;
		}

		divideSequenceReadBetweenNodes = true;

	}
	else if ((Node_Current_Index != -1 || File_Prefix != "") && Node_Total == -1)
	{
		display->logWarningMessage << "Multiplatform index argument detected; please also provide 'total number of nodes' using '-nt'." << endl;
		display->logWarningMessage << "Please provide a legal value." << endl;
		display->FlushWarning();
		return false;
	}
	else if (Node_Current_Index == -1 && (Node_Total != -1 || File_Prefix != ""))
	{
		display->logWarningMessage << "Multiplatform total nodes argument detected; also please provide 'node index' using '-ni'." << endl;
		display->logWarningMessage << "Please provide a legal value." << endl;
		display->FlushWarning();
		return false;
	}
	else if (File_Prefix == "" && (Node_Current_Index != -1 || Node_Total != -1))
	{
		display->logWarningMessage << "Multiplatform total nodes argument detected; please also provide 'node file prefix' using '-np'." << endl;
		display->logWarningMessage << "Please provide a legal value." << endl;
		display->FlushWarning();
		return false;
	}
	return true;
}

MultiNodePlatform::MultiNodePlatform(Display *d, LogManager *lfm, Files *f)
{
	display = d;
	logFileManager = lfm;
	files = f;
}
MultiNodePlatform::~MultiNodePlatform() {}