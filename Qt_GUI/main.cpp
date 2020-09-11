/********************************************************************************
 An interactive demo application for our progressive Haar wavelet based sampling
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
*			HaarWaveletSampling.* - our progressive wavelet based sampling method (**)
*			AdaptiveBinningSampling.* - the kd-tree sampling method
*				BinningTree.* - The tree structure of the kd-tree method 
*	ControlPanelWidget.* - displaying and setting parameters
*	DisplayPanelWidget.* - displaying class to color mapping

*********************************************************************************/

#include "qt_gui.h"
#include <QtWidgets/QApplication>

Param params = { 100000,1,0.25,2,2,2,0.2,100,false };
std::vector<int> selected_class_order{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19 };

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Qt_GUI w;
	w.show();
	return a.exec();
}
