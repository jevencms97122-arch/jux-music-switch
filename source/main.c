#include <switch.h>

int main(int argc, char* argv[])
{
    Result rc = 0;
    WebCommonConfig config;
    WebCommonReply reply;

    romfsInit();

    rc = webOfflineCreate(&config, "romfs:/");
    if (R_SUCCEEDED(rc)) {
        webConfigSetWhitelist(&config, ".*");
        webConfigSetFooter(&config, false);
        webConfigSetBackForwardButton(&config, false);
        webConfigShow(&config, &reply);
    }

    romfsExit();
    return 0;
}
