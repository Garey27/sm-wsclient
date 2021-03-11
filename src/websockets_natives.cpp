#include "extension.h"

//------------------------------------------------------------------------------
extern net::io_context ioc;
std::unordered_map<Handle_t, std::shared_ptr<session>> ws_sessions;

static cell_t OnWriteCB(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	IPluginFunction* callback = pContext->GetFunctionById(params[2]);

	session->second->set_onwrite_cb(callback);
	return 1;
}

static cell_t OnReadCB(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	IPluginFunction* callback = pContext->GetFunctionById(params[2]);

	session->second->set_onread_cb(callback);
	return 1;
}

static cell_t OnErrorCB(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	IPluginFunction* callback = pContext->GetFunctionById(params[2]);

	session->second->set_on_error_cb(callback);
	return 1;
}

static cell_t OnOpenCB(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	IPluginFunction* callback = pContext->GetFunctionById(params[2]);

	session->second->set_on_open_cb(callback);

	return 1;
}

static cell_t OnCloseCB(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	IPluginFunction* callback = pContext->GetFunctionById(params[2]);

	session->second->set_on_close_cb(callback);

	return 1;
}

static cell_t WebSocketClient(IPluginContext* pContext, const cell_t* params) {
	char* baseURL;
	pContext->LocalToString(params[1], &baseURL);

	int port = params[2];
	bool secure = params[3];

	if (!secure) {
		auto sess = std::make_shared<websocket_session>(ioc, baseURL, port);

		Handle_t hndl = handlesys->CreateHandle(htWebsocketClient, sess.get(),
			pContext->GetIdentity(),
			myself->GetIdentity(), NULL);
		if (hndl == BAD_HANDLE) {
			pContext->ThrowNativeError("Could not create WebSocket client handle.");
			return BAD_HANDLE;
		}
		ws_sessions.insert({ hndl, sess });
		return hndl;
	}

	ssl::context ctx{ ssl::context::sslv23 };
	ctx.set_verify_mode(ssl::context::verify_peer |
		ssl::context::verify_fail_if_no_peer_cert);
	ctx.set_default_verify_paths();
	// tag::ctx_setup_source[]
	boost::certify::enable_native_https_server_verification(ctx);

	auto sess =
		std::make_shared<websocket_session_secure>(ioc, ctx, baseURL, port);

	Handle_t hndl = handlesys->CreateHandle(htWebsocketClient, sess.get(),
		pContext->GetIdentity(),
		myself->GetIdentity(), NULL);
	if (hndl == BAD_HANDLE) {
		pContext->ThrowNativeError("Could not create WebSocket client handle.");
		return BAD_HANDLE;
	}
	ws_sessions.insert({ hndl, sess });
	return hndl;
}

static cell_t OpenSession(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}
	session->second->run();
	return 1;
}

static cell_t CloseSession(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}
	session->second->close();
	return 1;
}
static cell_t WriteDataBuffer(IPluginContext* pContext, const cell_t* params) {
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	cell_t* addr;
	/* Translate the address. */
	pContext->LocalToPhysAddr(params[2], &addr);
	size_t len = params[3];
	std::vector<uint8_t> char_arr{ addr, addr + len };
	/*char *read_address = (char *)addr;
	for (int i = 0; i < len; i++) {
		char_arr.push_back(read_address[i]);
	}*/
	session->second->write_buffer(char_arr);
	return 1;
}

static cell_t WriteDataString(IPluginContext* pContext, const cell_t* params) {
	HandleError err;
	HandleSecurity sec(pContext->GetIdentity(), myself->GetIdentity());

	Handle_t hndlSession = static_cast<Handle_t>(params[1]);
	auto session = ws_sessions.find(hndlSession);
	if (session == ws_sessions.end()) {
		return pContext->ThrowNativeError(
			"Invalid WebSocket client handle %x", htWebsocketClient);
	}

	char* str;
	pContext->LocalToString(params[2], &str);
	session->second->write_message(str);

	return 1;
}

const sp_nativeinfo_t websocket_natives[] = {
	{"WebSocketClient.WebSocketClient",
	 WebSocketClient}, 
	{"WebSocketClient.Open",
	 OpenSession}, 
	{"WebSocketClient.Close",
	 CloseSession}, 
	{"WebSocketClient.WriteBuffer",
	 WriteDataBuffer}, 
	{"WebSocketClient.WriteString",
	 WriteDataString},
	{"WebSocketClient.OnWriteCB",
	 OnWriteCB}, 
	{"WebSocketClient.OnReadCB",
	 OnReadCB}, 
	{"WebSocketClient.OnErrorCB",
	 OnErrorCB}, 
	{"WebSocketClient.OnOpenCB",
	 OnOpenCB},
	{"WebSocketClient.OnCloseCB",
	 OnCloseCB},
	{NULL, NULL} };