

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



std::string HTTPStatusException::getMessage() const {
	std::ostringstream s;
	s << "HTTP Exception: " << code << " " << message;
	return s.str();
}

UnsupportedURLSchema::UnsupportedURLSchema(std::string url):url(url) {

}

std::string UnsupportedURLSchema::getMessage() const {
	std::ostringstream s;
	s << "Unsupported schema: " << url;
	return s.str();

}

HttpsIsNotEnabled::HttpsIsNotEnabled(std::string addrport):addrport(addrport) {
}

std::string HttpsIsNotEnabled::getMessage() const {
	std::ostringstream s;
	s << "HTTPS is not enabled: " << addrport;
	return s.str();
}

}
