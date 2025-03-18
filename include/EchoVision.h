#ifndef MAIN_H
#define MAIN_H

#ifdef __X86_64
#define CAM_ID 2
#define __VISUAL

#else   // AARCH64

#define CAM_ID 0
#define __VISUAL
#endif

#define CAM_WIDTH 3840
#define CAM_HEIGHT 1080
#define CAM_FPS 30
#define PICTURE_DIR "./SaveImage/"
#define VIDEO_DIR "./SaveVideo/"

#define APP_SERVICE_INIT(func) int main(void){func();}

#endif // MAIN_H
