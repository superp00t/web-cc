# web-cc (Under Construction)

web.h is a single-header library you can embed in your C++ project to make asynchronous HTTP requests.

It is intended for use in single-threaded or graphical applications such as game engines, where blocking operations are unacceptable.

web.h supports three HTTP backends:
  - WinInet (Included with Windows)
  - libCURL
  - Emscripten Fetch (Used when compiling to WebAssembly, supported by any major web browser)

Tested on MinGW 64-bit.

## example.cc

```cpp

#include <stdio.h>
#include "web.h"

int main(int argc, char **argv)
{
	std::string url = "https://example.org/";

	if (argc > 1)
		url = std::string(argv[1]);

	// Allocate client
	web::Client client;

	// Optional User-Agent
	// web::Client client("custom user-agent here");

	// Optional SOCKS5 proxy (libCURL backend only)
	// web::Client client(WEB_UA_DEFAULT, "host:port");

	// web::Client will delete req once it is completed.
	// Don't "delete" it yourself!
	auto req = new web::Request("GET", url);

	// Don't access members of request after this function exits, as the class is deleted immediately after onComplete returns
	// To copy the data for use later:
	// 	request->content() will return a copy of the response body as a byte vector
	// And
	// 	request->text() will return a copy in the form of a string.
	req->onComplete([](web::Request *request)
	{
		printf("Server returned %d\n", request->status());
		printf("Body: %s\n", request->text().c_str());
	});

	client.addRequest(req);

	// This call to web::Client::active() is only here to make this program exit when the web::Request is completed.
	// In graphical applications, you should call perform() in the render loop unconditionally.
	while (client.active())
	{
		// req->onComplete will be 
		client.perform();
	}
}
```

### WinInet

`g++ example.cc -o example -lwininet`

### libCURL

`g++ example.cc -o example -DWEB_IMPL_CURL -lcurl`

### Emscripten

`emcc example.cc -o example`

