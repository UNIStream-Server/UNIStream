#!/bin/bash
#*************************************************************************#
# @param
# input_url: video path or image url. for example: /path/to/your/video.mp4, /parth/to/your/images/%d.jpg
# dev_id: device odinal index. default value is 0
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source ${CURRENT_DIR}/../env.sh

$CURRENT_DIR/../bin/test_run_pipeline  \
  --input_url $CURRENT_DIR/../../data/videos/cars.mp4\
  --input_num 1 \
  --config_fname "./config.json" \
  --dev_id 0

