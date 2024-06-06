#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wbemidl.h>
#include <comdef.h>
#include <stdio.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")

#define ID_TIMER 1
#define UPDATE_INTERVAL 1000
#define MAX_BAR_LENGTH 50

HWND hwndCpuLoad;
HQUERY hQuery;
HCOUNTER *hCounters;
int numCores;
char cpuName[128] = "CPU Load Monitor";

void InitPDH() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    numCores = sysInfo.dwNumberOfProcessors;

    hCounters = (HCOUNTER *)malloc(numCores * sizeof(HCOUNTER));
    PdhOpenQuery(NULL, 0, &hQuery);

    for (int i = 0; i < numCores; i++) {
        char counterPath[50];
        sprintf(counterPath, "\\Processor(%d)\\%% Processor Time", i);
        PdhAddEnglishCounter(hQuery, counterPath, 0, &hCounters[i]);
    }
    PdhCollectQueryData(hQuery);
}

void GetCPULoad(double *loads) {
    PDH_FMT_COUNTERVALUE counterVal;

    PdhCollectQueryData(hQuery);

    for (int i = 0; i < numCores; i++) {
        PdhGetFormattedCounterValue(hCounters[i], PDH_FMT_DOUBLE, NULL, &counterVal);
        loads[i] = counterVal.doubleValue;
    }
}

void GetCPUName() {
    HRESULT hres;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        printf("Failed to initialize COM library. Error code = 0x%x\n", hres);
        return;
    }

    hres = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL);

    if (FAILED(hres)) {
        printf("Failed to initialize security. Error code = 0x%x\n", hres);
        CoUninitialize();
        return;
    }

    IWbemLocator *pLoc = NULL;

    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID *)&pLoc);

    if (FAILED(hres)) {
        printf("Failed to create IWbemLocator object. Error code = 0x%x\n", hres);
        CoUninitialize();
        return;
    }

    IWbemServices *pSvc = NULL;

    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hres)) {
        printf("Could not connect. Error code = 0x%x\n", hres);
        pLoc->Release();
        CoUninitialize();
        return;
    }

    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hres)) {
        printf("Could not set proxy blanket. Error code = 0x%x\n", hres);
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    IEnumWbemClassObject *pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT Name FROM Win32_Processor"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        printf("Query for processor name failed. Error code = 0x%x\n", hres);
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            wcstombs(cpuName, vtProp.bstrVal, sizeof(cpuName));
        }
        VariantClear(&vtProp);

        pclsObj->Release();
    }

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

void CALLBACK TimerProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime) {
    InvalidateRect(hwnd, NULL, TRUE);
}

void DrawCPULoadBars(HDC hdc) {
    double loads[numCores];
    GetCPULoad(loads);

    for (int i = 0; i < numCores; i++) {
        int barLength = (int)(loads[i] * MAX_BAR_LENGTH / 100);
        char bar[MAX_BAR_LENGTH + 1];

        for (int j = 0; j < MAX_BAR_LENGTH; j++) {
            if (j < barLength) {
                bar[j] = '#';
            } else {
                bar[j] = ' ';
            }
        }
        bar[MAX_BAR_LENGTH] = '\0';

        TextOut(hdc, 10, 20 + i * 20, bar, MAX_BAR_LENGTH);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawCPULoadBars(hdc);
            EndPaint(hwnd, &ps);
        } break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    GetCPUName();
    InitPDH();

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWnd =  WndProc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CPUMonitorClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwndCpuLoad = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            "CPUMonitorClass",
            cpuName,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
            NULL, NULL, hInstance, NULL);

    if (hwndCpuLoad == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwndCpuLoad, nCmdShow);
    UpdateWindow(hwndCpuLoad);

    SetTimer(hwndCpuLoad, ID_TIMER, UPDATE_INTERVAL, (TIMERPROC)TimerProc);

    MSG Msg;
    while (GetMessage(&Msg, NULL, 0, 0)) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    for (int i = 0; i < numCores; i++) {
        PdhRemoveCounter(hCounters[i]);
    }
    PdhCloseQuery(hQuery);
    free(hCounters);

    return Msg.wParam;
}
