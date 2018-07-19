#pragma once

#include "../raii.h"

namespace simpleServer {

extern int invalidSocketValue;
class SocketObject: public ondra_shared::Handle<int, decltype(&::close), &::close, &invalidSocketValue> {

	using Handle<int, decltype(&::close), &::close, &invalidSocketValue>::Handle;

};

}
