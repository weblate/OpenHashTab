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

#include "virustotal.h"

#include "FileHashTask.h"
#include "Settings.h"
#include "stringencrypt.h"
#include "utl.h"
#include "https.h"
#include "json.h"

#include <unordered_map>
#include <sstream>

bool vt::CheckForToS(Settings* settings, HWND hwnd)
{
  if(!settings->virustotal_tos)
  {
    const auto answer = utl::FormattedMessageBox(
      hwnd,
      ESTRt(L"VirusTotal Terms of Service"),
      MB_YESNO,
      ESTRt(L"You must agree to VirusTotal's terms of service to use this.\r\n"
      L"The ToS is available at https://www.virustotal.com/about/terms-of-service\r\n"
      L"Do you agree with the VirusTotal Terms of Service?")
    );

    if (answer == IDYES)
      settings->virustotal_tos.Set(true);
  }
  return settings->virustotal_tos;
}

std::list<vt::Result> vt::Query(const std::list<FileHashTask*>& files, size_t algo)
{
  std::string query_str;
  {
    std::stringstream query;
    query << ESTRt("[{\"hash\":\"");
    for (const auto& h : files)
    {
      char hash[HashAlgorithm::k_max_size * 2 + 1]{};
      utl::HashBytesToString(hash, h->GetHashResult()[algo]);
      query << hash << ESTRt("\"},{\"hash\":\"");
    }
    query << ESTRt("\"}]");
    query_str = query.str();
  }

  const auto user_agent =   ESTR(L"VirusTotal")();
  const auto server_name =  ESTR(L"www.virustotal.com")();
  const auto method =       ESTR(L"POST")();
  const auto uri =          ESTR(L"/partners/sysinternals/file-reports?\x0061\x0070\x0069\x006b\x0065\x0079=4e3202fdbe953d628f650229af5b3eb49cd46b2d3bfe5546ae3c5fa48b554e0c")();
  const auto headers =      ESTR(L"Content-Type: application/json\r\n")();

  HTTPRequest r{};
  r.user_agent = user_agent.data();
  r.server_name = server_name.data();
  r.method = method.data();
  r.uri = uri.data();
  r.headers = headers.data();
  r.body = query_str.c_str();
  r.body_size = static_cast<DWORD>(query_str.size());

  const auto reply = DoHTTPS(r);

  if(reply.error_code)
    throw std::runtime_error(utl::FormatString(
      ESTRt("Error %08X at %d: %s"),
      reply.error_code,
      reply.error_location,
      utl::ErrorToString(reply.error_code).c_str()
    ));

  if(reply.http_code != 200)
    throw std::runtime_error(utl::FormatString(
      ESTRt("HTTP Status %d received. Server says: %s"),
      reply.http_code,
      reply.body.c_str()
    ));


  json_parser parser{ reply.body.c_str() };
  const auto root = parser.root();

  if(!root)
    throw std::runtime_error(utl::FormatString(
      ESTRt("JSON parse error. Body: %s"),
      reply.body.c_str()
    ));

  const auto j_data = json_getProperty(root, "data");
  if(!j_data || json_getType(j_data) != JSON_ARRAY)
    throw std::runtime_error(utl::FormatString(
      ESTRt("Malformed reply. Body: %s"),
      reply.body.c_str()
    ));

  auto j_child = json_getChild(j_data);
  std::unordered_map<std::string, Result> result_map;
  do
  {
    const auto j_found = json_getProperty(j_child, "found");
    const auto j_hash = json_getProperty(j_child, "hash");
    if(j_found && j_hash && json_getType(j_found) == JSON_BOOLEAN && json_getType(j_hash) == JSON_TEXT)
    {
      Result res{};
      if (json_getBoolean(j_found) == true)
      {
        res.found = true;
        const auto j_permalink = json_getProperty(j_child, "permalink");
        const auto j_positives = json_getProperty(j_child, "positives");
        const auto j_total = json_getProperty(j_child, "total");

        if (j_permalink && json_getType(j_permalink) == JSON_TEXT)
          res.permalink = json_getValue(j_permalink);
        if (j_positives && json_getType(j_positives) == JSON_INTEGER)
          res.positives = static_cast<int>(json_getInteger(j_positives));
        if (j_total && json_getType(j_total) == JSON_INTEGER)
          res.total = static_cast<int>(json_getInteger(j_total));
      }
      result_map[json_getValue(j_hash)] = res;
    }

    j_child = json_getSibling(j_child);
  } while (j_child);

  std::list<Result> result;
  for(const auto f : files)
  {
    char hash[HashAlgorithm::k_max_size * 2 + 1]{};
    utl::HashBytesToString(hash, f->GetHashResult()[algo]);
    const auto res = result_map.find(hash);
    if(res != end(result_map))
    {
      auto copy = res->second;
      copy.file = f;
      result.push_back(copy);
    }
  }

  return result;
}
