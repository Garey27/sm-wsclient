
#ifndef _EXTENSION_H_
#define _EXTENSION_H_

#include "smsdk_ext.h"
#include "unordered_map"
#undef min
#undef max
#include "websockets_session.hpp"

class Extension : public SDKExtension {
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlen, bool late);
	virtual void SDK_OnUnload();
	virtual void SDK_OnAllLoaded();
	virtual void SDK_OnPauseChange(bool paused);
	virtual void SDK_OnDependenciesDropped();
};

class WebsocketClientHandler : public IHandleTypeDispatch
{
public:
	void OnHandleDestroy(HandleType_t type, void *object);
};

extern WebsocketClientHandler	g_WebsocketClientHandler;
extern HandleType_t			htWebsocketClient;

extern const sp_nativeinfo_t websocket_natives[];
#endif
