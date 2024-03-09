// Copyright 2024 inonote
// Use of this source code is governed by the MIT License
// that can be found in the LICENSE file

#include "App.h"

static const wchar_t* kMainWindowClass = L"UmiguriSampleLedServer";
static const wchar_t* kMainWindowTitle = L"UmiguriSampleLedServer";

static const int kMainWindowWidth = 1300;
static const int kMainWindowHeight = 300;

static const int kMsgIdWebSocketNotify = WM_USER + 1;

static const int kLedServerPort = 8090;
static const char* kLedServerName = "SampleLedServer";
static const int kLedServerVersion[2] = { 123, 456 };
static const char* kLedServerHardwareName = "Dummy";
static const int kLedServerHardwareVersion[2] = {987, 654};

static const uint8_t kLedProtocolVersion = 0x01;
enum ULED_COMMAND : uint8_t {
    ULED_COMMAND_SET_LED = 0x10,
    ULED_COMMAND_INITIALIZE = 0x11,
    ULED_COMMAND_READY = 0x11 | 0x08,
    ULED_COMMAND_PING = 0x12,
    ULED_COMMAND_PONG = 0x12 | 0x08,
    // ULED_COMMAND_TOUCH_REPORT = 0x13 | 0x08,
    // ULED_COMMAND_ENABLE_TOUCH_REPORT = 0x20 | 0x08,
    // ULED_COMMAND_DISABLE_TOUCH_REPORT = 0x21 | 0x08,
    ULED_COMMAND_REQUEST_SERVER_INFO = 0xD0,
    ULED_COMMAND_REPORT_SERVER_INFO = 0xD0 | 0x08,
};

ATOM App::m_atomWndClass = 0;
std::map<HWND, App*> App::m_HwndMap;

int App::Start(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    if (!CreateMainWindow())
        return 0;

    if (!CreateServer())
        return 0;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    m_wsContext->wss.stop();
    return (int)msg.wParam;
}

bool App::CreateMainWindow() {
    if (!m_atomWndClass) {
        WNDCLASSEXW wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = m_hInstance;
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = kMainWindowClass;

        m_atomWndClass = RegisterClassExW(&wcex);

        if (!m_atomWndClass)
            return false;
    }

    m_hMainWnd = CreateWindowExW(0, kMainWindowClass, kMainWindowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, 0, kMainWindowWidth, kMainWindowHeight, nullptr, nullptr, m_hInstance, nullptr);
    m_HwndMap[m_hMainWnd] = this;

    ShowWindow(m_hMainWnd, SW_SHOW);
    return m_hMainWnd;
}

bool App::CreateServer() {
    m_wsContext = std::make_shared<WebSocketContext>();
    m_wsContext->hNotifyWnd = m_hMainWnd;
    m_wsContext->numPort = kLedServerPort;
    m_wsThread = std::make_unique<std::thread>(&ServerThread, m_wsContext);
    return true;
}

void App::PaintSlider(HDC hDC) {
    RECT clientRect;
    GetClientRect(m_hMainWnd, &clientRect);

    for (int i = 0; i < 16; ++i) {
        int left = clientRect.right * i / 16;
        int right = clientRect.right * (i + 1) / 16;
        
        // pad
        RECT keyRect = { left, 0, right, clientRect.bottom };
        SetBkColor(hDC, RGB(m_ledState.mainDeviceColors[i].r, m_ledState.mainDeviceColors[i].g, m_ledState.mainDeviceColors[i].b));
        ExtTextOutW(hDC, keyRect.left, keyRect.top, ETO_OPAQUE, &keyRect, nullptr, 0, nullptr);

        // separator
        if (i < 15) {
            keyRect.left = right - 5;
            keyRect.right = right + 5;
            SetBkColor(hDC, RGB(m_ledState.mainDeviceColors[i + 16].r, m_ledState.mainDeviceColors[i + 16].g, m_ledState.mainDeviceColors[i + 16].b));
            ExtTextOutW(hDC, keyRect.left, keyRect.top, ETO_OPAQUE, &keyRect, nullptr, 0, nullptr);

            keyRect.left = right - 1;
            keyRect.right = right + 1;
            SetBkColor(hDC, RGB(m_ledState.mainDeviceColors[i + 16].r * 5 / 8, m_ledState.mainDeviceColors[i + 16].g * 5 / 8, m_ledState.mainDeviceColors[i + 16].b * 5 / 8));
            ExtTextOutW(hDC, keyRect.left, keyRect.top, ETO_OPAQUE, &keyRect, nullptr, 0, nullptr);
        }
    }
}

void App::PopServerMessages() {
    std::scoped_lock<std::mutex> mlock(m_wsContext->mutex);
    while (!m_wsContext->receivedMessages.empty()) {
        WebSocketMessage& message = m_wsContext->receivedMessages.front();
        if (ValidateMessage(message.payload)) {

            switch ((uint8_t)message.payload[1]) {
            case ULED_COMMAND_PING:
            {
                std::vector<uint8_t> buf;
                buf.push_back(kLedProtocolVersion);
                buf.push_back(ULED_COMMAND_PONG);
                buf.push_back(6);
                buf.push_back(message.payload[3]);
                buf.push_back(message.payload[4]);
                buf.push_back(message.payload[5]);
                buf.push_back(message.payload[6]);
                buf.push_back(0x51);
                buf.push_back(0xED);
                m_wsContext->wss.send(message.hdl, buf.data(), buf.size(), websocketpp::frame::opcode::binary);

                OutputDebugStringW(L"App::PopServerMessages ULED_COMMAND_PING\r\n");
                break;
            }

            case ULED_COMMAND_INITIALIZE:
            {
                std::vector<uint8_t> buf;
                buf.push_back(kLedProtocolVersion);
                buf.push_back(ULED_COMMAND_READY);
                buf.push_back(0);

                m_wsContext->wss.send(message.hdl, buf.data(), buf.size(), websocketpp::frame::opcode::binary);
                OutputDebugStringW(L"App::PopServerMessages ULED_COMMAND_INITIALIZE\r\n");
                break;
            }

            case ULED_COMMAND_REQUEST_SERVER_INFO:
            {
                std::vector<uint8_t> buf;
                buf.resize(44 + 3);
                buf[0] = kLedProtocolVersion;
                buf[1] = ULED_COMMAND_REPORT_SERVER_INFO;
                buf[2] = 44;
                strncpy((char*)buf.data() + 3, kLedServerName, 16);
                buf[3 + 16] = kLedServerVersion[0] & 0xFF;
                buf[3 + 17] = (kLedServerVersion[0] >> 8) & 0xFF;
                buf[3 + 18] = kLedServerVersion[1] & 0xFF;
                buf[3 + 19] = (kLedServerVersion[1] >> 8) & 0xFF;
                strncpy((char*)buf.data() + 3 + 22, kLedServerHardwareName, 16);
                buf[3 + 38] = kLedServerHardwareVersion[0] & 0xFF;
                buf[3 + 39] = (kLedServerHardwareVersion[0] >> 8) & 0xFF;
                buf[3 + 40] = kLedServerHardwareVersion[1] & 0xFF;
                buf[3 + 41] = (kLedServerHardwareVersion[1] >> 8) & 0xFF;

                m_wsContext->wss.send(message.hdl, buf.data(), buf.size(), websocketpp::frame::opcode::binary);
                OutputDebugStringW(L"App::PopServerMessages ULED_COMMAND_REQUEST_SERVER_INFO\r\n");
                break;
            }

            case ULED_COMMAND_SET_LED:
            {
                int payloadIndex = 2;
                m_ledState.brightness = message.payload[++payloadIndex];
                for (int i = 0; i < 16 + 15; ++i) {
                    m_ledState.mainDeviceColors[i].r = message.payload[++payloadIndex];
                    m_ledState.mainDeviceColors[i].g = message.payload[++payloadIndex];
                    m_ledState.mainDeviceColors[i].b = message.payload[++payloadIndex];
                }
                for (int i = 0; i < 3; ++i) {
                    m_ledState.sideDeviceColors[i].r = message.payload[++payloadIndex];
                    m_ledState.sideDeviceColors[i].g = message.payload[++payloadIndex];
                    m_ledState.sideDeviceColors[i].b = message.payload[++payloadIndex];
                }

                InvalidateRect(m_hMainWnd, nullptr, false); // update slider view
                break;
            }
            }
        }
        else {
            OutputDebugStringW(L"App::PopServerMessages invalid payload\r\n");
        }
            
        m_wsContext->receivedMessages.pop();
    }
}

bool App::ValidateMessage(const WebSocketPayload& payload) {
    if (payload.size() < 3)
        return false;

    if (payload[0] != kLedProtocolVersion)
        return false;

    if (payload[2] != payload.size() - 3)
        return false;

    if (payload[1] == ULED_COMMAND_SET_LED && payload.size() != 103 + 3)
        return false;

    if (payload[1] == ULED_COMMAND_PING && payload.size() != 4 + 3)
        return false;

    return true;
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* thisApp = nullptr;
    {
        auto itr = m_HwndMap.find(hWnd);
        if (itr == m_HwndMap.end())
            return DefWindowProcW(hWnd, message, wParam, lParam);
        thisApp = itr->second;
    }

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        thisApp->PaintSlider(hDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case kMsgIdWebSocketNotify:
        thisApp->PopServerMessages();
        return 0;

    case WM_DESTROY:
        m_HwndMap.erase(hWnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void App::ServerThread(std::shared_ptr<WebSocketContext> ctx) {
    using namespace websocketpp::lib;

    ctx->wss.set_access_channels(websocketpp::log::alevel::all);
    ctx->wss.clear_access_channels(websocketpp::log::alevel::frame_payload);
    ctx->wss.init_asio();
    ctx->wss.set_message_handler(bind(&ServerOnMessage, ctx, placeholders::_1, placeholders::_2));
    ctx->wss.listen((uint16_t)ctx->numPort);
    ctx->wss.start_accept();
    ctx->wss.run();
}

void App::ServerOnMessage(std::shared_ptr<WebSocketContext> ctx, websocketpp::connection_hdl hdl, WebSocketServer::message_ptr msg) {
    {
        std::scoped_lock<std::mutex> mlock(ctx->mutex);
        ctx->receivedMessages.push({ hdl, msg->get_payload() });
    }

    // notify main thread
    PostMessageW(ctx->hNotifyWnd, kMsgIdWebSocketNotify, 0, 0);
}
