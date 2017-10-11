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
	SystemException(int err, const std::string &desc):err(err),desc(desc) {}
	SystemException(int err, std::string &&desc):err(err),desc(std::move(desc)) {}

	int getErrNo() const {return err;}

	std::string getMessage() const;


protected:
	int err;
	std::string desc;

};

class TimeoutException: public Exception {
public:
	TimeoutException() {}


	std::string getMessage() const {
		return "Timeout";
	}
};

class NoAsyncProviderException: public Exception {
public:
	NoAsyncProviderException() {}


	std::string getMessage() const {
		return "Asynchronous provider did not defined for the stream";
	}
};


class EndOfStreamException: public Exception {
public:
	std::string getMessage() const {
		return "Unexpected end of stream";
	}

};


}
