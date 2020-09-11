# Wavelet-based Scatterplots Sampling for Progressive Visualization

This is the interactive demo application for our algorithm.

You can click the "Select" button to load a dataset and click the "Start" button to start the progressive sampling.
If you want to review a specific frame after the sampling process, please first set the frame ID and then click the "Show" button.

<img src="figures/app.png?raw=true" alt="Screenshot of the application.">

### Results

<img src="figures/results.png" alt="Our sampling results of arXiv dataset.">

The sampling results of the dataset of [the UMAP projection](https://umap-learn.readthedocs.io/en/latest/) of [the arXiv repository](https://arxiv.org/).
(a) The origin scatterplot (left) and its translucent version with an identical opacity (right). (b) Same as (a), but colored by the color palette of 
[Paperscape](https://paperscape.org/). (c) The final result of our method with parameters sz = 2, stopLevel = 2, λ = 0.2, ε = 0.25. (d, e) The 1st to
4th frames and the last frame of our static sampling and the progressive version, respectively. Each data chunk contains 10k points. Blue indicates
the unchanged points, orange for newly entered ones and green for exited ones. Below the plots are the accumulative runtime since the algorithms began.

### Abstract
Visualizing large amounts of data using scatterplots needs to resort to some analytical data reduction method to avoid or
control overplotting. We introduce a Wavelet-based method that provides results as good as existing state-of-the-art but 10 times faster.
We also adapt our method to allow progressive visualization to be used when the data to visualize arrives by chunks, either because it is
loaded from a bandwidth-limiting channel or because it is generated on the fly by a progressive data analysis system. Our progressive
method computes new frames fast and computes the image parts that should be redrawn, further accelerating the visualization. We
validate our approach by evaluating static scatterplot images with existing quality metrics, but also by analyzing the stability of the
progressively generated frames. Our Haar-wavelet based sampling method is effective at supporting the interactive visualization of
several millions of data points in static and progressive systems.

### Dependencies
The following libraries are required:
* Qt5Core
* Qt5GUI
* Qt5Widgets
* Qt5Svg
* Qt5PrintSupport
