#pragma once

#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <stdint.h>
#include <stdio.h>

#define PANIC(str)                                                                   \
  printf("%s unexpected condition: %s:%d: %s\n", WEB_IMPL, __FILE__, __LINE__, str); \
  exit(1);

// #define WEB_IMPL_CURL
#define WEB_UA_DEFAULT "web-cc 0.1"

#ifndef WEB_IMPL_CURL
#ifdef __linux
#define WEB_IMPL_CURL
#elif _WIN32
#define WEB_IMPL_WININET
#include <windows.h>
#include <wininet.h>
#endif
#endif

#ifdef WEB_IMPL_CURL
#include <curl/curl.h>
#endif

#ifdef WEB_IMPL_CURL
#define WEB_IMPL "libCURL"
#endif

#ifdef WEB_IMPL_WININET
#define WEB_IMPL "WinInet"
#endif

#ifdef WEB_IMPL_EMSCRIPTEN
#define WEB_IMPL "Emscripten"
#endif

namespace web
{

std::vector<std::string> splitStr(std::string str, std::string token)
{
  std::vector<std::string> result;
  while (str.size())
  {
    int index = str.find(token);
    if (index != std::string::npos)
    {
      result.push_back(str.substr(0, index));
      str = str.substr(index + token.size());
      if (str.size() == 0)
        result.push_back(str);
    }
    else
    {
      result.push_back(str);
      str = "";
    }
  }
  return result;
}

class URL
{
public:
  std::string m_scheme;
  std::string m_host;
  int m_port;
  std::string m_path;

  URL(std::string url)
  {
    auto schemes = splitStr(url, "://");
    if (schemes.size() != 2)
    {
      return;
    }

    m_scheme = schemes.at(0);

    auto paths = splitStr(schemes.at(1), "/");

    auto hostPort = splitStr(paths.at(0), ":");

    if (hostPort.size() == 1)
    {
      m_host = paths.at(0);
      if (m_scheme == "http")
        m_port = 80;
      if (m_scheme == "https")
        m_port = 443;
    }
    else
    {
      m_host = hostPort.at(0);
      m_port = std::atoi(hostPort.at(1).c_str());
    }

    if (paths.size() == 1)
    {
      m_path = "/";
      return;
    }

    m_path = "/" + paths.at(1);
  }

  std::string toString()
  {
    std::string portString = "";

    if (m_scheme == "https" && m_port != 443)
      portString = ":" + std::to_string(m_port);

    if (m_scheme == "http" && m_port != 80)
      portString = ":" + std::to_string(m_port);

    return m_scheme + "://" + m_host + portString + m_path;
  }

  void debug()
  {
    printf("web::URL {\n");
    printf("  Scheme %s\n", m_scheme.c_str());
    printf("  Host %s\n", m_host.c_str());
    printf("  Port %d\n", m_port);
    printf("  Path %s\n", m_path.c_str());
    printf("}\n");
  }

  std::string host()
  {
    return m_host;
  }

  std::string path()
  {
    return m_path;
  }
};

class Client;
class Request;

#ifdef WEB_IMPL_CURL
size_t __curlWriteFunction(uint8_t *ptr, size_t size, size_t nmemb, Request *request);
#endif

#ifdef WEB_IMPL_WININET
DWORD WINAPI __WinInetPerformTask(Request *ptr);
#endif

class Request
{
public:
  // User defined options
  std::string m_method;
  URL m_url;
  std::function<void(Request *)> m_callback;

  // State variables
  Client *m_client;

#ifdef WEB_IMPL_WININET
  HINTERNET m_session;
  HINTERNET m_connection;
  HINTERNET m_httpFile;
#endif

#ifdef WEB_IMPL_CURL
  CURL *m_curl;
#endif
  std::mutex m_protect;

  bool m_requestInit;
  uint64_t m_bytesRead;
  bool m_complete;
  uint16_t m_statusCode;
  // State returned from server
  std::vector<uint8_t> m_content;

  Request(
      std::string method,
      URL url) : m_method(method),
                 m_url(url),
                 m_complete(false),
                 m_requestInit(false),
                 m_protect()
  {

#ifdef WEB_IMPL_CURL
    m_curl = curl_easy_init();
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, __curlWriteFunction);
    curl_easy_setopt(m_curl, CURLOPT_URL, m_url.toString().c_str());
#endif
  }

  void _appendContent(uint8_t *bytes, size_t len)
  {
    m_content.insert(m_content.end(), bytes, bytes + len);
  }

  std::vector<uint8_t> content()
  {
    return m_content;
  }

  uint8_t *data()
  {
    return m_content.data();
  }

  uint16_t status()
  {
    return m_statusCode;
  }

  std::string text()
  {
    std::string str;
    str.assign(m_content.begin(), m_content.end());
    return str;
  }

  bool complete()
  {
    m_protect.lock();
    bool is = m_complete;
    m_protect.unlock();
    return is;
  }

  void onComplete(std::function<void(Request *)> func)
  {
    m_callback = func;
  }

  void _invokeCallback()
  {
    if (m_callback)
      m_callback(this);
  }

  ~Request()
  {
#ifdef WEB_IMPL_CURL
    curl_easy_cleanup(m_curl);
#endif
#ifdef WEB_IMPL_WININET
    InternetCloseHandle(m_httpFile);
    InternetCloseHandle(m_connection);
    InternetCloseHandle(m_session);
#endif
  }
};

class Client
{
public:
#ifdef WEB_IMPL_CURL
  CURLM *m_multi_handle;
  int m_running_handles;
#endif
  std::string m_userAgent;
  std::vector<Request *> m_requests;
  std::string m_socks5;

  Client(std::string userAgent = WEB_UA_DEFAULT,
         std::string socks5 = "")
      : m_requests(),
        m_userAgent(userAgent),
        m_socks5(socks5)
  {
#ifdef WEB_IMPL_CURL
    m_multi_handle = curl_multi_init();
#endif
  }

  void addRequest(Request *request)
  {
    request->m_client = this;
    // Initialize
#ifdef WEB_IMPL_WININET
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)__WinInetPerformTask, request, 0, nullptr);
#endif
    m_requests.push_back(request);

#ifdef WEB_IMPL_CURL
    curl_easy_setopt(request->m_curl, CURLOPT_USERAGENT, m_userAgent.c_str());
    if (m_socks5 != "")
      curl_easy_setopt(request->m_curl, CURLOPT_PROXY, ("socks5h://" + m_socks5).c_str());
    curl_multi_add_handle(m_multi_handle, request->m_curl);
#endif
  }

  bool active()
  {
    return m_requests.size() > 0;
  }

  void perform()
  {
#ifdef WEB_IMPL_CURL
    curl_multi_perform(m_multi_handle, &m_running_handles);

    Request *request;
    CURLMsg *msg;
    int msgs_left;

    while ((msg = curl_multi_info_read(m_multi_handle, &msgs_left)))
    {
      request = nullptr;

      if (msg->msg == CURLMSG_DONE)
      {
        int idx;

        /* Find out which handle this message is about */
        for (auto req : m_requests)
        {
          if (req->m_curl == msg->easy_handle)
          {
            request = req;
            break;
          }
        }

        if (!request)
        {
          PANIC("no request in heap");
        }

        request->m_complete = true;
        long response_code;
        curl_easy_getinfo(request->m_curl, CURLINFO_RESPONSE_CODE, &response_code);
        request->m_statusCode = response_code;
      }
    }
#endif
    for (size_t it = 0; it < m_requests.size(); ++it)
    {
      Request *request = m_requests.at(it);
      if (request->complete())
      {
        // Execute a user-defined callback for this request.
        request->_invokeCallback();
#ifdef WEB_IMPL_CURL
        curl_multi_remove_handle(m_multi_handle, request->m_curl);
#endif
        delete request;
        m_requests.erase(m_requests.begin() + it);
        // Since this element was deleted, we must decrement the index, so that it remains the same next iteration
        // (as it will refer to the next element of the vector from the one we deleted)
        it--;
      }
    }
  }
};

// Callbacks for platform-specific code

#ifdef WEB_IMPL_CURL
size_t __curlWriteFunction(uint8_t *ptr, size_t size, size_t nmemb, Request *request)
{
  size_t realsize = size * nmemb;
  request->_appendContent(ptr, realsize);
  return realsize;
}
#endif

#ifdef WEB_IMPL_WININET
// Called as a thread. Performs the download in the background then signals to the main thread when it is complete.
DWORD WINAPI __WinInetPerformTask(Request *request)
{
#define BUFFERSIZE 1024
  LPCSTR lpszProxy = nullptr;

  DWORD dwAccessType = INTERNET_OPEN_TYPE_PRECONFIG;

  request->m_session = InternetOpenA(
      request->m_client->m_userAgent.c_str(),
      dwAccessType,
      nullptr,
      nullptr,
      INTERNET_FLAG_NO_CACHE_WRITE);

  if (request->m_client->m_socks5 != "")
  {
    PANIC("socks5 unsupported with WinInet backend");
  }

  request->m_connection = InternetConnectA(
      request->m_session,
      request->m_url.m_host.c_str(),
      request->m_url.m_port,
      "",
      "",
      INTERNET_SERVICE_HTTP,
      0,
      0);

  DWORD dwFlags = INTERNET_FLAG_NO_CACHE_WRITE;
  if (request->m_url.m_scheme == "https")
    dwFlags |= INTERNET_FLAG_SECURE;

  request->m_httpFile = HttpOpenRequestA(
      request->m_connection,
      request->m_method.c_str(),
      request->m_url.m_path.c_str(),
      nullptr,
      nullptr,
      nullptr,
      dwFlags,
      0);

  uint8_t buffer[BUFFERSIZE];

  if (!HttpSendRequestA(request->m_httpFile, nullptr, 0, 0, 0))
  {
    auto err = GetLastError();
    printf("HttpSendRequest error : (%lu)\n", err);
    return false;
  }

  while (!request->complete())
  {
    DWORD bytesReadNow = 0;

    // Now read the response data.
    bool bRead = InternetReadFile(
        request->m_httpFile,
        buffer,
        BUFFERSIZE,
        &bytesReadNow);

    if (!bRead)
    {
      return false;
    }

    request->_appendContent(buffer, bytesReadNow);

    request->m_bytesRead += bytesReadNow;

    if (bytesReadNow == 0)
    {
      DWORD statusCode = 0;
      DWORD length = sizeof(DWORD);
      HttpQueryInfo(
          request->m_httpFile,
          HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
          &statusCode,
          &length,
          NULL);
      request->m_protect.lock();
      request->m_complete = true;
      request->m_statusCode = statusCode;
      request->m_protect.unlock();
      return 0;
    }
  }

  return 0;
}
#endif
} // namespace web
