#include <switch.h>

int main(int argc, char* argv[])
{
    Result rc = 0;
    WebCommonConfig config;
    WebCommonReply reply;

    romfsInit();

    rc = webOfflineCreate(&config, "romfs:/index.html");
    if (R_SUCCEEDED(rc)) {
        webConfigSetWhitelist(&config, ".*");
        webConfigSetFooter(&config, false);
        webConfigSetBackForwardButton(&config, false);
        webConfigSetBootDisplayKind(&config, WebBootDisplayKind_Black);
        webConfigShow(&config, &reply);
    }

    romfsExit();
    return 0;
}
