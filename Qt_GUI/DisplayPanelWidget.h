#pragma once
#include <QtWidgets>

#include "global.h"
#include "SamplingProcessViewer.h"

extern std::vector<int> selected_class_order;

class DisplayPanelWidget : public QWidget
{
public:
	explicit DisplayPanelWidget(SamplingProcessViewer* viewer, QWidget* parent = nullptr);

protected:
	void resizeEvent(QResizeEvent* e);

private:
	void addClassToColorMappingTable(const std::unordered_map<uint, std::string>* class2label);

	void clearAllChildren();
	SamplingProcessViewer* viewer;
	QScrollArea* container;
};