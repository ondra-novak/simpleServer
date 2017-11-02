#include "linuxService.h"

#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>

#include "../abstractStreamFactory.h"

#include "../exceptions.h"
#include "tcpStreamFactory.h"
#include "../http_headers.h"

namespace simpleServer {


int ServiceControl::create(int argc, char **argv, StrViewA name, ServiceHandler handler) {


	if (argc < 3) {
		throw ServiceInvalidParametersException();
	}


	StrViewA tmp_arglist[argc];
	for (int i = 0; i < argc; i++) {
		tmp_arglist[i] = StrViewA(argv[i]);
	}
	ArgList arglist(tmp_arglist, argc);


	StrViewA pidfile = arglist[1];
	StrViewA command = arglist[2];
	ArgList remainArgs = arglist.substr(3);

	RefCntPtr<LinuxService> svc = new LinuxService(pidfile);


	if (command == "start") {
		if (!svc->enterDaemon()) {
			return svc->waitForExitCode();
		}
		return svc->startService(name, handler, remainArgs);
	} else if (command == "run") {
		return svc->startService(name, handler, remainArgs);
	} else if (command == "restart") {
		svc->postCommand( "stop", ArgList());
		if (!svc->enterDaemon()) {
				return svc->waitForExitCode();
			}
		return svc->startService(name, handler, remainArgs);
	} else {
		svc->postCommand(command, remainArgs);
	}
	return 0;
}





void LinuxService::dispatch() {



	Stream s;
	StreamFactory mother = TCPListen::create(createNetAddr(),-1,30000);
	if (umbilicalCord) {
		close(umbilicalCord);
		umbilicalCord = 0;
	}

	stopFunctionPromise.set_value([mother] {mother.stop();});

	 s = mother();

	while (s != nullptr) {
		processRequest(s);
		 s = mother();
	}
}

typedef StringPool<char> Pool;


Pool::String readLine(Pool &p, Stream s) {
	std::size_t m = p.begin_add();
	int c = s();
	while (c != -1 && c != '\n') p.push_back(c);
	return p.end_add(m);
}

void LinuxService::processRequest(Stream s) {

	Pool p;
	std::vector<Pool::String> args;
	Pool::String item = readLine(p,s);
	while (!item.getView().empty()) {
		args.push_back(item);
	}

	StrViewA argbuf[args.size()];
	for (std::size_t i =0, cnt = args.size(); i < cnt; ++i) {
		argbuf[i] = args[i].getView();
	}

	ArgList argList(argbuf, args.size());
	if (argList.empty()) {
		return;
	} else {
		auto it =cmdMap.find(argList[0]);
		if (it == cmdMap.end()) {
			s << "ERROR: command not supported:251";
		}

		int ret =  it->second(argList, s);
		s << ":" << ret;
	}


}

void LinuxService::addCommand(StrViewA command,
		UserCommandFn fn) {
	cmdMap[std::string(command)] = fn;
}

void LinuxService::stop() {
	auto fn = stopFunction.get();
	fn();
}

bool LinuxService::enterDaemon() {

	int fds[2];
	pipe2(fds, O_CLOEXEC);
	int fres = fork();
	if (fres == 0) {
		umbilicalCord = fds[1];
		close(fds[0]);
		return true;
	} else {
		umbilicalCord = fds[0];
		return false;
	}

}

int LinuxService::waitForExitCode() {
	if (umbilicalCord == 0) return 254;
	int buffer;
	int e = read(umbilicalCord, &buffer, sizeof(buffer));
	if (e == -1) {
		int err = errno;
		throw SystemException(err,__FUNCTION__);
	}
	if (e == 0) {
		return 254;
	} else {
		return buffer;
	}
}

int LinuxService::startService(StrViewA name, ServiceHandler hndl,
		ArgList args) {

	try {

		ServiceControl me(this);
		addCommand("stop",[=](ArgList, Stream) {
			me.stop();return 0;
		});

		int ret = hndl(this, name, args);
		sendExitCode(ret);

	} catch (...) {
		sendExitCode(253);
		throw;
	}

}

LinuxService::LinuxService(std::string controlFile):umbilicalCord(0),controlFile(controlFile),stopFunction(stopFunctionPromise.get_future()) {

	if (access(controlFile.c_str(),0) == 0) {
		try {
			tcpConnect(createNetAddr(),-1,-1);
		} catch (...) {
			unlink(controlFile.c_str());
		}
	}


}

LinuxService::~LinuxService() {
	if (umbilicalCord) close(umbilicalCord);
}

void LinuxService::sendExitCode(int code) {
	if (umbilicalCord != 0) {
		int i = write(umbilicalCord, &code, sizeof(code));
		if (i == -1) {
			int err = errno;
			throw SystemException(err,__FUNCTION__);
		}
	}
}

#ifndef SUN_LEN
# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)	      \
		      + strlen ((ptr)->sun_path))
#endif


NetAddr LinuxService::createNetAddr() {
	sockaddr_un sun;
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, controlFile.data(), sizeof(sun.sun_path));
	sun.sun_path[sizeof(sun.sun_path)-1] = 0;
	return NetAddr::create(BinaryView(reinterpret_cast<unsigned char *>(&sun), sizeof(sun)));
}

int LinuxService::postCommand(StrViewA command, ArgList args) {
	Stream s = tcpConnect(createNetAddr(),30000,30000);
	s << command << "\n";
	for(StrViewA x : args) {
		s << x << "\n";
	}

	int i = s();
	int exitCode = 0;
	bool hasExitCode = false;
	while (i != -1) {
		if (isdigit(i) && exitCode < std::numeric_limits<int>::max()/10) {
			hasExitCode = true;
			exitCode = exitCode * 10 + i;
		} else {
			if (hasExitCode) {
				std::cerr << i;
				hasExitCode = false;
			}
			exitCode = 0;
			std::cerr.put((char)i);
		}
		i = s();
	}

	return exitCode;

}

}
