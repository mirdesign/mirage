#pragma once
#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include "Enum.h"
#include "Files.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include <unordered_map>
using namespace std;
class Files;
class Configuration
{

private:
	Files *files;
	bool filesSet = false;
#ifdef __linux__
	string __CONFIGURATION_FILE = "./../mirage/data/mirage.identifier.cfg";
#else
	string __CONFIGURATION_FILE = "../../mirage/data/mirage.identifier.cfg";
#endif
	bool programParameterOverride = false;
	int programParameterOverride_argc;
	char** programParameterOverride_argv;

	bool ConfigurationFileRead = false;
	unordered_map<std::string, vector<std::string>>* Configuration_Settings;
	//vector<string> Configuration_Value;
	const string __CONFIGURATION_FILE_DELIMITER_NAME = ":";
	const string __CONFIGURATION_FILE_DELIMITER_COMMENT = "#";
	const string __CONFIGURATION_FILE_DELIMITER_VALUE = ";";

public:
	static string __NCBIACCESSION2TAXID_FILE_DELIMITER_COL;

	static vector<string> SplitString(string s, string delimiter, bool splitOnce);

	Output_Modes Output_Mode;

	void EnableProgramParameterOverride(int argc, char* argv[]);
	void ReadConfigurationFile(string configurationFilePath = "", string customDelimiterChar = "");
	void ReadConfigurationFile(ifstream* infile, string customDelimiterChar = "");

	vector<string> GetConfigurationSettingArray(string ConfigurationName, bool ExitOnNotFound = true);
	string GetConfigurationSetting(string ConfigurationName, bool ExitOnNotFound = true);
	vector<string> GetConfigurationSettingStringArray(string ConfigurationName);
	vector<size_t> GetConfigurationSettingSEQ_IDArray(string ConfigurationName);
	vector<bool> GetConfigurationSettingBoolArray(string ConfigurationName);
	bool GetConfigurationSettingBool(string ConfigurationName);
	bool TryGetConfigurationSettingBool(string ConfigurationName, bool DefaultValue = false);
	float GetConfigurationSettingFloat(string ConfigurationName);
	int GetConfigurationSettingNumber(string ConfigurationName);
	int GetConfigurationSettingPercentage(string ConfigurationName);

	void SetFiles(Files* f);
	Files* GetFiles();
	Configuration();
	~Configuration();
};
#endif