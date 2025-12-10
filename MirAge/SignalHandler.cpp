/*
*	Signal handler to detect errors during runtime
*/
#include "SignalHandler.h"

bool SignalHandler::Signal_CTRLC_Detected = false;
bool SignalHandler::ProgramSafeToKill = false;

///
/// Handle application signal by ending the program
///
#ifdef __linux__
void ExitSignalHandler(int signalID) {
	cout << endl << "Kill signal received. (" << signalID << ")" << endl;
	_Exit(EXIT_FAILURE); // MultiThread correct exit
}
#else
void SignalHandler::signalDetected(int signalID) {
#if defined(__CUDA__) || defined(__OPENCL__)
	if (Signal_CTRLC_Detected && ProgramSafeToKill)
		_Exit(EXIT_FAILURE);

	Signal_CTRLC_Detected = true;
#else
	_Exit(EXIT_FAILURE); // MultiThread correct exit
#endif
}
#endif

///
/// Listen to application signal, such us user interupt "CTRL-C" combination
///
void SignalHandler::ListenToSignalHandlers()
{
#ifdef __linux__
	signal(SIGSEGV, ExitSignalHandler); //	SIGSEGV	invalid memory access(segmentation fault)
	signal(SIGTERM, ExitSignalHandler); //  SIGTERM	termination request, sent to the program
	signal(SIGINT, ExitSignalHandler); //	SIGINT	external interrupt, usually initiated by the user
	signal(SIGILL, ExitSignalHandler); //	SIGILL	invalid program image, such as invalid instruction
	signal(SIGFPE, ExitSignalHandler); //	SIGFPE	erroneous arithmetic operation such as divide by zero
	signal(SIGABRT, ExitSignalHandler); //	SIGABRT	abnormal termination condition, as is e.g.initiated by abort()
	signal(137, ExitSignalHandler); //	
	signal(135, ExitSignalHandler); //	
#else
	signal(SIGINT, signalDetected);
#endif
}


///
/// Pause the program if CTRL-C is pressed.
/// This allows GPU runs to be finished, since killing the associated thread may crash the display driver
///
void SignalHandler::PauseProgramByUserRequest()
{
	if (Signal_CTRLC_Detected)
	{
		display->DisplayNewLine();
		display->DisplayNewLine();
		display->DisplayValue("Pausing program, please wait... (CTRL - 'C' is available now, but not recommended to use since you might crash your hardware... be patient)", true);
		ProgramSafeToKill = true;

		// Wait until all threads are finished
		while (multiThreading->runningThreadsAnalysis > 0 && multiThreading->runningThreadsBuildHashMap > 0 && multiThreading->runningThreadsAnalysing)
		{
			multiThreading->TerminateFinishedThreads(false);
			multiThreading->SleepGPU();
		}
		display->DisplayValue("It is now safe to kill this process (CTRL + 'C') or press ENTER key if you would like to continue.", true);

		// Wait for an user input
		char a = ' ';
		while (a != '\n')
			a = cin.get();

		// input received, user did not kill current process. So continue!
		ProgramSafeToKill = false;
		Signal_CTRLC_Detected = false;
		display->DisplayValue("ENTER key detected. Resume analysis...", true);
		display->DisplayNewLine();
	}
}


SignalHandler::SignalHandler(Display* d, MultiThreading* m)
{
	display = d;
	multiThreading = m;
}


SignalHandler::~SignalHandler()
{
}
