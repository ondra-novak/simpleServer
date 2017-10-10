#include "tcpStreamFactory.h"



namespace simpleServer {


NetAddr TCPStreamFactory::getLocalAddress(const StreamFactory& sf) {

	const TCPStreamFactory &f = dynamic_cast<const TCPStreamFactory &>(*sf);
	return f.getLocalAddress();

}

NetAddr TCPStreamFactory::getPeerAddress(const Stream& stream) {
	const TCPStream &f = dynamic_cast<const TCPStream &>(*stream);
	return f.getPeerAddr();
}


StreamFactory TCPConnect::create(NetAddr target,
		int connectTimeout, int ioTimeout) {

	return new TCPConnect(target, connectTimeout, ioTimeout);

}

Stream TCPConnect::create() {
	std::vector<int> sockets;

	NetAddr t = target;
	bool isNext;

	do {


		t.getNextAddr()

	} while (isNext)


}

}
