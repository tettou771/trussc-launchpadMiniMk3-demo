#include "TrussC.h"
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.title = "tcxMidi - Launchpad Mini Mk3";
    settings.width = 600;
    settings.height = 640;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
