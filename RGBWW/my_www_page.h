#pragma once

#include <ESPAsyncWebServer.h>
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <StreamString.h>
#include <Updater.h>
#include <vector>
#include "esphome/core/helpers.h"
#include <ArduinoJson.h>
#include "../../www_fs.h"
//#include "esphome/components/json/json_util.h"

static const char *TAG = "myWWWServer";

#ifndef USE_JSON
static char *global_json_build_buffer = nullptr;
static size_t global_json_build_buffer_size = 0;
using json_parse_t = std::function<void(JsonObject &)>;
using json_build_t = std::function<void(JsonObject &)>;

const char *build_json(const json_build_t &f, size_t *length);
std::string build_json(const json_build_t &f);
void parse_json(const std::string &data, const json_parse_t &f);



class VectorJsonBuffer : public ArduinoJson::Internals::JsonBufferBase<VectorJsonBuffer> {
public:
	class String {
	public:
		String(VectorJsonBuffer *parent): parent_(parent), start_(parent->size_) {}
		void append(char c) const {
			char *last = static_cast<char *>(this->parent_->do_alloc(1));
			*last = c;
		}
		const char *c_str() const {this->append('\0');return &this->parent_->buffer_[this->start_];}
	protected:
		VectorJsonBuffer *parent_;
		uint32_t start_;
	};
	void *alloc(size_t bytes) override {
		// Make sure memory addresses are aligned
		uint32_t new_size = round_size_up(this->size_);
		this->resize(new_size);
		return this->do_alloc(bytes);
	}
	size_t size() const { return this->size_; }
	void clear() {
		for (char *block : this->free_blocks_) free(block);
		this->size_ = 0;
		this->free_blocks_.clear();
	}
	String startString() { return {this}; }  // NOLINT

protected:
	void *do_alloc(size_t bytes) {
		const uint32_t begin = this->size_;
		this->resize(begin + bytes);
		return &this->buffer_[begin];
	}
	void resize(size_t size){
		if (size <= this->size_) {this->size_ = size;return;}
		this->reserve(size);
		this->size_ = size;
	}
	void reserve(size_t size) {
		if (size <= this->capacity_) return;
		uint32_t target_capacity = this->capacity_;
		if (this->capacity_ == 0) {
			// lazily initialize with a reasonable size
			target_capacity = JSON_OBJECT_SIZE(16);
		}
		while (target_capacity < size) target_capacity *= 2;
		char *old_buffer = this->buffer_;
		this->buffer_ = new char[target_capacity];
		if (old_buffer != nullptr && this->capacity_ != 0) {
			this->free_blocks_.push_back(old_buffer);
			memcpy(this->buffer_, old_buffer, this->capacity_);
		}
		this->capacity_ = target_capacity;
	}
	char *buffer_{nullptr};
	size_t size_{0};
	size_t capacity_{0};
	std::vector<char *> free_blocks_;
};

VectorJsonBuffer global_json_buffer;


void reserve_global_json_build_buffer(size_t required_size) 
{
	if (global_json_build_buffer_size == 0 || global_json_build_buffer_size < required_size) {
		delete[] global_json_build_buffer;
		global_json_build_buffer_size = std::max(required_size, global_json_build_buffer_size * 2);
		
		size_t remainder = global_json_build_buffer_size % 16U;
		if (remainder != 0) global_json_build_buffer_size += 16 - remainder;
		global_json_build_buffer = new char[global_json_build_buffer_size];
	}
}

const char *build_json(const json_build_t &f, size_t *length) 
{
	global_json_buffer.clear();
	JsonObject &root = global_json_buffer.createObject();

	f(root);

	// The Json buffer size gives us a good estimate for the required size.
	// Usually, it's a bit larger than the actual required string size
	//             | JSON Buffer Size | String Size |
	// Discovery   | 388              | 351         |
	// Discovery   | 372              | 356         |
	// Discovery   | 336              | 311         |
	// Discovery   | 408              | 393         |
	reserve_global_json_build_buffer(global_json_buffer.size());
	size_t bytes_written = root.printTo(global_json_build_buffer, global_json_build_buffer_size);

	if (bytes_written >= global_json_build_buffer_size - 1) {
		reserve_global_json_build_buffer(root.measureLength() + 1);
		bytes_written = root.printTo(global_json_build_buffer, global_json_build_buffer_size);
	}

	*length = bytes_written;
	return global_json_build_buffer;
}

void parse_json(const std::string &data, const json_parse_t &f) 
{
	global_json_buffer.clear();
	JsonObject &root = global_json_buffer.parseObject(data);

	if (!root.success()) {ESP_LOGW(TAG, "Parsing JSON failed.");return;}
	f(root);
}

std::string build_json(const json_build_t &f) 
{
	size_t len;
	const char *c_str = build_json(f, &len);
	return std::string(c_str, len);
}

#else
#include <ArduinoJson.h>
#endif

class WebPage;

void report_ota_error() {
	StreamString ss;
	Update.printError(ss);
	ESP_LOGW(TAG, "OTA Update failed! Error: %s", ss.c_str());
}

struct UrlMatch {
	std::string domain;  ///< The domain of the component, for example "sensor"
	std::string id;      ///< The id of the device that's being accessed, for example "living_room_fan"
	std::string method;  ///< The method that's being called, for example "turn_on"
	bool valid;          ///< Whether this match is valid
};


UrlMatch match_url(const std::string &url, bool only_domain = false) {
	UrlMatch match;
	match.valid = false;
	size_t domain_end = url.find('/', 1);
	if (domain_end == std::string::npos) return match;
	match.domain = url.substr(1, domain_end - 1);
	if (only_domain) {
		match.valid = true;
		return match;
	}
	if (url.length() == domain_end - 1) return match;
	size_t id_begin = domain_end + 1;
	size_t id_end = url.find('/', id_begin);
	match.valid = true;
	if (id_end == std::string::npos) {
		match.id = url.substr(id_begin, url.length() - id_begin);
		return match;
	}
	match.id = url.substr(id_begin, id_end - id_begin);
	size_t method_begin = id_end + 1;
	match.method = url.substr(method_begin, url.length() - method_begin);
	return match;
}


class MyWWWPage : public Component {
public:
	void init() {
		if (this->initialized_) {
			this->initialized_++;
			return;
		}
		this->server_ = new AsyncWebServer(this->port_);
		this->server_->begin();

		for (auto *handler : this->handlers_)
			this->server_->addHandler(handler);
		this->initialized_++;
	}
	void deinit() {
		this->initialized_--;
		if (this->initialized_ == 0) {
			delete this->server_;
			this->server_ = nullptr;
		}
	}
	AsyncWebServer *get_server() const { return server_; }
	float get_setup_priority() const override {return setup_priority::WIFI + 2.0f;}

	void add_handler(AsyncWebHandler *handler) {
		// remove all handlers
		this->handlers_.push_back(handler);
		if (this->server_ != nullptr)
			this->server_->addHandler(handler);
	}

	void add_ota_handler();

	void set_port(uint16_t port) { port_ = port; }
	uint16_t get_port() const { return port_; }

	void setup() override;

protected:
	friend class OTARequestHandler;
	int initialized_{0};
	uint16_t port_{80};
	AsyncWebServer *server_{nullptr};
	std::vector<AsyncWebHandler *> handlers_;
	WebPage *m_page;
};


class OTARequestHandler : public AsyncWebHandler {
public:
	OTARequestHandler(MyWWWPage *parent) : parent_(parent) {}
	void handleRequest(AsyncWebServerRequest *request) override {
		AsyncWebServerResponse *response;
		if (!Update.hasError()) {
			response = request->beginResponse(200, "text/plain", "Update Successful!");
		} else {
			StreamString ss;
			ss.print("Update Failed: ");
			Update.printError(ss);
			response = request->beginResponse(200, "text/plain", ss);
		}
		response->addHeader("Connection", "close");
		request->send(response);
	}
	void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,bool final) override {
		bool success;
		if (index == 0) {
			ESP_LOGI(TAG, "OTA Update Start: %s", filename.c_str());
			this->ota_read_length_ = 0;
			Update.runAsync(true);
			success = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
			if (!success) {report_ota_error();return;}
		} else if (Update.hasError()) {return;}
		success = Update.write(data, len) == len;
		if (!success) {report_ota_error();return;}
		this->ota_read_length_ += len;
		const uint32_t now = millis();
		if (now - this->last_ota_progress_ > 1000) {
			if (request->contentLength() != 0) {
				float percentage = (this->ota_read_length_ * 100.0f) / request->contentLength();
				ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage);
			} else {
				ESP_LOGD(TAG, "OTA in progress: %u bytes read", this->ota_read_length_);
			}
			this->last_ota_progress_ = now;
		}
		if (final) {
			if (Update.end(true)) {
				ESP_LOGI(TAG, "OTA update successful!");
				this->parent_->set_timeout(100, []() { App.safe_reboot(); });
			} else {
				report_ota_error();
			}
		}
	}
	bool canHandle(AsyncWebServerRequest *request) override {return request->url() == "/update" && request->method() == HTTP_POST;}
	bool isRequestHandlerTrivial() override { return false; }
protected:
	uint32_t last_ota_progress_{0};
	uint32_t ota_read_length_{0};
	MyWWWPage *parent_;
};

class WebPage : public Controller, public Component, public AsyncWebHandler {
public:
	WebPage(MyWWWPage *base) : base_(base) {}
	
	void set_username(const char *username) { username_ = username; }
	void set_password(const char *password) { password_ = password; }

	void setup() override;
	void dump_config() override;

	/// MQTT setup priority.
	float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

	/// Handle an index request under '/'.
	void handle_index_request(AsyncWebServerRequest *request);

	bool using_auth() { return username_ != nullptr && password_ != nullptr; }

	void on_light_update(light::LightState *obj) override;
	/// Handle a light request under '/light/<id>/</turn_on/turn_off/toggle>'.
	void handle_light_request(AsyncWebServerRequest *request, UrlMatch match);
	/// Dump the light state as a JSON string.
	std::string light_json(light::LightState *obj);

	/// Override the web handler's canHandle method.
	bool canHandle(AsyncWebServerRequest *request) override;
	/// Override the web handler's handleRequest method.
	void handleRequest(AsyncWebServerRequest *request) override;
	/// This web handle is not trivial.
	bool isRequestHandlerTrivial() override;

protected:
	MyWWWPage *base_;
	AsyncEventSource events_{"/events"};
	const char *username_{nullptr};
	const char *password_{nullptr};
};


void MyWWWPage::add_ota_handler() { this->add_handler(new OTARequestHandler(this)); }

void MyWWWPage::setup() {
	ESP_LOGD(TAG, "MyWWWPage::setup");
	m_page = new WebPage(this);
	m_page->setup();
}

void WebPage::setup() {
	ESP_LOGD(TAG, "Setting up web server...");
	this->base_->set_port(80);

	this->setup_controller();
	this->base_->init();

	this->events_.onConnect([this](AsyncEventSourceClient *client) {
		// Configure reconnect timeout
		client->send("", "ping", millis(), 30000);
		for (auto *obj : App.get_lights()) {
			if (!obj->is_internal()) {
				client->send(this->light_json(obj).c_str(), "state");
			}
		}
	});
#ifdef USE_LOGGER
	if (logger::global_logger != nullptr) logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) { this->events_.send(message, "log", millis()); });
#endif
	this->base_->add_handler(&this->events_);
	this->base_->add_handler(this);
	this->base_->add_ota_handler();
	this->set_interval(10000, [this]() { this->events_.send("", "ping", millis(), 30000); });
}

void WebPage::dump_config() {
	ESP_LOGCONFIG(TAG, "Web Server:");
	ESP_LOGCONFIG(TAG, "  Address: %s:%u", network_get_address().c_str(), this->base_->get_port());
	if (this->using_auth()) {ESP_LOGCONFIG(TAG, "  Basic authentication enabled");}
}


void WebPage::handle_index_request(AsyncWebServerRequest *request) 
{
	//ESP_LOGD(TAG, "Generating index.html");
	AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", __index_html, www_index_html_size);
	response->addHeader("Content-Encoding", "gzip");
	request->send(response);
}

void WebPage::on_light_update(light::LightState *obj) 
{
	if (obj->is_internal()) return;
	this->events_.send(this->light_json(obj).c_str(), "state");
}

void WebPage::handle_light_request(AsyncWebServerRequest *request, UrlMatch match) 
{
	//ESP_LOGD(TAG, "Light request");
	for (light::LightState *obj : App.get_lights()) {
		if (obj->is_internal()) continue;
		if (obj->get_object_id() != match.id) continue;
		
		if (request->method() == HTTP_GET) {
			std::string data = this->light_json(obj);
			request->send(200, "text/json", data.c_str());
		} else if (match.method == "toggle") {
			this->defer([obj]() { obj->toggle().perform(); });
			request->send(200);
		} else if (match.method == "turn_on") {
			auto call = obj->turn_on();
			if (request->hasParam("brightness")) call.set_brightness(request->getParam("brightness")->value().toFloat() / 255.0f);
			if (request->hasParam("r")) call.set_red(request->getParam("r")->value().toFloat() / 255.0f);
			if (request->hasParam("g")) call.set_green(request->getParam("g")->value().toFloat() / 255.0f);
			if (request->hasParam("b")) call.set_blue(request->getParam("b")->value().toFloat() / 255.0f);
			if (request->hasParam("white_value")) call.set_white(request->getParam("white_value")->value().toFloat() / 255.0f);
			if (request->hasParam("color_temp")) call.set_color_temperature(request->getParam("color_temp")->value().toFloat());
			if (request->hasParam("flash")) { float length_s = request->getParam("flash")->value().toFloat(); call.set_flash_length(static_cast<uint32_t>(length_s * 1000)); }
			if (request->hasParam("transition")) { float length_s = request->getParam("transition")->value().toFloat(); call.set_transition_length(static_cast<uint32_t>(length_s * 1000)); }
			if (request->hasParam("effect")) { const char *effect = request->getParam("effect")->value().c_str();call.set_effect(effect); }
			this->defer([call]() mutable { call.perform(); });
			request->send(200);
		} else if (match.method == "turn_off") {
			auto call = obj->turn_off();
			if (request->hasParam("transition")) {
				auto length = (uint32_t) request->getParam("transition")->value().toFloat() * 1000;
				call.set_transition_length(length);
			}
			this->defer([call]() mutable { call.perform(); });
			request->send(200);
		} else {
			request->send(404);
		}
		return;
	}
	request->send(404);
}

std::string WebPage::light_json(light::LightState *obj) 
{
	//ESP_LOGD(TAG, "Light json");
	return build_json([obj](JsonObject &root) {
		root["id"] = "light-" + obj->get_object_id();
		root["state"] = obj->remote_values.is_on() ? "ON" : "OFF";
#ifdef USE_JSON
		obj->dump_json(root);
#endif
	});
}

bool WebPage::canHandle(AsyncWebServerRequest *request) 
{
	if (request->url() == "/") return true;
	UrlMatch match = match_url(request->url().c_str(), true);

	if (!match.valid) return false;
	if ((request->method() == HTTP_POST || request->method() == HTTP_GET) && match.domain == "light") return true;
	return false;
}

void WebPage::handleRequest(AsyncWebServerRequest *request) 
{
	if (this->using_auth() && !request->authenticate(this->username_, this->password_)) return request->requestAuthentication();
	if (request->url() == "/") {this->handle_index_request(request);return;}
	UrlMatch match = match_url(request->url().c_str());
	if (match.domain == "light") {this->handle_light_request(request, match);return;}
}

bool WebPage::isRequestHandlerTrivial() { return false; }



