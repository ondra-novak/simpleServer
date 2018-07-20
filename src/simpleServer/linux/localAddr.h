/*
 * localAddr.h
 *
 *  Created on: 4. 1. 2018
 *      Author: ondra
 */

#ifndef SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_LOCALADDR_H_
#define SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_LOCALADDR_H_
#include <sys/socket.h>
#include <sys/un.h>
#include "../address.h"

#pragma once


namespace simpleServer {

class NetAddrSocket: public INetworkAddress {
public:

	NetAddrSocket(const std::string path, unsigned int permissions) {
		if (path.length() <= sizeof(sockAddr.sun_path)-1) {
			std::copy(path.c_str(),path.c_str()+path.length()+1, sockAddr.sun_path);
			this->permissions = permissions;
			sockAddr.sun_family = AF_UNIX;
		} else {
			throw std::runtime_error(std::string("Unix socket's path is too long:") + path);
		}

	}
	virtual std::string toString(bool) const {
		char perms[10];
		if (permissions) {
			snprintf(perms,10,":%03o", permissions);
			return std::string(sockAddr.sun_path).append(perms);
		} else {
			return sockAddr.sun_path;
		}

	}
	virtual BinaryView toSockAddr() const {
		return BinaryView(reinterpret_cast<const unsigned char *>(&sockAddr), sizeof (sockAddr));
	}

	virtual RefCntPtr<INetworkAddress> getNextAddr() const {return nullptr;}
	virtual const INetworkAddress &unproxy() const {return *this;}

	unsigned int getPermissions() const {return permissions;}
	const char *getPath() const {return sockAddr.sun_path;}

protected:
	sockaddr_un sockAddr;
	unsigned int permissions;

};

}




#endif /* SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_LOCALADDR_H_ */
