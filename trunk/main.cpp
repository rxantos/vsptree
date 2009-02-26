#include <QApplication>
#include "VSPTree.h"



int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
	QString argument;
	if ( argc > 1 )
	{
		argument = QString(argv[1]);
	}
    VSPTree VSPTree(app.applicationDirPath(),argument);
    return app.exec();
}
