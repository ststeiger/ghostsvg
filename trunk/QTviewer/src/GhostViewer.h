#pragma once

#include <QApplication>
#include <QWidget>
#include "ui_GSViewer.h"

class GhostViewer :public QWidget  /* Must inherit from QObject */
{
    Q_OBJECT  /* This is needed so that the meta-object compiler (moc) 
                 creates the needed files for these slots, which 
                 will receive signals from the ui */

    public slots:

        void openfile();
        void pageforward();
        void pagebackward();
        void zoomin();
        void zoomout();
        
        void RenderDoc(QString *fileName);


public:

    GhostViewer(void);
    ~GhostViewer(void);

    void SetupConnections(Ui_GhostViewer *uiwindow);

};
