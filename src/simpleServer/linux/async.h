#pragma once

namespace simpleServer {


class AsyncResource {
public:

	AsyncResource(int socket):socket(socket) {}

	int socket;

};




}
