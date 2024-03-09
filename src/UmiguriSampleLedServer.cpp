// Copyright (c) 2024 inonote
// Use of this source code is governed by the MIT License
// that can be found in the LICENSE file

#include "UmiguriSampleLedServer.h"

App* g_theApp;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    g_theApp = new App;
    return g_theApp->Start(hInstance);
}
