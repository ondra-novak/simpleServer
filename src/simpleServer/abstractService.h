#pragma once
#include <functional>
#include "shared/refcnt.h"
#include "stringview.h"



namespace simpleServer {

class Stream;

using ondra_shared::RefCntObj;
using ondra_shared::RefCntPtr;


typedef StringView<StrViewA> ArgList;


typedef std::function<int(ArgList, Stream output)> UserCommandFn;




class AbstractServiceControl: public RefCntObj {
public:

	///Dispatches service messages
	/** The main thread should stop here and dispatch messages.
	 * Function is left when stop command is dispatched
	 *
	 * @note on the beginning of the function, the program detaches from the console
	 * and closes standard input and output
	 */
	virtual void dispatch() = 0;
	///Adds custom command to the dispatcher
	/**
	 * @param command name
	 * @param fn function to call
	 *
	 * @note function can be called from the other thread, or in a handler
	 */
	virtual void addCommand(StrViewA command, UserCommandFn fn) = 0;
	///Stops dispatching
	/** It has the same effect as command "stop" */
	virtual void stop() = 0;

	///Enables restart function
	/** this function enables automatic restart of the service when it crashes. The function
	 * can be implemented differently on various platforms. On linux, this is implementad by performing
	 * operation fork() while child continues as service and master monitors the child in
	 * case of the crash. Under Windows, this would be implemented through  function
	 * RegisterApplicationRestart.
	 *
	 * Once the restart is enabled, it cannot be disabled.
	 *
	 * @note function is disabled when linux service is started at foreground. It is also
	 * unavailable when function is called later than dispatch(). It is also recomended to
	 * call this function before the initialization (but after the validation) otherwise it may
	 * affect already opened resources and descriptors. Especially under Linux, the
	 * restarted service continues exiting this function
	 */
	virtual void enableRestart() = 0;

	///Function returns true if service is in daemon mode (or will be put to daemon mode)
	/**
	 * @retval true service running at daemon mode. This can have several impacts, such
	 * as there is no output console and current working directory is root.
	 * @retval false service running at foreground.
	 *
	 * @note function can return just only whether the service was started by start/restart or run
	 */
	virtual bool isDaemon() const = 0;

	///Changes user/group of the service

	virtual void changeUser(StrViewA userInfo) = 0;
};


class ServiceControl;


///Definition if service handler
/**
 * Service handler is called when service starts. It expects, that service
 * performs an initialization and then calls ServiceControl::dispatch.
 * When that function returns, it performs cleanup and exits the handler
 *
 * @param ServiceControl control object
 * @param ArgList arguments (first argument is command used to start the service)
 *
 * Inside od handler, std::cin and std::cout are available, until the dispatch()
 * is called
 *
 * @return return value from the service initialization
 *
 */
typedef std::function<int(ServiceControl, StrViewA name, ArgList)> ServiceHandler;




///Service control object.
/** You need to create this object in order to run your application as a service.
 *
 * To create the object, call the ServiceControl::create()
 */
class ServiceControl: public RefCntPtr<AbstractServiceControl> {
public:

	using RefCntPtr<AbstractServiceControl>::RefCntPtr;



	///Enter to service mode
	/**
	 * @param argc argument count
	 * @param argv arguments from main
	 * @param name name of the service
	 * @param handler function called to implement service
	 * @return function returns the exit code from the program
	 */
	static int create(int argc, char **argv, StrViewA name, ServiceHandler handler);






	void dispatch() const {
		ptr->dispatch();
	}

	void addCommand(StrViewA command, UserCommandFn fn) const {
		ptr->addCommand(command, fn);
	}
	void stop() const {
		ptr->stop();
	}


	void enableRestart() {
		ptr->enableRestart();
	}
	bool isDaemon() const{
		ptr->isDaemon();
	}


	///Changes user (and group) od the service
	/** this function is primarily useful for linux environment. It allows
	 * to change effective user and group for the service, so the service is no longer
	 * running under the root account. The function can be implemented for other
	 * platforms. It should have empty implementation in case, that such
	 * operation is not defined
	 *
	 * @param userInfo Specify name of the user. Under linux, you can specify in
	 * pair of group name separated by collon. "user:group". If you ommit the group,
	 * the function use the user's active group. Failures during switching are reported
	 * throught the SystemException. A special meaing has an empty string, which performs
	 * no action. This allows to connect the argument with an configuration option, where
	 * empty name is interpreted as disable function.
	 */
	void changeUser(StrViewA userInfo) {
		ptr->changeUser(userInfo);
	}
};





}
