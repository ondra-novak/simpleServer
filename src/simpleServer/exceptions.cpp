

#define _UNIX03_SOURCE
#include <cstring>
#include <sstream>
#include "exceptions.h"

namespace simpleServer {



std::string SystemException::getMessage() const {
	if (desc.empty()) {
		return std::strerror(err);
	} else {
		return desc + " - " + std::strerror(err);
	}
}


}

std::string simpleServer::HTTPStatusException::getMessage() const {
	std::ostringstream s;
	s << "HTTP Exception: " << code << message;
	return s.str();
}
