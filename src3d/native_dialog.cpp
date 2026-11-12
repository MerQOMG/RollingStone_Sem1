#include "native_dialog.h"
#include <Windows.h>
#include <commdlg.h>
#include <array>
#include <stdexcept>
#include <string>
std::filesystem::path nativeSceneDialog(bool save,const std::filesystem::path&current,void*owner){
    // OPENFILENAME requires native separators and a valid absolute directory.
    // A relative `configs3d/scene.ini` is rejected with FNERR_INVALIDFILENAME.
    auto path=std::filesystem::absolute(current).lexically_normal();path.make_preferred();
    if(!std::filesystem::is_directory(path.parent_path()))path=std::filesystem::current_path()/path.filename();
    std::array<wchar_t,32768>buffer{};auto initial=path.wstring();if(initial.size()>=buffer.size())throw std::runtime_error("scene path is too long");std::copy(initial.begin(),initial.end(),buffer.begin());OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=static_cast<HWND>(owner);dialog.lpstrFilter=L"RollingStone scene (*.ini)\0*.ini\0All files\0*.*\0\0";dialog.lpstrFile=buffer.data();dialog.nMaxFile=DWORD(buffer.size());dialog.lpstrDefExt=L"ini";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    BOOL ok=save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);if(!ok){auto error=CommDlgExtendedError();if(error)throw std::runtime_error("file dialog failed code="+std::to_string(error));return {};}return buffer.data();
}
