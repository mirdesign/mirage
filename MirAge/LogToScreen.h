#pragma once
#ifndef LOGTOSCREEN_H
#define LOGTOSCREEN_H

#include <ostream>
#include <iostream>
#include <sstream>  // stringstream
using namespace std;


class LogToScreen {

private:
	std::stringstream* ssBuffer = new std::stringstream();

public:
	bool Enabled = true;
	bool DirectFlush = true;


	//Templated operator>> that uses the std::ostream: Everything that has defined 
	//an operator<< for the std::ostream (Everithing "printable" with std::cout 
	//and its colleages) can use this function.    
	template<class T> LogToScreen &operator <<(const T& data);
	LogToScreen &operator <<(std::ostream& (*data)(std::ostream&)); // So 'endl' is also accepted

	void MuteScreen();
	void Flush();
	string FlushBuffer();
	~LogToScreen();
};



template<class T>
inline LogToScreen & LogToScreen::operator<<(const T & data)
{
	if (!Enabled)
		return *this;

	if (DirectFlush)
	{
		cout << data;
		cout.flush();
	}
	else
	{
			(*ssBuffer) << data;
	}
	return *this;
}

#endif
