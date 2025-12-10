#include "Screen.h"
#include <string>
#include <iostream>
bool Screen::Screen_Resize = true;
int Screen::SCREEN_COLS = 160; // Console screen will get this width
int Screen::SCREEN_ROWS = 60; // Console screen will get this height

///
/// Change window size
///
void Screen::SetWindowSize()
{
#ifdef _WIN32
	// Set screen size in Windows
	try {
		if (Screen_Resize) {
			string sString;
			sString = "mode CON: cols=";
			sString += to_string(SCREEN_COLS);
			sString += " lines=";
			sString += to_string(SCREEN_ROWS);
			system(sString.c_str());
		}
	}
	catch (...) {}
#endif
}

///
/// Set screen settings
///
void Screen::InitialiseScreenSettings(LogToScreen* lts, LogToScreen* lrts, Output_Modes* om)
{
	cout.imbue(locale("")); // Set locale
	*lts << left; // Make screen left outlined
	lts->Enabled = ((*om) == Output_Modes::Default || (*om) == Output_Modes::ProgressBar); // Enable only when log to screen is required
	lrts->Enabled = ((*om) == Output_Modes::ResultsNoLog || (*om) == Output_Modes::Results); // Log results directly to screen if required
#ifdef _WIN32
	SetWindowSize();
#endif
}

Screen::Screen()
{
}


Screen::~Screen()
{
}
