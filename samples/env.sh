#!/bin/bash

# Current file's directory
ENV_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)

# UNIStream project directory
UNISTREAM_DIR=${ENV_DIR}/..

# sample directory which includes several demos
SAMPLES_DIR=${UNISTREAM_DIR}/samples

# the models used by demos, downloaded from offline model zoo if not exist
MODELS_DIR=${UNISTREAM_DIR}/data/models

# JSON configuration files
CONFIGS_DIR=${SAMPLES_DIR}/uni_launcher/configs
