TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += socketio
qtHaveModule(quick): SUBDIRS += imports
