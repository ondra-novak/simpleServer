#include "linuxService.h"

#include <fcntl.h>
#include <unistd.h>

#include "../exceptions.h"

namespace simpleServer {


int ServiceControl::create(int argc, char **argv, StrViewA name, ServiceHandler handler) {

	RefCntPtr<LinuxService> svc = new LinuxService;

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

	if (command == "start") {
		if (!svc->enterDaemon()) {
			return waitForExitCode();
		}
		return svc->startService(name, handler, remainArgs);
	} else if (command == "run") {
		return svc->startService(name, handler, remainArgs);
	} else if (command == "restart") {
		svc->postCommand(pidfile, "stop", ArgList());
		if (!svc->enterDaemon()) {
				return waitForExitCode();
			}
		return svc->startService(name, handler);
	} else {
		svc->postCommand(pidfile,command, remainArgs);
	}
	return 0;
}





void LinuxService::dispatch() {
}

void LinuxService::addCommand(StrViewA command,
		UserCommandFn fn) {
}

void LinuxService::stop() {
}

bool LinuxService::enterDaemon() {

	int fds[2];
	pipe2(fds, O_CLOEXEC);
	int fres = fork();
	if (fres == 0) {
		pupecniSnura = fds[1];
		close(fds[0]);
		return true;
	} else {
		pupecniSnura = fds[0];
		return false;
	}

}

int LinuxService::waitForExitCode() {
	if (pupecniSnura == 0) return 254;
	int buffer;
	int e = read(pupecniSnura, &buffer, sizeof(buffer));
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

		int ret = hndl(this, name, args);
		sendExitCode(ret);

	} catch (...) {
		sendExitCode(253);
	}

}

void LinuxService::sendExitCode(int code) {
	if (pupecniSnura != 0) {
		int i = write(pupecniSnura, &code, sizeof(code));
		if (i == -1) {
			int err = errno;
			throw SystemException(err,__FUNCTION__);
		}
	}
}
}
