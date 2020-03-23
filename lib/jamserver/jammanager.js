/**
 * This file listens for events in the key space and takes the required action. It also
 */

var Redis = require('redis');
var JAMDatastream = require('./jamdatastream');
var ebus = require('./ebus');

var jamdatastream_callbacks = {};
var jamdatasources = {};
var initalizing = true;
var debug = false;

const globals = require('./constants').globals;


module.exports = function(cmdopts, jsys) {
    var host = cmdopts.redhost;
    var port = cmdopts.redport;
    var pack = {};  //the object to return
    /**
     * Each element in the jamdata is a pair of key => jamdatastream.
     * Sample: key: jamdatastream: ...
     * key is the unique key which would receive the storage elements or act as the subscription handle/tag
     * object is a JAMDatastream object
     * @type {Object}
     */
    var jamdatastreams = {};
    var msg_receive_callback;

    // this would listen for all keyspace events (using the keyevent notification mechanism) and channel it to the right jamdatastream
    var listener = Redis.createClient({
        host: host,
        port: port,
        "return_buffers": true
    });

    // this is the handle that would be passed to all instances of the JAMDatastream for making a call to the Redis server
    var executor = Redis.createClient({
        host: host,
        port: port,
        "return_buffers": true
    });

    // Mahesh: This will run every time redis server is reconnected..
    listener.on('connect', function() {

    //     jamsys.setRedis(host, port);
        ebus.dataUp(host, port);
    //     jamsys.adUp('datadepot', {host: host, port: port});
    });

    // // Mahesh: This will run every time redis server is disconnected..
    listener.on('end', function() {
    //     jamsys.adDown('datadepot');
        ebus.dataDown();
    });

    listener.on('error', function() {
    //     jamsys.adDown('datadepot');
        ebus.dataDown();
    });

    //This will hold a connection to the redis of the parent of the current level in the hierarchy.
    //For e.g device->Fog; fog->Cloud
    var parentRedis = null; //This is for broadcaster. first default to null. connectToParent method will set the connection
    var parentRedisLogger = null;   //This is for logging data to the parent

    //This holds all listeners for data/event changes on Redis
    //Each listener subscribes to a particular key which will be accessible via listeners.{key}
    //listeners.{key} is an array of all listeners subscribing to that data changes in that key
    var listeners = {};

    var broadcasters = [];  //this keeps track of all broadcasters

    // TODO check if the redis connection was successful before attempting to query the redis server

    var parentConObj = null;    //the data retrieved from listening to the parent advertisement on their IP address
    var parentStatusListeners = [];

    ebus.on('fog-data-up', function(info) {
        console.log("Fog Data Up -------------------------", info, parentConObj);
        if( pack.isDevice && (parentConObj === null || parentConObj.host != info.host || parentConObj.port != info.port) )
            doConnectToParent(info);
    });

    ebus.on('fog-data-down', function() {

        if (pack.isDevice && (parentConObj !== null))
            doDisconnectFromParent();
    });

    ebus.on('cloud-data-up', function(info) {
        console.log("Cloud Data Up ------------------------XXXXX", info);
        if( pack.isFog && (parentConObj === null || parentConObj.host != info.host || parentConObj.port != info.port) )
            doConnectToParent(info);
    });

    ebus.on('cloud-data-down', function() {

        if (pack.isFog && (parentConObj !== ull))
            doDisconnectFromParent();
    });

    function doConnectToParent(info){
        parentConObj = info;
        parentRedis = pack.connectToParent(info.host, info.port);
        parentRedisLogger = pack.connectToParent(info.host, info.port);
        parentStatusListeners.forEach(function(obj){
            obj.func.call(obj.context, info, parentRedis);
        });

        parentRedis.on('error', function() {
            //parentConObj = null;
        });

        parentRedisLogger.on('error', function() {
            //parentConObj = null;
        });

        parentRedis.on('end', function() {
            //parentConObj = null;
        });

        parentRedisLogger.on('end', function() {
            //parentConObj = null;
        });
    }

    function doDisconnectFromParent() {

        if (parentRedis !== undefined && parentRedis !== null) {
            parentRedis.quit();
        }
        if (parentRedisLogger !== undefined && parentRedisLogger !== null)
            parentRedisLogger.quit();
        parentConObj = null;
    }

    //'__keyevent@*:*'
    function init() {
        // Turn on notify events for:
        // E: Keyspace events
        // z: Sorted sets
        listener.config(['set', 'notify-keyspace-events', 'KEA']);

        // // Allows other machine to access redis clients on this one.
    //    listener.rawCall(['config', 'set', 'protected-mode', 'no']);
    //    executor.rawCall(['config', 'set', 'protected-mode', 'no']);

        //listen for all events for all keys and process them in the listenerEvent function
        listener.psubscribe('__keyevent*');
        listener.on('pmessage', listenerEvent);
    }

    function listenerEvent(pat, ch, bufferData) {
        var data = bufferData.toString();

        if (data !== undefined) {

            if (jamdatastream_callbacks[data] != undefined) {
                // msg_receive_callback(data);
                let jamdatastream = jamdatastream_callbacks[data];
                jamdatastream.set_size++;
                if (jamdatastream.refresh_rate === 0) {
                    jamdatastream.request_value_refresh();
                }
            } else {
                var idx = data.lastIndexOf('.');
                if (idx !== -1) {
                    var jamdatasource_key = data.substring(0, idx);
                    var jamdatasource = jamdatasources[jamdatasource_key];
                    if (jamdatasource != undefined) {
                        idx = data.lastIndexOf('[');
                        var deviceId = data.substring(idx + 1, data.length - 1);
                        jamdatasource.addDatastream(deviceId);
                        let jamdatastream = jamdatastream_callbacks[data];
                        jamdatastream.set_size++;
                        if (jamdatastream.refresh_rate === 0) {
                            jamdatastream.request_value_refresh();
                        }
                    }
                }
            }

            //notify all listeners for this message on the current key
            if (listeners[data]) { //there are listeners listening on this key

                var keyObject = buildKeyObject(data);

                for (let listener of listeners[data]) {
                    if (listener.notify && typeof listener.notify === 'function')
                        listener.notify.call(listener, keyObject);
                    else if (typeof listener === 'function')
                        listener.call({}, keyObject);
                }
            }
        }
    }
    //Used to build subscription key for Apps that subscribe with Objects
    function buildKey(obj){
        var key = 'aps[' + obj.app + ']';
        if( obj.namespace )
            key = key + '.ns[' + obj.namespace + ']';
        if( obj.flow )  //for OutFlow and InFlow
            key = key + '.flow[' + obj.flow + ']';
        if( obj.datasource )
            key = key + '.ds[' + obj.datasource + ']';
        if( obj.datastream )
            key = key + '.dts[' + obj.datastream + ']';

        return key;
    }

    //Used to rebuild subscription key back to Objects
    function buildKeyObject(key){
        var obj = {}, content;

        var parts = key.split(".");

        for( let part of parts ){
            content = part.substring(part.indexOf("[") + 1, part.indexOf("]"));

            if( part.startsWith("aps") )
                obj.app = content;
            else if( part.startsWith("ns") )
                obj.namespace = content;
            else if( part.startsWith("flow") )
                obj.flow = content;
            else if( part.startsWith("ds") )
                obj.datasource = content;
            else if( part.startsWith("dts") )
                obj.datastream = content;
        }

        return obj;
    }

    init();

    pack = {
        add_jamdatasource: function(jamdatasource) {
            if (jamdatasource === undefined) {
                throw new Error("Undefined jamdatasource");
            }
            jamdatasources[jamdatasource.key] = jamdatasource;
        },

        add_jamdatastream: function(jamdatastream) {
            if (jamdatastream === undefined) {
                throw new Error("Undefined jamdatastream");
            }
            jamdatastream.redis = executor;
            jamdatastream_callbacks[jamdatastream.key] = jamdatastream;
        },

        // Used to delete the old key when a new key for a datastream is set
        delete_jamdatastream: function(key){
            if( jamdatastream_callbacks[key] )
                delete jamdatastream_callbacks[key];
        },

        log: function(key, value, slots, appID, deviceID) {
            // check if we already have a key saved in the array

            var jamdatastream;

            if (jamdatastreams[key]) { // we have added this key before
                jamdatastream = jamdatastreams[key].jamdatastream;
            } else { // a new key
                jamdatastream = new JAMDatastream(key, slots, executor);
                jamdatastreams[key] = jamdatastream;
            }

            jamdatastream.log(value, function(resp) {
                // resp is an object with 'status' and based on the status state, we could have error (if process failed),
                // or message (if process was successful)
            });
        },

        simpleLog: function(key, value, callback, redis, timestamp){
            if( !redis )
                redis = executor;

            //check if the key is an object

            var cb = function(e, d) {
                if (e) {
                    if (callback) {
                        callback({
                            status: false,
                            error: e
                        });
                    }
                } else if (callback) {
                    (function (time){
                        setTimeout(function() {
                            callback({
                                status: true,
                                message: 'Process Completed Successfully!',
                                timestamp: time
                            });
                        }, 0);
                    })(timestamp);
                }
            };

            redis.zadd([key, timestamp, value], cb);
        },

        subscribe: function(key, listener){
            //check if the key is an object
            if( Boolean(key) && typeof key === 'object' )   //convert the key to string
                key = buildKey(key);

            if( listeners[key] )
                listeners[key].push(listener);
            else
                listeners[key] = [listener];
        },

        unsubscribe: function(key, listener){
            //check if the key is an object
            if( Boolean(key) && typeof key === 'object' )   //convert the key to string
                key = buildKey(key);

            if( listeners[key] ){
                var i = 0;
                for( let l of listeners[key] ){
                    if( l == listener )
                        listeners[key].splice(i, 1);
                    i++;
                }
            }
        },

        buildKey: buildKey, //Added by Richboy on Sat 3 June 2017

        buildKeyObject: buildKeyObject, //Added by Richboy on Sat 3 June 2017

        getRedisHost() {
            return host;
        },

        getRedisPort() {
            return port;
        },

        app: cmdopts.app,
        redis: executor,
        parentRedis: parentRedis,   //DEPRECATED
        getParentRedis: function(){
            return parentRedis;
        },
        getParentRedisLogger: function(){
            return parentRedis;
        },
        /**
         * This method tries to connect to the redis of the parent of the current level in the hierarchy.
         */
        connectToParent(ip, port){
            return Redis.createClient({
                host: ip,
                port: port,
                "return_buffers": true
            });
        },
        isDevice: cmdopts.device,
        isFog: cmdopts.fog,
        isCloud: cmdopts.cloud,
        getLevelCode: function(){
            if( pack.isDevice )
                return "device";
            return pack.isFog ? "fog" : "cloud";
        },
        addParentUpSub: function(obj){ //add parent up subscription
            parentStatusListeners.push(obj);
        },
        getParentConObject: function(){
            return parentConObj;
        },
        deviceID: jsys.id,
        fullID: jsys.id + "_" + jsys.type,
        addBroadcaster: function(broadcaster){
            broadcasters.push(broadcaster);
        },
        getClock: function(){
            //go through all the broadcasters and get the max clock
            if( broadcasters.length === 0 )
                return null;
            let max = {clock: broadcasters[0].clock, subClock: broadcasters[0].subClock};

            for( let broadcaster of broadcasters ){
                if( broadcaster.clock > max.clock || (broadcaster.clock == max.clock && broadcaster.subClock > max.subClock) ){
                    max.clock = broadcaster.clock;
                    max.subClock = broadcaster.subClock;
                }
            }
            //we are returning it as: "2310" or "2310,2"
            return max.clock + '.' + max.subClock;
        },
        host: host,
        port: port
    };

    return pack;
}

