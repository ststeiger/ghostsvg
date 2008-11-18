#pragma once

#include "geometry.h"

class RenderedPage
{
public:

    RenderedPage(void);
    ~RenderedPage(void);

    void DestroyPage(void);
    void CreatePage(unsigned char *data,int width, int height, int number_channels, double scale);
    double GetScale();
    int GetWidth();
    int GetHeight();
    bool HasTransparency();
    bool IsFullPage();
    void SetScale(double);
    void SetWidth(int);
    void SetHeight(int);
    void SetHasTransparency(bool);
    void SetIsFullPage(bool);


private:

    double scale;

    int width;
    int height;
    int number_channels;
    bool transparency;

    bool fullpage;
    rect partialrect;

    unsigned char  *data;

};
