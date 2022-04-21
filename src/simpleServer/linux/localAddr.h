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
#include <unistd.h>
#include "socketObject.h"
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
	virtual std::string toString(bool) const override {
		char perms[20];
		if (permissions) {
			snprintf(perms,20,":%03o", permissions);
			return std::string(sockAddr.sun_path).append(perms);
		} else {
			return sockAddr.sun_path;
		}

	}
	virtual BinaryView toSockAddr() const override  {
		return BinaryView(reinterpret_cast<const unsigned char *>(&sockAddr), sizeof (sockAddr));
	}

	virtual RefCntPtr<INetworkAddress> getNextAddr() const override  {return nullptr;}
	virtual const INetworkAddress &unproxy() const override  {return *this;}

	unsigned int getPermissions() const {return permissions;}
	const char *getPath() const {return sockAddr.sun_path;}

	virtual SocketObject connect() const override {
		SocketObject s(socket(sockAddr.sun_family, SOCK_STREAM, 0));
		if (!s) throw SystemException(errno,"Failed to create socket");
		int nblock = 1;ioctl(s, FIONBIO, &nblock);
		::connect(s, reinterpret_cast<const struct sockaddr *>(&sockAddr),sizeof(sockAddr));
		return s;

	}
	virtual SocketObject listen() const override {
		struct stat buf;
		if (stat(sockAddr.sun_path, &buf) == 0) {
			if (buf.st_mode & S_IFSOCK) {
				unlink(sockAddr.sun_path);
			}
		}

		SocketObject s(socket(sockAddr.sun_family, SOCK_STREAM, 0));
		if (!s) throw SystemException(errno,"Failed to create socket");
		int enable=1;
		(void)ioctl(s, FIONBIO, &enable);
		if (::bind(s, reinterpret_cast<const struct sockaddr *>(&sockAddr),sizeof(sockAddr)) == -1) {
			int e = errno;
			throw SystemException(e,"Cannot bind socket to port:" + toString(false));
		}
		if (permissions) {
			if (chmod(sockAddr.sun_path,permissions) == -1) {
				int e = errno;
				throw SystemException(e,"cannot change permissions of the socket:" + toString(false));
			}
		}
		if (::listen(s,SOMAXCONN) == -1) {
			int e = errno;
			throw SystemException(e,"Cannot activate listen mode on the socket:" + toString(false));
		}
		return s;

	}

protected:
	sockaddr_un sockAddr;
	unsigned int permissions;

};

}

#endif /* SRC_SIMPLESERVER_SRC_SIMPLESERVER_LINUX_LOCALADDR_H_ */
