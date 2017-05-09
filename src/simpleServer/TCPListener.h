#pragma once
#include "address.h"
#include "refcnt.h"
#include "connection.h"
#include "stringview.h"
#include "tcplistenimpl.h"
namespace simpleServer {


///Listens for incoming connections
/**
 * Opens socket at specified port and/or interface and accepts any incoming connection
 *
 *
 * @code
 * TCPListener l(1234,localhost);
 * for(Connection c: l) {
 *     //... work with connection c ...//
 * }
 * @endif
 *
 * Object is MT safe. It is allowed to have multiple threads waiting on single listener
 * and other thread which can eventually call stop() to release listening threads
 */
class TCPListener {
public:


	///Open listening socket at specified network address
	/**
	 * @param addr network address
	 * @param params additional parameters (timeout, factory, etc)
	 */
	explicit TCPListener(NetAddr addr, const ConnectParams &params = ConnectParams());
	///Open listening socket at specified network port
	/**
	 * @param port port number
	 * @param range You can specify either 'network' to open port to whole network or
	 *              'localhost' to allow only localhost connections
	 * @param params additional parameters (timeout, factory, etc)
	 */
	explicit TCPListener(unsigned int port, Range range, const ConnectParams &params = ConnectParams());
	///Open listening socket at first unused port
	/**
	 * @param range You can specify either 'network' to open port to whole network or
	 *              'localhost' to allow only localhost connections
	 * @param port reference to a variable which receives port number
	 * @param params additional parameters (timeout, factory, etc)
	 */
	explicit TCPListener(Range range, unsigned int &port, const ConnectParams &params = ConnectParams());

	///Used internally
	TCPListener(PTCPListener other);


	typedef TCPListenerImpl::Iterator Iterator;

	///Initiates listening and returns iterator to first connection
	/**
	 * Function can block until the first connection is accepted. Returned
	 * iterator carries the connection. Advancing the iterator though the ++ operator
	 * cause accepting next connection. When listening is stopped, iterator
	 * is set to end()
	 *
	 * @return iterator through the connections
	 */
	Iterator begin();
	///Returns end iterator, which can be used only for comparison.
	/**
	 * @return Iteration reaches the end after the listener is stopped (see stop())
	 */
	Iterator end();

	///Stops the listener
	/**
	 * Once the listener is stopped, iterators advances to end()
	 */
	void stop();


protected:
	PTCPListener owner;

};


} /* namespace simpleServer */

