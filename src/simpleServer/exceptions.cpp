
#define _UNIX03_SOURCE
#include <cstring>
#include "exceptions.h"

namespace simpleServer {


std::string SystemException::getMessage() const {
	return std::strerror(err);
}

}
