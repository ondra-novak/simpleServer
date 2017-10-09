
#define _UNIX03_SOURCE
#include <cstring>
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
