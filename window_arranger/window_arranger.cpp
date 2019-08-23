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

#define WM_NEW_TEXT_INPUT WM_USER
//#define WM_WINDOWS_POSITIONS_CHANGED WM_USER+1

#define ERROR_EXIT __COUNTER__+1

static void log(const std::string& s) {
	static std::ofstream f("logmy.txt");//TODO
	f << s << std::endl;
	f.flush();
}

static int error(const std::string& s, int e = 0) {
	log("ERROR: " + s);
	return e;
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

static int processMessage(const std::string& data, TaskbarManager& tb);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
										 _In_opt_ HINSTANCE hPrevInstance,
										 _In_ LPWSTR    lpCmdLine,
										 _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	try {
		std::ios::sync_with_stdio();
		TaskbarManager tb;

		static auto parentThreadNativeId = GetCurrentThreadId();

		std::thread reader([] {
			uint32_t size = 0; int i = 3; //TODO
			while (std::cin.read(reinterpret_cast<char*>(&size), sizeof size)) {
				if(i--<0) break; //TODO
				log(size);
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

		Timer timer(3000);

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			switch (msg.message) {
				case WM_NEW_TEXT_INPUT:
				{
					std::string* data = (std::string*)msg.wParam;
					if (processMessage(*data, tb))
						return ERROR_EXIT;
					delete data;
				}
				break;

				case WM_TIMER:
				{
					// TODO
				}
				break;

				default:{} //TODO

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
	catch (Timer::Exception& ) {
		return error("can't create timer", ERROR_EXIT);
	}

	// never should be here!
}


static void postMessage(const std::string& message) {
	uint32_t size = static_cast<uint32_t>(message.length());
	std::cout.write(reinterpret_cast<char*>(&size), sizeof size);
	std::cout.write(&message[0], size);
	std::cout.flush();
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

struct NoPreValue {
	int toJson(rapidjson::MemoryPoolAllocator<>& allocator) { return 1; }
};
template <class T = NoPreValue>
static void postResponse(int id, const std::string& status = "OK", T preValue = T()) {
	rapidjson::Document d(rapidjson::kObjectType);
	auto& allctr = d.GetAllocator();
		
	d.AddMember("source", "browser", allctr)
		.AddMember("id", id, allctr)
		.AddMember("type", "response", allctr)
		.AddMember("status", status, allctr);
	//d["source"].SetString("browser");
	//d["id"].SetInt(id);
	//d["type"].SetString("response");
	//d["status"].SetString(status, d.GetAllocator());

	if (std::negation_v<std::is_same<T, NoPreValue>>)
		//d["value"] = preValue.toJson(d.GetAllocator());
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

static int processMessage(const std::string& data, TaskbarManager& tb) {
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
				std::vector<Handle> deleteFromObserved;
				for (const auto& handleJson : value["deleteFromObserved"].GetArray()) {
					deleteFromObserved.push_back(reinterpret_cast<Handle>(handleJson.GetInt64()));
				}
				tb.deleteFromObserved(deleteFromObserved);

				std::set<Handle> addToObserved;
				for (const auto& handleJson : value["addToObserved"].GetArray()) {
					addToObserved.insert(reinterpret_cast<Handle>(handleJson.GetInt64()));
				}

				postResponse(id, tb.addToObserved(addToObserved));
			}
			else if (type == "getArrangement") {
				if (value.IsString() && value.GetString() == std::string("all")) {
					postResponse(id, tb.getArrangement());
				}
				else {
					std::set<Handle> handleSet;
					for (auto it = value.Begin(); it != value.End(); ++it)
						handleSet.insert(reinterpret_cast<Handle>(it->GetInt64()));
					postResponse(id, tb.getArrangement(handleSet));
				}
			}
			else if (type == "setArrangement") {
				tb.setArrangement(Arrangement(value));
				postResponse(id);
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












//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
//LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//		switch (message)
//		{
//		case WM_COMMAND:
//				{
//						int wmId = LOWORD(wParam);
//						// Parse the menu selections:
//						switch (wmId)
//						{
//	/*					case IDM_ABOUT:
//								DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
//								break;
//						case IDM_EXIT:
//								DestroyWindow(hWnd);*/
//								break;
//						default:
//								return DefWindowProc(hWnd, message, wParam, lParam);
//						}
//				}
//				break;
//		case WM_PAINT:
//				{
//						PAINTSTRUCT ps;
//						HDC hdc = BeginPaint(hWnd, &ps);
//						// TODO: Add any drawing code that uses hdc here...
//						EndPaint(hWnd, &ps);
//				}
//				break;
//		case WM_DESTROY:
//				PostQuitMessage(0);
//				break;
//		default:
//				return DefWindowProc(hWnd, message, wParam, lParam);
//		}
//		return 0;
//}