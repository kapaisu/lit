#include "net.h"

#include <windows.h>
#include <winhttp.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace lit::net {

std::string http_get(std::wstring_view url, std::wstring_view user_agent) {
    URL_COMPONENTSW uc{};
    uc.dwStructSize      = sizeof(uc);
    uc.dwSchemeLength    = (DWORD)-1;
    uc.dwHostNameLength  = (DWORD)-1;
    uc.dwUrlPathLength   = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;

    std::wstring wurl(url);
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return {};

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET sess = WinHttpOpen(std::wstring(user_agent).c_str(),
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) return {};

    DWORD tmo = 30000;
    WinHttpSetTimeouts(sess, tmo, tmo, tmo, tmo);

    std::string body;
    HINTERNET conn = nullptr, req = nullptr;
    do {
        conn = WinHttpConnect(sess, host.c_str(), uc.nPort, 0);
        if (!conn) break;

        const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!req) break;

        if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) break;
        if (!WinHttpReceiveResponse(req, nullptr)) break;

        DWORD status = 0, ssz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &ssz, WINHTTP_NO_HEADER_INDEX);
        if (status < 200 || status >= 300) break;

        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail)) { body.clear(); break; }
            if (!avail) break;
            std::vector<char> buf(avail);
            DWORD got = 0;
            if (!WinHttpReadData(req, buf.data(), avail, &got)) { body.clear(); break; }
            body.append(buf.data(), got);
        }
    } while (false);

    if (req)  WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(sess);
    return body;
}

}
