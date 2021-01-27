#ifndef NOMINMAX
# define NOMINMAX
#endif
#include "extension.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <thread>

Extension g_zr;
SMEXT_LINK(&g_zr);

WebsocketClientHandler	g_WebsocketClientHandler;
HandleType_t htWebsocketClient;
extern std::unordered_map<Handle_t, std::shared_ptr<session>> ws_sessions;

net::io_context ioc;

static void FrameHook(bool simulating)
{
	for (auto& session : ws_sessions) {
		session.second->thread_mutex.lock();
		if (!session.second->callback_events.empty())
		{
			for (auto& cb_result : session.second->callback_events) {
				switch (cb_result.first) {
				case websocket_session::cb_on_open:
					if (session.second->on_open_cb) {
						session.second->on_open_cb->PushCell(session.first);
						session.second->on_open_cb->PushCell(
							cb_result.second.error_code.value());
						session.second->on_open_cb->Execute(NULL);
					}

					break;
				case websocket_session::cb_on_close:
					if (session.second->on_close_cb) {
						session.second->on_close_cb->PushCell(session.first);
						session.second->on_close_cb->PushCell(
							cb_result.second.error_code.value());
						session.second->on_close_cb->Execute(NULL);
					}

					break;
				case websocket_session::cb_on_read:
					if (session.second->on_read_cb) {
						std::vector<cell_t> cell_arr{
							cb_result.second.buffer.begin(),
							cb_result.second.buffer.end() };
						session.second->on_read_cb->PushCell(session.first);
						session.second->on_read_cb->PushArray(
							cell_arr.data(), cell_arr.size(), 0);
						session.second->on_read_cb->PushArray(
							(cell_t*)cb_result.second.buffer.data(), cb_result.second.buffer.size(), 0);
						session.second->on_read_cb->PushCell(
							cb_result.second.bytes_writted);
						session.second->on_read_cb->Execute(NULL);
					}
					break;
				case websocket_session::cb_on_error:
					if (session.second->on_error_cb) {
						session.second->on_error_cb->PushCell(session.first);
						session.second->on_error_cb->PushCell(cb_result.second.error_code.value());
						session.second->on_error_cb->PushString(cb_result.second.error_string.c_str());
						session.second->on_error_cb->Execute(NULL);
					}
					break;
				case websocket_session::cb_on_write:
					if (session.second->on_write_cb) {
						session.second->on_write_cb->PushCell(session.first);
						session.second->on_write_cb->PushCell(cb_result.second.bytes_writted);
						session.second->on_write_cb->Execute(NULL);
					}
					break;
				}
			}
			session.second->callback_events.clear();
		}
		session.second->thread_mutex.unlock();
	}
}

bool Extension::SDK_OnLoad(char* error, size_t maxlen, bool late) {
	sharesys->AddNatives(myself, websocket_natives);
	htWebsocketClient =
		handlesys->CreateType("WebSocketClient", &g_WebsocketClientHandler, 0,
			NULL, NULL, myself->GetIdentity(), NULL);
	sharesys->RegisterLibrary(myself, "websockets_client");
	smutils->AddGameFrameHook(&FrameHook);
	std::thread([] {
		boost::asio::executor_work_guard<decltype(ioc.get_executor())> work{
			ioc.get_executor() };		
		ioc.run();
		}).detach();
		return true;
}

void Extension::SDK_OnUnload() {
	ioc.stop();
}

void Extension::SDK_OnAllLoaded() {
}

void Extension::SDK_OnPauseChange(bool paused) {
}

void Extension::SDK_OnDependenciesDropped() {
}


void WebsocketClientHandler::OnHandleDestroy(HandleType_t type, void* object) {
	for(auto& sess: ws_sessions)
	{
		if(sess.second.get() == static_cast<session*>(object))
		{
			ws_sessions.erase(sess.first);
			break;
		}
	}
}