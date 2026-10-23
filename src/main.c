#include "setup.h"
#include "app_state.h"

int main(void)
{
    Global_Init();
    App_Init();

    while (1)
    {
        App_Step();
    }
}
