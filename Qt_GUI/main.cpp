/********************************************************************************
 An interactive demo application for our progressive Pyramid-based sampling
 Copyright 2020 Xin Chen <chenxin199634@gmail.com>

 Distributed under MIT license, or public domain
 if desired and recognized in your jurisdiction.
 See file LICENSE for detail.

  Files - Descriptions
* main.cpp - the entry file
* global.h - constants & type definitions
* utils.* - reading data & preprocessing
* qt_gui.* - window definition
*	SamplingProcessViewer.* - the graphic screen & create a sampling thread
*		samplingworker.* - invoking sampling methods in worker thread
*			HierarchicalSampling.* - our progressive pyramid-based sampling method (**)
*			AdaptiveBinningSampling.* - the kd-tree based sampling method (doi: 10.1109/TVCG.2019.2934541)
*				BinningTree.* - The tree structure of the kd-tree based sampling method
*			ReservoirSampling.* - the optimal reservoir sampling (see https://en.wikipedia.org/wiki/Reservoir_sampling#An_optimal_algorithm)
*			RandomSampling.* - the classic random sampling method implemented with std::shuffle()
*	ControlPanelWidget.* - displaying and setting parameters
*	DisplayPanelWidget.* - displaying class to color mapping

*********************************************************************************/

#include "qt_gui.h"
#include <QtWidgets/QApplication>

Param params = { 100000,0,6,6,10,0.1,0.2,0.25,false,1,30,false };
std::vector<int> selected_class_order{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19 };

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Qt_GUI w;
	w.show();
	return a.exec();
}
