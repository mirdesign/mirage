#pragma once
#ifndef SIGNAL_H
#define SIGNAL_H
using namespace std;
#include <stdlib.h> // Exit
#include <signal.h>
#include "Display.h"
#include "MultiThreading.h"

class SignalHandler
{
private:


	static void signalDetected(int signalID);
	Display* display;
	MultiThreading* multiThreading;
public:
	static bool Signal_CTRLC_Detected;
	static bool ProgramSafeToKill;

	void ListenToSignalHandlers();
	void PauseProgramByUserRequest();
	SignalHandler(Display* d, MultiThreading* m);
	~SignalHandler();
};

#endif