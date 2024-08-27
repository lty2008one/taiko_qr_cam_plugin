#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include <unistd.h>

#include <windows.h>

using namespace std;
using namespace cv;
using namespace chrono;

unsigned long long last_cam_time = 0;
unsigned long long last_send_time = 0;
unsigned long long last_using_time = 0;
bool cam_opened = false;
bool qr_detected = false;
std::vector<uint8_t> qr_buffer = {};
std::string log_prefix = "[ T613 QR ] ";

bool alive = true;

// ------ DLL Interface Define
typedef int32 (*pts_scan_init)(void);                                                             // 初始化扫码器DLL资源
typedef int32 (*pts_scan_deinit)(void);                                                           // 释放扫码器资源
typedef int32 (*ts_scan_callback)(void *pParam, uint8 *pBuf, uint32 uiBufLen);                    // 扫码信息的回调函数
typedef void (*ts_state_callback)(void *pParam, uint8 ucState);                                   // 扫码器的连接回调
typedef int32 (*pts_scan_callback_register)(void *pParam, ts_scan_callback fScanCallback);        // 注册 扫码信息的回调函数
typedef int32 (*pts_state_callback_register)(void *pParam, ts_state_callback fStateFun);          // 注册 扫码器的连接回调
typedef int32 (*pts_scan_set_light_mode)(uint8 ucMode, uint8 isTemp);                             // 灯光状态控制 1,0:常亮  2,0:关闭
typedef uint32 (*pts_scan_sw)(uint8 ucEn);                                                        // 扫码状态控制 0:关闭扫码  1:开启扫码
// ------ DLL Method Define
pts_scan_init ts_scan_init;
pts_scan_deinit ts_scan_deinit;
pts_scan_callback_register ts_scan_callback_register;
pts_state_callback_register ts_state_callback_register;
pts_scan_set_light_mode ts_scan_set_light_mode;
pts_scan_sw ts_scan_sw;
// ------ DLL Method Define
void scan_change(uint8 state) {
  std::string desc = state ? "Enable" : "Disable";
  if(SUCC == ts_scan_sw(state)) {
    std::cout << log_prefix << "Scan " << desc << "d!" << std::endl;
  } else {
    std::cout << log_prefix << desc << " Scan Failed!" << std::endl;
  }
}
void light_change(uint8 state) {
  std::string desc = state ? "Open" : "Close";
  if(SUCC == pts_scan_set_light_mode(state + 1, 0)) {
    std::cout << log_prefix << "Light " << desc << "!" << std::endl;
  } else {
    std::cout << log_prefix << desc << " Light Failed!" << std::endl;
  }
}
void bind_change(uint8 state) {
  if (state) {
    light_change(1);
    scan_change(1);
  } else {
    scan_change(0);
    light_change(0);
  }
}
void* initMethod(HINSTANCE lib, const char* method_name) {
  void* pMethod = (void*)GetProcAddress(lib, method_name);
  if (pMethod == NULL) std::cout << log_prefix << "Init " << method_name << " Method Error: " << GetLastError() << std::endl;
  return pMethod;
}
void initRegister(HINSTANCE lib) {
  ts_scan_init = (pts_scan_init)initMethod(lib, "ts_scan_init");
	ts_scan_deinit = (pts_scan_deinit)initMethod(lib, "ts_scan_deinit");
  ts_scan_callback_register = (pts_scan_callback_register)initMethod(lib, "ts_scan_get_data_fun_register");
  ts_state_callback_register = (pts_state_callback_register)initMethod(lib, "ts_scan_state_fun_register");
  ts_scan_set_light_mode = (pts_scan_set_light_mode)initMethod(lib, "ts_scan_set_light_mode");
  ts_scan_sw = (pts_scan_sw)initMethod(lib, "ts_scan_sw");
}
int32 scan_callback(void *pParam, uint8 *pBuf, uint32 uiBufLen) {
  if((NULL == pBuf) || (0 == uiBufLen)) return FAIL;
  std::cout << log_prefix << "code detected, len = " << information.length() << std::endl;
  qr_buffer.clear();
  for (int i = 0; i < uiBufLen; ) {
      qr_buffer.push_back(static_cast<uint8_t>(data));
  }
  qr_detected = true;
  bind_change(0);
  return SUCC;
}
void state_callback(void *pParam, uint8 ucState) {
  std::cout << log_prefix << "SCANNER " << ((ucState) ? "Connected!" : "Disconnected!") << std::endl;
}



extern "C"
{
    void CamUpdate();

    __declspec(dllexport) void Init(void)
    {
        HINSTANCE t613Lib = LoadLibrary("C++_hidpos_dll.dll");
        initRegister(t613Lib);

        ts_scan_callback_register(NULL, scan_callback);
        ts_state_callback_register(NULL, state_callback);
        ts_scan_init();
        bind_change(1);

        std::cout << log_prefix << "Init Finished!" << std::endl;
    }

    __declspec(dllexport) void UsingQr(void)
    {
        last_using_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    __declspec(dllexport) bool CheckQr(void)
    {
        if (qr_detected)
        {
            std::cout << log_prefix << "Found Code! " << std::endl;
            qr_detected = false;
            return true;
        }

        return false;
    }

    __declspec(dllexport) int GetQr(int len_limit, uint8_t *buffer)
    {
        if (qr_buffer.size() == 0 || qr_buffer.size() > len_limit)
        {
            std::cout << log_prefix << "Discard Code, max acceptable len: " << len_limit << ", current len: " << qr_buffer.size() << std::endl;
            return 0;
        }
        std::cout << log_prefix << "GetCode length=" << qr_buffer.size() << std::endl;
        last_send_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        memcpy(buffer, qr_buffer.data(), qr_buffer.size());
        bind_change(1);
        return qr_buffer.size();
    }

    __declspec(dllexport) void Exit(void)
    {
        std::cout << log_prefix << "Exit" << std::endl;
        bind_change(0);
        ts_scan_deinit();
    }

}
