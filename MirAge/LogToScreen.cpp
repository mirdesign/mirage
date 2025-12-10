///
/// Log information to stream
///
#include "LogToScreen.h"


LogToScreen::~LogToScreen()
{
	delete ssBuffer;
}

///
/// Log data to stream directly or in buffer
///
LogToScreen & LogToScreen::operator<<(std::ostream &(*data)(std::ostream &))
{
	if (!Enabled)
		return *this;

	if (DirectFlush)
	{
		std::cout << data;
		std::cout.flush();
	}
	else
	{
		(*ssBuffer) << data;
	}
	return *this;
}

///
/// Place future logs into buffer until flush is called
///
void LogToScreen::MuteScreen()
{
	DirectFlush = false;
}

///
/// Flush stream
///
void LogToScreen::Flush()
{
	cout.flush();
}

///
/// Flush buffered logs
///
string LogToScreen::FlushBuffer() {
	std::string retVal = (*ssBuffer).str();
	ssBuffer->clear();
	ssBuffer->str("");
	return retVal;
}