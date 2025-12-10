#pragma once
#ifndef FILES_H
#define FILES_H
#include "Enum.h"
#include "Display.h"
#include <stdio.h>
#include <cstring>
#include <fstream>

using namespace std;
class Display;
class Configuration;


class Files
{
private:

	Display *display;

public:


	string queryFilePath;
	vector<string> referenceFilePath;
	bool refDataFile_Divide;
	int refDataFile_DivideBy = -1;

	string NCBIAccession2TaxIDFile;

	static const string TryGetFileExtention(char *fileName);
	const string GetFileExtention(char *fileName);
	string GetFileNameFromPath(string path);
	void fileNotExistError(char  * filename, string message);
	void fileExistError(char * filename, string message);
	bool file_exist(char *fileName);
	bool folder_exist(char *folderName);
	Files(Display *d);

};
#endif