#include "resources.h"

namespace simpleServer {
Resource client_index_html = {
		"text/html;charset=utf-8",
		R"html(

<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<link href='https://fonts.googleapis.com/css?family=Fira Mono' rel='stylesheet'>
<link href='styles.css' rel='stylesheet'>
<meta charset="utf-8">
<title>RPC Console</title>
</head>
<body onload="start()">
<script type="text/javascript" src="rpc.js"></script>
<div id="themeToggle">
<div class="output" id="output"></div>
<div class="panel" id="panelcontrol">
	<div class="label" id="label"> Command: </div>
	<div class="input_outer">
		<div class="quickhelp empty	" id="quickhelp"></div>
        <div class="input" id="input" contenteditable="true"></div>
		<menu class="mainmenu" id="mainmenu">
			<li id="clearWorkspace">Clear workspace</li>
			<li class="separator"></li>
			<li id="splitVert">Split vertically (<span style="font-size: 120%">◧</span>)</li>
			<li id="splitHorz">Split horizontally (<span style="font-size: 120%">⬓</span>)</li>
			<li id="unsplit">Unsplit</li>
			<li class="separator"></li>
			<li id="saveContext">Save context</li>
			<li id="clearContext">Clear context</li>
			<li id="restoreContext">Restore context</li>
		    <li class="separator"></li>
			<li id="toggleTheme">Toggle theme</li>	
		</menu>		
		<div class="bottom_tips hidden" id="tabhelp">Press TAB for completion</div>
		<div class="bottom_tips hidden" id="no_context_list">Hint: Type 'key=value' to add a context value </div>
		<div class="context_list" id="context_list"></div>
	</div>
	<div><button id="sendbutt" class="bottombutt">▶</button></div>
	<div><button id="menubutt" class="bottombutt menubutt">☰</button></div>
   </div>
</div>
<script src="https://cdn.jsdelivr.net/npm/promise-polyfill@7/dist/polyfill.min.js"></script>
<script type="text/javascript">
"use strict";
function start() {
	var rpc = new RpcClient("");
	var input = window.input;
	var tabpressed = false;
	var ctxHint = false;	
	themeToggle.setAttribute("class",localStorage["theme"]);

	var wsrpc = new WsRpcClient("");
	wsrpc.connect().then(function() {		
		rpc = wsrpc;
		systemMessage("WebSocket enabled", "Successfully connected to websocket",false);
		rpc.onnotify = function(name, params) {
			outputNotify(name,params);
		}
	});

	
	function setInput(z) {
        input.innerText = z;
        input.focus();
        setEndOfContenteditable(input);
     }
     function restoreContextFn() {
     	var x = localStorage["context"];
     	if (x) {
     		try {
     			rpc.context = JSON.parse(x);
				wsrpc.context = rpc.context;
     			updateContextView(rpc);
     		} catch (e) {
     			systemMessage("Failed to restore context","Error:"+e.toString(), true);
     		}
     	}
     }
	function updateCtxHint() {
		if (!ctxHint && Object.keys(rpc.context).length == 0) {
			no_context_list.classList.remove("hidden");
			ctxHint = true;
		} else {
			no_context_list.classList.add("hidden");
		}
	}
	input.addEventListener("keydown", function(ev) {
		var x = ev.which || ev.keyCode;
		if (x == 13 && !ev.getModifierState("Shift")) {
			hideHelp();
			tabhelp.classList.add("hidden");
			ev.preventDefault();
			sendCommand(rpc, input.innerText).then(function(){			
				input.focus();
				updateCtxHint();
			});
			input.innerText="";
		} else if (x == 9) {
			ev.preventDefault();
			tabpressed = true;
			tabhelp.classList.add("hidden");
			showHelp(rpc, input.innerText, setInput);
		} else if (x == 27) {
			hideHelp();
		} else {
			if (!tabpressed 				
					&& input.innerText.length > 3 
					&& !isHelpShown()) {
				tabhelp.classList.remove("hidden");
			}
		}
	});
	input.addEventListener("input", function() {
		removeHTML(input);
	});
	sendbutt.addEventListener("click",function() {
		sendCommand(rpc, input.innerText).then(function(){			
				input.focus();
				updateCtxHint();
		});
		input.innerText="";
	});
	label.addEventListener("click",function() {
		if (isHelpShown()) { 
			  hideHelp();
		}else {
			showHelp(rpc, "", setInput);
		}
		input.focus();
	});
	menubutt.addEventListener("click", function() {
		panelcontrol.classList.toggle("menu_visible");
		scrollAnim(500);
	});
	toggleTheme.addEventListener("click",function() {
		themeToggle.classList.toggle("theme_light");
		hideMainMenu();
		localStorage["theme"] = themeToggle.className;
	});
	clearWorkspace.addEventListener("click",function() {
		hideMainMenu();
		if (confirm("Confirm clear workspace")) {
			clearElement(output);
		}
	});
	saveContext.addEventListener("click", function() {
		localStorage["context"] = JSON.stringify(rpc.context);
		systemMessage(saveContext.firstChild.nodeValue, "Context has been saved");
		hideMainMenu();
	});
	clearContext.addEventListener("click", function() {
		rpc.context = {};
		updateContextView(rpc);		
		hideMainMenu();
	});
	restoreContext.addEventListener("click", function() {
		restoreContextFn();
		hideMainMenu();
	});
	splitVert.addEventListener("click", function() {
		split(false,themeToggle);
	});
	splitHorz.addEventListener("click", function() {
		split(true,themeToggle);
	});
	if (window.parent == window) unsplit.classList.add("disabled");
	else unsplit.addEventListener("click", function() {
		window.parent.location.reload();
	});

	input.focus();
	restoreContextFn();

}


function removeHTML(elem) {
	var hasHTML = elem.firstElementChild;
	if (!hasHTML) return;
	if (elem.dataset.removeHtmlRecursiveCall) return;
	elem.dataset.removeHtmlRecursiveCall = 1;
	var selection="<?!krles!?>"

	document.execCommand("insertText",false,selection);
	var text = elem.innerText;
    var selofs = text.indexOf(selection);
	document.execCommand("undo");
	text = elem.innerText;
	var re = new RegExp(String.fromCharCode(160), "g");
	text = text.replace(re, " ");
	while (elem.firstChild) elem.removeChild(elem.firstChild);
    var tnode = document.createTextNode(text);
	elem.appendChild(tnode);
	if (selofs >= 0) {
    	var range = document.createRange();
		range.setStart(tnode,selofs);
		range.setEnd(tnode,selofs);
		var docsel = document.getSelection();
		docsel.removeAllRanges();
		docsel.addRange(range);
	}
	delete elem.dataset.removeHtmlRecursiveCall;
}

function hideMainMenu() {
		panelcontrol.classList.remove("menu_visible");

}

function split(horz, elm) {
	clearElement(elm);
	elm.style.display="flex";
	elm.style.height="100vh";
	if (horz) elm.style.flexDirection="column";
	var loc = location.href;
	var p = loc.indexOf("?");
    if (p != -1) loc = loc + "x"; else loc = loc+"?x";
	for (var i = 0; i < 2; i++) {
		var d = document.createElement("iframe");
		d.style.width = "100%";
		d.style.height = "100%";		
        d.setAttribute("src",loc);
        elm.appendChild(d);
    }
}

function systemMessage(title, text, error) {
	var root = document.createElement("div");
	output.appendChild(root);
	var z = addControls(null,root, {method:title});
	z.innerHTML= text;
	z.classList.remove("pending");
	z.classList.add(error?"error":"result");
}
function setEndOfContenteditable(contentEditableElement)
{
    var range,selection;
    if(document.createRange)//Firefox, Chrome, Opera, Safari, IE 9+
    {
        range = document.createRange();//Create a range (a range is a like the selection but invisible)
        range.selectNodeContents(contentEditableElement);//Select the entire contents of the element with the range
        range.collapse(false);//collapse the range to the end point. false means collapse to end rather than the start
        selection = window.getSelection();//get the selection object (allows you to change selection)
        selection.removeAllRanges();//remove any selections already made
        selection.addRange(range);//make the range you have just created the visible selection
    }
    else if(document.selection)//IE 8 and lower
    { 
        range = document.body.createTextRange();//Create a range (a range is a like the selection but invisible)
        range.moveToElementText(contentEditableElement);//Select the entire contents of the element with the range
        range.collapse(false);//collapse the range to the end point. false means collapse to end rather than the start
        range.select();//Select the range (make it the visible selection
    }
}

function hideHelp() {
	window.quickhelp.classList.add("empty");
	window.quickhelp.classList.remove("pending");
	setTimeout(clearElement.bind(null,window.quickhelp),400	);	
	
}

function isHelpShown() {
	return !(!quickhelp.firstChild);
}

function showHelp(rpc, txt, setfn) {
	var tm = setTimeout(function() {
		window.quickhelp.classList.add("pending");
	},500);
	return rpc.methods().then(function(methods) {
		clearTimeout(tm);
        var s = window.quickhelp;
        clearElement(s);
		var thisval = txt;
	    txt = txt.trim();	    
	    var found = false;	  
	    var finl = "";
	    for (var i = 0; i < methods.length; i++) {
	        var m = methods[i];
	        if (m.substr(0,txt.length) == txt) {
	            if (found) {
	                var c = m.substr(0,thisval.length);
	                for (var j = txt.length; j < thisval.length;j++)
	                if (j > c.length || c[j] != thisval[j]) {
	                    thisval = thisval.substr(0,j);
	                    break;
	                }
	                finl = "";
	            } else {                
	                thisval = m+" [";
	                found = true;
	                finl=m;
	            }
			var elm = document.createElement("div");
			elm.appendChild(document.createTextNode(m));
			elm.addEventListener("click", function(m){
				setfn(m+" [");
				hideHelp();
			}.bind(null,m));
			s.appendChild(elm);	                
	        }
	    }
	    setfn(thisval);
	    if (finl != "") {
	    	clearElement(s);
	    	window.quickhelp.classList.add("empty");
	    } else {
	    	window.quickhelp.classList.remove("empty");
	    }
		window.quickhelp.classList.remove("pending");
		scrollAnim(500);
	},function() {
		window.quickhelp.classList.remove("pending");
	});
    
}


function scroll() {
    window.scrollTo(0,document.body.scrollHeight);	
    window.input.focus();
}

function scrollAnim(time) {
		var ix = setInterval(scroll,25);
		setTimeout(clearInterval.bind(null,ix),time);
}

function clearElement(elem) {
	while (elem.firstChild) elem.removeChild(elem.firstChild);
}


function updateContextView(rpc) {
	var c = window.context_list;
	clearElement(c);
	for (var x in rpc.context) {
		var d = document.createElement("div");
		var d1 = document.createElement("div");
		var d2 = document.createElement("div");
		var d3 = document.createElement("div");
		var b1 = document.createElement("button");
		d1.appendChild(document.createTextNode(x));
		d2.appendChild(formatJson(rpc.context[x]));
		d3.appendChild(b1);
		b1.appendChild(document.createTextNode("x"));
		b1.addEventListener("click", function(x) {
			var c = {};
			c[x] = null;
			rpc.updateContext(c);
			updateContextView(rpc);
		}.bind(null,x));
		d.appendChild(d3);
		d.appendChild(d1);
		d.appendChild(d2);
		c.appendChild(d);
	}
}

function addControls(rpc, root, cmd) {
	root.classList.add("row");
    var res = document.createElement("div");
    res.classList.add("outdata");
    res.classList.add("pending");
    var ln = outputCommand(root,cmd.method, cmd.args);
    if (rpc) {
		var reloadButton = document.createElement("button");
		reloadButton.classList.add("reload");
		reloadButton.classList.add("bottombutt");
		reloadButton.appendChild(document.createTextNode("⟳"));
		reloadButton.addEventListener("click", function(e) {  
			e.stopPropagation();  	
			res.classList.remove("result");
			res.classList.remove("error");
			res.classList.add("pending");
			rpc.call2(cmd.method, cmd.args)
			   .then(function(x) {clearElement(res); return x;},
					   function(x) {clearElement(res); throw x;})
			   .then(outputResult.bind(null, res),outputError.bind(null,res));
		});
    ln.appendChild(reloadButton);
    }
    var closeButton = document.createElement("button");    
    closeButton.classList.add("close");
	closeButton.classList.add("bottombutt");
	closeButton.appendChild(document.createTextNode("✖"));
	res.close = function() {
    	var h = root.clientHeight;
    	root.style.opacity="1";
    	root.style.height=h+"px";	
    	root.style.overflow="hidden";
    	root.style.transition="all 0.7s ease-in-out";
    	setTimeout(function() {
    		root.style.opacity="0";
    		root.style.height="0";
    	},50);
    	setTimeout(function() {
    		root.parentElement.removeChild(root);
    	},1000);
    };
    closeButton.addEventListener("click", function(e) {
    	e.stopPropagation();
		res.close();});
    ln.appendChild(closeButton);
    root.appendChild(res);
    return res;
}

function sendCommand(rpc, cmd) {
	var parsed = parseCmd(cmd);
	if (parsed) {
		var root = document.createElement("div");
		var d = addControls(rpc,root, parsed);
	    window.output.appendChild(root);
		scroll();
		return rpc.call2(parsed.method, parsed.args)
			.then(outputResult.bind(null,d), outputError.bind(null,d))			
			.then(scroll)
			.then(updateContextView.bind(null,rpc))
			
	} else {
		parsed = parseContextChange(cmd);
		if (parsed) {
			var c = [];
			c[parsed.name] = parsed.value;
			rpc.updateContext(c);
			updateContextView(rpc);
			return Promise.resolve(1);
		} else {
			throw "Invalid command";
		}

	}
}

var notifyList = {};

function outputNotify(name,result) {
	var elm = notifyList[name];
	if (elm) setTimeout(elm.close,10000);
	var root = document.createElement("div");
	var d = addControls(null,root, {method:"NOTIFY: ",args:name});
	window.output.appendChild(root);
	elm = notifyList[name] = d;
	elm.classList.replace("pending","notify");
	var json = formatJson(result);
	elm.appendChild(json);
	scroll();
}
	

function outputCommand(elem, method, args) {
	var d = document.createElement("div");
	d.classList.add("outdata");
	d.classList.add("command");
	var c = document.createElement("span")
	c.classList.add("command");
	c.appendChild(document.createTextNode(method));
	d.appendChild(c);
	var p = document.createElement("span");
	p.classList.add("params");
	var strargs = args?JSON.stringify(args,null," "):"";
	var strargs2 = args?JSON.stringify(args,null):"";
	p.appendChild(document.createTextNode(strargs));
	d.appendChild(p);
    elem.appendChild(d);   

	var lstn = function(){
    	window.input.innerText =  method+" "+strargs2;
    	window.input.focus();
    }

    c.addEventListener("click", lstn);
    p.addEventListener("click", lstn);
    return d;
}

function outputResult(d,result) {
	d.classList.replace("pending","result");
	var json = formatJson(result);
	d.appendChild(json);	
}

function outputError(d,result) {
	d.classList.replace("pending","error");
	var rejections ;
	if (result.code == -32602) {
		rejections = result.data;
		delete result.data;
	}
	var json = formatJson(result);
	d.appendChild(json);
	if (rejections) {		
		var table = document.createElement("json-object");
		rejections.reverse().forEach(function(x) {
			var row =document.createElement("json-pair");
			table.appendChild(row);
			var ref = "param";
			x[0].forEach(function(y) {
				if (typeof y == "number") ref = ref + "[" + y +"]";
				else if (typeof y == "string") ref = ref + "." + y;
			});
			var t1 = document.createElement("json-key");
			t1.appendChild(document.createTextNode(ref));
			row.appendChild(t1);
			row.appendChild(document.createTextNode("="));
			var t2 = document.createElement("json-value");
			t2.appendChild(formatJson(x[1]));
			row.appendChild(t2);
		});
		var rejsect = document.createElement("rpc-rejections");
		rejsect.appendChild(document.createTextNode("Required parameter(s):"));
		d.appendChild(rejsect);
        d.appendChild(table);
	}
}

function formatJson(json) {	
	var d;

	if (json === null) {
		d = document.createElement("json-null");
		d.appendChild(document.createTextNode("null"));		
	} else if (typeof json == "object") {
		if (Array.isArray(json)) {
			d = document.createElement("json-array");
			var len = json.length;
			d.appendChild(document.createTextNode("["));
			for (var i = 0; i < len; i++) {
				if (i) d.appendChild(document.createTextNode(", "));
				d.appendChild(formatJson(json[i]));
			}
			d.appendChild(document.createTextNode("]"));
			
		} else {
			d = document.createElement("json-object");
			d.appendChild(document.createTextNode("{"));
			var prevEl = null;
			for (var x in json) {
				var dr = document.createElement("json-pair");
				if (prevEl)prevEl.appendChild(document.createTextNode(", "));
				var name = JSON.stringify(x);
				var dk = document.createElement("json-key");
				dk.appendChild(document.createTextNode(name));
				var dv = document.createElement("json-value");
				dv.appendChild(formatJson(json[x]));
				dr.appendChild(dk);
				dr.appendChild(document.createTextNode(":"));
				dr.appendChild(dv);
				d.appendChild(dr);
				prevEl = dv;
			}
			d.appendChild(document.createTextNode("}"));
		}
	} else {
		d = document.createElement("json-"+typeof json);
		var x = JSON.stringify(json.toString());
//		if (typeof json == "string") x = "\""  + x +"\"";
		d.appendChild(document.createTextNode(x));
	} 
	return d;
}
	
function parseCmd(cmd) {
	var i1 = /^ *([-a-zA-Z0-9_.]+)[ ]*([{[][\s\S]*)$/;
	var m = i1.exec(cmd);
	if (m && m[2].length) {
		return {
			method: m[1],
			args: eval("("+m[2]+")")
		};
	} else {
		return null;
	}
}

function parseContextChange(cmd) {
	var i1 = /^ *([-a-zA-Z0-9_.]+) *[=:]([\s\S]*)$/;
	var m = i1.exec(cmd);
	if (m && m[2].length) {
		return {
			name: m[1],
			value: eval("("+m[2]+")")
		};
	} else {
		return null;
	}
}

</script>
</body>
</html>

)html"
};}
