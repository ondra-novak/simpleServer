#pragma once

#include <future>
#include <functional>
#include <iosfwd>
#include <queue>
#include <map>
#include <stack>

#include "../abstractService.h"
#include "../abstractStreamFactory.h"
#include "../address.h"

namespace simpleServer {


class LinuxService: public AbstractServiceControl {
public:

	LinuxService(std::string controlFile);
	~LinuxService();

protected:

	virtual void enableRestart() override ;
	virtual bool isDaemon() const override ;

	virtual void dispatch() override ;
	virtual void addCommand(StrViewA command, UserCommandFn &&fn) override ;
	virtual void stop() override ;

	friend class ServiceControl;

	typedef std::function<int()> Action;
	int enterDaemon(Action &&action);


	int startService(StrViewA name, ServiceHandler &&hndl, ArgList args);


	std::string controlFile;

	typedef std::stack<Action> OnInitAction;

	OnInitAction onInitStack;
	void onInit(Action &&a);

	NetAddr createNetAddr();

	void processRequest(Stream s);
	int runCommand(StrViewA command, ArgList args, Stream s);

	std::map<std::string, UserCommandFn> cmdMap;

	int postCommand(StrViewA command, ArgList args, std::ostream &output, int timeout = 30000, bool timeoutIsEnd=false);

	bool checkPidFile(std::ostream &out);
	bool checkPidFile();
	bool checkPidFileSilent();
	void stopOtherService();
	void cleanWaitings();

	std::queue<Stream> waitEnd;
	StreamFactory mother;
	bool daemonEntered = false;
	bool restartEnabled = false;

	virtual void changeUser(StrViewA userInfo) override ;

	void initControlFile();

};



}
