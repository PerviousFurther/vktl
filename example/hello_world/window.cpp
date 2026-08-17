
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>

bool is_running(void* window) {
	MSG msg = {};

	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return IsWindow(static_cast<HWND>(window));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void* create_win() {
	const wchar_t CLASS_NAME[] = L"DwmNoWhiteWindowClass";

	auto hInstance = GetModuleHandle(nullptr);
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	wc.hbrBackground = (HBRUSH)0;

	if (!RegisterClassEx(&wc)) return NULL;

	HWND hwnd = CreateWindowEx(
		0, CLASS_NAME, L"I dont like vulkan.",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, hInstance, NULL
	);

	if (hwnd == NULL) return NULL;

	// BOOL useDarkMode = TRUE;
	// DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
	// 
	// MARGINS margins = { -1, -1, -1, -1 };
	// DwmExtendFrameIntoClientArea(hwnd, &margins);

	ShowWindow(hwnd, SW_NORMAL);
	UpdateWindow(hwnd);

	return hwnd;
}
