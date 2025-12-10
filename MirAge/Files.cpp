#include "Files.h"

///
/// Checks whether a file exists by attempting to open it for reading.
///
bool Files::file_exist(char *fileName)
{
	ifstream fileStream(fileName);
	bool fileExist = fileStream.good();
	fileStream.close();
	return fileExist;
}

///
/// Checks whether a folder exists by using the stat function to retrieve information about the specified path.
///
bool Files::folder_exist(char *folderName)
{
	struct stat info;

	if (stat(folderName, &info) == 0)
		if (info.st_mode & S_IFDIR)
			return true;

	return false;
}


Files::Files(Display * d)
{
	display = d;
}

void Files::fileNotExistError(char *filename, string message)
{
	display->logWarningMessage << "File does not exist." << endl;
	display->logWarningMessage << message << endl;
	display->logWarningMessage << "File: " << string(filename) << endl;
	display->FlushWarning(true);
}

void Files::fileExistError(char *filename, string message)
{
	display->logWarningMessage << "File already exist." << endl;
	display->logWarningMessage << message << endl;
	display->logWarningMessage << "Provided file path: '" << string(filename) << "'" << endl;
	display->FlushWarning(true);
}

///
/// Try to return the file extension of the given file name.
///
const string Files::TryGetFileExtention(char *fileName)
{
	char *ext = strrchr(fileName, '.');
	if (ext == NULL)
		return "";
	return string(ext);
}

///
/// Return the file extension of the given file name. If the file does not exist, an error is raised.
///
const string Files::GetFileExtention(char *fileName)
{
	if (!file_exist(fileName))
		fileNotExistError(fileName, "Could not get file extention.");

	char *ext = strrchr(fileName, '.');
	if (ext == NULL)
		return "";
	return string(ext);
}


string Files::GetFileNameFromPath(string path)
{
	string filename(path);
	// Remove directory if present.
	// Do this before extension removal incase directory has a period character.
#ifdef __linux__
	const size_t last_slash_idx = filename.find_last_of("/");
#else
	const size_t last_slash_idx = filename.find_last_of("\\");
#endif
	if (string::npos != last_slash_idx)
	{
		filename.erase(0, last_slash_idx + 1);
	}

	// Remove extension if present.
	const size_t period_idx = filename.rfind('.');
	if (std::string::npos != period_idx)
	{
		filename.erase(period_idx);
	}
	return filename;
}


