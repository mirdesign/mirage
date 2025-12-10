#pragma once
#ifndef Screen_H
#define Screen_H

#include "LogToScreen.h"
#include "Analyser.h"
using namespace std;

class Screen
{
public:
	/********************
	*
	* Windows Console
	*
	*********************/
	static bool Screen_Resize;
	static int SCREEN_COLS; // Console screen will get this width
	static int SCREEN_ROWS; // Console screen will get this height
	static void SetWindowSize();
	void InitialiseScreenSettings(LogToScreen * lts, LogToScreen * lrts, Output_Modes* om);
	Screen();
	~Screen();
};
#endif
