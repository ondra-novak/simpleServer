#pragma once
#include <shared/stringview.h>

namespace simpleServer {

using ondra_shared::StrViewA;

struct Resource {
	StrViewA contentType;
	StrViewA data;
};



extern Resource client_index_html;
extern Resource client_rpc_js;
extern Resource client_styles_css;

}
