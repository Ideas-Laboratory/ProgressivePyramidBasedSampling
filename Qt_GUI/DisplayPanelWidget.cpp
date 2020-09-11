#include "DisplayPanelWidget.h"

using namespace std;

DisplayPanelWidget::DisplayPanelWidget(SamplingProcessViewer *spv, QWidget * parent) : QWidget(parent), viewer(spv)
{
	setFixedWidth(440);
	container = dynamic_cast<QScrollArea*>(parent);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setAlignment(Qt::AlignTop);
	setLayout(layout);

	connect(spv, &SamplingProcessViewer::classChanged, [this](const std::unordered_map<uint, std::string>* class2label) {
		clearAllChildren();
		addClassToColorMappingTable(class2label);
	});
}

void DisplayPanelWidget::resizeEvent(QResizeEvent * e)
{
	QWidget::resizeEvent(e);
	container->verticalScrollBar()->setValue(height());
}

void DisplayPanelWidget::addClassToColorMappingTable(const std::unordered_map<uint, std::string>* class2label)
{
	QGroupBox *mapping_box = new QGroupBox("Class to Color mapping:", this);
	QVBoxLayout *box_layout = new QVBoxLayout(mapping_box);
	mapping_box->setLayout(box_layout);
	QTableWidget *tw = new QTableWidget(this);
	tw->setShowGrid(false);
	tw->setEditTriggers(QAbstractItemView::NoEditTriggers);
	tw->verticalHeader()->setVisible(false);

	tw->setRowCount(class2label->size());
	tw->setColumnCount(2);
	tw->setHorizontalHeaderLabels(QStringList{ "Class", "Color" });
	tw->setColumnWidth(0, 300);
	tw->setColumnWidth(1, 70);
	int rows = 0;
	for (auto &pr : *class2label) {
		QTableWidgetItem *newItem = new QTableWidgetItem(tr(pr.second.c_str()));
		tw->setItem(rows, 0, newItem);
		newItem = new QTableWidgetItem();
		newItem->setBackground(this->viewer->getColorBrushes()[pr.first]);
		tw->setItem(rows, 1, newItem);
		++rows;
	}
	tw->setRowCount(rows);

	box_layout->addWidget(tw);
	mapping_box->setFixedHeight(tw->horizontalHeader()->height() + 60 + tw->rowHeight(0)*rows);
	this->layout()->addWidget(mapping_box);
}

void DisplayPanelWidget::clearAllChildren()
{
	for (int i = this->layout()->count() - 1; i > -1; --i) {
		QWidget *w = this->layout()->itemAt(i)->widget();
		this->layout()->removeWidget(w);
		w->close();
	}
}

