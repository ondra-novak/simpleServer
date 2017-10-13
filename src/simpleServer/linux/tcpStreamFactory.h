#include <memory>
#include "../abstractStreamFactory.h"
#include "../address.h"

namespace simpleServer {


class TCPStreamFactory: public AbstractStreamFactory {
public:

	///Retrieves local address of current stream factory
	virtual NetAddr getLocalAddress() const {return target;}

	///Retrieves local address of given stream factory
	/**
	 * @param sf reference to stream factory
	 * @return address associated with the stream factory
	 * @exception std::bad_cast argument is not TCPStreamFactory
	 */
	static NetAddr getLocalAddress(const StreamFactory &sf);
	///Retrieves peer address of given stream
	/**
	 * @param sf reference to stream
	 * @return peer address
	 * @exception std::bad_cast argument is not TCPStream
	 */
	static NetAddr getPeerAddress(const Stream &stream);


	virtual void stop() override {
		stopped = true;
	}

	TCPStreamFactory(NetAddr target,int ioTimeout)
		:target(target),ioTimeout(ioTimeout),stopped(false) {}
protected:

	NetAddr target;
	int ioTimeout;
	std::atomic<bool> stopped;

};


///connect factory
class TCPConnect: public TCPStreamFactory {
public:

	static StreamFactory create(NetAddr target,
			int connectTimeout=-1,
			int ioTimeout=-1);


protected:
	TCPConnect(NetAddr target, int connectTimeout, int ioTimeout);

	virtual Stream create() override;

	int connectTimeout;

};


Stream tcpConnect(NetAddr target, int connectTimeout=-1, int ioTimeout = -1);




class TCPListen: public TCPStreamFactory {
public:

	static StreamFactory create(NetAddr source,
			int listenTimeout = -1, int ioTimeout=-1);

	static StreamFactory create(bool localhost = true, unsigned int port=0,
			 int listenTimeout = -1, int ioTimeout=-1);

protected:

	TCPListen(NetAddr source, int listenTimeout, int ioTimeout);
	TCPListen(bool localhost, unsigned int port, int listenTimeout, int ioTimeout);
	virtual Stream create() override;
	~TCPListen();
	virtual void stop() override;

protected:

	int listenTimeout;
	std::vector<int> openSockets;


};


Stream tcpListen(NetAddr target, int listenTimeout, int ioTimeout = -1);



}