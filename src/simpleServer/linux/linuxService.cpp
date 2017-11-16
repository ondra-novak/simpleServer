#include "linuxService.h"

#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "../abstractStreamFactory.h"

#include "../exceptions.h"
#include "tcpStreamFactory.h"
#include "../http_headers.h"

namespace simpleServer {


int ServiceControl::create(int argc, char **argv, StrViewA name, ServiceHandler handler) {
	try {
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
		} else if (command == "stop") {
			svc->stopOtherService();return 0;
		} else if (command == "wait") {
			return svc->postCommand("wait",ArgList(),std::cerr,-1,true);
		} else if (command == "restart") {
			svc->stopOtherService();
			//svc->postCommand( "stop", ArgList(), std::cerr);
			if (!svc->enterDaemon()) {
					return svc->waitForExitCode();
				}
			return svc->startService(name, handler, remainArgs);
		} else if (command == "status") {
			if (svc->checkPidFile()) return 0;
			else std::cerr << "Service not running" << std::endl;
			return 1;
		} else {
				svc->postCommand(command, remainArgs, std::cerr);
		}
		return 0;
	} catch (SystemException &e) {
		if (e.getErrNo() == ECONNREFUSED || e.getErrNo() == ENOENT) {
			std::cerr << "Service not running" << std::endl;
		} else {
			std::cerr << "ERROR: " << e.what() << std::endl;
		}
		return e.getErrNo();
	} catch (std::exception &e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		return ENOENT;
	}
}





void LinuxService::dispatch() {





	Stream s;
	setsid();
	if (umbilicalCord) {
		sendExitCode(0);
		close(umbilicalCord);
		umbilicalCord = 0;
	}


	 s = mother();

	while (s != nullptr) {
		processRequest(s);
		s = nullptr;
		 s = mother();
		 cleanWaitings();
	}
}

typedef StringPool<char> Pool;


Pool::String readLine(Pool &p, Stream s) {
	std::size_t m = p.begin_add();
	int c = s();
	while (c != -1 && c != '\n') {
		p.push_back(c);
		c = s();
	}
	return p.end_add(m);
}

void LinuxService::processRequest(Stream s) {

	Pool p;
	std::vector<Pool::String> args;
	Pool::String item = readLine(p,s);
	while (!item.getView().empty()) {
		args.push_back(item);
		item = readLine(p,s);
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
			s << "ERROR: command '" << argList[0] << "' is not supported~-1";

		} else {
			int ret =  it->second(argList, s);
			s << "~" << ret;
		}
		s.flush();
	}


}

void LinuxService::addCommand(StrViewA command,
		UserCommandFn fn) {
	cmdMap[std::string(command)] = fn;
}

void LinuxService::stop() {
	mother.stop();
}

bool LinuxService::enterDaemon() {

	int fds[2];
	pipe2(fds, O_CLOEXEC);
	int fres = fork();
	if (fres == 0) {
		daemonEntered = true;
//		std::cout << "press enter after debugger attach" << std::endl;
	//	std::cin.get();
		umbilicalCord = fds[1];
		close(fds[0]);
		return true;
	} else if (fres == -1) {
		int e = errno;
		throw SystemException(e, __FUNCTION__);
	}else {
		umbilicalCord = fds[0];
		close(fds[1]);
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

		if (checkPidFile()) return 255;
		mother = TCPListen::create(createNetAddr(),-1,30000);

		ServiceControl me(this);
		addCommand("stop",[=](ArgList, Stream sx) {
			me.stop();
			waitEnd.push(sx);
			return getpid();
		});
		addCommand("status",[=](ArgList, Stream sx) {
			sx << "Service '" << name << "' runnning as pid " << getpid() << "\n";
			return 0;
		});
		addCommand("wait",[=](ArgList, Stream sx) {
			waitEnd.push(sx);
			return 0;
		});

		int ret = hndl(this, name, args);
		unlink(controlFile.c_str());
		sendExitCode(ret);

	} catch (...) {
		unlink(controlFile.c_str());
		sendExitCode(253);
		throw;
	}

}

LinuxService::LinuxService(std::string controlFile):umbilicalCord(0),controlFile(controlFile) {


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

int LinuxService::postCommand(StrViewA command, ArgList args, std::ostream &output, int timeout , bool timeoutIsEnd) {
		Stream s = tcpConnect(createNetAddr(),1000,timeout);
		s << command << "\n";
		for(StrViewA x : args) {
			s << x << "\n";
		}
		s << "\n";
		s.flush();

		int i = s();
		int exitCode = 0;
		bool hasExitCode = false;
		bool collectExitCode = false;
		try {
			while (i != -1) {
				if (collectExitCode) {
					if (isdigit(i)) {
						exitCode = exitCode * 10 + (i-'0');
						hasExitCode = true;
					} else {
						if (collectExitCode) {
							output << '~';
							collectExitCode = false;
						}
						if (hasExitCode) {
							output << exitCode;
							hasExitCode = false;
						}
						output.put(i);
					}
				} else if (i == '~') {
					collectExitCode = true;
					exitCode= 0;
				} else {
					output.put(i);
				}
				i = s();
			}
		} catch (TimeoutException &e) {
			if (timeoutIsEnd && hasExitCode) {
				return exitCode;
			} else {
				throw;
			}
		}

		return exitCode;
}

bool LinuxService::checkPidFile() {
	if (access(controlFile.c_str(),0) == 0) {
		try {
			postCommand("status",ArgList(), std::cerr);
			return true;
		} catch (...) {
			unlink(controlFile.c_str());
			return false;
		}
	}
}

void LinuxService::stopOtherService() {
	auto b = std::chrono::steady_clock::now();
	pid_t p = (pid_t)postCommand("stop", ArgList(), std::cerr,30000,true);
	auto e = std::chrono::steady_clock::now();
	if (p) {
		auto d = std::chrono::duration_cast<std::chrono::seconds>(e - b);
		if (d.count() > 29) {
			if (kill(p, SIGKILL) == 0) {
				std::cerr << "Terminated! (pid=" <<p<<")" <<std::endl;
			}
		}
	}
}

void LinuxService::cleanWaitings() {
	auto cnt = waitEnd.size();
	for (decltype(cnt) i = 0; i < cnt; i++) {
		Stream s ( waitEnd.front());
		waitEnd.pop();
		if (s.waitForInput(0) == false)
			waitEnd.push(s);
	}
}

void LinuxService::enableRestart() noexcept {
	if (restartEnabled || umbilicalCord == 0) return;
	do {
		time_t startTime;
		time(&startTime);
		/*
		int input[2];
		int output[2];

		if (pipe2(input,O_CLOEXEC)) {
			int e = errno;
			throw SystemException(e, "pipe2 input");
		}
		if (pipe2(output,O_CLOEXEC)) {
			int e = errno;
			throw SystemException(e, "pipe2 output");
		}
		*/
		pid_t chld = fork();
		if (chld == 0) break;
		if (chld == -1) {
			int e = errno;
			if (e != EINTR) {
				throw SystemException(e, "fork");
			}
		}

		if (umbilicalCord) {
			close(umbilicalCord);
			umbilicalCord = 0;
		}

		int status;
		if (waitpid(chld,&status, 0) == -1) {
			int e = errno;
			if (e != EINTR) {
				throw SystemException(e, "waitpid");
			}
		}

		if (WIFEXITED(status)) {
			exit(WEXITSTATUS(status));
		}
		if (WIFSIGNALED(status)) {
			int signal =  WTERMSIG(status);
			if (signal == SIGTERM|| signal == SIGKILL)
				exit(0);
		}

		time_t endTime;
		time(&endTime);
		if (endTime - startTime < 5) {
			exit(255);
		}
	}
	while (true);
	restartEnabled = true;
}

bool LinuxService::isDaemon() const {
	return daemonEntered;
}

}

