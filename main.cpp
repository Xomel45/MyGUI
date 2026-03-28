#include "src/App.h"

int main()
{
    App app;
    if (!app.init())
        return 1;
    app.run();
    app.shutdown();
    return 0;
}
