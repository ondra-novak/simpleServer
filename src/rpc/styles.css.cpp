#include "resources.h"

namespace simpleServer {
Resource client_styles_css = {
		"text/css",
		R"css(

body {
	background-color:#333;
	font-family: "Fira Mono", monospace;
	font-size: 0.4cm;
	color: #CCC;
	margin:0;
}
.panel {
	background-color: #444;
	padding: 5px;
	display:flex;
	position:sticky;
	bottom: 0px;
	overflow: hidden;
	min-height:0px;
	transition: all 0.5s;
	}

.input_outer {
flex-grow: 1;
}



.quickhelp {
	background-color: #555;
	max-height: 40vh;
	overflow-y:auto;
	overflow-x:hidden;
	transition: all 0.4s;
}
.quickhelp.empty {
	max-height: 0vh;
	overflow-y:hidden;
}
.quickhelp.empty.pending,.quickhelp.pending {
	max-height: 0vh;
	overflow-y:hidden;
	max-height: 100px	;
}

.label {
	transition: all 0.2s;
	transform: scale(1,1) translate(0,0);	
	cursor: pointer;
	width:5em;
	padding: 5px 5px 0 0;
	text-shadow: 0px 0px 0px white;
	flex-grow: 0;	
	transform-origin: 50% 20px;
	font-weight:400;
}


.label:hover {
	transform: scale(1.1,1.1);
	text-shadow: 0px 0px 8px white;
	font-weight:600;
}
.label:active {
	transform: scale(1.0,1.0);		
}

.outdata button {
	transition: all 0.3s;
	transform: scale(1,1);
}

.outdata button:hover {
	transform: scale(1.5,1.5);
}
.outdata button:active {
	transform: scale(1,1);
}

.quickhelp > div:first-child {
	margin-top: 5px;
}
.quickhelp > div:last-child {
	margin-bottom: 5px;
}

.quickhelp > div {
	padding-left: 5px;
	padding-right: 5px;	
	cursor: pointer;
	transition: transform 0.3s;
	transform: scale(1,1) translate(0,0);
	transform-origin: 0% 50%;
	text-shadow: 0 0 0;	
}	

.quickhelp > div:hover {
	transform: scale(1.1,1.1);
	text-shadow: 0 0 8px black;
	color:white;
}	

.quickhelp > div:active {
	transform: scale(1,1);
	background-color: #A55
}	

.input {
border: 1px solid black;
background-color:#222;
padding: 5px;
min-height: 1em;
}

json-string {
	color: #AAFFAA;
	font-weight:normal;
	text-shadow: none;
}
json-number {
	color: #fff596;	
	font-weight:normal;
	text-shadow: none;
}
json-pair {
	display:table;		
	text-shadow: none;
	margin-left: 1em;
}
json-key {
	display:table-cell;
	color:white;
	font-weight:600;	
}
json-value {
	display:table-cell;
	font-weight:normal;
}
json-object,json-array {
	font-weight:bold;
	color: #fff;
	vertical-align: baseline;
	text-shadow: 2px 2px 4px black;
}
json-boolean {
	font-weight: bold;
	color: #79fffd;
	text-shadow: 2px 2px 4px black;
}
json-null {
	font-weight: bold;
	color: #ff5374;	
	text-shadow: 2px 2px 4px black;
}

json-array > json-object {
	display:block;	
	margin-left: 1em;
}

.outdata {
	transition: all 0.5s;
}

.outdata.result {
	background-image: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAHNSURBVFhH7ZW9LwRBGMZXIxIh8REJBSJ3s5sLElEodFSURHMfZu5O/BkqiQKNmmgonJ3Zy4VLNFoliU5FoTgJQi133tl7j3M7jnU317hf8jY7z87zzMy7s0aTJjohgq6ZguUIZ+uh1FIrPm4MxKGzJmd5CFAoFr2yeGIUh/USyEY6iWB3n+bFgh25RoleCKd7leZuAE4zKNFH0GbzKnM4jpdQKjaIMj2EjpLdsMp7VQDoiSTK9AFnfKgyhwY8RYk+TIcuqswh1KOZTgygTA8jPNoHq3xQBTBtFkaZPsBIeIyh4AKyUaIPIuJRlTlUTu4MyvxhOXQSzu4GLpNzS8R68LEHebawyieFecESbAFl/pDm8M0+f0zG2aUyRMFogbFsuWmpIPgBqvxh8Uj/F/NSKULAylc8Oih5D4ydhLtQ5o9gOj6lmtStshDwyQ2D0atCkydiec6d7K/A9m15Ji4VhCCZ1V7ZG9+M7+I0tWFyuq00cEv9vUPT3kJTduAUtVM9REXJredsBl+tH78NAUeyg6/Un59CyPti/CzajnI9VAnxRmw6jTK9qELA6jdxuDFAs23Ihiuas4uhfdqGQ40jcBybkP9+7efe5J9gGO+3vMGOdEcI6gAAAABJRU5ErkJggg==");
	background-repeat: no-repeat;
	background-position: 3px 5px;
	min-height: 34px;
	padding: 5px 38px;
	margin: 3px;
	background-color: #233;
	opacity: 1;
}
.outdata.notify {
	background-image: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAAA3NCSVQICAjb4U/gAAAACXBIWXMAAADdAAAA3QFwU6IHAAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAY9QTFRF////AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGAAAGAAAFAAAFAAAEAAAEAAAEAAAEAAADAAADAAADAAADAAADAAADAAADAAACAAACAAACAAACAAAEAAAEAAAEAAAEAAAEAAAEAAAEAAAEAAADAAADAAADAAADAAADAQEDBQEEAAADBAMEBAQEAAADAAADCAYEBgUEAAACBQUECQQFAAAEAAAEAAADAwIDAAADBQIEAAADBAMEBAIEKCEJBAMDJB8JAAADBAMEHAsJAgIDFBEGKCIKOTAMFxMHMysLAQEDCQgFJxALXU4TY1MUcmAWAgIDBAIEPDMNAgIDAwMEW0wSZVUUAAADCggFDw0GEQ4GRzwPTSATUUQRZSoYZ1cUdWMXk3wclH0clX4cl38cmYEdm4MdnYQdqY4fqo8gtJght00quJsiwlEsw6QkxqckyKkl0bAm418z78or9M4s9c8s99At+tMt/tYu/2s5/9cujLuVjQAAAGF0Uk5TAAEEBQYHCQsMEhQXGRwdICQmKi0uMDU8QUZIUFFTVFhcYmh1e36Dh4qMjY6QkZOXo7Cytby9vb/FyMjO0NHR1dbb4+vt8fL19fb29/j4+fn5+fr6+/v7/Pz8/f39/v7+/veov9wAAAFUSURBVBgZxcFVQ8IAFAbQz24sRDfBxga7WwHF7sK6ds9AxUTw/nAVmBvCfPUchCvs7y/EH5JHj49Hk6GtgF5eqADaDDQ1RQZo0bfT0tzcErXrEZXgnPX4mf2eWaeAKHIXd3wc4NtZzEWk7gUfh/gWuhEhn575xzPlJzWlQCUV5hlWmTEn1sdBUTKAjm1W2e5AWTYUVb1wnLPKmQOddVA0NmfSE6s8Uqa1DYouq3H9nVW8m8bSEfzIIaFyjMOMV+ZRFmS1gzEtqxxmpSV2qDMWIcVi2uQVK96ky6PJtCKqQUD6RBIa5r2suN/f29tqQAVl4FuCADPd8C+3ZIaQiCBh/ZpDJMl/ehCwS+WQ9a2xzO3mi5OgjWHIpqXXKKRpyHooqh7IdGKQ7fDuy6FNDNLht2rXw8fHg6saWuJbaXmZWuOhzWSxmPB/xGIVERF0dlKx6xDyCdWxsrrZ5GQMAAAAAElFTkSuQmCC");
	background-repeat: no-repeat;
	background-position: 3px 5px;
	min-height: 34px;
	padding: 5px 38px;
	margin: 3px;
	background-color: #224;
	opacity: 1;
}
.outdata.error {
	background-color: #522;
	min-height: 34px;
	background-image: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAGtSURBVFhH7ZY/TsMwFMYDSIyIgRPAxoRAYohdlT9LFBuYcp7OPQJiA3EBoFwAjsDOQpMglSwI0S34Jc9N0zi2UwJTftI3JH5+35enpLXT0WHi3XN30oGzipfWjHl/a+IdbuBlc1LHWYk4uYw4TUNG716C3XVcMhL57nHIyGfE6GTMe/t42x4wFw2uwHxOI5sQYC6Mv4p9JBEPcIDLdsgnV0gbomouRZI3393DMj0xo9vVBiUpQ9Sb5xJTuMVSPTj+e1WTOZVCRGe9E525mMA0YuQIy81Ac7FxVG1UUhbCxjz0qYet7cmaM/qobopi9OlPzCVWIWr1S3PJciFaMpc0C9GyuSQPQZ7VpoVi5p7jlnaJOT3Vv3AoMSkIi9vawfypLajNEI3NC1n9d2gx/bxaaPkQZnMyhRcOxq1en6l5CAvzb/mpQfNWQ8BJJjtMqBsJFeaSLIQwqdYWCjkZYrme14v+ptjwsdggV9VcYgoRcjrAUjNwghFmSblJvbmkPgS5SYNgDcvsKIcwm0sghBj3w8yckevG5hI4RkF6eCnxlhUYYghjX9q8o+N/cJwfWgIGFjn3s+IAAAAASUVORK5CYII=");
	background-repeat: no-repeat;
	background-position: 3px 5px;
	padding: 5px 38px;
	margin: 3px;
	opacity: 1;
}
.outdata.command {
	font-weight: bold;
	padding: 5px 10px;
	border-left: 4px solid #48A;
	position:relative;
	padding-right: 70px;
	overflow:hidden;
	box-sizing: border-box; 
	width: 100%;
}
.outdata.pending,.quickhelp.pending {
	padding: 5px 38px;	
	margin: 3px;
	min-height: 40px;
	position: relative;
	z-index: 0;
	opacity: 0.5;

}
.outdata.pending:before,.quickhelp.pending:before
{
	background-repeat: no-repeat;
	background-position: 3px 5px;
    content: "";
    position: absolute;
    width: 32px;
    height: 32px;
    left: 3px;
    top: 10px;
    background: url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAAyVBMVEUAAAC9vsDCw8W9v8G9vsD29/fr7O29vsDm5+jx8fK/wML29vf///+9vsHR0tPm5+jt7u7w8PHr7Ozc3d7n6OnExcfGx8nJyszMzM7V1tjZ2tvf4OG9vsDHyMrp6uvIycu9v8HAwcTPz9HCw8W9vsC9vsDBwsTj5OXj5OXW19i9vsDm5+jw8PHy8vL09fW9vsDFxsjJyszAwsS9vsDU1dbLzM7Y2drb3N3c3d7e3+DHyMrBwsTh4uPR0tPDxcfa293R0tTV1ti9vsAll6RJAAAAQnRSTlMA7+t/MAZHQDYjHxABv6xSQjEgFgzi2M7Emop0cGVLD/ryt7Cfj3dmYWBgWjgsGd/Cv7SvopSSg4B6eHZrQjkyKx1dd0xhAAAA9klEQVR4AYWPeXOCMBBHFxIgCQKC3KCo1Wprtfd98/0/VBPb6TC0Wd+/783+ZqELrQkRoIMujFbiaLTwpESC029reB7919d7u6SgYaE8aUCivW84oEUY0lPQc408pxBqHxCIHGiw4Lxtl5h35ALFglouAAaZTj00OJ7NrvDANI/Q4PlQMDbNFA3ekiQRaHGRpmM0eMqyyxgLRlme4ydu8/n8Az3h+37xiRWv/k1RRlhxUtyVD8yCXwaDflHeP1Zr5sIey3WtfvFeVS+rTWAzFobhNhrFf4omWK03wcS2h8OzLd/1TyhiNvkJQu5amocjznm0i6HDF1RMG1aMA/PYAAAAAElFTkSuQmCC");    -webkit-transform: rotate(30deg);
	animation: rotation 1s infinite linear;
}

@keyframes rotation {
    from {transform: rotate(0deg);}
    to   {transform: rotate(360deg);}
}

button {
	cursor: pointer;
}


button.close {
	position: absolute;
	right: 2px;
	top: 50%;
	margin-top:-13px;

	
}
button.reload{
	position: absolute;
	right: 36px;
	top: 50%;
	margin-top:-13px;
	
}
div.row {
	position:relative;		
	overflow: hidden;
	
}

.outdata.command .command {
	color: yellow;
	margin-right: 0.2em;
}
.outdata.command .command, .outdata.command .params {
	transform: translate(0,0);
	transition: all 0.2s;
	display:inline-block;
	cursor: pointer;
}
.outdata.command:active .command,.outdata.command:active .params {
	transform: translate(2px,2px);
}
.outdata.command:hover .command,.outdata.command:hover .params {
	text-shadow: 0px 0px 16px white;
}

.context_list {
	display: table;
	font-size: 12px;
}


.bottom_tips {
	transition: all 0.5s;
	font-size: 12px;
	height: 1em;
	overflow: hidden;
}

.bottom_tips.hidden {
	height: 0;
}

.context_list > div{
	display:table-row;	
}
.context_list > div > div{
	display: table-cell;
	padding: 0px 2px
}
.context_list button {
	border:0;
	background-color: transparent;
	color: #F88;
	text-shadow: 1px 1px 4px black;
	cursor:pointer;
}
.bottombutt {
	height:29px;
	width: 29px;
	text-align:center;
	display:block;
	background-color: transparent;
	border: 0;
	color: white;
	font-size: 15px;
	text-shadow: 2px 2px 8px black;
	transform: translate(-2px, -2px);
	transition: all 0.2s;
}

.bottombutt:hover {
	color: yellow;
	border: 0;
}
.bottombutt:active {
	color: yellow;
	transform: translate(0px, 0px);
	text-shadow: 0px 0px 0px;
	border: 0;
}
.bottombutt:focus {
	outline: 0;
}


menu {
	margin:0;
	padding: 0;
	list-style-type: none;
	background-color: #554;	
	padding: 5px 0px;
	cursor: pointer;
}

menu > li{
	background-color: #554;	
	transition: all 0.3s;	
	padding: 0px 5px;
}

menu > li:hover {
	background-color: #484;
}
menu > li:active {
	background-color: #844;
}

menu > li.disabled {
	opacity: 0.5;
}
menu > li.disabled:active {
	background-color: inherit;
}
menu > li.disabled:hover {
	background-color: inherit;
}

.panel menu.mainmenu {
	width:7cm;
	right: 7px;
	top: 0.9cm;

	box-shadow: 2px 2px 4px black;
	transition: all 0.5s;
	max-height: 0px;
	box-sizing: content-box;	
	overflow: hidden;
	padding: 0;	
	position: absolute;

}
.panel.menu_visible menu.mainmenu {
	max-height: 5.8cm;
	padding: 5px 0px;	
}

.panel.menu_visible {
	min-height: 5.8cm;
}

.panel.menu_visible .menubutt {
	background-color: #554;		
}


.separator {
	margin: 0.1cm 0px 0.1cm 0px;
	border-top: 1px solid black;

}

rpc-rejections {
	display:block;
	border-top: 1px solid;
	margin-top: 5px;	
	margin-bottom: 5px;	
	font-weight: bold;
}

.theme_light{
	background-color:#ccc;
	min-height: 100vh;
	color: #444;


}
.theme_light .quickhelp {
	background-color: #eee;
	color: #666;
}

.theme_light .label {
	text-shadow: 0px 0px 0px white;
}

.theme_light .label:hover {
	text-shadow: 0px 0px 8px white;
}
.theme_light .quickhelp > div:hover {
	text-shadow: 0 0 8px white;
	background-color: white;
	color:black;
}	
.theme_light .quickhelp > div:active {
	background-color: #FAA	
}	
.theme_light .input {
	background-color:#EEE	;
}

.theme_light json-string {
	color: #005500;
}
.theme_light json-number {
	color: #000A4C;	
}
.theme_light json-key {
	color:black;
}
.theme_light json-object,.theme_light json-array {
	color: #444;
	text-shadow: 2px 2px 4px #AAA;
}
.theme_light json-boolean {
	color: #4aa09f;
	text-shadow: 2px 2px 4px #AAA;
}
.theme_light json-null {
	color: #c34059;	
	text-shadow: 2px 2px 4px #AAA;
}
.theme_light .outdata.result {
	background-color: #dde8df;	
}
.theme_light .outdata.notify {
	background-color: #efefdd;	
}
.theme_light .outdata.error {
	background-color: #efe2e2;	
}
.theme_light .outdata.command {
		border-left: 4px solid #2271cf;
}
.theme_light .outdata.command .command {
	color: blue;
}
.theme_light .panel {
	background-color: #AAA;
}
.theme_light .context_list button {
	color: #ff5374;
}
.theme_light .bottombutt {
	color: black;		
	text-shadow: 2px 2px 8px #888;
}
.theme_light menu {
	background-color: #e9e9cd;
}
.theme_light menu >li{
	background-color: #e9e9cd;
}
.theme_light menu >li:hover{
	background-color: #FFA;
}
.theme_light .outdata.pending {
	background-color: #EEEEEE;
}
.theme_light .outdata.pending:before,.theme_light  .quickhelp.pending:before {
	filter: brightness(0);
}

.theme_light .panel.menu_visible .menubutt {
	background-color: #e9e9cd;		
}

iframe {
	border:0;
	border-right: 1px solid;	
	border-bottom: 1px solid;
	box-sizing: border-box;
}

)css"};}
