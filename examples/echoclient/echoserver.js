/**
 * @author Kurt Pattyn (pattyn.kurt@gmail.com)
 * @version 0.1
 *
 */
var io = require('socket.io').listen(9000, {
	"log": false,
	"close timeout": 30
});

io.sockets.on('connection', function(socket) {
	socket.emit('news', 1, 'world', function(data) {
		console.log("Message was delivered.");
	});
	socket.on('event with 2 arguments', function(data1, data2,callback) {
		console.log('Received event with arguments:' + data1 + " and " + data2);
		if (callback) {
			callback("Okay received your event with 2 arguments");
		}
	});
	socket.on('event with a json object', function(object, callback) {
		console.log('Received event with object:' + JSON.stringify(object));
		if (callback) {
			callback("Okay received your json object.");
		}
	});
	socket.emit("event from server", {"attribute1" : "hello", "attribute2": "world!"});
});
