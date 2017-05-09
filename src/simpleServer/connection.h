#pragma once

#include <functional>

#include "address.h"
#include "refcnt.h"
#include "stringview.h"

namespace simpleServer {

class IConnection: public RefCntObj {
public:
	virtual ~IConnection() {}

	virtual BinaryView readData(unsigned int prevRead) = 0;
	virtual void writeData(const BinaryView &data) = 0;
	virtual void flush() = 0;
	virtual void closeOutput() = 0;
	virtual void closeInput() = 0;

	virtual unsigned int getIOTimeout() const = 0;
	virtual void setIOTimeout(unsigned int t) = 0;
	virtual NetAddr getPeerAddr() const  = 0;
};

typedef RefCntPtr<IConnection> PConnection;

class Connection;


static const unsigned int infinity = (unsigned int)-1;

typedef std::function<Connection(NetAddr, std::uintptr_t)> ConnectionFactory;

class ConnectParams {
public:
	unsigned int waitTimeout;
	ConnectionFactory factory;


	ConnectParams():waitTimeout(infinity),factory(nullptr) {}
	ConnectParams(unsigned int timeout):waitTimeout(timeout),factory(nullptr) {}
	ConnectParams(unsigned int timeout, const ConnectionFactory &f):waitTimeout(timeout),factory(f) {}
};

class Connection {
public:



	explicit Connection(PConnection conn):conn(conn) {}

	void flush() {conn->flush();}
	void closeOutput() {conn->closeOutput();}
	void closeInput() {conn->closeInput();}


	BinaryView operator()(unsigned int prevRead) {
		return conn->readData(prevRead);
	}

	void operator()(const BinaryView &data) {
		return conn->writeData(data);
	}

	NetAddr getPeerAddr() const {return conn->getPeerAddr();}

	unsigned int getIOTimeout() const {return conn->getIOTimeout();}
	void setIOTimeout(unsigned int t) {conn->setIOTimeout(t);}

	PConnection getHandle() const {return conn;}

	static Connection connect(const NetAddr &addr,const ConnectParams &params = ConnectParams());

	bool operator==(const Connection &c) const {
		return conn == c.conn;
	}
	bool operator!=(const Connection &c) const {
		return conn != c.conn;
	}

protected:

	PConnection conn;

};



}
