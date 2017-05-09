#include "TCPListener.h"

namespace simpleServer {


TCPListener::TCPListener(NetAddr addr, const ConnectParams& params)
:owner(new TCPListenerImpl(addr,params))
{
}

TCPListener::TCPListener(unsigned int port, Range range,const ConnectParams& params)
:owner(new TCPListenerImpl(port,range,params)) {
}

TCPListener::TCPListener(Range range, unsigned int& port,const ConnectParams& params)
:owner(new TCPListenerImpl(range,port,params))
{
}

TCPListener::TCPListener(PTCPListener other):owner(other) {

}

TCPListener::Iterator TCPListener::begin() {
	return owner->begin();
}

TCPListener::Iterator TCPListener::end() {
	return owner->end();
}

void TCPListener::stop() {
	owner->stop();
}






}
