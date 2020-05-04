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

#ifdef __EMSCRIPTEN__
#define WEB_IMPL_EMSCRIPTEN
#include <emscripten/fetch.h>
#endif

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
enum StatusCode : uint16_t
{
  // Internal
  ConnectionFailed = 0,
  CORSBlocked = 1,

  StatusContinue = 100,               // RFC 7231, 6.2.1
  SwitchingProtocols = 101,           // RFC 7231, 6.2.2
  Processing = 102,                   // RFC 2518, 10.1
  EarlyHints = 103,                   // RFC 8297
  OK = 200,                           // RFC 7231, 6.3.1
  Created = 201,                      // RFC 7231, 6.3.2
  Accepted = 202,                     // RFC 7231, 6.3.3
  NonAuthoritativeInfo = 203,         // RFC 7231, 6.3.4
  NoContent = 204,                    // RFC 7231, 6.3.5
  ResetContent = 205,                 // RFC 7231, 6.3.6
  PartialContent = 206,               // RFC 7233, 4.1
  MultiStatus = 207,                  // RFC 4918, 11.1
  AlreadyReported = 208,              // RFC 5842, 7.1
  IMUsed = 226,                       // RFC 3229, 10.4.1
  MultipleChoices = 300,              // RFC 7231, 6.4.1
  MovedPermanently = 301,             // RFC 7231, 6.4.2
  Found = 302,                        // RFC 7231, 6.4.3
  SeeOther = 303,                     // RFC 7231, 6.4.4
  NotModified = 304,                  // RFC 7232, 4.1
  UseProxy = 305,                     // RFC 7231, 6.4.5
  TemporaryRedirect = 307,            // RFC 7231, 6.4.7
  PermanentRedirect = 308,            // RFC 7538, 3
  BadRequest = 400,                   // RFC 7231, 6.5.1
  Unauthorized = 401,                 // RFC 7235, 3.1
  PaymentRequired = 402,              // RFC 7231, 6.5.2
  Forbidden = 403,                    // RFC 7231, 6.5.3
  NotFound = 404,                     // RFC 7231, 6.5.4
  MethodNotAllowed = 405,             // RFC 7231, 6.5.5
  NotAcceptable = 406,                // RFC 7231, 6.5.6
  ProxyAuthRequired = 407,            // RFC 7235, 3.2
  RequestTimeout = 408,               // RFC 7231, 6.5.7
  Conflict = 409,                     // RFC 7231, 6.5.8
  Gone = 410,                         // RFC 7231, 6.5.9
  LengthRequired = 411,               // RFC 7231, 6.5.10
  PreconditionFailed = 412,           // RFC 7232, 4.2
  RequestEntityTooLarge = 413,        // RFC 7231, 6.5.11
  RequestURITooLong = 414,            // RFC 7231, 6.5.12
  UnsupportedMediaType = 415,         // RFC 7231, 6.5.13
  RequestedRangeNotSatisfiable = 416, // RFC 7233, 4.4
  ExpectationFailed = 417,            // RFC 7231, 6.5.14
  Teapot = 418,                       // RFC 7168, 2.3.3
  MisdirectedRequest = 421,           // RFC 7540, 9.1.2
  UnprocessableEntity = 422,          // RFC 4918, 11.2
  Locked = 423,                       // RFC 4918, 11.3
  FailedDependency = 424,             // RFC 4918, 11.4
  TooEarly = 425,                     // RFC 8470, 5.2.
  UpgradeRequired = 426,              // RFC 7231, 6.5.15
  PreconditionRequired = 428,         // RFC 6585, 3
  TooManyRequests = 429,              // RFC 6585, 4
  RequestHeaderFieldsTooLarge = 431,  // RFC 6585, 5
  UnavailableForLegalReasons = 451,   // RFC 7725, 3
  InternalServerError = 500,          // RFC 7231, 6.6.1
  NotImplemented = 501,               // RFC 7231, 6.6.2
  BadGateway = 502,                   // RFC 7231, 6.6.3
  ServiceUnavailable = 503,           // RFC 7231, 6.6.4
  GatewayTimeout = 504,               // RFC 7231, 6.6.5
  HTTPVersionNotSupported = 505,      // RFC 7231, 6.6.6
  VariantAlsoNegotiates = 506,        // RFC 2295, 8.1
  InsufficientStorage = 507,          // RFC 4918, 11.5
  LoopDetected = 508,                 // RFC 5842, 7.2
  NotExtended = 510,                  // RFC 2774, 7
  NetworkAuthenticationRequired = 511 // RFC 6585, 6
};

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

#ifdef WEB_IMPL_WININET
DWORD WINAPI __WinInetPerformTask(Request *ptr);
#endif

#ifdef WEB_IMPL_CURL
size_t __CURLWriteFunction(uint8_t *ptr, size_t size, size_t nmemb, Request *request);
#endif

#ifdef WEB_IMPL_EMSCRIPTEN
void __EmscriptenOnSuccess(emscripten_fetch_t *fetch);
void __EmscriptenOnError(emscripten_fetch_t *fetch);
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

#ifdef WEB_IMPL_EMSCRIPTEN
  emscripten_fetch_attr_t m_fetch_attr;
#endif
  std::mutex m_protect;

  bool m_requestInit;
  uint64_t m_bytesRead;
  bool m_complete;
  // State returned from server
  StatusCode m_statusCode;
  std::vector<uint8_t> m_content;

  Request(
      std::string method,
      URL url) : m_method(method),
                 m_url(url),
                 m_complete(false),
                 m_requestInit(false),
                 m_protect(),
                 m_callback(nullptr)
  {

#ifdef WEB_IMPL_CURL
    m_curl = curl_easy_init();
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, __CURLWriteFunction);
    curl_easy_setopt(m_curl, CURLOPT_URL, m_url.toString().c_str());

    if (m_method == "GET")
      curl_easy_setopt(m_curl, CURLOPT_GET, 1L);
    else if (m_method == "POST")
      curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
    else
      curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, m_method.c_str());
#endif

#ifdef WEB_IMPL_EMSCRIPTEN
    memset(&m_fetch_attr, 0, sizeof(emscripten_fetch_attr_t));
    strcpy(m_fetch_attr.requestMethod, m_method.c_str());
    m_fetch_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    m_fetch_attr.onsuccess = __EmscriptenOnSuccess;
    m_fetch_attr.onerror = __EmscriptenOnError;
    m_fetch_attr.userData = (void *)this;
    emscripten_fetch(&m_fetch_attr, m_url.toString().c_str());
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

  StatusCode status()
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
          PANIC("CURL referred to non-existent request");
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
size_t __CURLWriteFunction(uint8_t *ptr, size_t size, size_t nmemb, Request *request)
{
  size_t realsize = size * nmemb;
  request->_appendContent(ptr, realsize);
  return realsize;
}
#endif

#ifdef WEB_IMPL_EMSCRIPTEN
void __EmscriptenOnSuccess(emscripten_fetch_t *fetch)
{
  Request *request = (Request *)(fetch->userData);
  request->_appendContent((uint8_t *)fetch->data, fetch->numBytes);
  request->m_complete = true;
  request->m_statusCode = (StatusCode)fetch->status;
  emscripten_fetch_close(fetch);
}

void __EmscriptenOnError(emscripten_fetch_t *fetch)
{
  Request *request = (Request *)(fetch->userData);
  request->m_complete = true;
  request->m_statusCode = (StatusCode)fetch->status;
  emscripten_fetch_close(fetch);
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
      request->m_statusCode = (StatusCode)statusCode;
      request->m_protect.unlock();
      return 0;
    }
  }

  return 0;
}
#endif
} // namespace web
