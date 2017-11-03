#pragma once

#include <future>
#include <functional>
#include <iosfwd>
#include <queue>
#include <map>
#include "../abstractService.h"
#include "../abstractStreamFactory.h"
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

	std::map<std::string, UserCommandFn> cmdMap;

	int postCommand(StrViewA command, ArgList args, std::ostream &output, int timeout = 30000, bool timeoutIsEnd=false);

	bool checkPidFile();
	void stopOtherService();
	void cleanWaitings();
	void idleRun();

	std::queue<Stream> waitEnd;
	StreamFactory mother;
};



}
