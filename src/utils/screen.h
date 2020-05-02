#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

	void screenFlip();
	void screenFill(u8 r, u8 g, u8 b, u8 a);
	void screenClear();
	void screenInit();
	void screenPrint(char *fmt, ...);
	int screenGetPrintLine();
	void screenSetPrintLine(int line);
	int screenPrintAt(int x, int y, char *fmt, ...);

	typedef enum OSScreenID
	{
		SCREEN_TV = 0,
		SCREEN_DRC = 1,
	} OSScreenID;

#ifdef __cplusplus
}
#endif