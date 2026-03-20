#!/bin/bash
#*************************************************************************#
# @param
# input_url: video path or image url. for example: /path/to/your/video.mp4, /parth/to/your/images/%d.jpg
# how_to_show: [image] means dump as images, [video] means dump as video (output.avi),
#              default value is [video]
# output_dir: where to store the output file. default value is "./"
# output_frame_rate: output frame rate, valid when [how_to_show] set to [video]. default value is 25
# label_path: label path
# model_path: your offline model path
# func_name:  function name in your model, default value is [subnet0]
# pad_value: pad values in format "128, 128, 128" in model input pixel format order,
#            default value is "114, 114, 114"
# mean_value: mean values in format "100.0, 100.0, 100.0" in model input pixel format order,
#             default value is "0, 0, 0"
# std: std values in format "100.0, 100.0, 100.0", in model input pixel format order,
#      default value is "1.0, 1.0, 1.0"
# dev_id: device odinal index. default value is 0
#*************************************************************************#

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE[0]});pwd)
source ${CURRENT_DIR}/../env.sh

OUTPUT_DIR=${CURRENT_DIR}/output
mkdir -p ${OUTPUT_DIR}

$CURRENT_DIR/../bin/osd_test_pipeline  \
  --input_url rtsp://admin:Admin123.@172.20.209.45:554/unicast/c1/s0/live \
  --input_num 1 \
  --how_to_show video  \
  --output_dir ${OUTPUT_DIR} \
  --dev_id 0

