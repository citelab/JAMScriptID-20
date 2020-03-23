'use strict'

const mqtt = require('mqtt');
const cbor = require('cbor');
const JAMP = require('./jamprotocol');
const mqttconsts = require('./constants').mqtt;

var copts = {
    will: {topic: "/admin/announce/all",
        payload: cbor.encode({"cmd": "KILL", "opt": "ALL", "cond": "-", "condvec": 0, "actname": "-", "actid": "-", "actarg": "-", "args": []}),
        qos: 1,
        retain: 0},
    clientId: "dummyDevice-0000",
    keepalive: mqttconsts.keepAlive,
    clean: false,
    connectTimeout: mqttconsts.connectionTimeout,
};

var mserv = mqtt.connect("tcp://localhost:6000", copts);

setInterval(function(){
    console.log("Injecting the request...");
    var tmsg = {"cmd": "REXEC-ASY", "opt": "BROKER", "cond": "-", "condvec": 0, "actname": "testfunc", "actid": "-", "actarg": "-", "args": []};
    var encode = cbor.encode(tmsg);
    mserv.publish('/admin/announce/all', encode);
}, 1000);


