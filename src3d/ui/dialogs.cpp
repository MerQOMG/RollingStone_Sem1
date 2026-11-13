// Win32 dialogs. Kept apart from raylib headers because the two define clashing names.
#include <Windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <system_error>
#include "ui/dialogs.h"
namespace rs3d::gui {
void initDialogs(){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);}
std::filesystem::path pickSceneFile(bool save,const std::filesystem::path&current,void*owner){
    auto path=std::filesystem::absolute(current).lexically_normal();path.make_preferred();
    if(!std::filesystem::is_directory(path.parent_path()))path=std::filesystem::current_path()/path.filename();
    std::array<wchar_t,32768> buffer{};auto initial=path.wstring();if(initial.size()>=buffer.size())return {};std::copy(initial.begin(),initial.end(),buffer.begin());
    OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=static_cast<HWND>(owner);d.lpstrFilter=L"场景文件 (*.ini)\0*.ini\0所有文件\0*.*\0\0";d.lpstrFile=buffer.data();d.nMaxFile=DWORD(buffer.size());
    d.lpstrDefExt=L"ini";d.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    BOOL ok=save?GetSaveFileNameW(&d):GetOpenFileNameW(&d);if(!ok)return {};return buffer.data();
}
std::filesystem::path pickFolder(const std::filesystem::path&start,void*owner,const wchar_t*title){
    std::filesystem::path result;IFileOpenDialog*dialog=nullptr;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&dialog))))return result;
    DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
    if(title)dialog->SetTitle(title);
    std::error_code ec;auto abs=std::filesystem::absolute(start,ec);
    if(!ec&&std::filesystem::is_directory(abs)){IShellItem*item=nullptr;if(SUCCEEDED(SHCreateItemFromParsingName(abs.wstring().c_str(),nullptr,IID_PPV_ARGS(&item)))){dialog->SetFolder(item);item->Release();}}
    if(SUCCEEDED(dialog->Show(static_cast<HWND>(owner)))){IShellItem*item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR name=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&name))){result=name;CoTaskMemFree(name);}item->Release();}}
    dialog->Release();return result;
}
void openInExplorer(const std::filesystem::path&p){ShellExecuteW(nullptr,L"open",std::filesystem::absolute(p).wstring().c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
std::vector<std::filesystem::path> recentResults(const std::filesystem::path&root,std::size_t limit){
    namespace fs=std::filesystem;std::vector<std::pair<fs::file_time_type,fs::path>> found;std::error_code ec;
    if(!fs::is_directory(root,ec))return {};
    for(fs::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec)){
        if(!it->is_directory(ec))continue;auto manifest=it->path()/"manifest.ini";if(!fs::exists(manifest,ec))continue;found.push_back({fs::last_write_time(manifest,ec),it->path()});
    }
    std::sort(found.begin(),found.end(),[](auto&a,auto&b){return a.first>b.first;});
    std::vector<fs::path> out;for(auto&f:found){if(out.size()>=limit)break;out.push_back(f.second);}return out;
}
void*nativeWindowHandle(){return GetActiveWindow();}
}
