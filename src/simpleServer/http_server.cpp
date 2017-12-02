/*
 * http_server.cpp
 *
 *  Created on: 26. 10. 2017
 *      Author: ondra
 */

#include "http_server.h"

#include <thread>

#include "threadPoolAsync.h"


#include "tcp.h"

namespace simpleServer {


static StreamFactory initializeStreamFactory(NetAddr port) {
	return TCPListen::create(port,-1,30000);
}

static AsyncProvider initializeAsyncProvider(unsigned int threads, unsigned int dispatchers) {
	if (threads == 0) threads = std::thread::hardware_concurrency();
	if (threads == 0) threads = 1;
	if (dispatchers == 0) dispatchers = 1;
	if (threads < dispatchers) threads = dispatchers;
	return ThreadPoolAsync::create(threads,dispatchers);
}


MiniHttpServer::MiniHttpServer(NetAddr port, unsigned int threads, unsigned int dispatchers)
:MiniHttpServer(initializeStreamFactory(port),initializeAsyncProvider(threads,dispatchers))
{

}

MiniHttpServer::MiniHttpServer(StreamFactory sf,unsigned int threads, unsigned int dispatchers)
:MiniHttpServer(sf,initializeAsyncProvider(threads,dispatchers))

{
}

MiniHttpServer::MiniHttpServer(NetAddr port, AsyncProvider asyncProvider)
:MiniHttpServer(initializeStreamFactory(port),asyncProvider)

{
}

static RefCntPtr<_intr::MiniServerImpl> initServerCore(StreamFactory sf, AsyncProvider ap) {
	RefCntPtr<_intr::MiniServerImpl> srv = new _intr::MiniServerImpl;
	srv->setAp(ap);
	srv->setSf(sf);
	return srv;
}

MiniHttpServer::MiniHttpServer(StreamFactory sf, AsyncProvider asyncProvider)
:srv(initServerCore(sf,asyncProvider)), onError(srv->ehndl), preHandler(srv->preHandler)
{
}

MiniHttpServer::~MiniHttpServer() {
	if (running) srv->stopCycle();
}

MiniHttpServer &MiniHttpServer::operator >>(HTTPHandler handler) {
	if (!running) {
		srv->setHndl(handler);
		srv->runCycle();
		running = true;
	}
	return *this;
}



void _intr::MiniServerImpl::runCycle() {

	RefCntPtr<MiniServerImpl> me(this);
	sf(ap, [=](AsyncState st, Stream s){

		if (st == asyncOK) {
			if (me->preHandler != nullptr) {
				//preHandler defined, wait for first bytes
				s.readAsync([=](AsyncState st, BinaryView b) {
					//put the buffer back - will be read by preHandler
					s.putBack(b);
					//try preHandler
					if (!me->preHandler(s)) {
						//if preHandler rejected request, continue with http
						HTTPRequest::parseHttp(s, me->hndl, true);
					}
				});
				me->runCycle();
			} else {

				HTTPRequest::parseHttp(s, me->hndl, true);
				me->runCycle();
			}
		} else {
			if (me->ehndl) me->ehndl();
		}
	});

}

void _intr::MiniServerImpl::stopCycle() {
	ap.stop();
	sf.stop();
}


} /* namespace simpleServer */

