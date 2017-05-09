#pragma once


#include <string>
#include <stdexcept>

namespace simpleServer {

class Exception: public virtual std::exception {
public:

	const char *what() const throw() {
		if (msg.empty()) msg = getMessage();
		return msg.c_str();
	}

	virtual  std::string getMessage() const = 0;

protected:
	mutable std::string msg;

};

class SystemException: public Exception {
public:

	SystemException(int err):err(err) {}

	int getErrNo() const {return err;}

	std::string getMessage() const;


protected:
	int err;

};

class TimeoutException: public Exception {
public:
	TimeoutException() {}


	std::string getMessage() const {
		return "Timeout";
	}
};

class EndOfStreamException: public Exception {
public:
	std::string getMessage() const {
		return "Unexpected end of stream";
	}

};


}
