#pragma once
#include "address.h"
#include "refcnt.h"
#include "connection.h"
#include "stringview.h"
namespace simpleServer {

enum Range {
	///specify whole network (IPv4)
	network,
	///specify localhost area only (IPv4)
	localhost,
	///specify whole network (IPv6)
	network6,
	///specify localhost area only (IPv6)
	localhost6,
};



class ITCPListener: public RefCntObj {
public:

	class Iterator {
	public:

		Iterator(RefCntPtr<ITCPListener> owner, Connection firstConn):owner(owner),conn(firstConn) {}
		Connection operator *();
		Iterator &operator++();
		Iterator operator++(int);
		bool operator==(const Iterator &other) const;
		bool operator!=(const Iterator &other) const;

	protected:
		RefCntPtr<ITCPListener> owner;
		Connection conn;
	};



	Iterator begin();
	Iterator end();

	///Stop listening and shutdown the opened port
	/** Once listener is stopped, all threads blocked by accept are released */
	virtual void stop() = 0;

	typedef std::function<void(AsyncState, const Connection *)> AsyncCallback;


	virtual void asyncAccept(const AsyncControl &cntr, AsyncCallback callback, unsigned int timeoutOverride) = 0;

protected:

	virtual Connection accept() = 0;

	bool isBadConnection(const Connection& conn);

};


typedef RefCntPtr<ITCPListener> PTCPListener;

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


	typedef ITCPListener::Iterator Iterator;

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


	///Callback definition for assynchronous listening
	/**
	 * @param AsyncState state of asynchronous operation
	 * @param Connection * pointer to newly created connection. The pointer is defined only when AsyncState is asyncOk, otherwise it is nullptr. You have
	 * to copy the object to new variable
	 */
	typedef ITCPListener::AsyncCallback AsyncCallback;

	///Initialize asynchronous listening
	/**
	 *
	 * @param cntr object which probides asynchronous operation
	 * @param callback function which will be called on new connection
	 * @param timeoutOverride allows to override timeout (default=0 which means no override)
	 *
	 * @note The callback function accepts two arguments. The second argument must be nullptr, otherwise
	 * an error condition happened. In this case, the first argument is invalid and must not be used.
	 * The second argument then points to an exception object which describes the problem.
	 *
	 *
	 * @note There can be only one pending listen operation, otherwise the behaviour is undefined. You
	 * also have to avoid running synchronous and asynchronous listenning at time.
	 *
	 * The function stop() cancels asynchronous listener
	 *
	 */
	void asyncAccept(const AsyncControl &cntr, AsyncCallback callback, unsigned int timeoutOverride = 0);


protected:
	PTCPListener owner;

};


} /* namespace simpleServer */

