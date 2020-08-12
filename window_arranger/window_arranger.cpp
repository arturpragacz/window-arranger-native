// windows_arranger.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "window_arranger.h"
#include "TaskbarManager.h"
#include "utils.h"
#include "Timer.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <io.h>
#include <fcntl.h>
#include <type_traits>
#include <thread>


#define WM_NEW_TEXT_INPUT WM_USER
//#define WM_WINDOWS_POSITIONS_CHANGED WM_USER+1

#define ERROR_EXIT __COUNTER__+1


static void postMessage(const std::string& message) {
	uint32_t size = static_cast<uint32_t>(message.length());
	std::cout.write(reinterpret_cast<char*>(&size), sizeof size);
	std::cout.write(&message[0], size);
	std::cout.flush();
}

static void postMessage(const rapidjson::Value& value) {
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	value.Accept(writer);
	postMessage(buffer.GetString());
}


void fillOwnMessage(rapidjson::Document& d, int id, std::string_view status) {
	auto& allctr = d.GetAllocator();

	d.AddMember("source", "app", allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", "arrangementChanged", allctr)
		.AddMember("status", rapidjson::Value(status.data(), static_cast<rapidjson::SizeType>(status.length())), allctr);
}

static void postOwnMessage(int id, std::string_view status = "OK") {
	rapidjson::Document d(rapidjson::kObjectType);

	fillOwnMessage(d, id, status);

	postMessage(d);
}

template <class T>
static void postOwnMessage(int id, std::string_view status, T value) {
	rapidjson::Document d(rapidjson::kObjectType);

	fillOwnMessage(d, id, status);

	auto& allctr = d.GetAllocator();
	d.AddMember("value", value.toJson(allctr), allctr);

	postMessage(d);
}

template <class T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
static void postOwnMessage(int id, T value) {
	postOwnMessage(id, "OK", value);
}


void fillResponse(rapidjson::Document& d, int id, std::string_view status) {
	auto& allctr = d.GetAllocator();

	d.AddMember("source", "browser", allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", "response", allctr)
		.AddMember("status", rapidjson::Value(status.data(), static_cast<rapidjson::SizeType>(status.length())), allctr);
}

static void postResponse(int id, std::string_view status = "OK") {
	rapidjson::Document d(rapidjson::kObjectType);

	fillResponse(d, id, status);

	postMessage(d);
}

template <typename T>
static void postResponse(int id, std::string_view status, T value) {
	rapidjson::Document d(rapidjson::kObjectType);

	fillResponse(d, id, status);

	auto& allctr = d.GetAllocator();
	d.AddMember("value", value.toJson(allctr), allctr);

	postMessage(d);
}

template <typename T, typename = std::enable_if_t<!std::is_convertible_v<T, std::string_view>>>
static void postResponse(int id, T value) {
	postResponse(id, "OK", value);
}


static int processMessage(const std::string& data, TaskbarManager& tbm) {
	using namespace std::string_literals;
	rapidjson::Document d;
	if (d.Parse(data.c_str()).HasParseError())
		return 1;

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
				tbm.deleteFromObserved(deleteFromObserved);

				std::set<WindowHandle> addToObserved;
				for (const auto& handleStringJson : value["addToObserved"].GetArray()) {
					addToObserved.insert(convertCStringToWindowHandle(handleStringJson.GetString()));
				}

				postResponse(id, tbm.addToObserved(addToObserved));
			}
			else if (type == "getArrangement") {
				if (value.IsString() && value.GetString() == std::string("all")) {
					postResponse(id, tbm.getArrangement());
				}
				else {
					std::set<WindowHandle> handleSet;
					for (const auto& handleStringJson : value.GetArray()) {
						handleSet.insert(convertCStringToWindowHandle(handleStringJson.GetString()));
					}
					postResponse(id, tbm.getArrangement(handleSet));
				}
			}
			else if (type == "setArrangement") {
				postResponse(id, tbm.setArrangement(Arrangement(value)));
			}
			else {
				postResponse(id, "WRONG_TYPE");
			}
		}
		catch (TaskbarManager::Exception& e) {
			error("TaskbarManager: " + e.str);
			return 1;
		}
	}

	return 0;
}


// TODO: better error handling
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	logg("START");

	try {
		std::FILE* standardStreams[] = {stdin, stdout, stderr};
		for (auto& stream : standardStreams) {
			int fileDescriptor = _fileno(stream);
			if (_setmode(fileDescriptor, _O_BINARY) == -1)
				return error("Can't set binary mode of file descriptor: " + std::to_string(fileDescriptor), ERROR_EXIT);
		}
		std::ios::sync_with_stdio();

		TaskbarManager tbm;

		static auto parentThreadNativeId = GetCurrentThreadId();

		std::thread reader([] {
			uint32_t size = 0; 
			while (std::cin.read(reinterpret_cast<char*>(&size), sizeof size)) {
				auto data = new std::string(size, '\0');

				if (std::cin.read(&(*data)[0], size)) {
					if (!PostThreadMessage(parentThreadNativeId, WM_NEW_TEXT_INPUT,
						(WPARAM)data, (LPARAM)nullptr)) {
						break;
					}
				}
			}
			logg("Extracted in last operation: " + std::to_string(std::cin.gcount()));
			logg("Flags after last operation: " + std::to_string(std::cin.rdstate()));
			PostThreadMessage(parentThreadNativeId, WM_QUIT,
				(WPARAM)0, (LPARAM)0);
		});

		Timer timer(4000);

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			switch (msg.message) {
				case WM_NEW_TEXT_INPUT: {
					std::string* data = (std::string*)msg.wParam;
					if (processMessage(*data, tbm))
						return ERROR_EXIT;
					delete data;
				}
				break;

				case WM_TIMER: {
					Arrangement changed = tbm.updateArrangement();
					if (changed) {
						static int messageIdCounter = 1;
						postOwnMessage(messageIdCounter++, changed);
					}
				}
				break;

				default: {
					logg("other message: " + std::to_string(msg.message));
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		reader.join();

		logg("END");
		return (int)msg.wParam;
	}
	catch (TaskbarManager::Exception& e) {
		return error("TaskbarManager: " + e.str, ERROR_EXIT);
	}
	catch (Timer::Exception&) {
		return error("can't create timer", ERROR_EXIT);
	}

	// never should be here!
}
