%modules = (
    "QtSocketIo" => "$basedir/src/socketio",
);

%moduleheaders = (
);

%modulepris = (
    "QtSocketIo" => "$basedir/modules/qt_socketio.pri",
);

%classnames = (
    "qtsocketioclient.h" => "QtSocketIo",
);

%dependencies = (
    "qtbase" => "",
	"qtwebsockets" => ""
);
