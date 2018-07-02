#pragma once

namespace simpleServer {


class AsyncResource {
public:

	AsyncResource(int socket, int op)
	:socket(socket)
	,op(op) {}

	AsyncResource():socket(0),op(0) {}

	int socket;
	int op;

};




}
