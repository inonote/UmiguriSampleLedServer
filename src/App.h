// Copyright (c) 2024 inonote
// Use of this source code is governed by the MIT License
// that can be found in the LICENSE file

#pragma once
#define WIN32_LEAN_AND_MEAN
#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_STL_
#define _WEBSOCKETPP_CONSTEXPR_TOKEN_
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define ASIO_ERROR_CATEGORY_NOEXCEPT noexcept(true)

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <windows.h>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <map>


class App {
public:
	int Start(HINSTANCE hInstance);

protected:
	using WebSocketServer = websocketpp::server<websocketpp::config::asio>;
	using WebSocketPayload = std::string;

	struct WebSocketMessage {
		websocketpp::connection_hdl hdl;
		WebSocketPayload payload;
	};

	struct WebSocketContext {
		int numPort = 0;
		std::mutex mutex;
		WebSocketServer wss;
		std::queue<WebSocketMessage> receivedMessages;
		HWND hNotifyWnd = nullptr;
	};

	struct LedRgb {
		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;
	};

	struct LedState {
		uint8_t brightness = 0xFF;
		LedRgb mainDeviceColors[16 + 15];
		LedRgb sideDeviceColors[3];
	};

	HINSTANCE m_hInstance = nullptr;
	HWND m_hMainWnd = nullptr;
	static ATOM m_atomWndClass;

	LedState m_ledState;

	std::unique_ptr<std::thread> m_wsThread;
	std::shared_ptr<WebSocketContext> m_wsContext;

	bool CreateMainWindow();
	bool CreateServer();

	void PaintSlider(HDC hDC);

	void PopServerMessages();
	bool ValidateMessage(const WebSocketPayload& payload);

	static std::map<HWND, App*> m_HwndMap;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	static void ServerThread(std::shared_ptr<WebSocketContext> ctx);
	static void ServerOnMessage(std::shared_ptr<WebSocketContext> ctx, websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg);
};

