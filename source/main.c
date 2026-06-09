#include <switch.h>

int main(int argc, char* argv[])
{
    Result rc = 0;
    WebCommonConfig config;
    WebCommonReply reply;

    rc = webPageCreate(&config, "http://188.115.125.74:3000/jux");
    if (R_SUCCEEDED(rc)) {
        webConfigSetWhitelist(&config, ".*");
        webConfigShow(&config, &reply);
    }

    return 0;
}
