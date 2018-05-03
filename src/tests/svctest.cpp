#include <unistd.h>

#include "../simpleServer/abstractStream.h"
#include "../simpleServer/abstractService.h"
#include "shared/stdLogOutput.h"


using namespace simpleServer;




int main(int argc, char **argv) {

	return


	ServiceControl::create(argc, argv,"exampleService",[](ServiceControl control, StrViewA name, ArgList args) {


		std::cout << "Service running: " << name << std::endl;

		control.dispatch();

		return 0;
	});


}


