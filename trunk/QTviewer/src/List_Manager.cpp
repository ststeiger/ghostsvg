#include "List_Manager.h"
#include "RenderedPage.h"
#include "viewtypes.h"

List_Manager::List_Manager(void)
{

    /* Initialize the list */
    
    this->page->prevpageobject = NULL;
    this->page->nextpageobject = NULL;
        
}

List_Manager::~List_Manager(void)
{
 
    /* Destroy entire list */

    


}


void List_Manager::AddObject(RenderedPage *renderedpage, int pagenumber, double scale)
{

    /* Add item to end of list */

    pageobject *new_object;

    pageobject *end_object = this->GetLastObject(); 
    new_object = new pageobject;

    new_object->renderedpage = renderedpage;
    new_object->pagenumber = pagenumber;
    new_object->renderedpage->SetScale(scale);

    end_object->nextpageobject = new_object;
    new_object->prevpageobject = end_object;

}


void List_Manager::DeleteObject()
{

 /* Remove item from list.  DeAllocate rendered page memory */
 /* Delete the first in the list as we add them at the end */

    pageobject *first_object = this->GetFirstObject(); 

    if (first_object != NULL)
    {
        /* Need to do some checking here */

        pageobject *prev_object = first_object->prevpageobject;
        pageobject *next_object = first_object->nextpageobject;

        /* Update the list connections */
        prev_object->nextpageobject = next_object;
        next_object->prevpageobject = prev_object;

        /* Destroy the page */
        first_object->renderedpage->DestroyPage();

    }

}

List_Manager::pageobject *List_Manager::GetFirstObject()
{
        
    pageobject *prev_object = this->page->prevpageobject;
    while (prev_object != NULL)
    {

        if (prev_object->prevpageobject == NULL)
        {
            return(prev_object);

        } else {

            prev_object = prev_object->prevpageobject;

        }
        
     }

     return(page);

}


List_Manager::pageobject *List_Manager::GetLastObject(void)
{
        
   pageobject *next_object = this->page->nextpageobject;
    while (next_object != NULL)
    {

        if (next_object->nextpageobject == NULL)
        {
            return(next_object);

        } else {

            next_object = next_object->nextpageobject;

        }
        
     }

     return(page);

}




RenderedPage *List_Manager::Search(int pagenumber, double scale)
{

/* Search list for page number at specific scale */

    /* Check current */

    if (this->page->pagenumber == pagenumber && scale == this->page->renderedpage->GetScale() )
    {
        /* Current node is desired one */
        return(this->page->renderedpage);

    } else {

        /* Go forward */
        
        pageobject *next_object = this->page->nextpageobject;
        while (next_object != NULL)
        {
            
            if (next_object->pagenumber == pagenumber && scale == next_object->renderedpage->GetScale() )
            {

                return(next_object->renderedpage);

            } else {

                next_object = next_object->nextpageobject;

            }
        }

        /* Did not find one going forward */

       /* Go backward */
        
        pageobject *prev_object = this->page->prevpageobject;
        while (prev_object != NULL)
        {
            
            if (prev_object->pagenumber == pagenumber && scale == prev_object->renderedpage->GetScale() )
            {

                return(prev_object->renderedpage);

            } else {

                prev_object = prev_object->prevpageobject;

            }
        }

        /* Nothing found */
        return(NULL);


    }

}




RenderedPage *List_Manager::Search(int pagenumber)
{

/* Search list for page number any scale */

/* Search list for page number at specific scale */

    /* Check current */

    if (this->page->pagenumber == pagenumber)
    {
        /* Current node is desired one */
        return(this->page->renderedpage);

    } else {

        /* Go forward */
        
        pageobject *next_object = this->page->nextpageobject;
        while (next_object != NULL)
        {
            
            if (next_object->pagenumber == pagenumber)
            {

                return(next_object->renderedpage);

            } else {

                next_object = next_object->nextpageobject;

            }
        }

        /* Did not find one going forward */

       /* Go backward */
        
        pageobject *prev_object = this->page->prevpageobject;
        while (prev_object != NULL)
        {
            
            if (prev_object->pagenumber == pagenumber)
            {

                return(prev_object->renderedpage);

            } else {

                prev_object = prev_object->prevpageobject;

            }
        }

        /* Nothing found */
        return(NULL);

    }
}

