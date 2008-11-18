#include "GhostViewer.h"
#include "ui_GSViewer.h"
#include <QtGui>  /* For file I/O gui */

#include <spectre.h>
#include "spectre-utils.h"


GhostViewer::GhostViewer(void)
{


}



GhostViewer::~GhostViewer(void)
{
}

void GhostViewer::SetupConnections(Ui_GhostViewer *uiwindow)
{

    /* This connects the signals from the generated ui to the slots in GhostViewer */

    connect(uiwindow->action_Open,SIGNAL(triggered()),this,SLOT(openfile()));
    connect(uiwindow->action_Forward,SIGNAL(triggered()),this,SLOT(pageforward()));
    connect(uiwindow->action_Backward,SIGNAL(triggered()),this,SLOT(pagebackward()));
    connect(uiwindow->action_Zoom_In,SIGNAL(triggered()),this,SLOT(zoomin()));
    connect(uiwindow->actionZoom_Out,SIGNAL(triggered()),this,SLOT(zoomout()));

}


void GhostViewer::openfile()
{

 
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open PDF, PS, EPS or XPS file"),
                                          QDir::currentPath(),
                                          tr("PDL Files (*.ps *.pdf *.eps *.xps)"));

     if (fileName.isEmpty())
         return;


     /* Lets see if we can have libspectre render this file */





    
}


void GhostViewer::RenderDoc(QString *fileName)
{


	SpectreDocument      *document;
	SpectreRenderContext *rc;
	unsigned int          i;

	/* TODO: check argv */

	printf ("Testing libspectre version: %s\n", SPECTRE_VERSION_STRING);

       // const char *const_char_name = (const char *) (fileName->data);
	spectre_document_load (NULL, fileName->toLatin1());

/*	document = spectre_document_new ();
	spectre_document_load (document, argv[1]);
	if (spectre_document_status (document)) {
		printf ("Error loading document %s: %s\n", argv[1],
			spectre_status_to_string (spectre_document_status (document)));
		spectre_document_free (document);

		return 1;
	}

*/



}


void GhostViewer::pageforward()
{

}

void GhostViewer::pagebackward()
{


}

void GhostViewer::zoomin()
{


}

void GhostViewer::zoomout()
{



}
