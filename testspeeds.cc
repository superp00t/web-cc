#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <chrono>
#include "web.h"

#ifdef WEB_IMPL_EMSCRIPTEN
#include <emscripten.h>
#endif

uint64_t getTime()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void hexDump (const char * desc, const void * addr, const int len) {
    int i;
    unsigned char buff[17];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    else if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

// Allocate client
web::Client client(WEB_UA_DEFAULT);
uint64_t    lastIteration;
// If a call to perform() takes longer than this, it will reduce your application's QoE past a reasonable threshold.
uint64_t maxDelay = 1000 / 40;

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	printf("Using %s backend\n", WEB_IMPL);

	std::string url = "https://crypto.dog/websocket";

	if (argc > 1)
		url = std::string(argv[1]);

	// web::Client will delete once the request is completed.
	web::Request *req = new web::Request("GET", url);

	req->onComplete([](web::Request *request)
	{
		printf("Server returned %d\n", request->m_statusCode);
		hexDump(request->m_url.toString().c_str(), request->data(), request->m_content.size());
	});

	client.addRequest(req);

	lastIteration = getTime();

	printf("Acceptable delay: %llu milliseconds\n", maxDelay);

	// This call to web::Client::active() is only here to make this program exit when the web::Request is completed.
	// In graphical applications, you should call perform() in the render loop.
#ifdef WEB_IMPL_EMSCRIPTEN
    emscripten_set_main_loop([]() {
        if (!client.active())
        {
            emscripten_cancel_main_loop();
            return;
        }
#else
	while (client.active())
	{
#endif
		client.perform();
		uint64_t overrun;
		if ((overrun = (getTime() - lastIteration)) > maxDelay)
		{
			printf("web::Client %s took too much time to perform. (%llu milliseconds)\n", WEB_IMPL, overrun);
		}
		else
		{
			if (overrun > 0)
				printf("%s iteration within acceptable limits. (%llu milliseconds)\n", WEB_IMPL, overrun);
		}
		lastIteration = getTime();
#ifdef WEB_IMPL_EMSCRIPTEN
    }, 60, 0);
#else 
    }
#endif
}