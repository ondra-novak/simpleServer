#pragma once

namespace simpleServer {


class AsyncResource {
public:

	AsyncResource(int socket, int op)

	:socket(socket)
	,op(op) {}

	int socket;
	int op;

};




}
