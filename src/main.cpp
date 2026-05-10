#include "Application.h"

int main(int argc, char* argv[])
{
    Application app;

    if (const auto exitCode = app.parseArgs(argc, argv)) {
        return *exitCode;
    }

    return app.run();
}
