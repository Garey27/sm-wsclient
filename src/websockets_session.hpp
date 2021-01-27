#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

extern net::io_context ioc;
// Sends a WebSocket message and prints the response
class session {
public:
	enum connection_state
	{
		disconnected,
		connected,
		connecti
	};
	~session()
	{
		/*
		if (on_read_cb) {
			forwards->ReleaseForward(on_read_cb);
			on_read_cb = nullptr;
		}
		if (on_write_cb) {
			forwards->ReleaseForward(on_write_cb);
			on_write_cb = nullptr;
		}
		if (on_open_cb) {
			forwards->ReleaseForward(on_open_cb);
			on_open_cb = nullptr;
		}
		if (on_close_cb) {
			forwards->ReleaseForward(on_close_cb);
			on_close_cb = nullptr;
		}
		if (on_error_cb) {
			forwards->ReleaseForward(on_error_cb);
			on_error_cb = nullptr;
		}
		*/
	}
	IChangeableForward* on_read_cb = nullptr;
	IChangeableForward* on_write_cb = nullptr;
	IChangeableForward* on_open_cb = nullptr;
	IChangeableForward* on_close_cb = nullptr;
	IChangeableForward* on_error_cb = nullptr;
	bool reading = false;
	enum callback_type {
		cb_on_read,
		cb_on_write,
		cb_on_open,
		cb_on_close,
		cb_on_error
	};
	struct cb_data {
		std::vector<uint8_t> buffer;
		size_t bytes_writted;
		std::string error_string;
		beast::error_code error_code;
	};
	std::mutex thread_mutex;
	std::deque<std::pair<callback_type, cb_data>> callback_events;
	std::deque<boost::asio::mutable_buffer> queue_;
	virtual void write_buffer(std::vector<uint8_t> buffer) = 0;
	virtual void write_message(std::string msg) = 0;
	void set_onread_cb(IPluginFunction* callback) {
		IChangeableForward* forward = forwards->CreateForwardEx(
			NULL, ET_Ignore, 4, NULL, Param_Cell, Param_Array, Param_Array, Param_Cell);
		if (forward == NULL || !forward->AddFunction(callback)) {
			smutils->LogError(myself, "Could not create callback.");
			return;
		}
		on_read_cb = forward;
	}

	void set_onwrite_cb(IPluginFunction* callback) {
		IChangeableForward* forward =
			forwards->CreateForwardEx(NULL, ET_Ignore, 2, NULL, Param_Cell, Param_Cell);
		if (forward == NULL || !forward->AddFunction(callback)) {
			smutils->LogError(myself, "Could not create callback.");
			return;
		}
		on_write_cb = forward;
	}

	void set_on_open_cb(IPluginFunction* callback) {
		IChangeableForward* forward =
			forwards->CreateForwardEx(NULL, ET_Ignore, 2, NULL, Param_Cell, Param_Cell);
		if (forward == NULL || !forward->AddFunction(callback)) {
			smutils->LogError(myself, "Could not create callback.");
			return;
		}
		on_open_cb = forward;
	}

	void set_on_close_cb(IPluginFunction* callback) {
		IChangeableForward* forward =
			forwards->CreateForwardEx(NULL, ET_Ignore, 2, NULL, Param_Cell, Param_Cell);
		if (forward == NULL || !forward->AddFunction(callback)) {
			smutils->LogError(myself, "Could not create callback.");
			return;
		}
		on_close_cb = forward;
	}

	void set_on_error_cb(IPluginFunction* callback) {
		IChangeableForward* forward = forwards->CreateForwardEx(
			NULL, ET_Ignore, 3, NULL, Param_Cell, Param_Cell, Param_String);
		if (forward == NULL || !forward->AddFunction(callback)) {
			smutils->LogError(myself, "Could not create callback.");
			return;
		}
		on_error_cb = forward;
	}
	// Start the asynchronous operation
	virtual void run() = 0;
	virtual void close() = 0;
};


class websocket_session : public session {
	tcp::resolver resolver_;
	websocket::stream<beast::tcp_stream> ws_;
	beast::flat_buffer read_buffer_;
	std::string host_;
	int port_;
	void fail(beast::error_code ec, char const* what) {
		std::stringstream error_msg;
		error_msg << what << ": " << ec.message() << std::endl;

		cb_data cb{};
		cb.error_code = ec;
		cb.error_string = error_msg.str();
		thread_mutex.lock();
		callback_events.push_back({ cb_on_error, cb });
		thread_mutex.unlock();
		if (!queue_.empty()) {
			queue_.pop_front();
		}

		if (!queue_.empty()) {
			ws_.async_write(queue_.front(), beast::bind_front_handler(
				&websocket_session::on_write,
				this));
		}
	}
public:
	// Resolver and socket require an io_context
	explicit websocket_session(net::io_context& ioc,
		std::string host, int port)
		: resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc)),
		host_(host), port_(port) {
		ws_.binary(true);
	}
	~websocket_session() {
		ws_.close(websocket::close_code::normal);
	}
	void write_buffer(std::vector<uint8_t> buffer) override {
		// Send the message
		queue_.push_back(net::buffer(buffer));

		if (queue_.size() > 1)
			return;

		ws_.async_write(queue_.front(),
			beast::bind_front_handler(&websocket_session::on_write,
				this));
	}
	void write_message(std::string msg) override {
		// Send the message
		queue_.push_back(net::buffer((void*)msg.data(), msg.size() + 1));

		printf("%d\n", queue_.back().size());
		if (queue_.size() > 1)
			return;

		ws_.async_write(queue_.front(),
			beast::bind_front_handler(&websocket_session::on_write,
				this));
	}
	
	// Start the asynchronous operation
	void run() override {
		// Look up the domain name
		auto host = host_.c_str();
		auto port = std::to_string(port_).c_str();
		resolver_.async_resolve(
			host_, std::to_string(port_),
			beast::bind_front_handler(&websocket_session::on_resolve,
				this));
	}

	void close() override {
		ws_.async_close(websocket::close_code::normal,
			beast::bind_front_handler(
				&websocket_session::on_close,
				this));
	}

	void
		on_close(beast::error_code ec)
	{
		if (ec)
			return fail(ec, "close");

		cb_data cb{};
		cb.error_code = ec;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_close, cb });
		thread_mutex.unlock();
	}
	
	void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
		if (ec)
			return fail(ec, "resolve");

		// Set a timeout on the operation
		beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

		// Make the connection on the IP address we get from a lookup
		beast::get_lowest_layer(ws_).async_connect(
			results, beast::bind_front_handler(&websocket_session::on_connect,
				this));
	}

	void on_connect(beast::error_code ec,
		tcp::resolver::results_type::endpoint_type ep) {
		if (ec)
			return fail(ec, "connect");

		// Turn off the timeout on the tcp_stream, because
		// the websocket stream has its own timeout system.
		beast::get_lowest_layer(ws_).expires_never();

		// Set suggested timeout settings for the websocket
		ws_.set_option(websocket::stream_base::timeout::suggested(
			beast::role_type::client));

		// Set a decorator to change the User-Agent of the handshake
		ws_.set_option(
			websocket::stream_base::decorator([](websocket::request_type& req) {
				req.set(http::field::user_agent,
					std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-client-async");
				}));

		// Update the host_ string. This will provide the value of the
		// Host HTTP header during the WebSocket handshake.
		// See https://tools.ietf.org/html/rfc7230#section-5.4
		host_ += ':' + std::to_string(ep.port());

		// Perform the websocket handshake
		ws_.async_handshake(host_, "/",
			beast::bind_front_handler(&websocket_session::on_handshake,
				this));
	}


	void on_handshake(beast::error_code ec) {
		if (ec)
			return fail(ec, "handshake");
		cb_data cb{};
		cb.error_code = ec;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_open, cb });
		thread_mutex.unlock();
		/*
		 if (on_connected_cb) {
			on_connected_cb->PushCell(ec.value());
			on_connected_cb->Execute(NULL);
		}
		*/
	}

	void on_write(beast::error_code ec, std::size_t bytes_transferred) {
		if (ec)
			return fail(ec, "write");
		cb_data cb{};
		cb.bytes_writted = bytes_transferred;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_write, cb });
		thread_mutex.unlock();
		/*
		if (on_write_cb) {
			on_write_cb->PushCell(bytes_transferred);
			on_write_cb->Execute(NULL);
		}
		*/
		// Read a message into our buffer
		if (!reading) {
			reading = true;
			ws_.async_read(read_buffer_, beast::bind_front_handler(
				&websocket_session::on_read,
				this));
		}
	}

	void on_read(beast::error_code ec, std::size_t bytes_transferred) {
		reading = false;
		if (ec)
			return fail(ec, "read");

		cb_data cb{};
		for (auto it = net::buffer_sequence_begin(read_buffer_.data()),
			end = net::buffer_sequence_end(read_buffer_.data());
			it != end; ++it) {
			net::const_buffer buf = *it;
			cb.buffer.insert(
				cb.buffer.end(), static_cast<char const*>(buf.data()),
				static_cast<char const*>(buf.data()) + buf.size());
		}
		read_buffer_.clear();
		cb.bytes_writted = bytes_transferred;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_read, cb });
		thread_mutex.unlock();
		if (!queue_.empty()) {
			queue_.pop_front();
		}

		if (!queue_.empty()) {
			ws_.async_write(queue_.front(), beast::bind_front_handler(
				&websocket_session::on_write,
				this));

		}
		else if(!reading) {
			reading = true;
			ws_.async_read(read_buffer_, beast::bind_front_handler(
				&websocket_session::on_read,
				this));
		}
	}
};
class websocket_session_secure : public session {
	void fail(beast::error_code ec, char const* what) {
		std::stringstream error_msg;
		error_msg << what << ": " << ec.message() << std::endl;

		cb_data cb{};
		cb.error_code = ec;
		cb.error_string = error_msg.str();
		thread_mutex.lock();
		callback_events.push_back({ cb_on_error, cb });
		thread_mutex.unlock();
		if (!queue_.empty()) {
			queue_.pop_front();
		}

		if (!queue_.empty()) {
			ws_.async_write(queue_.front(), beast::bind_front_handler(&websocket_session_secure::on_write,
				this));
		}
	}

	tcp::resolver resolver_;
	websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
	beast::flat_buffer read_buffer_;
	std::string host_;
	int port_;

public:
	explicit websocket_session_secure(net::io_context& ioc, ssl::context& ctx,
		std::string host, int port)
		: resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc), ctx),
		host_(host), port_(port) {
		ws_.binary(true);
	}
	~websocket_session_secure() {
		ws_.close(websocket::close_code::normal);
		if (on_read_cb) {
			forwards->ReleaseForward(on_read_cb);
		}
		if (on_write_cb) {
			forwards->ReleaseForward(on_write_cb);
		}
		if (on_open_cb) {
			forwards->ReleaseForward(on_open_cb);
		}
		if (on_close_cb) {
			forwards->ReleaseForward(on_close_cb);
		}
		if (on_error_cb) {
			forwards->ReleaseForward(on_error_cb);
		}
	}
	void write_buffer(std::vector<uint8_t> buffer) override {
		// Send the message
		queue_.push_back(net::buffer(buffer));

		if (queue_.size() > 1)
			return;

		ws_.async_write(queue_.front(), beast::bind_front_handler(
			&websocket_session_secure::on_write,
			this));
	}
	void write_message(std::string msg) override {
		// Send the message
		queue_.push_back(net::buffer((void*)msg.data(), msg.size() + 1));
		printf("%d\n", queue_.back().size());

		if (queue_.size() > 1)
			return;

		ws_.async_write(queue_.front(), beast::bind_front_handler(
			&websocket_session_secure::on_write,
			this));
	}	
	// Start the asynchronous operation
	void run() override {
		// Look up the domain name
		auto host = host_.c_str();
		auto port = std::to_string(port_).c_str();
		resolver_.async_resolve(
			host_, std::to_string(port_),
			beast::bind_front_handler(&websocket_session_secure::on_resolve,
				this));
	}

	void close() override {
		ws_.async_close(websocket::close_code::normal,
			beast::bind_front_handler(
				&websocket_session_secure::on_close,
				this));
	}

	void
		on_close(beast::error_code ec)
	{
		if (ec)
			return fail(ec, "close");
		cb_data cb{};
		cb.error_code = ec;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_close, cb });
		thread_mutex.unlock();
	}
	
	void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
		if (ec)
			return fail(ec, "resolve");

		// Set a timeout on the operation
		beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

		// Make the connection on the IP address we get from a lookup
		beast::get_lowest_layer(ws_).async_connect(
			results,
			beast::bind_front_handler(&websocket_session_secure::on_connect,
				this));
	}

	void on_connect(beast::error_code ec,
		tcp::resolver::results_type::endpoint_type ep) {
		if (ec)
			return fail(ec, "connect");

		// Update the host_ string. This will provide the value of the
		// Host HTTP header during the WebSocket handshake.
		// See https://tools.ietf.org/html/rfc7230#section-5.4
		host_ += ':' + std::to_string(ep.port());
		// Set a timeout on the operation
		beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

		// Set SNI Hostname (many hosts need this to handshake successfully)
		if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
			host_.c_str())) {
			ec = beast::error_code(static_cast<int>(::ERR_get_error()),
				net::error::get_ssl_category());
			return fail(ec, "connect");
		}

		// Perform the SSL handshake
		ws_.next_layer().async_handshake(
			ssl::stream_base::client,
			beast::bind_front_handler(
				&websocket_session_secure::on_ssl_handshake,
				this));
	}

	void on_ssl_handshake(beast::error_code ec) {
		if (ec)
			return fail(ec, "ssl_handshake");
		// Turn off the timeout on the tcp_stream, because
		// the websocket stream has its own timeout system.
		beast::get_lowest_layer(ws_).expires_never();

		// Set suggested timeout settings for the websocket
		ws_.set_option(websocket::stream_base::timeout::suggested(
			beast::role_type::client));

		// Set a decorator to change the User-Agent of the handshake
		ws_.set_option(
			websocket::stream_base::decorator([](websocket::request_type& req) {
				req.set(http::field::user_agent,
					std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-client-async-ssl");
				}));

		// Perform the websocket handshake
		ws_.async_handshake(
			host_, "/",
			beast::bind_front_handler(&websocket_session_secure::on_handshake,
				this));
	}

	void on_handshake(beast::error_code ec) {
		if (ec)
			return fail(ec, "handshake");
		cb_data cb{};
		cb.error_code = ec;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_open, cb });
		thread_mutex.unlock();
		/*
		 if (on_connected_cb) {
			on_connected_cb->PushCell(ec.value());
			on_connected_cb->Execute(NULL);
		}
		*/
	}

	void on_write(beast::error_code ec, std::size_t bytes_transferred) {
		if (ec)
			return fail(ec, "write");
		cb_data cb{};
		cb.bytes_writted = bytes_transferred;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_write, cb });
		thread_mutex.unlock();
		/*
		if (on_write_cb) {
			on_write_cb->PushCell(bytes_transferred);
			on_write_cb->Execute(NULL);
		}
		*/
		
		// Read a message into our buffer
		if (!reading)
		{
			reading = true;
			ws_.async_read(read_buffer_, beast::bind_front_handler(
				&websocket_session_secure::on_read,
				this));
		}
	}

	void on_read(beast::error_code ec, std::size_t bytes_transferred) {
		reading = false;
		
		if (ec)
			return fail(ec, "read");

		cb_data cb{};
		for (auto it = net::buffer_sequence_begin(read_buffer_.data()),
			end = net::buffer_sequence_end(read_buffer_.data());
			it != end; ++it) {
			net::const_buffer buf = *it;
			cb.buffer.insert(
				cb.buffer.end(), static_cast<char const*>(buf.data()),
				static_cast<char const*>(buf.data()) + buf.size());
		}
		read_buffer_.clear();
		cb.bytes_writted = bytes_transferred;
		thread_mutex.lock();
		callback_events.push_back({ cb_on_read, cb });
		thread_mutex.unlock();
		if (!queue_.empty()) {
			queue_.pop_front();
		}

		if (!queue_.empty()) {
			ws_.async_write(queue_.front(),
				beast::bind_front_handler(&websocket_session_secure::on_write,
					this));
		}
		else if (!reading) {
			reading = true;
			ws_.async_read(read_buffer_, beast::bind_front_handler(
				&websocket_session_secure::on_read,
				this));
		}
	}
};
