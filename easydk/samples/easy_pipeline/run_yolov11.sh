#! /bin/bash
CURRENT_DIR=$(dirname $(readlink -f $0) )

pushd $CURRENT_DIR

mkdir -p cache
mkdir -p output

PrintUsages(){
    echo "Usages: run.sh [ax650]"
}

if [ $# -ne 1 ]; then
    PrintUsages
    exit 1
fi

MODEL_DIR=${CURRENT_DIR}/../data/model
if [[ ${1} == "ax650" ]]; then
    MODEL_PATH="${MODEL_DIR}/yolo11x.axmodel"
	LABEL_PATH="${MODEL_DIR}/label_yolo11x.txt"
else
    PrintUsages
    exit 1
fi

DATA_PATH="${CURRENT_DIR}/../data/videos/cars.mp4"

./bin/stream_app  \
     --model_path $MODEL_PATH \
	 --label_path $LABEL_PATH \
     --data_path $DATA_PATH \
     --model_name "yolov11" \
     --input_number 1 \
     --output_name "output/yolov11_output.h265" \
     --device_id 0 \
     --frame_rate 30  \
     --colorlogtostderr \
     --alsologtostderr
popd
