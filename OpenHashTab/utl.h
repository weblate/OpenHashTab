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
#pragma once

#include <string>
#include <vector>

#ifdef _DEBUG
inline void DebugMsg(PCSTR fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char text[4096];
  vsprintf_s(text, fmt, args);
  va_end(args);
  OutputDebugStringA(text);
}
#else
inline void DebugMsg(PCSTR fmt, ...) { }
#endif

namespace utl
{
  inline HINSTANCE GetInstance() { return (HINSTANCE)&__ImageBase; }

  template <typename Char>
  Char hex(uint8_t n, bool upper = true)
  {
    if (n < 0xA)
      return Char('0') + n;
    return Char(upper ? 'A' : 'a') + (n - 0xA);
  };

  template <typename Char>
  uint8_t unhex(Char ch)
  {
    if (ch >= 0x80 || ch <= 0)
      return 0xFF;

    const auto c = (char)ch;

#define TEST_RANGE(c, a, b, offset) if (uint8_t(c) >= uint8_t(a) && uint8_t(c) <= uint8_t(b))\
  return uint8_t(c) - uint8_t(a) + (offset)

    TEST_RANGE(c, '0', '9', 0x0);
    TEST_RANGE(c, 'a', 'f', 0xa);
    TEST_RANGE(c, 'A', 'F', 0xA);

#undef TEST_RANGE

    return 0xFF;
  };

  template <typename Char>
  void HashBytesToString(Char* str, const std::vector<uint8_t>& hash, bool upper = true)
  {
    for (auto b : hash)
    {
      *str++ = utl::hex<Char>(b >> 4, upper);
      *str++ = utl::hex<Char>(b & 0xF, upper);
    }
    *str = Char(0);
  }

  template <typename Char>
  std::vector<uint8_t> HashStringToBytes(Char* str)
  {
    auto it = str;
    do
      if (const auto c = *it; !c || utl::unhex<Char>(c) != 0xFF)
        break;
    while (++it);

    std::vector<uint8_t> res;

    uint8_t byte = 0;
    for (auto i = 0; it[i]; ++i)
      if (const auto nibble = utl::unhex<Char>(it[i]); nibble == 0xFF)
        if (i % 2 == 0)
          break;
        else
          return {};
      else
        if (i % 2 == 0)
          byte = nibble << 4;
        else
          res.push_back(byte | nibble);

    return res;
  }


  template <typename Char>
  auto FormatStringV(_In_z_ _Printf_format_string_ const Char* fmt, va_list va) -> std::basic_string<Char>
  {
    using cfn_t = int(*)(const Char*, va_list);
    using fn_t = int(*)(Char*, size_t, const Char*, va_list);
    cfn_t cfn;
    fn_t fn;
    if constexpr (std::is_same_v<Char, char>)
    {
      cfn = &_vscprintf;
      fn = &vsprintf_s;
    }
    else
    {
      cfn = &_vscwprintf;
      fn = &vswprintf_s;
    }

    std::basic_string<Char> str;

    int len = cfn(fmt, va);
    str.resize(len);
    fn(str.data(), str.size() + 1, fmt, va);

    return str;
  }

  template <typename Char>
  auto FormatString(_In_z_ _Printf_format_string_ const Char* fmt, ...) -> std::basic_string<Char>
  {
    va_list args;
    va_start(args, fmt);
    auto str = FormatStringV(fmt, args);
    va_end(args);
    return str;
  }

  int FormattedMessageBox(HWND hwnd, LPCWSTR caption, UINT type, _In_z_ _Printf_format_string_ LPCWSTR fmt, ...);

  std::wstring GetString(UINT id);

  std::wstring GetWindowTextString(HWND hwnd);

  void SetWindowTextStringFromTable(HWND hwnd, UINT id);

  long FloorIconSize(long size);

  HICON SetIconButton(HWND button, int resource);

  bool AreFilesTheSame(HANDLE a, HANDLE b);

  std::wstring MakePathLongCompatible(std::wstring file);

  HANDLE OpenForRead(const std::wstring& file, bool async = false);

  DWORD SetClipboardText(HWND hwnd, std::wstring_view text);

  std::wstring GetClipboardText(HWND hwnd);

  std::wstring SaveDialog(HWND hwnd, const wchar_t* defpath, const wchar_t* defname);

  DWORD SaveMemoryAsFile(const wchar_t* path, const void* p, DWORD size);

  std::wstring UTF8ToWide(const char* p);
  std::string WideToUTF8(const wchar_t* p);

  std::wstring ErrorToString(DWORD error);

  std::pair<const char*, size_t> GetResource(LPCWSTR name, LPCWSTR type);

  HFONT GetDPIScaledFont(HWND hwnd, int pt);

  int GetDPIScaledPixels(HWND hwnd, int px);

  struct Version
  {
    uint16_t major{};
    uint16_t minor{};
    uint16_t patch{};

    constexpr Version(uint16_t major, uint16_t minor, uint16_t patch)
      : major(major)
      , minor(minor)
      , patch(patch) {}

    constexpr Version() = default;

    constexpr uint64_t AsNumber() const { return ((uint64_t)major << 32) | ((uint64_t)minor << 16) | patch; }

    constexpr bool operator==(const Version& rhs) const { return AsNumber() == rhs.AsNumber(); }
    constexpr bool operator<(const Version& rhs) const { return AsNumber() < rhs.AsNumber(); }
    constexpr bool operator>(const Version& rhs) const { return AsNumber() > rhs.AsNumber(); }
  };

  Version GetLatestVersion();
}
