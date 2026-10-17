/* main.c - PS5GreenApple entry point */
#include "common.h"
#include "desktop.h"

int main(int argc, char **argv)
{
    if (video_init(argc, argv) != 0)
        return 1;
    desktop_init();
    while (!video_should_close()) {
        video_poll();
        desktop_frame();
        video_present();
    }
    video_shutdown();
    return 0;
}
