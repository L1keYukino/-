#include "src/ui/settings_dialog.hpp"
#include "src/config/config.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace vim {

struct SD {
    EngineConfig* cfg;
    HWND hApiKey, hShowViz, hMic;
    HWND hPttDisp, hPttBtn, hModeDisp, hModeBtn;
    uint32_t ptt_m=3, ptt_v=0x56, mode_m=3, mode_v='B';
    int rec=0; bool saved=false, viz=true;
};

static std::wstring hk_name(uint32_t m, uint32_t v) {
    std::wstring s;
    if(m&2)s+=L"Ctrl+"; if(m&1)s+=L"Alt+"; if(m&4)s+=L"Shift+"; if(m&8)s+=L"Win+";
    if(v==VK_XBUTTON1)s+=L"鼠标侧键1"; else if(v==VK_XBUTTON2)s+=L"鼠标侧键2";
    else if(v==VK_MBUTTON)s+=L"鼠标中键"; else if(v==VK_RBUTTON)s+=L"鼠标右键";
    else{wchar_t n[64]={0};UINT sc=MapVirtualKeyW(v,MAPVK_VK_TO_VSC);GetKeyNameTextW(sc<<16,n,64);if(n[0])s+=n;else s+=L"VK_"+std::to_wstring(v);}
    return s;
}

static void capture_key(SD* d, HWND hwnd, uint32_t vk, bool is_kb) {
    if(is_kb && (vk==VK_CONTROL||vk==VK_MENU||vk==VK_SHIFT||vk==VK_LWIN||vk==VK_RWIN||vk==VK_CAPITAL)) return;
    uint32_t mods=0;
    if(GetAsyncKeyState(VK_CONTROL)&0x8000)mods|=2; if(GetAsyncKeyState(VK_MENU)&0x8000)mods|=1;
    if(GetAsyncKeyState(VK_SHIFT)&0x8000)mods|=4; if(GetAsyncKeyState(VK_LWIN)&0x8000||GetAsyncKeyState(VK_RWIN)&0x8000)mods|=8;
    if(d->rec==1){d->ptt_m=mods;d->ptt_v=vk;SetWindowTextW(d->hPttDisp,hk_name(mods,vk).c_str());SetWindowTextW(d->hPttBtn,L"录制");EnableWindow(d->hPttBtn,TRUE);}
    else{d->mode_m=mods;d->mode_v=vk;SetWindowTextW(d->hModeDisp,hk_name(mods,vk).c_str());SetWindowTextW(d->hModeBtn,L"录制");EnableWindow(d->hModeBtn,TRUE);}
    d->rec=0;KillTimer(hwnd,1);
}

static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l){
    auto* d=reinterpret_cast<SD*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    switch(m){
    case WM_CREATE:{
        auto* cs=reinterpret_cast<CREATESTRUCT*>(l);d=reinterpret_cast<SD*>(cs->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(d));
        HINSTANCE hi=cs->hInstance;int y=12,x=12;
        CreateWindowW(L"STATIC",L"DeepSeek API Key:",WS_CHILD|WS_VISIBLE,x,y,120,20,h,nullptr,hi,nullptr);
        d->hApiKey=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT,x,y+18,340,22,h,nullptr,hi,nullptr);
        {std::wstring w(d->cfg->llm_fallback.api_key.begin(),d->cfg->llm_fallback.api_key.end());SetWindowTextW(d->hApiKey,w.c_str());}
        y+=50;
        // Mic device
        CreateWindowW(L"STATIC",L"麦克风设备(重启生效):",WS_CHILD|WS_VISIBLE,x,y,160,20,h,nullptr,hi,nullptr);
        d->hMic=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,x,y+18,340,200,h,nullptr,hi,nullptr);
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"默认设备");
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"HECATE G6 PRO (44.1k, device 1)");
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"HECATE G6 PRO (48k mono, device 58)");
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"Realtek Audio (device 3)");
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"Realtek 麦克风阵列 (device 31)");
        SendMessageW(d->hMic,CB_ADDSTRING,0,(LPARAM)L"立体声混音 (device 36)");
        SendMessageW(d->hMic,CB_SETCURSEL,0,0);
        y+=50;
        // PTT hotkey
        CreateWindowW(L"STATIC",L"录音快捷键:",WS_CHILD|WS_VISIBLE,x,y,80,20,h,nullptr,hi,nullptr);
        d->hPttDisp=CreateWindowW(L"STATIC",hk_name(d->ptt_m,d->ptt_v).c_str(),WS_CHILD|WS_VISIBLE|SS_CENTER,100,y,150,22,h,nullptr,hi,nullptr);
        d->hPttBtn=CreateWindowW(L"BUTTON",L"录制",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,260,y,60,22,h,(HMENU)3,hi,nullptr);
        y+=26;
        // Mode hotkey
        CreateWindowW(L"STATIC",L"切换模式:",WS_CHILD|WS_VISIBLE,x,y,80,20,h,nullptr,hi,nullptr);
        d->hModeDisp=CreateWindowW(L"STATIC",hk_name(d->mode_m,d->mode_v).c_str(),WS_CHILD|WS_VISIBLE|SS_CENTER,100,y,150,22,h,nullptr,hi,nullptr);
        d->hModeBtn=CreateWindowW(L"BUTTON",L"录制",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,260,y,60,22,h,(HMENU)5,hi,nullptr);
        y+=32;
        d->hShowViz=CreateWindowW(L"BUTTON",L"显示音律条",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,x,y,150,22,h,(HMENU)4,hi,nullptr);
        SendMessageW(d->hShowViz,BM_SETCHECK,d->viz?BST_CHECKED:BST_UNCHECKED,0);
        y+=30;
        CreateWindowW(L"STATIC",L"提示：修改后需重启生效",WS_CHILD|WS_VISIBLE,x,y,300,18,h,nullptr,hi,nullptr);
        y+=28;
        CreateWindowW(L"BUTTON",L"保存",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,190,y,75,26,h,(HMENU)1,hi,nullptr);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,275,y,75,26,h,(HMENU)2,hi,nullptr);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==1){
            wchar_t b[256];GetWindowTextW(d->hApiKey,b,256);std::wstring w(b);d->cfg->llm_fallback.api_key=std::string(w.begin(),w.end());d->cfg->llm_fallback.enabled=!d->cfg->llm_fallback.api_key.empty();
            d->cfg->mode.ptt_hotkey.modifiers=d->ptt_m;d->cfg->mode.ptt_hotkey.virtual_key=d->ptt_v;
            d->cfg->mode.mode_hotkey.modifiers=d->mode_m;d->cfg->mode.mode_hotkey.virtual_key=d->mode_v;
            int mic=static_cast<int>(SendMessageW(d->hMic,CB_GETCURSEL,0,0));
            const char* devs[]={"","1","58","3","31","36"};
            if(mic>=0&&mic<6)d->cfg->audio.device_id=devs[mic];
            bool vz=SendMessageW(d->hShowViz,BM_GETCHECK,0,0)==BST_CHECKED;{std::ofstream f("config/show_viz.txt");if(f)f<<(vz?"1":"0");}
            d->saved=true;DestroyWindow(h);return 0;}
        if(LOWORD(w)==2){DestroyWindow(h);return 0;}
        if(LOWORD(w)==3){d->rec=1;SetWindowTextW(d->hPttBtn,L"...");EnableWindow(d->hPttBtn,FALSE);SetFocus(h);SetTimer(h,1,5000,nullptr);return 0;}
        if(LOWORD(w)==5){d->rec=2;SetWindowTextW(d->hModeBtn,L"...");EnableWindow(d->hModeBtn,FALSE);SetFocus(h);SetTimer(h,1,5000,nullptr);return 0;}
        break;
    case WM_TIMER:if(w==1&&d&&d->rec){d->rec=0;KillTimer(h,1);if(d->rec==1){SetWindowTextW(d->hPttBtn,L"录制");EnableWindow(d->hPttBtn,TRUE);}else{SetWindowTextW(d->hModeBtn,L"录制");EnableWindow(d->hModeBtn,TRUE);}}return 0;
    case WM_KEYDOWN:case WM_SYSKEYDOWN:if(d&&d->rec)capture_key(d,h,(uint32_t)w,true);return 0;
    case WM_XBUTTONDOWN:case WM_MBUTTONDOWN:case WM_RBUTTONDOWN:{uint32_t vk=(uint32_t)w;if(m==WM_XBUTTONDOWN)vk=(GET_XBUTTON_WPARAM(w)==1)?VK_XBUTTON1:VK_XBUTTON2;else if(m==WM_MBUTTONDOWN)vk=VK_MBUTTON;else vk=VK_RBUTTON;if(d&&d->rec)capture_key(d,h,vk,false);return 0;}
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

bool show_settings_dialog(HINSTANCE hi, HWND parent, HWND, EngineConfig& cfg){
    SD d;d.cfg=&cfg;d.ptt_m=cfg.mode.ptt_hotkey.modifiers;d.ptt_v=cfg.mode.ptt_hotkey.virtual_key;
    d.mode_m=cfg.mode.mode_hotkey.modifiers;d.mode_v=cfg.mode.mode_hotkey.virtual_key;
    {std::ifstream f("config/show_viz.txt");if(f){char c;f>>c;d.viz=(c=='1');}}
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=wp;wc.hInstance=hi;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"VIM_Settings";RegisterClassExW(&wc);
    HWND hw=CreateWindowExW(WS_EX_DLGMODALFRAME,L"VIM_Settings",L"语音输入法 - 设置",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,390,350,parent,nullptr,hi,&d);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(d.saved){try{save_config_to_file("config/default_config.json",cfg);spdlog::info("Settings saved");return true;}catch(...){spdlog::error("Failed to save config");}}
    return false;
}
} // namespace vim
