
#include "pch.h"
#include "Messenger.h"
#include "Arrangement.h"
#include "Json.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>


const std::string Messenger::CLASSNAME = "Messanger";


void Messenger::postMessage(const std::string& message) const {
	uint32_t size = static_cast<uint32_t>(message.length());
	std::cout.write(reinterpret_cast<char*>(&size), sizeof size);
	std::cout.write(&message[0], size);
	std::cout.flush();
}

void Messenger::postMessage(const rapidjson::Value& value) const {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	value.Accept(writer);
	postMessage(buffer.GetString());
}


void Messenger::fillMessage(rapidjson::Document& d, std::string_view source, int id, std::string_view msgType, std::string_view status) const {
	auto& allctr = d.GetAllocator();

	d.AddMember("source", rapidjson::Value(source.data(), static_cast<rapidjson::SizeType>(source.length())), allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", rapidjson::Value(msgType.data(), static_cast<rapidjson::SizeType>(msgType.length())), allctr)
		.AddMember("status", rapidjson::Value(status.data(), static_cast<rapidjson::SizeType>(status.length())), allctr);
}

template<typename T>
void Messenger::addValueToMessage(rapidjson::Document& d, const T& value) const {
	auto& allctr = d.GetAllocator();
	d.AddMember("value", toJson(value, allctr), allctr);
}


void Messenger::fillOwnMessage(rapidjson::Document& d, int id, std::string_view msgType, std::string_view status) const {
	fillMessage(d, "app", id, msgType, status);
}

void Messenger::postOwnMessage(int id, std::string_view msgType, std::string_view status) const {
	rapidjson::Document d(rapidjson::kObjectType);

	fillOwnMessage(d, id, msgType, status);

	postMessage(d);
}

template<typename T>
void Messenger::postOwnMessage(int id, std::string_view msgType, std::string_view status, const T& value) const {
	rapidjson::Document d(rapidjson::kObjectType);

	fillOwnMessage(d, id, msgType, status);
	addValueToMessage(d, value);

	postMessage(d);
}

template<typename T, typename>
void Messenger::postOwnMessage(int id, std::string_view msgType, const T& value) const {
	postOwnMessage(id, msgType, "OK", value);
}


void Messenger::fillResponse(rapidjson::Document& d, int id, std::string_view status) const {
	fillMessage(d, "browser", id, "response", status);
}

void Messenger::postResponse(int id, std::string_view status) const {
	rapidjson::Document d(rapidjson::kObjectType);

	fillResponse(d, id, status);

	postMessage(d);
}

template<typename T>
void Messenger::postResponse(int id, std::string_view status, const T& value) const {
	rapidjson::Document d(rapidjson::kObjectType);

	fillResponse(d, id, status);
	addValueToMessage(d, value);

	postMessage(d);
}

template<typename T, typename>
void Messenger::postResponse(int id, const T& value) const {
	postResponse(id, "OK", value);
}


int Messenger::processMessage(const std::string& data) const {
	using namespace std::string_literals;
	rapidjson::Document d;
	if (d.Parse(data.c_str()).HasParseError())
		throw Exception{ EXCEPTION_STRING + " parseError" };

	std::string source = d["source"].GetString();
	int id = d["id"].GetInt();
	std::string type = d["type"].GetString();
	rapidjson::Value& value = d["value"];

	if (source == "browser") {
		try {
			if (type == "changeObserved") {
				std::vector<WindowHandle> deleteFromObserved;
				for (const auto& handleStringJson : value["deleteFromObserved"].GetArray()) {
					deleteFromObserved.push_back(convertCStringToWindowHandle(handleStringJson.GetString()));
				}
				tbm.deleteFromObserved(deleteFromObserved.begin(), deleteFromObserved.end());

				std::vector<WindowHandle> addToObserved;
				for (const auto& handleStringJson : value["addToObserved"].GetArray()) {
					addToObserved.push_back(convertCStringToWindowHandle(handleStringJson.GetString()));
				}

				postResponse(id, tbm.addToObserved(addToObserved.begin(), addToObserved.end()));
			}
			else if (type == "getArrangement") {
				auto& handlesJson = value["handles"];
				auto& inObserved = value["inObserved"];

				if (handlesJson.IsString() && handlesJson.GetString() == std::string("all")) {
					postResponse(id, tbm.getArrangement());
				}
				else {
					std::vector<WindowHandle> handles;
					for (const auto& handleStringJson : handlesJson.GetArray()) {
						handles.push_back(convertCStringToWindowHandle(handleStringJson.GetString()));
					}
					postResponse(id, tbm.getArrangement(handles.begin(), handles.end(), inObserved.GetBool()));
				}
			}
			else if (type == "setArrangement") {
				postResponse(id, tbm.setArrangement(Arrangement(value, wgf)));
			}
			else {
				postResponse(id, "WRONG_TYPE");
			}
		}
		catch (TaskbarManager::Exception& e) {
			throw Exception{ EXCEPTION_STRING + " | " + e.str };
		}
	}

	return 0;
}

void Messenger::processTimer() const {
	Arrangement changed = tbm.updateArrangement();
	if (changed) {
		static int messageIdCounter = 1;
		postOwnMessage(messageIdCounter++, "arrangementChanged", changed);
	}
}
