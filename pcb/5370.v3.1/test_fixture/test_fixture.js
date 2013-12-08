var b = require('bonescript');
var cp = require('child_process');

var LRST = "P9_11";
var LVMA = "P8_26";
var LR_W = "P9_18";
var CLK =  "P8_7";
var LIRQ = "P9_17";
var LNMI = "P9_16";
var DIR =  "P8_19";
var OE =   "P8_18";
var LD = [ "P8_17", "P8_16", "P8_15", "P8_14", "P8_13", "P8_12", "P8_11", "P8_10" ];
var LA = [ "P9_30", "P9_27", "P9_26", "P9_24", "P9_23", "P9_22", "P9_21" ];

var v = {
    "P9_11":"LRST", "P8_26":"LVMA", "P8_7":"CLK",  "P9_18":"LR_W", "P9_17":"LIRQ", "P9_16":"LNMI", "P8_19":"DIR", "P8_18":"OE",
	"P8_17":"LD0", "P8_16":"LD1", "P8_15":"LD2", "P8_14":"LD3", "P8_13":"LD4", "P8_12":"LD5", "P8_11":"LD6", "P8_10":"LD7",
	"P9_30":"LA0", "P9_27":"LA1", "P9_26":"LA2", "P9_24":"LA3", "P9_23":"LA4", "P9_22":"LA5", "P9_21":"LA6",
	"USR0":"USR0", "USR1":"USR1", "USR2":"USR2", "USR3":"USR3"
};

var curCLK, curLR_W, curLNMI, curLIRQ, curLA1, curLA3, curLA5;
var lastCLK, lastLR_W, lastLNMI, lastLIRQ, lastLA1, lastLA3, lastLA5;

function l(s)
{
	console.log(s);
	//b.echo(s);
}

function init()
{
   var i;
    
	dirIn(CLK);
	dirIn(LVMA);
	dirIn(LRST);
	dirIn(LR_W);
	dirIn(LIRQ);
	dirIn(LNMI);
	
	for (i=0; i<7; i++) dirIn(LA[i]);
	
	dirOut(DIR, 0);
	dirOut(OE, 0);
	
	for (i=0; i<8; i++) dirOut(LD[i], 1);
	
	dirOut("USR0", 0);
	dirOut("USR1", 0);
	dirOut("USR2", 0);
	dirOut("USR3", 0);
}

function dirIn(pin)
{
	//l("dirIn "+v[pin]);
	var m = b.getPinMode(pin);
	if (m.mux != 7) l("dirIn "+v[pin]+" "+pin+" mux "+m.mux);
	if (m.mux != 7) l(m);
	//b.pinMode(pin, b.INPUT_PULLUP);
	b.pinMode(pin, b.INPUT);
}

function dirOut(pin, d)
{
	//l("dirOut "+v[pin]+" "+d);
	var m = b.getPinMode(pin);
	if (m.mux != 7) l("dirOut "+v[pin]+" "+pin+" mux "+m.mux);
	if (m.mux != 7) l(m);
	b.pinMode(pin, b.OUTPUT);
	outp(pin, d);
}

function outp(pin, d)
{
	//l("outp "+v[pin]+" "+d);
	b.digitalWrite(pin, d);
}

var inShort, inCur, inLast;

function inErr(pin, cur, last)
{
	if (cur != last) {
		//l(" CHG: "+pin+" "+last+" -> "+cur);
		inShort = pin;
		inCur = cur;
		inLast = last;
	}
}

function inp(pin)
{
	return _inp(pin, 1);
}

function setLast()
{
	_inp("", 0);
}

function _inp(pin, check)
{
	var inData;
	
	curCLK = b.digitalRead(CLK);
	curLR_W = b.digitalRead(LR_W);
	curLNMI = b.digitalRead(LNMI);
	curLIRQ = b.digitalRead(LIRQ);
	curLA1 = b.digitalRead(LA[1]);
	curLA3 = b.digitalRead(LA[3]);
	curLA5 = b.digitalRead(LA[5]);

	if (check) {
		inShort = 0;
		if (pin === CLK) inData = curCLK; else { inErr("CLK", curCLK, lastCLK); }
		if (pin === LR_W) inData = curLR_W; else { inErr("LR_W", curLR_W, lastLR_W); }
		if (pin === LNMI) inData = curLNMI; else { inErr("LNMI", curLNMI, lastLNMI); }
		if (pin === LIRQ) inData = curLIRQ; else { inErr("LIRQ", curLIRQ, lastLIRQ); }
		if (pin === LA[1]) inData = curLA1; else { inErr("LA1", curLA1, lastLA1); }
		if (pin === LA[3]) inData = curLA3; else { inErr("LA3", curLA3, lastLA3); }
		if (pin === LA[5]) inData = curLA5; else { inErr("LA5", curLA5, lastLA5); }
	}
	
	lastCLK = curCLK;
	lastLR_W = curLR_W;
	lastLNMI = curLNMI;
	lastLIRQ = curLIRQ;
	lastLA1 = curLA1;
	lastLA3 = curLA3;
	lastLA5 = curLA5;

	//l("inp "+v[pin]+" "+inData+" e="+inShort);
	return inData;
}

var be = 0;
var bf = 0;

function check(pi, po, d, e)
{
	var f = -1;
	var s = "o: "+v[po]+"("+po+") / i:"+v[pi]+"("+pi+")";
	var m;
	
	var inData = inp(pi);
	//l("check_in "+v[pi]+" "+inData);
	if (inShort) {
		f = 2;
		m = "shorted to "+inShort;
	} else
	if ((d === 0) && (inData == 1)) f = 0; else
	if ((d == 1) && (inData === 0)) f = 1;

	if (f != -1) {
		if (f <= 1) m = "did not read as "+f;
		l("FAIL: err "+e+", "+m+", "+s);
		be = e;
		bf = 0;
		blinkFail();
		return 1;
	} else {
		//l("  OK: chk "+e+", read "+d+", "+s);
		return 0;
	}
}

function setLEDs(d)
{
	outp("USR0", (d&1)? 1:0);
	outp("USR1", (d&2)? 1:0);
	outp("USR2", (d&4)? 1:0);
	outp("USR3", (d&8)? 1:0);
}

function halt(s)
{
	l(s);
	setLEDs(0);
	cp.exec("halt");
}

var runDir = 1;
var runLED = 1;

function blinkRun()
{
	setLEDs(runLED);
	runLED = runDir? (runLED << 1) : (runLED >> 1);
	if (runDir && runLED > 8) { runDir = 0; runLED = 4; }
	if (!runDir && runLED === 0) { runDir = 1; runLED = 2; }
}

var failCnt = 0;

function blinkFail()
{
	bf = bf? 0 : be;
	setLEDs(bf);
	
	if (failCnt++ < 20) {
		setTimeout(blinkFail, 1000);
	} else {
		halt("errors, halting..");
	}
}

var tp = [];
tp.push( { po: LRST, pi: LNMI, err: 1 } );
tp.push( { po: LVMA, pi: CLK, err: 2 } );
tp.push( { po: LA[6], pi: LR_W, err: 3 } );
tp.push( { po: LA[0], pi: LA[1], err: 4 } );
tp.push( { po: LA[2], pi: LA[3], err: 5 } );
tp.push( { po: LA[4], pi: LA[5], err: 6 } );

l("test_fixture init");
init();
cp.exec("date", function(err, stdout, stderr) { l(stdout); } );

var runCnt = 0;

setLast();
proc();

function proc()
{
	var t;
	
	blinkRun();

	for (t=0; t < tp.length; t++) {
		var pi = tp[t].pi;
		var po = tp[t].po;
		var err = tp[t].err;
		
		dirOut(po, 1);
		if (check(pi, po, 1, err)) return;
		outp(po, 0);
		if (check(pi, po, 0, err)) return;
		dirIn(po);
		setLast();
	}
	
	if (check(LIRQ, "none", 1, 7)) return;

	for (t=0; t < 8; t++) {
		outp(LD[t], 0);
		if (check(LIRQ, LD[t], 0, t+8)) return;
		outp(LD[t], 1);
		if (check(LIRQ, LD[t], 1, t+8)) return;
	}
	
	if (runCnt++ < (4*10)) {
		setTimeout(proc, 10);
	} else {
		halt("no errors, halting..");
	}
	
}
