// return document element reference either by id or name
function g(id_or_name)
{
	var e = document.getElementById(id_or_name);
	if (e == null) {
		e = document.getElementsByName(id_or_name);
		if (e != null) e = e[0];	// use first from array
	}
	var debug;
	try {
		debug = e.value;
	} catch(ex) {
		console.log("FAILED: id_or_name="+id_or_name);
		console.alert("FAILED: id_or_name="+id_or_name);
		//console.trace();
	}
	return e;
}

var visible = false;

function trace(s) {
	g('trace').style.visibility = visible? "visible" : "hidden";
	g('trace').innerHTML="trace: "+s;
}

function traceA(s) {
	g('trace').style.visibility = visible? "visible" : "hidden";
   g('trace').innerHTML+=" "+s;
}

function traceAN(s) {
	g('trace').style.visibility = visible? "visible" : "hidden";
   g('trace').innerHTML+=s;
}

var ws;

function connect()
{
	url = "ws://"+window.location.host+"/5370.html:5372?query=query_string";
	traceA("conn "+url);
	ws=new WebSocket(url);
	ws.binaryType='arraybuffer';
	ws.onmessage=ws_onmessage;
	ws.onopen=ws_onopen;
	ws.onclose=ws_onclose;
	
	g('trace').innerHTML="trace: ";
	g('console').value = "";
}

var ws_onopen = function()
{ 
	traceA("open");
}

var ws_onclose = function()
{
	traceA("close");
}

var ws_onmessage=function(ev)
{
	//traceA("msg "+ev.data.length+" "+ev.data);

	//var i;
	//var ba=new Uint8Array(ev.data);
	//traceA("msg "+ba.length+" <");
	//for (i=0; i<ba.length; i++) traceAN(ba[i]);
	//traceAN(">");
	
	g('console').value += ev.data;
	g('console').scrollTop += g('console').scrollHeight;		// scroll to bottom
}

function bodyonload()
{
	connect();
}
