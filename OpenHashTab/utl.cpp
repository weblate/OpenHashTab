//    Copyright 2019-2020 namazso <admin@namazso.eu>
//    This file is part of OpenHashTab.
//
//    OpenHashTab is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    OpenHashTab is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with OpenHashTab.  If not, see <https://www.gnu.org/licenses/>.
#include "stdafx.h"

#include "utl.h"

#include <memory>

int utl::FormattedMessageBox(HWND hwnd, LPCWSTR caption, UINT type, LPCWSTR fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  const auto str = FormatStringV(fmt, args);
  va_end(args);
  return MessageBoxW(hwnd, str.c_str(), caption, type);
}

std::wstring utl::GetString(UINT id)
{
  LPCWSTR v = nullptr;
  const auto len = LoadStringW(GetInstance(), id, reinterpret_cast<LPWSTR>(&v), 0);
  return {v, v + len};
}

std::wstring utl::GetWindowTextString(HWND hwnd)
{
  SetLastError(0);
  // GetWindowTextLength may return more than actual length, so we can't use a std::wstring directly
  const auto len = GetWindowTextLengthW(hwnd);
  // if text is 0 long, GetWindowTextLength returns 0, same as when error happened
  if (len == 0 && GetLastError() != 0)
    return {};
  const auto p = std::make_unique<wchar_t[]>(len + 1);
  GetWindowTextW(hwnd, p.get(), len + 1);
  return {p.get()};
}

void utl::SetWindowTextStringFromTable(HWND hwnd, UINT id)
{
  SetWindowTextW(hwnd, utl::GetString(id).c_str());
}

long utl::FloorIconSize(long size)
{
  constexpr static long icon_sizes[] = { 256, 192, 128, 96, 64, 48, 40, 32, 24, 16 };
  for (auto v : icon_sizes)
    if (size >= v)
    {
      size = v;
      break;
    }
  return size;
}

HICON utl::SetIconButton(HWND button, int resource)
{
  RECT rect{};
  GetWindowRect(button, &rect);
  const auto max_raw = std::min(rect.right - rect.left, rect.bottom - rect.top);
  const auto max = FloorIconSize(max_raw * 3 / 4);

  const auto icon = LoadImageW(
    GetInstance(),
    MAKEINTRESOURCEW(resource),
    IMAGE_ICON,
    max,
    max,
    LR_DEFAULTCOLOR
  );

  SendMessageW(button, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)icon);
  return (HICON)icon;
}

bool utl::AreFilesTheSame(HANDLE a, HANDLE b)
{
  if (const auto kernel32 = GetModuleHandleW(L"kernel32"))
  {
    using fn_t = decltype(GetFileInformationByHandleEx);
    if (const auto pfn = reinterpret_cast<fn_t*>(GetProcAddress(kernel32, "GetFileInformationByHandleEx")))
    {
      struct xFILE_ID_INFO {
        ULONGLONG VolumeSerialNumber;
        FILE_ID_128 FileId;
      };
      constexpr static auto FileIdInfo = static_cast<FILE_INFO_BY_HANDLE_CLASS>(18);

      xFILE_ID_INFO fiia{}, fiib{};

      if (pfn(a, FileIdInfo, &fiia, sizeof(fiia)) && pfn(b, FileIdInfo, &fiib, sizeof(fiib)))
      {
        const auto& ida = fiia.FileId.Identifier;
        const auto& idb = fiib.FileId.Identifier;
        return fiia.VolumeSerialNumber == fiib.VolumeSerialNumber
          && std::equal(std::begin(ida), std::end(ida), std::begin(idb));
      }
    }
  }

  BY_HANDLE_FILE_INFORMATION fia, fib;
  if (!GetFileInformationByHandle(a, &fia) || !GetFileInformationByHandle(b, &fib))
    return false;

  return fia.dwVolumeSerialNumber == fib.dwVolumeSerialNumber
    && fia.nFileIndexLow == fib.nFileIndexLow
    && fia.nFileIndexHigh == fib.nFileIndexHigh;
}

std::wstring utl::MakePathLongCompatible(std::wstring file)
{
  constexpr static wchar_t prefix[] = L"\\\\";
  constexpr static auto prefixlen = std::size(prefix) - 1;
  const auto file_cstr = file.c_str();
  if (file.size() < prefixlen || 0 != wcsncmp(file_cstr, prefix, prefixlen))
    file.insert(0, L"\\\\?\\");
  return file;
}

HANDLE utl::OpenForRead(const std::wstring& file, bool async)
{
  return CreateFileW(
    MakePathLongCompatible(file).c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL | (async ? FILE_FLAG_OVERLAPPED : 0),
    nullptr
  );
}

DWORD utl::SetClipboardText(HWND hwnd, std::wstring_view text)
{
  DWORD error;

  if (OpenClipboard(hwnd))
  {
    EmptyClipboard();

    const auto len = text.length();

    if (const auto cb = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t)))
    {
      // Lock the handle and copy the text to the buffer. 
      if (const auto lcb = static_cast<LPWSTR>(GlobalLock(cb)))
      {
        memcpy(lcb, text.data(), (len + 1) * sizeof(wchar_t));
        const auto ref = GlobalUnlock(cb);
        error = GetLastError();
        if (ref != 0 || error == ERROR_SUCCESS)
        {
          // Place the handle on the clipboard.
          if (SetClipboardData(CF_UNICODETEXT, cb) == nullptr)
            error = GetLastError();
        }
      }
      else
        error = GetLastError();
    }
    else
      error = GetLastError();

    CloseClipboard();
  }
  else
    error = GetLastError();

  return error;
}

std::wstring utl::GetClipboardText(HWND hwnd)
{
  std::wstring wstr;
  if (OpenClipboard(hwnd))
  {
    const auto hglb = GetClipboardData(CF_UNICODETEXT);
    if(hglb)
    {
      const auto text = static_cast<LPCWSTR>(GlobalLock(hglb));

      if(text)
      {
        wstr = text;
        GlobalUnlock(hglb);
      }
    }

    CloseClipboard();
  }

  return wstr;
}

std::wstring utl::SaveDialog(HWND hwnd, const wchar_t* defpath, const wchar_t* defname)
{
  wchar_t name[PATHCCH_MAX_CCH];
  wcscpy_s(name, defname);

  OPENFILENAME of = { sizeof(OPENFILENAME), hwnd };
  of.lpstrFile = name;
  of.nMaxFile = static_cast<DWORD>(std::size(name));
  of.lpstrInitialDir = defpath;
  of.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
  if (!GetSaveFileNameW(&of))
  {
    const auto error = CommDlgExtendedError();
    // if error is 0 the user just cancelled the action
    if (error)
      FormattedMessageBox(
        hwnd,
        L"Error",
        MB_ICONERROR | MB_OK,
        L"GetSaveFileName returned with error: %08X",
        error
      );
    return {};
  }
  // Compiler keeps crying about it even though it's impossible
  name[std::size(name) - 1] = 0;
  return { name };
}

DWORD utl::SaveMemoryAsFile(const wchar_t* path, const void* p, DWORD size)
{
  const auto h = CreateFileW(
    MakePathLongCompatible(path).c_str(),
    GENERIC_WRITE,
    0,
    nullptr,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    nullptr
  );

  DWORD error = ERROR_SUCCESS;

  if (h == INVALID_HANDLE_VALUE)
  {
    error = GetLastError();
    return error;
  }

  DWORD written = 0;
  if (!WriteFile(h, p, size, &written, nullptr))
    error = GetLastError();

  CloseHandle(h);
  return error;
}

std::wstring utl::UTF8ToWide(const char* p)
{
  const auto wsize = MultiByteToWideChar(
    CP_UTF8,
    0,
    p,
    -1,
    nullptr,
    0
  );

  std::wstring wstr;
  // size includes null
  wstr.resize(wsize - 1);

  MultiByteToWideChar(
    CP_UTF8,
    0,
    p,
    -1,
    wstr.data(),
    wsize
  );

  return wstr;
}

std::string utl::WideToUTF8(const wchar_t* p)
{
  const auto size = WideCharToMultiByte(
    CP_UTF8,
    0,
    p,
    -1,
    nullptr,
    0,
    nullptr,
    nullptr
  );

  std::string str;
  // size includes null
  str.resize(size - 1);

  WideCharToMultiByte(
    CP_UTF8,
    0,
    p,
    -1,
    str.data(),
    size,
    nullptr,
    nullptr
  );

  return str;
}

std::wstring utl::ErrorToString(DWORD error)
{
  std::wstring wstr{};

  // FormatMessageW: "This buffer cannot be larger than 64K bytes."
  wstr.resize(64 * 1024 / sizeof(wchar_t));

  const auto size = FormatMessageW(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    error,
    MAKELANGID(LANG_USER_DEFAULT, SUBLANG_DEFAULT),
    wstr.data(),
    static_cast<DWORD>(wstr.size()),
    nullptr
  );
  wstr.resize(size);
  const auto pos = wstr.find_last_not_of(L"\r\n");
  if (pos != std::wstring::npos)
    wstr.resize(pos);
  return wstr;
}

std::pair<const char*, size_t> utl::GetResource(LPCWSTR name, LPCWSTR type)
{
  const auto rc = FindResourceW(
    GetInstance(),
    name,
    type
  );
  const auto rc_data = LoadResource(GetInstance(), rc);
  const auto size = SizeofResource(GetInstance(), rc);
  const auto data = static_cast<const char*>(LockResource(rc_data));
  return { data, size };
}

HFONT utl::GetDPIScaledFont(HWND hwnd, int pt)
{
  const auto hdc = GetDC(hwnd);
  const auto hf = CreateFontW(
    -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72),
    0,
    0,
    0,
    FW_NORMAL,
    FALSE,
    FALSE,
    FALSE,
    DEFAULT_CHARSET,
    OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS,
    PROOF_QUALITY,
    FF_DONTCARE,
    utl::GetString(IDS_FONT).c_str()
  );
  ReleaseDC(hwnd, hdc);
  return hf;
}

int utl::GetDPIScaledPixels(HWND hwnd, int px)
{
  const auto hdc = GetDC(hwnd);
  const auto ret = MulDiv(px, GetDeviceCaps(hdc, LOGPIXELSY), 96);
  ReleaseDC(hwnd, hdc);
  return ret;
}
