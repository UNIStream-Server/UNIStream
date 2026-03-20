#!/bin/bash
#*************************************************************************#
# @param
# input_url: video path or image url. for example: /path/to/your/video.mp4, /parth/to/your/images/%d.jpg
# how_to_show: [image] means dump as images, [video] means dump as video (output.avi),
#              default value is [video]
# output_dir: where to store the output file. default value is "./"
# output_frame_rate: output frame rate, valid when [how_to_show] set to [video]. default value is 25
# dev_id: device odinal index. default value is 0
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source ${CURRENT_DIR}/../env.sh

OUTPUT_DIR=${CURRENT_DIR}/output
mkdir -p ${OUTPUT_DIR}

$CURRENT_DIR/../bin/single_decode_pipeline  \
  --input_url $CURRENT_DIR/../../data/videos/cars.mp4 \
  --input_num 1 \
  --how_to_show video  \
  --output_dir ${OUTPUT_DIR} \
  --dev_id 0

