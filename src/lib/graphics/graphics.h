
#pragma once


// copied from "Adafruit_GFX" library
class Graphics
{
public:
    Graphics(int16_t w, int16_t h);

protected:
    int16_t WIDTH;        ///< This is the 'raw' display width - never changes
    int16_t HEIGHT;       ///< This is the 'raw' display height - never changes
    int16_t _width;       ///< Display width as modified by current rotation
    int16_t _height;      ///< Display height as modified by current rotation
};
