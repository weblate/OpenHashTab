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

#include "MainDialog.h"

#include "Coordinator.h"
#include "Exporter.h"
#include "Settings.h"
#include "utl.h"
#include "SettingsDialog.h"
#include "FileHashTask.h"
#include "wnd.h"
#include "virustotal.h"

#include <sstream>

enum class HashColorType
{
  Error,
  Match,
  Insecure,
  Mismatch,
  Unknown
};

static bool ColorLine(LPNMLVCUSTOMDRAW plvcd, HashColorType type)
{
  // No hash to compare to  - system colors
  // Error processing file  - system bg, red text
  // Hash mismatch          - red bg, white text for all algos
  // Secure hash matches    - green bg, white text for algo matching
  // Insecure hash matches  - orange bg, white text for algo matching
  
  switch(type)
  {
  case HashColorType::Unknown:
    return false;
  case HashColorType::Error:
    plvcd->clrText = RGB(255, 55, 23);
    break;
  case HashColorType::Match:
    plvcd->clrText = RGB(255, 255, 255);
    plvcd->clrTextBk = RGB(45, 170, 23);
    break;
  case HashColorType::Insecure:
    plvcd->clrText = RGB(255, 255, 255);
    plvcd->clrTextBk = RGB(170, 82, 23);
    break;
  case HashColorType::Mismatch:
    plvcd->clrText = RGB(255, 255, 255);
    plvcd->clrTextBk = RGB(230, 55, 23);
    break;
  }
  return true;
}

static HashColorType HashColorTypeForFile(FileHashTask* file, size_t hasher)
{
  if (file->GetError() != ERROR_SUCCESS)
    return HashColorType::Error;

  const auto match = file->GetMatchState();
  if (match == FileHashTask::MatchState_Mismatch)
    return HashColorType::Mismatch;

  if (match != FileHashTask::MatchState_None && static_cast<size_t>(match) == hasher)
  {
    if (HashAlgorithm::g_hashers[hasher].IsSecure())
      return HashColorType::Match;
    
    return HashColorType::Insecure;
  }
  return HashColorType::Unknown;
}

static const Exporter* GetSelectedExporter(HWND combo)
{
  const auto sel = ComboBox_GetCurSel(combo);
  const auto len = ComboBox_GetLBTextLen(combo, sel);
  std::wstring wname;
  wname.resize(len);
  ComboBox_GetLBText(combo, sel, wname.data());
  const auto name = utl::WideToUTF8(wname.c_str());
  for (const auto exporter : Exporter::k_exporters)
    if (name == exporter->GetName())
      return exporter;
  return nullptr;
}

static std::wstring ListView_GetItemTextStr(HWND hwnd, int item, int subitem)
{
  wchar_t name[PATHCCH_MAX_CCH];
  name[0] = 0;
  ListView_GetItemText(hwnd, item, subitem, name, std::size(name));
  return name;
}

MainDialog::MainDialog(HWND hwnd, void* prop_page)
  : _hwnd(hwnd)
  , _prop_page(static_cast<Coordinator*>(prop_page))
{
  _prop_page->RegisterWindow(hwnd);
}

MainDialog::~MainDialog()
{
  _prop_page->Cancel(false);
  _prop_page->UnregisterWindow();
}

INT_PTR MainDialog::CustomDrawListView(LPARAM lparam, HWND list)
{
  const auto lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lparam);

  switch (lplvcd->nmcd.dwDrawStage)
  {
  case CDDS_PREPAINT:  // NOLINT(bugprone-branch-clone)
    return CDRF_NOTIFYITEMDRAW;

  case CDDS_ITEMPREPAINT:
    return CDRF_NOTIFYSUBITEMDRAW;

  case CDDS_SUBITEM | CDDS_ITEMPREPAINT: //Before a subitem is drawn
  {
    switch (lplvcd->iSubItem)
    {
    case ColIndex_Hash:
    {
      LVITEMW lvitem
      {
        LVIF_PARAM,
        static_cast<int>(lplvcd->nmcd.dwItemSpec)
      };
      ListView_GetItem(list, &lvitem);
      if (!lvitem.lParam) // TODO: handle virustotal shit better
        break;
      const auto file_hash = FileHashTask::FromLparam(lvitem.lParam);
      const auto file = file_hash.first;
      const auto hasher = file_hash.second;
      const auto color_type = HashColorTypeForFile(file, hasher);
      if(ColorLine(lplvcd, color_type))
        return CDRF_NEWFONT;

      // fall through for normal color
    }
    default:
      break;
    }

  }
  default:
    break;
  }
  return CDRF_DODEFAULT;
}

void MainDialog::AddItemToFileList(LPCWSTR filename, LPCWSTR algorithm, LPCWSTR hash, LPARAM lparam)
{
  const auto list = _hwnd_HASH_LIST;
  LVITEMW lvitem
  {
    LVIF_PARAM,
    INT_MAX,
    0,
    0,
    0,
    const_cast<LPWSTR>(L"")
  };
  lvitem.lParam = lparam;
  const auto item = ListView_InsertItem(list, &lvitem);
  ListView_SetItemText(list, item, ColIndex_Filename, const_cast<LPWSTR>(filename));
  ListView_SetItemText(list, item, ColIndex_Algorithm, const_cast<LPWSTR>(algorithm));
  ListView_SetItemText(list, item, ColIndex_Hash, const_cast<LPWSTR>(hash));
}

void MainDialog::ListDoubleClick(int item, int subitem)
{
  if (item == -1)
    return;
  const auto list = _hwnd_HASH_LIST;
  // It's possible it will hold an error message
  const auto hash = ListView_GetItemTextStr(list, item, ColIndex_Hash);
  if (subitem == ColIndex_Hash)
  {
    utl::SetClipboardText(_hwnd, hash);
  }
  else
  {
    const auto name = ListView_GetItemTextStr(list, item, ColIndex_Filename);
    utl::SetClipboardText(_hwnd, (std::wstring{ hash } + L" *" + name).c_str());
  }
  SetTempStatus(utl::GetString(IDS_COPIED).c_str(), 1000);
}

void MainDialog::ListPopupMenu(POINT pt)
{
  const auto list = _hwnd_HASH_LIST;

  const auto count = ListView_GetItemCount(list);
  if (!count)
    return;

  LVHITTESTINFO lvhtinfo = { pt };
  const auto item = ListView_HitTest(list, &lvhtinfo);

  MapWindowPoints(list, HWND_DESKTOP, &pt, 2);
  const auto menu = CreatePopupMenu();

#define CTLSTR(name) IDM_ ## name, utl::GetString(IDS_ ## name).c_str()

  AppendMenuW(menu, 0, CTLSTR(COPY_HASH));
  AppendMenuW(menu, 0, CTLSTR(COPY_LINE));
  AppendMenuW(menu, 0, CTLSTR(COPY_FILE));
  AppendMenuW(menu, 0, CTLSTR(COPY_EVERYTHING));

#undef CTLSTR

  const auto selection = (int)TrackPopupMenuEx(
    menu,
    TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
    pt.x,
    pt.y,
    _hwnd,
    nullptr
  );

  if (!selection)
    return;

  if (selection != IDM_COPY_EVERYTHING && item == -1)
    return;

  if (selection == IDM_COPY_HASH || selection == IDM_COPY_FILE)
  {
    const auto col = selection == IDM_COPY_HASH ? ColIndex_Hash : ColIndex_Filename;
    utl::SetClipboardText(_hwnd, ListView_GetItemTextStr(list, item, col));
  }
  else if (selection == IDM_COPY_LINE)
  {
    const auto str = ListView_GetItemTextStr(list, item, ColIndex_Filename)
      + L"\t" + ListView_GetItemTextStr(list, item, ColIndex_Algorithm)
      + L"\t" + ListView_GetItemTextStr(list, item, ColIndex_Hash);
    utl::SetClipboardText(_hwnd, str);
    return;
  }
  else
  {
    std::wstringstream clipboard;
    for (auto i = 0; i < count; ++i)
    {
      clipboard
        << ListView_GetItemTextStr(list, i, ColIndex_Filename) << L"\t"
        << ListView_GetItemTextStr(list, i, ColIndex_Algorithm) << L"\t"
        << ListView_GetItemTextStr(list, i, ColIndex_Hash) << L"\r\n";
    }
    utl::SetClipboardText(_hwnd, clipboard.str());
  }

  SetTempStatus(utl::GetString(IDS_COPIED).c_str(), 1000);
}

std::string MainDialog::GetSumfileAsString(const Exporter* exporter, bool for_clipboard)
{
  std::list<FileHashTask*> files;
  for (const auto& file : _prop_page->GetFiles())
    files.push_back(file.get());
  return exporter->GetExportString(&_prop_page->settings, for_clipboard, files);
}

void MainDialog::SetTempStatus(LPCWSTR status, UINT time)
{
  _temporary_status = true;
  SetWindowTextW(_hwnd_STATIC_PROCESSING, status);
  SetTimer(_hwnd, k_status_update_timer_id, time, nullptr);
}

void MainDialog::UpdateDefaultStatus(bool force_reset)
{
  if (force_reset)
    _temporary_status = false;

  if (!_temporary_status)
  {
    const auto msg = _finished ? IDS_DONE : IDS_PROCESSING;
    wchar_t done[64];
    swprintf_s(
      done,
      L"%s (%u/%u/%u/%u)",
      utl::GetString(msg).c_str(),
      _count_match,
      _count_mismatch,
      _count_unknown,
      _count_error
    );
    SetWindowTextW(_hwnd_STATIC_PROCESSING, done);
  }
}

INT_PTR MainDialog::DlgProc(UINT msg, WPARAM wparam, LPARAM lparam)
{
  DebugMsg("DlgProc uMsg: %04X wParam: %08X lParam: %016X\n", msg, wparam, lparam);

  constexpr static wnd::MessageMatcher<MainDialog> matchers[] = {
    { &MainDialog::OnInitDialog,        WM_INITDIALOG },
    { &MainDialog::OnClose,             WM_CLOSE },
    { &MainDialog::OnNeedAdjust,        WM_WINDOWPOSCHANGING },
    { &MainDialog::OnNeedAdjust,        WM_WINDOWPOSCHANGED },
    { &MainDialog::OnFileFinished,      wnd::WM_USER_FILE_FINISHED, wnd::Match_w, wnd::k_user_magic_wparam },
    { &MainDialog::OnAllFilesFinished,  wnd::WM_USER_ALL_FILES_FINISHED, wnd::Match_w, wnd::k_user_magic_wparam },
    { &MainDialog::OnFileProgress,      wnd::WM_USER_FILE_PROGRESS, wnd::Match_w, wnd::k_user_magic_wparam },
    { &MainDialog::OnStatusUpdateTimer, WM_TIMER,   wnd::Match_w,   k_status_update_timer_id },
    { &MainDialog::OnHashListNotify,    WM_NOTIFY,  wnd::Match_w,   IDC_HASH_LIST },
    { &MainDialog::OnHashEditChanged,   WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_EDIT_HASH, EN_CHANGE) },
    { &MainDialog::OnClipboardClicked,  WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_BUTTON_CLIPBOARD, BN_CLICKED) },
    { &MainDialog::OnSettingsClicked,   WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_BUTTON_SETTINGS, BN_CLICKED) },
    { &MainDialog::OnExportClicked,     WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_BUTTON_EXPORT, BN_CLICKED) },
    { &MainDialog::OnCancelClicked,     WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_BUTTON_CANCEL, BN_CLICKED) },
    { &MainDialog::OnVTClicked,         WM_COMMAND, wnd::Match_wlh, MAKELONG(IDC_BUTTON_VT, BN_CLICKED) }
  };

  INT_PTR ret = FALSE;
  if (RouteMessage(this, std::begin(matchers), std::end(matchers), msg, wparam, lparam, ret))
    return ret;

  return FALSE;
}

INT_PTR MainDialog::OnInitDialog(UINT, WPARAM, LPARAM)
{
  SetClassLongPtrW(
    _hwnd,
    GCLP_HICON,
    reinterpret_cast<LONG_PTR>(LoadIconW(utl::GetInstance(), MAKEINTRESOURCEW(IDI_ICON1)))
  );

  const auto hfont = utl::GetDPIScaledFont(_hwnd, 8);
  for (auto wnd = GetWindow(_hwnd, GW_CHILD); wnd; wnd = GetWindow(wnd, GW_HWNDNEXT))
    SetWindowFont(wnd, hfont, TRUE);

  utl::SetIconButton(_hwnd_BUTTON_VT, IDI_ICON_VT);
  utl::SetIconButton(_hwnd_BUTTON_SETTINGS, IDI_ICON_COG);

  utl::SetWindowTextStringFromTable(_hwnd_STATIC_CHECK_AGAINST, IDS_CHECK_AGAINST);
  utl::SetWindowTextStringFromTable(_hwnd_STATIC_EXPORT_TO, IDS_EXPORT_TO);
  utl::SetWindowTextStringFromTable(_hwnd_BUTTON_EXPORT, IDS_EXPORT_BTN);
  utl::SetWindowTextStringFromTable(_hwnd_STATIC_PROCESSING, IDS_PROCESSING);
  utl::SetWindowTextStringFromTable(_hwnd_BUTTON_CLIPBOARD, IDS_CLIPBOARD);
  utl::SetWindowTextStringFromTable(_hwnd_BUTTON_CANCEL, IDS_CANCEL);

  SendMessageW(_hwnd_HASH_LIST, LVM_SETTEXTBKCOLOR, 0, CLR_NONE);
  SendMessageW(_hwnd_HASH_LIST, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);

  // we put the string table ID in the text length field, to fix it up later
  LVCOLUMN cols[] =
  {
    {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, utl::GetDPIScaledPixels(_hwnd, 140),   nullptr, IDS_FILENAME},
    {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, utl::GetDPIScaledPixels(_hwnd, 70),    nullptr, IDS_ALGORITHM},
    {LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, utl::GetDPIScaledPixels(_hwnd, 1100),  nullptr, IDS_HASH},
  };

  for (auto i = 0u; i < std::size(cols); ++i)
  {
    auto& col = cols[i];
    const auto wstr = utl::GetString(col.cchTextMax);
    col.pszText = const_cast<LPWSTR>(wstr.c_str());
    ListView_InsertColumn(_hwnd_HASH_LIST, i, &cols[i]);
  }

  SendMessageW(_hwnd_PROGRESS, PBM_SETRANGE32, 0, Coordinator::k_progress_resolution);

  // !!! enabled algorithms MAY BE CHANGED by this call, if a sumfile is not in a enabled format according to extension
  _prop_page->AddFiles();

  for (const auto& exporter : Exporter::k_exporters)
    if (exporter->IsEnabled(&_prop_page->settings))
      ComboBox_AddString(_hwnd_COMBO_EXPORT, utl::UTF8ToWide(exporter->GetName()).c_str());

  ComboBox_SetCurSel(_hwnd_COMBO_EXPORT, 0);

  if (_prop_page->IsSumfile())
    utl::SetWindowTextStringFromTable(_hwnd_STATIC_SUMFILE, IDS_SUMFILE);

  if (_prop_page->GetFiles().size() == 1)
    ListView_SetColumnWidth(_hwnd_HASH_LIST, ColIndex_Filename, 0);

  _prop_page->ProcessFiles();

  return FALSE;
}

INT_PTR MainDialog::OnFileFinished(UINT, WPARAM, LPARAM lparam)
{
  const auto file = reinterpret_cast<FileHashTask*>(lparam);

  if (const auto error = file->GetError(); error == ERROR_SUCCESS)
  {
    switch (file->GetMatchState())
    {
    case FileHashTask::MatchState_None:
      ++_count_unknown;
      break;
    case FileHashTask::MatchState_Mismatch:
      ++_count_mismatch;
      break;
    default:
      ++_count_match;
      break;
    }

    const auto& results = file->GetHashResult();

    for (auto i = 0u; i < HashAlgorithm::k_count; ++i)
    {
      auto& result = results[i];
      if (!result.empty())
      {
        wchar_t hash_str[HashAlgorithm::k_max_size * 2 + 1];
        utl::HashBytesToString(hash_str, result, _prop_page->settings.display_uppercase);
        const auto tname = utl::UTF8ToWide(HashAlgorithm::g_hashers[i].GetName());
        AddItemToFileList(file->GetDisplayName().c_str(), tname.c_str(), hash_str, file->ToLparam(i));
      }
    }
  }
  else
  {
    ++_count_error;
    AddItemToFileList(
      file->GetDisplayName().c_str(),
      utl::GetString(IDS_ERROR).c_str(),
      utl::ErrorToString(error).c_str(),
      file->ToLparam(0)
    );
  }

  UpdateDefaultStatus();

  return FALSE;
}

INT_PTR MainDialog::OnAllFilesFinished(UINT, WPARAM, LPARAM)
{
  _finished = true;

  // We only enable settings button after processing is done because changing enabled algorithms could result
  // in much more problems
  Button_Enable(_hwnd_BUTTON_SETTINGS, true);
  Button_Enable(_hwnd_BUTTON_EXPORT, true);
  Button_Enable(_hwnd_BUTTON_CLIPBOARD, true);
  Button_Enable(_hwnd_BUTTON_VT, true);
  Edit_Enable(_hwnd_EDIT_HASH, true);

  ShowWindow(_hwnd_PROGRESS, 0);
  ShowWindow(_hwnd_BUTTON_CANCEL, 0);

  UpdateDefaultStatus();

  const auto clip = utl::GetClipboardText(_hwnd);
  const auto find_hash = utl::HashStringToBytes(clip.c_str());
  if (find_hash.size() >= 4) // at least 4 bytes for a valid hash
  {
    SetWindowTextW(_hwnd_EDIT_HASH, (clip.c_str()));
    OnHashEditChanged(0, 0, 0); // fake a change as if the user pasted it
  }

  return FALSE;
}

INT_PTR MainDialog::OnFileProgress(UINT, WPARAM, LPARAM lparam)
{
  SendMessageW(_hwnd_PROGRESS, PBM_SETPOS, lparam, 0);
  return FALSE;
}

INT_PTR MainDialog::OnStatusUpdateTimer(UINT, WPARAM, LPARAM)
{
  UpdateDefaultStatus(true);
  return FALSE;
}

INT_PTR MainDialog::OnHashListNotify(UINT, WPARAM, LPARAM lparam)
{
  const auto phdr = reinterpret_cast<LPNMHDR>(lparam);
  switch (phdr->code)
  {
  case NM_CUSTOMDRAW:
    SetWindowLongPtrW(_hwnd, DWLP_MSGRESULT, CustomDrawListView(lparam, _hwnd_HASH_LIST));
    return TRUE;

  case NM_DBLCLK:
  {
    const auto lpnmia = reinterpret_cast<LPNMITEMACTIVATE>(lparam);
    LVHITTESTINFO lvhtinfo = { lpnmia->ptAction };
    ListView_SubItemHitTest(_hwnd_HASH_LIST, &lvhtinfo);
    ListDoubleClick(lvhtinfo.iItem, lvhtinfo.iSubItem);
    break;
  }

  case NM_RCLICK:
    ListPopupMenu(reinterpret_cast<LPNMITEMACTIVATE>(lparam)->ptAction);
    break;

  default:
    break;
  }
  return FALSE;
}

INT_PTR MainDialog::OnExportClicked(UINT, WPARAM, LPARAM)
{
  const auto exporter = GetSelectedExporter(_hwnd_COMBO_EXPORT);
  if (exporter && !_prop_page->GetFiles().empty())
  {
    const auto ext = utl::UTF8ToWide(exporter->GetExtension());
    const auto path_and_basename = _prop_page->GetSumfileDefaultSavePathAndBaseName();
    const auto name = path_and_basename.second + L"." + ext;
    const auto sumfile_path = utl::SaveDialog(_hwnd, path_and_basename.first.c_str(), name.c_str());
    if (!sumfile_path.empty())
    {
      const auto content = GetSumfileAsString(exporter, false);
      const auto err = utl::SaveMemoryAsFile(sumfile_path.c_str(), content.c_str(), content.size());
      if (err != ERROR_SUCCESS)
        utl::FormattedMessageBox(
          _hwnd,
          L"Error",
          MB_ICONERROR | MB_OK,
          L"utl::SaveMemoryAsFile returned with error: %08X",
          err
        );
    }
  }

  return FALSE;
}

INT_PTR MainDialog::OnCancelClicked(UINT, WPARAM, LPARAM)
{
  _prop_page->Cancel(true);

  return FALSE;
}

INT_PTR MainDialog::OnVTClicked(UINT, WPARAM, LPARAM)
{
  if (vt::CheckForToS(&_prop_page->settings, _hwnd))
  {
    const int algos[] = {
      HashAlgorithm::IdxByName("SHA-256"),
      HashAlgorithm::IdxByName("SHA-1"),
      HashAlgorithm::IdxByName("MD5")
    };
    const auto algo = std::find_if(std::begin(algos), std::end(algos), [&](const int& v)
      {
        return _prop_page->settings.algorithms[v];
      });
    if (algo == std::end(algos))
    {
      MessageBoxW(_hwnd, L"No compatible algorithm", L"Error", MB_ICONERROR | MB_OK);
    }
    else
    {
      std::list<FileHashTask*> l;
      for (const auto& f : _prop_page->GetFiles())
        if (!f->GetError())
          l.push_back(f.get());
      try
      {
        const auto result = vt::Query(l, *algo);
        for (const auto& r : result)
          AddItemToFileList(
            r.file->GetDisplayName().c_str(),
            r.found ? utl::FormatString(L"VT (%d/%d)", r.positives, r.total).c_str() : L"VT",
            r.found ? utl::UTF8ToWide(r.permalink.c_str()).c_str() : L"Not found",
            (LPARAM)0
          );
        Button_Enable(_hwnd_BUTTON_VT, false);
      }
      catch (const std::runtime_error& e)
      {
        MessageBoxW(
          _hwnd,
          utl::UTF8ToWide(e.what()).c_str(),
          L"Runtime error",
          MB_ICONERROR | MB_OK
        );
      }
    }
  }

  return FALSE;
}

INT_PTR MainDialog::OnClose(UINT, WPARAM, LPARAM)
{
  // we should never ever receive WM_CLOSE when running as a property sheet, so DestroyWindow is safe here
  DestroyWindow(_hwnd);
  
  return FALSE;
}

INT_PTR MainDialog::OnNeedAdjust(UINT, WPARAM, LPARAM)
{
  _adapter.Adjust();

  return FALSE;
}

INT_PTR MainDialog::OnHashEditChanged(UINT, WPARAM, LPARAM)
{
  const auto find_hash = utl::HashStringToBytes(utl::GetWindowTextString(_hwnd_EDIT_HASH).c_str());
  auto found = false;
  for (const auto& file : _prop_page->GetFiles())
  {
    const auto& result = file->GetHashResult();
    for (auto i = 0; i < HashAlgorithm::k_count; ++i)
    {
      if (!result[i].empty() && result[i] == find_hash)
      {
        found = true;
        const auto algorithm_name = utl::UTF8ToWide(HashAlgorithm::g_hashers[i].GetName());
        const auto txt = algorithm_name + L" / " + file->GetDisplayName();
        SetWindowTextW(_hwnd_STATIC_CHECK_RESULT, txt.c_str());
        break;
      }
    }
    if (found)
      break;
  }
  if (!found)
    SetWindowTextW(_hwnd_STATIC_CHECK_RESULT, utl::GetString(IDS_NOMATCH).c_str());

  return FALSE;
}

INT_PTR MainDialog::OnClipboardClicked(UINT, WPARAM, LPARAM)
{
  const auto exporter = GetSelectedExporter(_hwnd_COMBO_EXPORT);
  if (!_prop_page->GetFiles().empty() && exporter)
  {
    const auto str = GetSumfileAsString(exporter, true);
    utl::SetClipboardText(_hwnd, utl::UTF8ToWide(str.c_str()).c_str());
  }

  return FALSE;
}

INT_PTR MainDialog::OnSettingsClicked(UINT, WPARAM, LPARAM)
{
  DialogBoxParamW(
    ATL::_AtlBaseModule.GetResourceInstance(),
    MAKEINTRESOURCEW(IDD_SETTINGS),
    _hwnd,
    &wnd::DlgProcClassBinder<SettingsDialog>,
    reinterpret_cast<LPARAM>(&_prop_page->settings)
  );

  return FALSE;
}