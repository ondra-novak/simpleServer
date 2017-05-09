#pragma once
#include "address.h"
#include "connection.h"
#include "refcnt.h"

namespace simpleServer {


enum Range {
	network,
	localhost
};



class TCPListenerImpl: public RefCntObj {
public:





	///Create TCPListener bound to specified address in the network
	/**
	 * @param addr network address
	 * @param waitTimeout specify how long it can wait for new connection. Default is infinite
	 */
	explicit TCPListenerImpl(NetAddr addr, const ConnectParams &params = ConnectParams());
	explicit TCPListenerImpl(unsigned int port, Range range, const ConnectParams &params = ConnectParams());
	explicit TCPListenerImpl(Range range, unsigned int &port, const ConnectParams &params = ConnectParams());
	~TCPListenerImpl();

	class Iterator {
	public:

		Iterator(RefCntPtr<TCPListenerImpl> owner, Connection firstConn):owner(owner),conn(firstConn) {}
		Connection operator *();
		Iterator &operator++();
		Iterator operator++(int);
		bool operator==(const Iterator &other) const;
		bool operator!=(const Iterator &other) const;

	protected:
		RefCntPtr<TCPListenerImpl> owner;
		Connection conn;
	};


	Iterator begin();
	Iterator end();

	///Stop listening and shutdown the opened port
	/** Once listener is stopped, all threads blocked by accept are released */
	void stop();


protected:

	///accept next connection
	/** Function is MT-safe. Multiple thread can accept connections
	 *
	 * Function accepts first connection in the queue. If there is no connection,
	 * then function blocks for specified timeout. The waiting can be interrupted
	 * by calling close()
	 *
	 * */

	Connection accept();

	static bool isBadConnection(const Connection &conn);


	void waitForConnection();
	Connection createConnection(int sock, const BinaryView &addrInfo);

	int sock;
	ConnectParams params;

};

class TCPListenerImpl;

typedef RefCntPtr<TCPListenerImpl> PTCPListener;



}
