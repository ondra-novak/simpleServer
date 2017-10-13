#pragma once

namespace simpleServer {


class AsyncResource {
public:

	AsyncResource(int socket, RefCntPtr<AbstractStream> stream):socket(socket),stream(stream) {}

	int socket;
	RefCntPtr<AbstractStream> stream;

};




}
