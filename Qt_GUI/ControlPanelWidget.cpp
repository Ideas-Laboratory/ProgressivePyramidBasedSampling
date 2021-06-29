#include "ControlPanelWidget.h"

using namespace std;

ControlPanelWidget::ControlPanelWidget(SamplingProcessViewer* viewer, QWidget* parent) : QWidget(parent), viewer(viewer)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	setLayout(layout);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	{// point related stuff
		QGroupBox* point_group = new QGroupBox("Point Settings:", this);
		QLabel* point_size_label = new QLabel("Point radius:", this);
		QSpinBox* spin_point_size = new QSpinBox(this);
		spin_point_size->setRange(1, 100000);
		spin_point_size->setValue(params.point_radius);
		spin_point_size->setToolTip(
			"The radius of point, used in the rendering procedure.");
		connect(spin_point_size, QOverload<int>::of(&QSpinBox::valueChanged),
			[this](int value) { params.point_radius = value; this->viewer->redrawPoints(); });

		QGridLayout* point_group_layout = new QGridLayout(point_group);
		point_group->setLayout(point_group_layout);
		point_group_layout->addWidget(point_size_label, 0, 0);
		point_group_layout->addWidget(spin_point_size, 0, 1);

		layout->addWidget(point_group);
	}

	{// algo related stuff
		QGroupBox* algorithm_group = new QGroupBox("Algorithm Settings:", this);

		QLabel* grid_width_label = new QLabel("Grid width:", this);
		QSpinBox* spin_grid_width = new QSpinBox(this);
		spin_grid_width->setRange(1, 100);
		spin_grid_width->setValue(params.grid_width);
		spin_grid_width->setToolTip(
			"The width of uniform grid, used in the sampling procedure.");
		connect(spin_grid_width, QOverload<int>::of(&QSpinBox::valueChanged),
			[this](int value) { params.grid_width = value; this->viewer->gridWidthChanged(true); });

		QLabel* frame_id_label = new QLabel("Frame ID:", this);
		QSpinBox* spin_frame_id = new QSpinBox(this);
		if (params.is_streaming)
			spin_frame_id->setRange(1, INT_MAX);
		else
			spin_frame_id->setRange(1, this->viewer->getPointNum() / params.chunk_size + 1);
		spin_frame_id->setValue(params.displayed_frame_id + 1);
		spin_frame_id->setToolTip(
			"You can change it and use the \"Show\" button to display the result of a previous frame.");
		connect(spin_frame_id, QOverload<int>::of(&QSpinBox::valueChanged),
			[](int value) { params.displayed_frame_id = value - 1; });
		connect(viewer, &SamplingProcessViewer::finished, [this, spin_frame_id]() {
			if (!params.is_streaming) spin_frame_id->setMaximum(200);
			else spin_frame_id->setMaximum(INT_MAX);
		});
		connect(viewer, &SamplingProcessViewer::frameChanged, spin_frame_id, &QSpinBox::setValue);

		QLabel* batch_label = new QLabel("Chunk size:", this);
		QSpinBox* spin_batch = new QSpinBox(this);
		spin_batch->setRange(1, 2000000);
		spin_batch->setValue(params.chunk_size);
		spin_batch->setToolTip(
			"The number of points loaded at one time.");
		connect(spin_batch, QOverload<int>::of(&QSpinBox::valueChanged), [this, spin_frame_id](int value) {
			params.chunk_size = value;
			auto& result = div((int)this->viewer->getPointNum(), params.chunk_size);
			spin_frame_id->setMaximum(result.rem == 0 ? result.quot : result.quot + 1);
		});
		
		QLabel* step_label = new QLabel("Time step:", this);
		QSpinBox* spin_step = new QSpinBox(this);
		spin_step->setToolTip(
			"The size of time step used in streaming setting, unit is day.");
		spin_step->setValue(params.time_step);
		connect(spin_step, QOverload<int>::of(&QSpinBox::valueChanged),
			[this](int value) { params.time_step = value;	});

		QLabel* window_label = new QLabel("Window size:", this);
		QSpinBox* spin_window = new QSpinBox(this);
		spin_window->setToolTip(
			"The size of time window used in streaming setting, unit is day.");
		spin_window->setValue(params.time_window);
		connect(spin_window, QOverload<int>::of(&QSpinBox::valueChanged),
			[this](int value) { params.time_window = value;	});

		QCheckBox* streaming_option = new QCheckBox("Streaming data", this);
		streaming_option->setChecked(params.is_streaming);
		connect(streaming_option, &QCheckBox::clicked,
			[this](bool value) { params.is_streaming = value; });

		QLabel* stop_level_label = new QLabel("Stop level:", this);
		QSpinBox* spin_stop_level = new QSpinBox(this);
		spin_stop_level->setToolTip(
			"Control the observed level of details in the sampling result.");
		spin_stop_level->setValue(params.stop_level);
		connect(spin_stop_level, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
			params.stop_level = value;
		});

		QLabel* density_threshold_label = new QLabel("Density threshold:", this);
		QDoubleSpinBox* spin_density_threshold = new QDoubleSpinBox(this);
		spin_density_threshold->setToolTip(
			"Determine which regions are classified as low-density.");
		spin_density_threshold->setDecimals(4);
		spin_density_threshold->setRange(0.0, 1.0);
		spin_density_threshold->setValue(params.density_threshold);
		connect(spin_density_threshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			[this](double value) { params.density_threshold = value; });

		QLabel* outlier_weight_label = new QLabel("Outlier weight:", this);
		QDoubleSpinBox* spin_outlier_weight = new QDoubleSpinBox(this);
		spin_outlier_weight->setToolTip(
			"Determine how many data points in low-density regions we want to keep.");
		spin_outlier_weight->setDecimals(4);
		spin_outlier_weight->setRange(0.0, 1.0);
		spin_outlier_weight->setSingleStep(0.1);
		spin_outlier_weight->setValue(params.outlier_weight);
		connect(spin_outlier_weight, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			[this](double value) { params.outlier_weight = value; });

		QLabel* eps_label = new QLabel("Ratio threshold:", this);
		QDoubleSpinBox* spin_eps = new QDoubleSpinBox(this);
		spin_eps->setToolTip(
			"Influence the temporal coherence between successive frames.");
		spin_eps->setDecimals(2);
		spin_eps->setRange(0.0, 1.0);
		spin_eps->setValue(params.ratio_threshold);
		connect(spin_eps, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			[this](double value) { params.ratio_threshold = value; });

		QGridLayout* algoGroupLayout = new QGridLayout(algorithm_group);
		algorithm_group->setLayout(algoGroupLayout);
		algoGroupLayout->addWidget(grid_width_label, 0, 0);
		algoGroupLayout->addWidget(spin_grid_width, 0, 1);
		algoGroupLayout->addWidget(density_threshold_label, 1, 0);
		algoGroupLayout->addWidget(spin_density_threshold, 1, 1);
		algoGroupLayout->addWidget(outlier_weight_label, 2, 0);
		algoGroupLayout->addWidget(spin_outlier_weight, 2, 1);
		algoGroupLayout->addWidget(stop_level_label, 3, 0);
		algoGroupLayout->addWidget(spin_stop_level, 3, 1);
		algoGroupLayout->addWidget(eps_label, 4, 0);
		algoGroupLayout->addWidget(spin_eps, 4, 1);
		algoGroupLayout->addWidget(batch_label, 5, 0);
		algoGroupLayout->addWidget(spin_batch, 5, 1);
		algoGroupLayout->addWidget(frame_id_label, 6, 0);
		algoGroupLayout->addWidget(spin_frame_id, 6, 1);
		algoGroupLayout->addWidget(step_label, 7, 0);
		algoGroupLayout->addWidget(spin_step, 7, 1);
		algoGroupLayout->addWidget(window_label, 8, 0);
		algoGroupLayout->addWidget(spin_window, 8, 1);
		algoGroupLayout->addWidget(streaming_option, 9, 0, 1, -1);

		layout->addWidget(algorithm_group);
	}

	// open button
	QGroupBox* openBox = new QGroupBox("Open file as input:", this);
	QVBoxLayout* openBoxLayout = new QVBoxLayout(openBox);
	openBox->setLayout(openBoxLayout);
	QPushButton* fileButton = new QPushButton("Select", this);
	buttons.push_back(fileButton);
	openBoxLayout->addWidget(fileButton);
	layout->addWidget(openBox);

	// save buttons
	QGroupBox* saveGroup = new QGroupBox("Save as:", this);
	QGridLayout* saveLayout = new QGridLayout(saveGroup);
	QPushButton* save_PNG = new QPushButton("PNG", this);
	QPushButton* save_SVG = new QPushButton("SVG", this);
	QPushButton* save_PDF = new QPushButton("PDF", this);
	QPushButton* save_CSV = new QPushButton("CSV", this);
	save_CSV->setEnabled(false);

	saveLayout->addWidget(save_PNG, 0, 0); saveLayout->addWidget(save_SVG, 0, 1); saveLayout->addWidget(save_PDF, 0, 2); saveLayout->addWidget(save_CSV, 1, 0, 1, -1);
	buttons.push_back(save_PNG); buttons.push_back(save_SVG); buttons.push_back(save_PDF); buttons.push_back(save_CSV);
	saveGroup->setLayout(saveLayout);
	layout->addWidget(saveGroup);

	// start button
	QGroupBox* startGroup = new QGroupBox("Run Algorithm:", this);
	QVBoxLayout* startLayout = new QVBoxLayout(startGroup);

	QPushButton* startButton = new QPushButton("Start", this);
	buttons.push_back(startButton);
	startLayout->addWidget(startButton);

	QPushButton* showButton = new QPushButton("Show", this);
	buttons.push_back(showButton);
	startLayout->addWidget(showButton);

	startGroup->setLayout(startLayout);
	layout->addWidget(startGroup);

	{
		QGroupBox* fn_group = new QGroupBox("Filename Box", this);
		QLabel* filename_label = new QLabel("Filename:", fn_group);
		showCurrentFileName(MY_DATASET_FILENAME);
		QGridLayout* fnGroupLayout = new QGridLayout(fn_group);
		fn_group->setLayout(fnGroupLayout);
		fnGroupLayout->addWidget(filename_label, 0, 0);
		fnGroupLayout->addWidget(fn, 0, 1);
		layout->addWidget(fn_group);
	}

	connect(fileButton, &QPushButton::pressed, [this, save_CSV]() {
		QString path = QFileDialog::getOpenFileName(this, tr("Open Dataset"), QString(),
			tr("Comma-Separated Values Files (*.csv)"));

		if (path.isEmpty())
			return;

		save_CSV->setEnabled(false);
		QString fn = QFileInfo(path).fileName();
		showCurrentFileName(fn);

		this->viewer->setDataName(fn.split('.').front().toStdString());
		this->viewer->setDataPath(path.toStdString());
		});
	connect(save_PNG, &QPushButton::pressed, [this]() { showSaveDialog("Save Image as PNG", "PNG Image", "png", [this](const QString& path) { this->viewer->saveImagePNG(path); }); });
	connect(save_SVG, &QPushButton::pressed, [this]() { showSaveDialog("Save Image as SVG", "SVG Image", "svg", [this](const QString& path) { this->viewer->saveImageSVG(path); }); });
	connect(save_PDF, &QPushButton::pressed, [this]() { showSaveDialog("Save Image as PDF", "PDF", "pdf", [this](const QString& path) { this->viewer->saveImagePDF(path); }); });
	connect(save_CSV, &QPushButton::pressed, [this]() { showSaveDialog("Save Image as CSV", "Comma-Separated Values Files", "csv", [this](const QString& path) { this->viewer->writeResult(path); }); });
	connect(viewer, &SamplingProcessViewer::finished, [this]() {
		this->enableButtons();
		this->viewer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		});
	connect(startButton, &QPushButton::released, [this]() {
		this->disableButtons();
		this->viewer->sample();
		});
	connect(showButton, &QPushButton::released, [this]() {
		this->viewer->redrawPoints();
		});
	connect(viewer, &SamplingProcessViewer::classChanged, [this](const std::unordered_map<uint, std::string>* class2label) {
		this->addClassSelectionBox(class2label);
		});
}

void ControlPanelWidget::addClassSelectionBox(const std::unordered_map<uint, std::string>* class2label)
{
	if (class_selection_box) {
		class_selection_box->close();
		int i = layout()->count() - 1;
		layout()->removeWidget(layout()->itemAt(i)->widget()); // remove stretch
		layout()->removeWidget(layout()->itemAt(i - 1)->widget()); // remove box
		label2class.clear();
	}

	class_selection_box = new QGroupBox("Select Classes:", this);
	QVBoxLayout* vbox = new QVBoxLayout();
	for (auto& pr : *class2label) {
		QCheckBox* checkbox = new QCheckBox(pr.second.c_str(), class_selection_box);
		checkbox->setChecked(std::find(selected_class_order.begin(), selected_class_order.end(), pr.first) != selected_class_order.end());
		vbox->addWidget(checkbox);
		label2class.emplace(pr.second, pr.first);

		connect(checkbox, &QCheckBox::stateChanged, [this, pr](int state) {
			if (state == Qt::Unchecked) {
				selected_class_order.erase(std::find(selected_class_order.begin(), selected_class_order.end(), pr.first));
			}
			else {
				selected_class_order.push_back(pr.first);
			}
			this->viewer->redrawPoints();
			});
	}
	class_selection_box->setLayout(vbox);
	layout()->addWidget(class_selection_box);
	((QBoxLayout*)layout())->addStretch(1);
}

void ControlPanelWidget::showCurrentFileName(const QString& s)
{
	fn->setText(s);
}

void ControlPanelWidget::showSaveDialog(const string& caption, const string& filter_desc, const string& ext, function<void(const QString&)> doSomeStuff)
{
	QFileDialog dialog(this, tr(caption.c_str()), QString(), tr((filter_desc + " (*." + ext + ")").c_str()));
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setDefaultSuffix(ext.c_str());

	if (dialog.exec() == 0)
		return;

	QString path = dialog.selectedFiles().first();

	if (path.isEmpty())
		return;

	doSomeStuff(path);
}

void ControlPanelWidget::disableButtons()
{
	for (auto b : buttons)
		b->setEnabled(false);
}

void ControlPanelWidget::enableButtons()
{
	for (auto b : buttons)
		b->setEnabled(true);
}
