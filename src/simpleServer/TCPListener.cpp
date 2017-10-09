#include "TCPListener.h"

namespace simpleServer {




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
/*

void TCPListener::asyncAccept(const AsyncDispatcher &cntr, AsyncCallback callback, unsigned int timeoutOverride) {
	owner->asyncAccept(cntr, callback, timeoutOverride);
}

*/

Connection ITCPListener::Iterator::operator *() {
	return conn;
}

ITCPListener::Iterator& ITCPListener::Iterator::operator ++() {
	conn = owner->accept();
	return *this;
}

ITCPListener::Iterator ITCPListener::Iterator::operator ++(int) {
	Iterator res = *this;
	conn = owner->accept();
	return res;
}

bool ITCPListener::isBadConnection(const Connection& conn) {
	return conn == Connection(nullptr);
}


ITCPListener::Iterator ITCPListener::begin() {
	Connection c = accept();
	if (isBadConnection(c)) return end();
	return Iterator(this,c);
}

ITCPListener::Iterator ITCPListener::end() {
	return Iterator(this,Connection(nullptr));
}


bool ITCPListener::Iterator::operator ==(const Iterator& other) const {
	return owner == other.owner && conn == other.conn;
}

bool ITCPListener::Iterator::operator !=(const Iterator& other) const {
	return !operator==(other);
}

}
