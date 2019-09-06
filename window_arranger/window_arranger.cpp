// windows_order_fixer.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "window_arranger.h"
#include "TaskbarManager.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <fstream>
#include <thread>
#include <sstream>

#define WM_NEW_TEXT_INPUT WM_USER
//#define WM_WINDOWS_POSITIONS_CHANGED WM_USER+1

#define ERROR_EXIT __COUNTER__+1

static void log(const std::string& s) {
	static std::ofstream f("logmy.txt");//TODO better logging
	f << s << std::endl;
	f.flush();
}

static int error(const std::string& s, int e = 0) {
	log("ERROR: " + s);
	return e;
}


WindowHandle convertCStringToWindowHandle(const char* str) {
	return reinterpret_cast<WindowHandle>(strtoll(str, NULL, 0));
}

std::string convertWindowHandleToString(WindowHandle wh) {
	std::stringstream ss;
	ss << std::hex << std::showbase << reinterpret_cast<uint64_t>(wh);

	return ss.str();
}

static void postMessage(const std::string& message) {
	uint32_t size = static_cast<uint32_t>(message.length());
	std::cout.write(reinterpret_cast<char*>(&size), sizeof size);
	std::cout.write(&message[0], size);
	std::cout.flush();
}

struct NoPreValue {
	//int toJson(rapidjson::MemoryPoolAllocator<>& allocator) { return 1; }
};

template <class T = NoPreValue>
static void postOwnMessage(int id, const std::string& status = "OK", T preValue = T()) {
	rapidjson::Document d(rapidjson::kObjectType);
	auto& allctr = d.GetAllocator();

	d.AddMember("source", "app", allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", "arrangementChanged", allctr)
		.AddMember("status", status, allctr);

	if constexpr (std::negation_v<std::is_same<T, NoPreValue>>)
		d.AddMember("value", preValue.toJson(allctr), allctr);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	d.Accept(writer);
	postMessage(buffer.GetString());
}

template <class T>
static void postOwnMessage(int id, T preValue) {
	postOwnMessage(id, "OK", preValue);
}

//rapidjson::Document createResponseJsonDoc(int id, const std::string& status) {
//	rapidjson::Document d;
//
//	d["source"].SetString("browser");
//	d["id"].SetInt(id);
//	d["type"].SetString("response");
//	d["status"] = status;
//
//	return d;
//}

template <class T = NoPreValue>
static void postResponse(int id, const std::string& status = "OK", T preValue = T()) {
	rapidjson::Document d(rapidjson::kObjectType);
	auto& allctr = d.GetAllocator();
		
	d.AddMember("source", "browser", allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", "response", allctr)
		.AddMember("status", status, allctr);

	if constexpr (std::negation_v<std::is_same<T, NoPreValue>>)
		d.AddMember("value", preValue.toJson(allctr), allctr);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	d.Accept(writer);
	postMessage(buffer.GetString());
}

template <class T>
static void postResponse(int id, T preValue) {
	postResponse(id, "OK", preValue);
}

static int processMessage(const std::string& data, TaskbarManager& tbm) {
	using namespace std::string_literals;
	rapidjson::Document d;
	if (d.Parse(data.c_str()).HasParseError())
		return 1;

	std::string source = d["source"].GetString();
	int id = d["id"].GetInt();
	std::string type = d["type"].GetString();
	rapidjson::Value& value = d["value"]; // TODO: sprawdü, czy jest???

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
					for (auto it = value.Begin(); it != value.End(); ++it)
						handleSet.insert(convertCStringToWindowHandle(it->GetString()));
					postResponse(id, tbm.getArrangement(handleSet));
				}
			}
			else if (type == "setArrangement") {
				postResponse(id, tbm.setArrangement(Arrangement(value)));
			}
			else if (type == "updateArrangement") {
				postResponse(id, tbm.updateArrangement());
			}
			else {
				postResponse(id, "WRONG_TYPE"s);
			}
		}
		catch (TaskbarManager::Exception& e) {
			error("TaskbarManager: " + e.str);
			return 1;
		}
	}

	return 0;
}


class Timer {
	UINT_PTR timerId;
public:
	Timer(UINT elapse) {
		timerId = SetTimer(NULL, NULL, elapse, NULL);
		if (timerId == 0) {
			throw Exception();
		}
	}
	~Timer() { KillTimer(NULL, timerId); }
	struct Exception {};
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// TODO: check extension ID!

	try {
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
			PostThreadMessage(parentThreadNativeId, WM_QUIT,
				(WPARAM)0, (LPARAM)0);
		});

		Timer timer(4000);

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			switch (msg.message) {
			case WM_NEW_TEXT_INPUT:
			{
				std::string* data = (std::string*)msg.wParam;
				if (processMessage(*data, tbm))
					return ERROR_EXIT;
				delete data;
			}
			break;

			case WM_TIMER:
			{
				Arrangement changed = tbm.updateArrangement();
				if (changed) {
					static int messageIdCounter = 1;
					postOwnMessage(messageIdCounter++, changed);
				}
			}
			break;

			default:
			{
				log("other message: " + std::to_string(msg.message));
			}

			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		reader.join();

		log("END");
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
