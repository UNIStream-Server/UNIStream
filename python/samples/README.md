# UNIStream Python API Samples

Two application samples are provided. ``${UNISTREAM_DIR}`` represents UNIStream source directory.

- pyunistream_demo.py, represents how to build and start a pipeline, add and remove resources and so on.
- yolov3_detector.py, provides a demonstration of how to customize preprocessing and postprocessing in python and set it to the Inferencer module.

Before run the samples, please make sure UNIStream Python API is compiled and installed. And the requirements are meet. Execute the following commands to install the dependencies.

```sh
cd {UNISTREAM_DIR}/python
pip install -r requirement.txt
```

## Pyunistream Demo

The source code is ``{UNISTREAM_DIR}/python/samples/pyunistream_demo.py`` .

The configuration file is ``{UNISTREAM_DIR}/python/samples/python_demo_config.json`` .

**How to run:**

```sh
cd {UNISTREAM_DIR}/python/samples/
python pyunistream_demo.py
```

## Yolov3 Detector

The source code is ``{UNISTREAM_DIR}/python/samples/yolov3_detector.py`` .

The configuration file is ``{UNISTREAM_DIR}/python/samples/yolov3_detection_config.json`` .

**How to run:**

```sh
cd {UNISTREAM_DIR}/python/samples/
python yolov3_detector.py
```

