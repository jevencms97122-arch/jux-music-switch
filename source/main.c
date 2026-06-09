#include <switch.h>

int main(int argc, char* argv[])
{
    Result rc = 0;
    WebCommonConfig config;
    WebCommonReply reply;
    const char* url = "http://188.115.125.74:3000/jux";

    // webSystemCreate uses a different applet path, more compatible with CFW
    rc = webSystemCreate(&config, url);
    if (R_FAILED(rc)) {
        // fallback to standard page
        rc = webPageCreate(&config, url);
        if (R_SUCCEEDED(rc))
            webConfigSetWhitelist(&config, ".*");
    }

    if (R_SUCCEEDED(rc)) {
        webConfigSetFooter(&config, false);
        // cast to int to hide URL bar without relying on undefined enum value
        webConfigSetDisplayUrlKind(&config, (WebDisplayUrlKind)1);
        webConfigShow(&config, &reply);
    }

    return 0;
}
