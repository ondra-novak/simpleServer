#pragma once

#include <future>
#include <functional>
#include <map>
#include "../abstractService.h"
#include "../address.h"

namespace simpleServer {


class LinuxService: public AbstractServiceControl {
public:

	LinuxService(std::string controlFile);
	~LinuxService();

protected:



	virtual void dispatch();
	virtual void addCommand(StrViewA command, UserCommandFn fn) ;
	virtual void stop();

	friend class ServiceControl;

	bool enterDaemon();

	int waitForExitCode();
	int startService(StrViewA name, ServiceHandler hndl, ArgList args);
	void sendExitCode(int code);

	int umbilicalCord=0;

	std::string controlFile;


	NetAddr createNetAddr();

	void processRequest(Stream s);
	std::promise<std::function<void()> >stopFunctionPromise;
	std::future<std::function<void()> >stopFunction;

	std::map<std::string, UserCommandFn> cmdMap;

	int postCommand(StrViewA command, ArgList args);
};



}
