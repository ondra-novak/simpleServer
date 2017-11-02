#pragma once

#include "../abstractService.h"

namespace simpleServer {


class LinuxService: public AbstractServiceControl {
public:

	virtual void dispatch();
	virtual void addCommand(StrViewA command, UserCommandFn fn) ;
	virtual void stop();

	friend class ServiceControl;

	bool enterDaemon();

	int waitForExitCode();
	int startService(StrViewA name, ServiceHandler hndl, ArgList args);
	void sendExitCode(int code);

	int pupecniSnura=0;

};



}
