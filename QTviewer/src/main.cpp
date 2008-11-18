#include <QApplication>
#include "ui_GSViewer.h"
#include "GhostViewer.h"

 int main(int argc, char *argv[])
 {

     QApplication app(argc, argv);
     Ui_GhostViewer uiMainWindow;
     QMainWindow MainWindow;

     uiMainWindow.setupUi(&MainWindow);
     
     /* Create our GhostViewer Widget object */

     GhostViewer Viewer;

     /* Connect ui signals to viewer slots */

     Viewer.SetupConnections(&uiMainWindow);

     MainWindow.show();

     return app.exec();



 }