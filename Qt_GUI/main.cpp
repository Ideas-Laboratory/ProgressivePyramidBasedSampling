#include "qt_gui.h"
#include <QtWidgets/QApplication>

Param params = { 100000,0,0.25,2,2,2,0.2,100,false };
std::vector<int> selected_class_order{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19 };
//std::vector<int> selected_class_order{ 0,6,7,10 };

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Qt_GUI w;
	w.show();
	return a.exec();
}
