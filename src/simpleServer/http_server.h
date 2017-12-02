#pragma once
#include "abstractStreamFactory.h"
#include "address.h"
#include "asyncProvider.h"
#include "http_parser.h"

namespace simpleServer {

class HTTPRequest;


typedef std::function<void()> ErrorHandler;
typedef std::function<bool(const Stream &sx)> PreHTTPHandler;

namespace _intr {

	class MiniServerImpl: public RefCntObj {
	public:

		const AsyncProvider& getAp() const {
			return ap;
		}

		void setAp(const AsyncProvider& ap) {
			this->ap = ap;
		}

		const ErrorHandler& getEhndl() const {
			return ehndl;
		}

		void setEhndl(const ErrorHandler& ehndl) {
			this->ehndl = ehndl;
		}

		const HTTPHandler& getHndl() const {
			return hndl;
		}

		void setHndl(const HTTPHandler& hndl) {
			this->hndl = hndl;
		}

		const StreamFactory& getSf() const {
			return sf;
		}

		void setSf(const StreamFactory& sf) {
			this->sf = sf;
		}

		void runCycle();
		void stopCycle();


		ErrorHandler ehndl;
		PreHTTPHandler preHandler;


	protected:

		StreamFactory sf;
		AsyncProvider ap;
		HTTPHandler hndl;


	};

}



class MiniHttpServer {
public:

	MiniHttpServer(NetAddr port, unsigned int threads, unsigned int dispatchers);
	MiniHttpServer(StreamFactory sf,unsigned int threads, unsigned int dispatchers);
	MiniHttpServer(NetAddr port, AsyncProvider asyncProvider);
	MiniHttpServer(StreamFactory sf, AsyncProvider asyncProvider);
	~MiniHttpServer();

	MiniHttpServer &operator >> (HTTPHandler handler);




protected:


	RefCntPtr<_intr::MiniServerImpl> srv;
	bool running = false;
public:
	///Put there a function called on connection error
	ErrorHandler &onError;
	///Put there a function called before http parser is called
	/** This allows to construct a hybrid service which supports multiple protocols.
	 *  Depends on the first bytes, the server can choose different protocol
	 *
	 *  @note preHandler shoudl putBack the read bytes if it rejected the request
	 */
	PreHTTPHandler &preHandler;


};
}

