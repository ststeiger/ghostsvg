#include "RenderedPage.h"
#include <stdlib.h>

RenderedPage::RenderedPage(void)
{
}

RenderedPage::~RenderedPage(void)
{
}


void RenderedPage::DestroyPage(void)
{

    free(this->data);

}


void RenderedPage::CreatePage(unsigned char *data,int width, 
                              int height, int number_channels, double scale)
{

    /* Data already created, just storing in structure */

    this->data = data;
    this->width = width;
    this->height = height;
    this->number_channels = number_channels;
    this->scale = scale;

}



    double RenderedPage::GetScale()
    {
        
        return(this->scale);

    }
    
    void RenderedPage::SetScale(double scale)
    {
        
        this->scale = scale ;

    }

    int RenderedPage::GetWidth()
    {

        return(this->width);
    }

    void RenderedPage::SetWidth(int width)
    {

        this->width = width;
    }


    int RenderedPage::GetHeight()
    {

        return(this->height);
    }

    void RenderedPage::SetHeight(int height)
    {

        this->height = height;
    }

    bool RenderedPage::HasTransparency()
    {

        return(this->transparency);


    }
    
    void RenderedPage::SetHasTransparency(bool trans_set)
    {

        this->transparency = trans_set;


    }


    bool RenderedPage::IsFullPage()
    {

        return(this->fullpage);

    }


    void RenderedPage::SetIsFullPage(bool fullpage)
    {

        this->fullpage = fullpage;

    }

