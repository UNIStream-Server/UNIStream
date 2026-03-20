# UNIStream Python API #
UNIStream provides Python API gives python programmers access to some of the UNIStream C++ API. It helps python programmers develop application code based on UNIStream. Python API is based on pybind11 and depends on Python (3+).



UNIStream Python API mainly supports the following behaviors:

- Build pipelines with built-in modules.

- Start and stop pipelines.

- Add resources to DataSource module and remove resources while the pipeline is running.

- Get Stream Message from pipeline.

- Get UNIFrameInfoPtr after it is processed by a certain module or all modules in the pipeline.

- Write custom preprocessing and post processing and set it to Inferencer or Inferencer2 modules.

- Write custom module and insert it to pipelines.

- Get performance information.

  

## Getting started ##

UNIStream Python API is not compiled and installed by default. To compile and install Python API package, execute the following commands. ``${UNISTREAM_DIR}`` represents UNIStream source directory.

```sh
cd {UNISTREAM_DIR}/python
python setup.py install
```

 After that, run the following line by python interpreter to check if the package is installed successfully:

```python
import unistream
```


## Samples ##

Two application samples are provided, located at ``{UNISTREAM_DIR}/python/samples`` .

- pyunistream_demo.py, represents how to build and start a pipeline, add and remove resources and so on.
- yolov3_detector.py, provides a demonstration of how to customize preprocessing and postprocessing and set it to the Inferencer module.

Before run the samples, please install the dependencies by execute the following commands:

```sh
cd {UNISTREAM_DIR}/python
pip install -r requirement.txt
```


