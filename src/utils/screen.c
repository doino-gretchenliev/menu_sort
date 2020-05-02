#include "dynamic_libs/os_functions.h"
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include "screen.h"

static int current_line = 0;
int buf0_size, buf1_size;

void screenFlip()
{
    //Flush the cache
    DCFlushRange((void *)0xF4000000 + buf0_size, buf1_size);
    DCFlushRange((void *)0xF4000000, buf0_size);
    //Flip the buffer
    OSScreenFlipBuffersEx(0);
    OSScreenFlipBuffersEx(1);
}

void screenFill(u8 r, u8 g, u8 b, u8 a)
{
    u32 color = (r << 24) | (g << 16) | (b << 8) | a;
    OSScreenClearBufferEx(0, color);
    OSScreenClearBufferEx(1, color);
}

void screenClear()
{
    screenFill(0, 0, 0, 0);
    screenFlip();
    screenFill(0, 0, 0, 0);
    screenFlip();
}

void screenInit()
{
    OSScreenInit();
    //Grab the buffer size for each screen (TV and gamepad)
    buf0_size = OSScreenGetBufferSizeEx(SCREEN_TV);
    buf1_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
    //Set buffer to some free location in MEM1
    OSScreenSetBufferEx(SCREEN_TV, (void *)0xF4000000);
    OSScreenSetBufferEx(SCREEN_DRC, ((void *)0xF4000000 + buf0_size));
    //Enable the screen
    OSScreenEnableEx(SCREEN_TV, 1);
    OSScreenEnableEx(SCREEN_DRC, 1);
}

void screenPrint(char *fmt, ...)
{
    if (current_line == 16)
    {
        screenClear();
        current_line = 0;
    }
    char *tmp = NULL;
    va_list va;
    va_start(va, fmt);
    if ((vasprintf(&tmp, fmt, va) >= 0) && tmp)
    {
        OSScreenPutFontEx(1, 0, current_line, tmp);
        screenFlip();
        OSScreenPutFontEx(1, 0, current_line, tmp);
        screenFlip();
    }
    va_end(va);
    if (tmp)
        free(tmp);
    current_line++;
}

int screenGetPrintLine()
{
    return (current_line - 1);
}

void screenSetPrintLine(int line)
{
    current_line = line;
}

int screenPrintAt(int x, int y, char *fmt, ...)
{
    char *tmp = NULL;
    va_list va;
    va_start(va, fmt);
    if ((vasprintf(&tmp, fmt, va) >= 0) && tmp)
    {
        OSScreenPutFontEx(1, x, y, tmp);
        screenFlip();
        OSScreenPutFontEx(1, x, y, tmp);
        screenFlip();
    }
    va_end(va);
    if (tmp)
        free(tmp);
}
