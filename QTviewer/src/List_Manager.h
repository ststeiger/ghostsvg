#pragma once

#include "RenderedPage.h"

class List_Manager
{
public:
    List_Manager(void);
    ~List_Manager(void);

    struct pageobject
    {
        RenderedPage *renderedpage;
        pageobject *nextpageobject;
        pageobject *prevpageobject;
        int pagenumber;

    }*page;

private:

    void AddObject(RenderedPage *renderedpage, int pagenumber, double scale);
    void DeleteObject();
    RenderedPage *Search(int pagenumber, double scale);
    RenderedPage *Search(int pagenumber);
    pageobject *GetFirstObject();
    pageobject *GetLastObject();

};
