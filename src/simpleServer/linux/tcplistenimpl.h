#pragma once
#include "../TCPListener.h"

namespace simpleServer {




class TCPListenerImpl: public ITCPListener {
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


	///Stop listening and shutdown the opened port
	/** Once listener is stopped, all threads blocked by accept are released */
	void stop();



//	void asyncAccept(const AsyncDispatcher &cntr, AsyncCallback callback, unsigned int timeoutOverride);

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





}
