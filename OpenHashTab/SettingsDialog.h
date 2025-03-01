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

#include "wnd.h"

struct Settings;

class SettingsDialog
{
  HWND _hwnd;

  MAKE_IDC_MEMBER(_hwnd, BUTTON_CHECK_FOR_UPDATES);
  MAKE_IDC_MEMBER(_hwnd, ALGORITHM_LIST);

  bool _done_setup = false;
  Settings* _settings;

public:
  SettingsDialog(HWND handle, void* settings) : _hwnd(handle), _settings((Settings*)settings) {}

  INT_PTR DlgProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
