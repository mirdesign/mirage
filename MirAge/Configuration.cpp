#include "Configuration.h"
string Configuration::__NCBIACCESSION2TAXID_FILE_DELIMITER_COL = "\t";

///
/// Enable lookup of program parameters and override configuration file settings with provided parameters
///
void Configuration::EnableProgramParameterOverride(int argc, char* argv[])
{
	this->programParameterOverride = true;
	this->programParameterOverride_argc = argc;
	this->programParameterOverride_argv = argv;

	for (int i = 1; i < argc; i++) // +2: Do not lower case input values; only setting names, therefore we also start at position 1, we skip the application name
		if (this->programParameterOverride_argv[i][0] == '-')
			transform(this->programParameterOverride_argv[i], this->programParameterOverride_argv[i] + strlen(this->programParameterOverride_argv[i]), this->programParameterOverride_argv[i], ::tolower);
}


///
/// Split string at one or multiple position using a delimiter
///
vector<string> Configuration::SplitString(string s, string delimiter, bool splitOnce = true)
{
	vector<string> retVal;
	bool bDelimiterFound(false);

	size_t pos = 0;
	while ((pos = s.find(delimiter)) != string::npos) {
		bDelimiterFound = false;
		retVal.push_back(s.substr(0, pos));
		s.erase(0, pos + delimiter.length());

		if (splitOnce)
		{
			bDelimiterFound = true;
			retVal.push_back(s);
			break;
		}
	}

	if (!bDelimiterFound)
		retVal.push_back(s);

	return retVal;
}


const char* removeInTrim = " \t\n\r\f\v ";

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = removeInTrim)
{
	s.erase(s.find_last_not_of(t) + 1);
	return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = removeInTrim)
{
	s.erase(0, s.find_first_not_of(t));
	return s;
}

// trim from both ends of string (left & right)
inline std::string& trim(std::string& s, const char* t = removeInTrim)
{
	return ltrim(rtrim(s, t), t);
}


///
/// Read the taxonomy file
///
unsigned int FileRead(istream & is, vector <char> & buff) {
	is.read(&buff[0], buff.size());
	return is.gcount();
}

///
/// Read and parse the configuration file
///
void Configuration::ReadConfigurationFile(string configurationFilePath, string customDelimiterChar)
{
	if (configurationFilePath == "")
		configurationFilePath = __CONFIGURATION_FILE;

	if (!(GetFiles()->file_exist(&configurationFilePath[0])))
		GetFiles()->fileNotExistError(&configurationFilePath[0], "Could not read configuration file");


	ifstream infile;
	infile.open(configurationFilePath);
	ReadConfigurationFile(&infile, customDelimiterChar);
}

void Configuration::ReadConfigurationFile(ifstream* infile, string customDelimiterChar)
{
	Configuration_Settings = new unordered_map<string, vector<string>>();

	string delimiter = __CONFIGURATION_FILE_DELIMITER_NAME;
	if (customDelimiterChar != "")
		delimiter = customDelimiterChar;
	string line;
	int lineCount = 1;

	//if (!silent)
	//	cout << "Reading configuration file (" << configurationFilePath << ")... ";
	while (!infile->eof())
	{
		getline(*infile, line);

		// Remove comments
		line = SplitString(line, __CONFIGURATION_FILE_DELIMITER_COMMENT)[0];
		line = trim(line);
		if (line != "")
		{
			// Get configuration Name and Value
			vector<string> sLine = SplitString(line, delimiter);

			// Exit if we did not find a setting and value
			if (sLine.size() != 2)
			{
				cerr << endl << "ERROR in configuration file, setting or value not found at line " << lineCount << ": [" << line << "]" << endl;
				exit(1);
			}

			string configSetting = sLine[0];
			string configValue = sLine[1];

			configSetting = trim(configSetting);
			configValue = trim(configValue);

			transform(configSetting.begin(), configSetting.end(), configSetting.begin(), ::tolower);

			vector<std::string>* newV = new vector<std::string>{ configValue };
			auto retInsertConfig = Configuration_Settings->insert({ configSetting,  *newV }); // Try to insert, if it fails; we know it's already in memory and get the config as return value
			if (!retInsertConfig.second)
			{
				// Insert failed, key already exist. So append the values
				retInsertConfig.first->second.push_back(configValue);
			}

			delete newV;

		}
		lineCount++;
	}

	infile->close();

	this->ConfigurationFileRead = true;
}

///
/// Get the value for a specific configuration setting. Will return "" when ExitOnNotFound is false and the setting has not been found.
///
vector<string> Configuration::GetConfigurationSettingArray(string ConfigurationName, bool ExitOnNotFound)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	// ToLower
	transform(ConfigurationName.begin(), ConfigurationName.end(), ConfigurationName.begin(), ::tolower);

	// Check if we find a parameter to override the confirugation
	if (this->programParameterOverride)
	{
		bool nextIsValue(false);
		string parameterValue = "";

		for (int i = 0; i < this->programParameterOverride_argc; i++)
		{
			if (nextIsValue)
			{
				parameterValue = this->programParameterOverride_argv[i];
				break;
			}

			string parameterName = "-" + ConfigurationName;
			if (strcmp(this->programParameterOverride_argv[i], &parameterName[0]) == 0)
				nextIsValue = true;
		}

		if (parameterValue != "")
			return vector<string> { parameterValue };
	}

	// Read the configuration file
	int valueIndex = 0;
	bool valueFound(false);

	unordered_map<string, vector<string>>::const_iterator found = Configuration_Settings->find(ConfigurationName);

	if (found != Configuration_Settings->end())
	{
		return found->second;
	}
	else
	{
		if (!ExitOnNotFound)
		{
			return vector<string> { "" };
		}
		else
		{
			cerr << endl << "ERROR with configuration file, setting not found: [" << ConfigurationName << "]" << endl;
			exit(1);
		}
	}

	cerr << endl << "ERROR with configuration file, setting not found: [" << ConfigurationName << "]" << endl;
	exit(1);
}


///
/// Get the value for a specific configuration setting. Will return "" when ExitOnNotFound is false and the setting has not been found.
///
string Configuration::GetConfigurationSetting(string ConfigurationName, bool ExitOnNotFound)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	return GetConfigurationSettingArray(ConfigurationName, ExitOnNotFound)[0];
}

///
/// Get the value for a specific configuration setting
///
vector<string> Configuration::GetConfigurationSettingStringArray(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	string value = GetConfigurationSetting(ConfigurationName);

	vector<string> retVal = SplitString(value, __CONFIGURATION_FILE_DELIMITER_VALUE, false);

	return retVal;
}

///
/// Get the value for a specific configuration setting
///
vector<size_t> Configuration::GetConfigurationSettingSEQ_IDArray(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	vector<size_t> retVal;
	string value = GetConfigurationSetting(ConfigurationName);
	vector<string> sValues = SplitString(value, __CONFIGURATION_FILE_DELIMITER_VALUE, false);

	for (string &v : sValues)
	{
		size_t iValue = -1;

		try {
			iValue = (size_t)stoull(v.c_str());
		}
		catch (exception ex)
		{
			cerr << endl << "ERROR with configuration file, setting has a wrong or unexpected value: [" << ConfigurationName << " = " << GetConfigurationSetting(ConfigurationName) << "]" << endl;
			exit(1);
		}

		retVal.push_back(iValue);
	}
	return retVal;
}

///
/// Get the value for a specific configuration setting
///
vector<bool> Configuration::GetConfigurationSettingBoolArray(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	vector<bool> retVal;
	string value = GetConfigurationSetting(ConfigurationName);
	vector<string> sValues = SplitString(value, __CONFIGURATION_FILE_DELIMITER_VALUE, false);

	for (string &v : sValues)
	{

		if (v == "1" || v == "true" || v == "yes")
		{
			retVal.push_back(true);
		}
		else if (v == "0" || v == "false" || v == "no")
		{
			retVal.push_back(false);
		}
		else
		{
			cerr << endl << "ERROR with configuration file, setting has a wrong or unexpected value: [" << ConfigurationName << " = " << v << "]" << endl;
			exit(1);
		}
	}
	return retVal;
}

///
/// Get the value for a specific configuration setting
///
bool Configuration::GetConfigurationSettingBool(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	int retVal(false);
	string value = GetConfigurationSetting(ConfigurationName);
	transform(value.begin(), value.end(), value.begin(), ::tolower);

	if (value == "1" || value == "true" || value == "yes")
	{
		return true;
	}
	else if (value == "0" || value == "false" || value == "no")
	{
		return false;
	}
	else
	{
		cerr << endl << "ERROR in configuration file!" << endl;
		cerr << "Setting has a wrong or unexpected value: [" << ConfigurationName << " = " << value << "].Expected '1/0', 'true/false' or 'yes/no'." << endl;
		exit(1);
	}
}
///
/// Get the value for a specific configuration setting
///
bool Configuration::TryGetConfigurationSettingBool(string ConfigurationName, bool DefaultValue)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	int retVal(false);
	string value = GetConfigurationSetting(ConfigurationName, false);
	transform(value.begin(), value.end(), value.begin(), ::tolower);

	if (value == "1" || value == "true" || value == "yes" || value == "y")
	{
		return true;
	}
	else if (value == "0" || value == "false" || value == "no" || value == "n")
	{
		return false;
	}
	else
	{
		return DefaultValue;
	}
}



///
/// Get the value for a specific configuration setting
///
int Configuration::GetConfigurationSettingPercentage(string ConfigurationName)
{
	int retVal = GetConfigurationSettingNumber(ConfigurationName);

	if (retVal < 0 || retVal > 100)
	{
		cerr << endl << "ERROR with configuration file, out of boundary value provided for a percentage type setting: [" << ConfigurationName << " = " << GetConfigurationSetting(ConfigurationName) << "]" << endl;
		exit(1);
	}
	return retVal;

}



///
/// Get the value for a specific configuration setting
///
float Configuration::GetConfigurationSettingFloat(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	float retVal = -1.0;

	try {
		string s = GetConfigurationSetting(ConfigurationName);
		retVal = atof(s.c_str());
	}
	catch (exception ex)
	{
		cerr << endl << "ERROR with configuration file, setting has a wrong or unexpected value: [" << ConfigurationName << " = " << GetConfigurationSetting(ConfigurationName) << "]" << endl;
		exit(1);
	}

	return retVal;

}
///
/// Get the value for a specific configuration setting
///
int Configuration::GetConfigurationSettingNumber(string ConfigurationName)
{
	if (!this->ConfigurationFileRead)
		this->ReadConfigurationFile();

	int retVal = -1;

	try {
		string s = GetConfigurationSetting(ConfigurationName);
		retVal = atoi(s.c_str());
	}
	catch (exception ex)
	{
		cerr << endl << "ERROR with configuration file, setting has a wrong or unexpected value: [" << ConfigurationName << " = " << GetConfigurationSetting(ConfigurationName) << "]" << endl;
		exit(1);
	}

	return retVal;

}

void Configuration::SetFiles(Files * f)
{
	filesSet = true;
	files = f;
}

Files * Configuration::GetFiles()
{
	if (!filesSet)
		throw invalid_argument("File reference not set");
	return files;
}

Configuration::Configuration()
{
}

Configuration::~Configuration()
{
}