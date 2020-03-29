#include "linuxService.h"

#include <limits>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <signal.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "../raii.h"
#include "../abstractStreamFactory.h"

#include "../exceptions.h"
#include "tcpStreamFactory.h"
#include "fileStream.h"
#include "../http_headers.h"
#include "../logOutput.h"
#include "../vla.h"
#include "fdstream.h"


namespace simpleServer {

using ondra_shared::AbstractLogProviderFactory;
using ondra_shared::VLA;
using ondra_shared::logInfo;
using ondra_shared::logRotate;

int ServiceControl::create(int argc, char **argv, StrViewA name, ServiceHandler &&handler) {
	if (argc < 3) {
		throw ServiceInvalidParametersException();
	}

	VLA<StrViewA, 10> tmp_arglist(argc);


	for (int i = 0; i < argc; i++) {
		tmp_arglist[i] = StrViewA(argv[i]);
	}
	ArgList arglist(tmp_arglist);


	StrViewA pidfile = arglist[1];
	StrViewA command = arglist[2];
	ArgList remainArgs = arglist.substr(3);
	return create(name,pidfile,command,std::move(handler),remainArgs);
}

int ServiceControl::create(StrViewA name, StrViewA pidfile, StrViewA command, ServiceHandler &&handler, ArgList remainArgs, bool forceStart) {

	bool handleExcept = false;
	try {


		RefCntPtr<LinuxService> svc = new LinuxService(pidfile);


		if (command == "start") {
			svc->onInit([&]{
				return svc->runCommand("run", ArgList(), Stream());
			});
			return svc->enterDaemon([&]{
				return svc->startService(name, std::move(handler), remainArgs);
			});

		} else if (command == "run") {
			svc->onInit([&]{
				return svc->runCommand("run", ArgList(), Stream());
			});
			return svc->startService(name, std::move(handler), remainArgs);
		} else if (command == "stop") {
			handleExcept = true;
			svc->stopOtherService();return 0;
		} else if (command == "wait") {
			handleExcept = true;
			return svc->postCommand("wait",ArgList(),std::cerr,-1,true);
		} else if (command == "restart") {
			handleExcept = true;
			svc->stopOtherService();
			handleExcept = false;
			svc->onInit([&]{
				return svc->runCommand("run", ArgList(), Stream());
			});
			return svc->enterDaemon([&]{
				return svc->startService(name, std::move(handler), remainArgs);
			});
		} else if (command == "status") {
			if (svc->checkPidFile()) {return 0;}
			else std::cerr << "Service '" << name.data << "' is not running" << std::endl;
			return 1;
		} else {
				if (forceStart && !svc->checkPidFileSilent()) {
					int retval = 255;
					svc->onInit([&]{
						auto ret = svc->runCommand(command, remainArgs, Stream(new FileStream(1)));
						svc->stop();
						retval = ret;
						return ret;
					});
					auto retval2 = svc->startService(name, std::move(handler), ArgList());
					if (retval2) return retval2;else return retval;
				} else {
					handleExcept = true;
					auto res = svc->postCommand(command, remainArgs, std::cerr);
					std::cerr << std::endl;
					return res;
				}
		}
		return 0;
	} catch (SystemException &e) {
		if (handleExcept && (e.getErrNo() == ECONNREFUSED || e.getErrNo() == ENOENT)) {
			std::cerr << "Service '" << name.data << "' is not running" << std::endl;
		} else {
			std::cerr << "ERROR: " << e.what() << std::endl;
		}
		return e.getErrNo();
	} catch (std::exception &e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		return ENOENT;
	}
}





int LinuxService::runCommand(StrViewA command, ArgList args, Stream s) {
	auto it =cmdMap.find(command);
	if (it== cmdMap.end()) {
		return 255;
	}
	return it->second(args,s);
}


void LinuxService::dispatch() {


	if (getuid() == 0) {
		logWarning("The service is starting under the root priviledge. This is not recomended for the security reasons.");
	}



	Stream s;
	// move to root (detach from mounted volume)
	if (chdir("/") != 0) {
		int err = errno;
		throw SystemException(err,"Failed to call chdir('/') (dispatch)");
	}
	logInfo("The service started, pid= $1",getpid());

	while (!onInitStack.empty()) {
		auto fn = std::move(onInitStack.top());
		onInitStack.pop();
		fn();
	}


	s = mother();



	while (s != nullptr) {
		processRequest(s);
		s = nullptr;
		 s = mother();
		 cleanWaitings();
	}

	logInfo("The service is exiting");
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

	try {
		Pool p;
		std::vector<Pool::String> args;
		Pool::String item = readLine(p,s);
		while (!item.getView().empty()) {
			args.push_back(item);
			item = readLine(p,s);
		}

		VLA<StrViewA, 10> argList(args.size());
		for (std::size_t i =0, cnt = args.size(); i < cnt; ++i) {
			argList[i] = args[i].getView();
		}

		if (argList.empty()) {
			return;
		} else {
			auto it =cmdMap.find(argList[0]);
			if (it == cmdMap.end()) {
				s << "ERROR: command '" << argList[0] << "' is not supported~255";

			} else {
				int ret =  it->second(argList.substr(1), s);
				s << "~" << ret;
			}
			s.flush();
		}
	} catch (std::exception &e) {
		logWarning("Failed to process command: $1",e.what());
		s << "ERROR: " << e.what() << "~255";
		s.flush();
	}


}

void LinuxService::addCommand(StrViewA command,
		UserCommandFn &&fn) {
	cmdMap.emplace(std::string(command),std::move(fn));
}

void LinuxService::stop() {
	mother.stop();
}

static void closefd(int fd) {
	close(fd);
}
static int invalid_fd = -1;

typedef ondra_shared::Handle<int, void(*)(int), &closefd, &invalid_fd> FD;


static void writeExitCode(int fd, int exitCode) {
	if (::write(fd,&exitCode, sizeof(exitCode)) == -1) {
		throw SystemException(errno);
	}
}

static int readExitCode(int fd)  {
	int exitCode = 0;
	if (::read(fd,&exitCode, sizeof(exitCode)) == -1) {
		throw SystemException(errno);
	}
	return exitCode;
}

static void closeStd() {
	//redirect stdout and stderr to /dev/null
	int n = open("/dev/null",O_WRONLY);
	int m = open("/dev/null",O_RDONLY);
	//close stderr
	close(2);
	//close stdout
	close(1);
	//close stdin
	close(0);
	//we need to fill fds 0,1,2 because nobody expects, that these descriptors are empty
	dup2(m, 0);
	dup2(n, 1);
	dup2(n, 2);
	close(n);
	close(m);

}

int LinuxService::enterDaemon(Action &&action) {


	int fds[2];
	if (pipe2(fds, O_CLOEXEC) != 0) {
		int err = errno;
		throw SystemException(err,"Failed to call pipe2 (enterDaemon)");
	}

	FD readEnd(fds[0]), writeEnd(fds[1]);

	int fres = fork();
	if (fres == 0) {
		readEnd.close();

		daemonEntered = true;

		onInit([&]{
			//finalize daemon
			//starting in daemon mode, become new session leader
			setsid();
			//send exit code 0 to the umbilical cord
			try {
				writeExitCode(writeEnd,0);
			} catch (SystemException &e) {
				//in case of EPIPE, continue - other end is already closed
				if (e.getErrNo() != EPIPE) throw;
			}
			closeStd();
			writeEnd.close();
			return 0;
		});

		try {
			int ret = action();
			if (!writeEnd.is_invalid()) {
				writeExitCode(writeEnd, ret);
				writeEnd.close();
			}
			return ret;
		} catch (...) {
			if (!writeEnd.is_invalid()) {
				writeExitCode(writeEnd, 253);
			}
			throw;
		}
	} else if (fres == -1) {
		int e = errno;
		throw SystemException(e, __FUNCTION__);
	}else {
		writeEnd.close();
		return readExitCode(readEnd);
	}
}


int LinuxService::startService(StrViewA name, ServiceHandler &&hndl,
		ArgList args) {

	if (checkPidFile()) return 255;
	initControlFile();

	addCommand("stop",[=](ArgList, Stream sx) {
		this->stop();
		waitEnd.push(sx);
		return getpid();
	});
	addCommand("status",[=](ArgList, Stream sx) {
		sx << "Service '" << name << "' is running as pid " << getpid() ;
		return 0;
	});
	addCommand("wait",[=](ArgList, Stream sx) {
		waitEnd.push(sx);
		return 0;
	});
	addCommand("pidof",[=](ArgList, Stream sx) {
		sx << getpid() ;
		return 0;
	});

	int ret = hndl(this, name, args);
	unlink(controlFile.c_str());
	return ret;


}

void LinuxService::initControlFile() {
	mother = TCPListen::create(createNetAddr(),-1,30000);
}

LinuxService::LinuxService(std::string controlFile):controlFile(controlFile) {


}

LinuxService::~LinuxService() {

}


#ifndef SUN_LEN
# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)	      \
		      + strlen ((ptr)->sun_path))
#endif


NetAddr LinuxService::createNetAddr() {
	std::string netpath = "unix:";
	netpath.append(controlFile.data(), controlFile.length());
	netpath.append(":666");
	return NetAddr::create(netpath,0,NetAddr::IPvAll);
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

bool LinuxService::checkPidFile(std::ostream &out) {
	if (access(controlFile.c_str(),0) == 0) {
		try {
			postCommand("status",ArgList(), out);
			out << "\n";
			return true;
		} catch (...) {
			unlink(controlFile.c_str());
			return false;
		}
	}
	return false;
}

bool LinuxService::checkPidFile() {
	return checkPidFile(std::cerr);
}


bool LinuxService::checkPidFileSilent() {
	std::ostringstream dummy;
	return checkPidFile(dummy);
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

static std::pair<FD,FD> makePipe() {
	int fd[2];
	if (pipe2(fd,O_CLOEXEC)) {
		throw SystemException(errno, "Can't create pipe");
	}
	return {
		FD(fd[0]),FD(fd[1])
	};
}

void LinuxService::enableRestart()  {
	if (restartEnabled || !daemonEntered) return;
	do {
		time_t startTime;
		time(&startTime);

		auto cerr = makePipe();


		pid_t chld = fork();
		if (chld == 0) {
			dup3(cerr.second.get(), 2, O_CLOEXEC);
			break;
		}
		cerr.second.close();

		if (chld == -1) {
			int e = errno;
			if (e != EINTR) {
				throw SystemException(e, "fork");
			}
		}

		closeStd();

		boost::fdistream is(cerr.first.get());

		std::string line;
		while (std::getline(is, line)) {
			logNote("stderr: $1", line);
		}

		int status;
		if (waitpid(chld,&status, 0) == -1) {
			int e = errno;
			if (e != EINTR) {
				throw SystemException(e, "waitpid");
			}
		}

		if (WIFEXITED(status)) {
			int x = WEXITSTATUS(status);
			logProgress("Normal exit: $1", x);
			exit(x);
		}
		if (WIFSIGNALED(status)) {
			int signal =  WTERMSIG(status);
			if (signal == SIGTERM|| signal == SIGKILL)
				exit(0);
		}


		time_t endTime;
		time(&endTime);
		if (endTime - startTime < 5) {
			logError("The service crashed with status $1 (no restart)", WTERMSIG(status));
			exit(255);
		}
		logError("The service crashed with status $1. Restarting...", WTERMSIG(status));
		sleep(1);
		logRotate();
		if (access(controlFile.c_str(),0) != 0) {
			initControlFile();
		}
	}
	while (true);
	restartEnabled = true;
}

bool LinuxService::isDaemon() const {
	return daemonEntered;
}

void LinuxService::changeUser(StrViewA userInfo) {
	if (userInfo.empty()) return;

	auto sep = userInfo.indexOf(":");
	StrViewA user;
	StrViewA group;

	if (sep == userInfo.npos) {
		user = userInfo;
	} else {
		user = userInfo.substr(0,sep);
		group = userInfo.substr(sep+1);
	}

	char strUser[user.length+1];
	char strGroup[group.length+1];

	std::copy(user.begin(),user.end(),strUser);
	std::copy(group.begin(),group.end(),strGroup);

	strUser[user.length] = 0;
	strGroup[group.length] = 0;

	int reqSz =  sysconf(_SC_GETGR_R_SIZE_MAX);
	if (reqSz == -1) {
		reqSz = 16384;
	}
	std::vector<char> buffer(reqSz,0);
	passwd uinfo, *uinfores;
	if (getpwnam_r(strUser,&uinfo,buffer.data(),buffer.size(),&uinfores)) {
		int e = errno;
		throw SystemException(e, "changeUser: Failed to retrieve user information");
	}
	if (uinfores == nullptr) {
		throw SystemException(ESRCH, "changeUser: User not found");
	}

	uid_t uid = uinfo.pw_uid;
	gid_t gid = uinfo.pw_gid;


	if (!group.empty()) {
		::group gr, *gres;
		if (getgrnam_r(strGroup,&gr,buffer.data(),buffer.size(),&gres)) {
			int e = errno;
			throw SystemException(e, "changeUser: Failed to retrieve group information");
		}
		if (gres == nullptr) {
			throw SystemException(ESRCH, "changeUser: Group not found");
		}

		gid = gr.gr_gid;
	}

	if (setgid(gid)) {
		int e = errno;
		throw SystemException(e, "changeUser: Cannot change current group");
	}
	if (setuid(uid)) {
		int e = errno;
		throw SystemException(e, "changeUser: Cannot change current user");
	}


}

void LinuxService::onInit(Action &&a) {
	onInitStack.push(std::move(a));
}

}

