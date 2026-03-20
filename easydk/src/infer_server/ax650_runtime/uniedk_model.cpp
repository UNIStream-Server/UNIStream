/*************************************************************************
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#include <cstring>
#include "glog/logging.h"

#include "uniedk_model.hpp"

static bool read_file(const char *fn, std::vector<unsigned char> &data)
{
    FILE *fp = fopen(fn, "r");
    if (fp != nullptr)
    {
        fseek(fp, 0L, SEEK_END);
        auto len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        data.clear();
        size_t read_size = 0;
        if (len > 0)
        {
            data.resize(len);
            read_size = fread(data.data(), 1, len, fp);
        }
        fclose(fp);
        return read_size == (size_t)len;
    }
	
    return false;
}

static inline int GetModelInfo(int grpid, axclrtEngineIOInfo io_info, axclrtEngineIO io, 
	                           std::vector<int64_t> *input_dims, std::vector<std::vector<int64_t>> *output_dims, 
	                           std::vector<uint64_t> *input_sizes, std::vector<uint64_t> *output_sizes, 
	                           std::vector<axclrtEngineDataLayout> *input_layouts, std::vector<axclrtEngineDataLayout> *output_layouts)
{
    auto inputNum = axclrtEngineGetNumInputs(io_info);
    auto outputNum = axclrtEngineGetNumOutputs(io_info);

	input_sizes->resize(inputNum);

    for (uint32_t i = 0; i < inputNum; i++)
    {
        auto bufSize = axclrtEngineGetInputSizeByIndex(io_info, grpid, i);
		input_sizes->at(i) = bufSize;
        axclError ret = 0;

        axclrtEngineIODims dims;
        ret = axclrtEngineGetInputDims(io_info, grpid, i, &dims);
        if (ret != 0)
        {
			LOG(ERROR) << "[EasyDK] [ax_model] GetModelInfo(): axclrtEngineGetInputDims fail:" << ret;
            return -1;
        }

		input_dims->resize(dims.dimCount);
		for (int32_t j = 0; j < dims.dimCount; j++)
	    {
	        input_dims->at(j) = dims.dims[j];
		}

		fprintf(stdout, "axclrtEngineGetInputDims dimCount[%d]\n", dims.dimCount);
		fprintf(stdout, "axclrtEngineGetInputDims dimCount[0]=[%d]\n", dims.dims[0]);
		fprintf(stdout, "axclrtEngineGetInputDims dimCount[1]=[%d]\n", dims.dims[1]);
		fprintf(stdout, "axclrtEngineGetInputDims dimCount[2]=[%d]\n", dims.dims[2]);
		fprintf(stdout, "axclrtEngineGetInputDims dimCount[3]=[%d]\n", dims.dims[3]);

		ret = axclrtEngineGetInputDataLayout(io_info, i, &input_layouts->at(i));
		if (ret != 0)
        {
			LOG(ERROR) << "[EasyDK] [ax_model] GetModelInfo(): axclrtEngineGetInputDataLayout fail:" << ret;
            return -1;
        }
		
		fprintf(stdout, "axclrtEngineGetInputDataLayout input_layouts=[%d]\n", input_layouts->at(i));
    }

    output_sizes->resize(outputNum);
    for (uint32_t i = 0; i < outputNum; i++)
    {
        auto bufSize = axclrtEngineGetOutputSizeByIndex(io_info, grpid, i);
		output_sizes->at(i) = bufSize;
        axclError ret = 0;

        axclrtEngineIODims dims;
        ret = axclrtEngineGetOutputDims(io_info, grpid, i, &dims);
        if (ret != 0)
        {
			LOG(ERROR) << "[EasyDK] [ax_model] GetModelInfo(): axclrtEngineGetOutputDims fail:" << ret;
            return -1;
        }

		output_dims->at(i).resize(dims.dimCount);
		for (int32_t j = 0; j < dims.dimCount; j++)
	    {
			output_dims->at(i).at(j) = dims.dims[j];
		}
		
		fprintf(stdout, "axclrtEngineGetOutputDims dimCount[%d]\n", dims.dimCount);
		fprintf(stdout, "axclrtEngineGetOutputDims dimCount[0]=[%d]\n", dims.dims[0]);
		fprintf(stdout, "axclrtEngineGetOutputDims dimCount[1]=[%d]\n", dims.dims[1]);
		fprintf(stdout, "axclrtEngineGetOutputDims dimCount[2]=[%d]\n", dims.dims[2]);
		fprintf(stdout, "axclrtEngineGetOutputDims dimCount[3]=[%d]\n", dims.dims[3]);

		ret = axclrtEngineGetOutputDataLayout(io_info, i, &output_layouts->at(i));
		if (ret != 0)
        {
			LOG(ERROR) << "[EasyDK] [ax_model] GetModelInfo(): axclrtEngineGetOutputDataLayout fail:" << ret;
            return -1;
        }
		fprintf(stdout, "axclrtEngineGetOutputDataLayout output_layouts=[%d]\n", output_layouts->at(i));
    }

    return 0;
}

AX_MModel::~AX_MModel() {
  VLOG(2) << "[EasyDK ax_model] [~AX_MModel] Unload ";
}

int AX_MModel::init(const std::string& model_file) noexcept {
    std::vector<unsigned char> model_buffer;
    if (!read_file(model_file.c_str(), model_buffer))
    {
		LOG(ERROR) << "[EasyDK] [ax_model] init(): read_file fail: " << model_file;
		return -1;
	}

	return init(model_buffer.data(), model_buffer.size());
}

int AX_MModel::init(const void* mem_ptr, const size_t size) noexcept {
	void *devMem = nullptr;
    axclrtMalloc(&devMem, size, AXCL_MEM_MALLOC_NORMAL_ONLY);

	axclrtMemcpy(devMem, mem_ptr, size, AXCL_MEMCPY_HOST_TO_DEVICE);

	int ret = axclrtEngineLoadFromMem(devMem, size, &modelId);
	if (0 != ret)
    {
		LOG(ERROR) << "[EasyDK] [ax_model] init(): axclrtEngineLoadFromMem fail";
        return ret;
    }
	axclrtFree(devMem);

	printf("axclrtEngineLoadFromMem success modelId[%lu]\n", modelId);
	VLOG(2) << "[EasyDK ax_model] [init] modelId: " << modelId;

	ret = axclrtEngineCreateContext(modelId, &context);
    if (0 != ret)
    {
        LOG(ERROR) << "[EasyDK] [ax_model] init(): axclrtEngineCreateContext fail";
        return ret;
    }

	printf("axclrtEngineCreateContext success context[%lu]\n", context);
	VLOG(2) << "[EasyDK ax_model] [init] context: " << context;

	ret = axclrtEngineGetIOInfo(modelId, &io_info);
    if (0 != ret)
    {
		LOG(ERROR) << "[EasyDK] [ax_model] init(): axclrtEngineGetIOInfo fail";
        return ret;
    }

	inputNum = axclrtEngineGetNumInputs(io_info);
	outputNum = axclrtEngineGetNumOutputs(io_info);
	input_types.resize(inputNum);
	output_types.resize(outputNum);

	printf("inputNum[%u] outputNum[%u]\n", inputNum, outputNum);

	/* in/out type */
	for (uint32_t index = 0; index < inputNum; index++)
	{
	    axclrtEngineGetInputDataType(io_info, index, &input_types[index]);
		printf("InputDataType[%d]\n", input_types[index]);
	}

	for (uint32_t index = 0; index < outputNum; index++)
	{
	    axclrtEngineGetOutputDataType(io_info, index, &output_types[index]);
		printf("OutputDataType[%d]\n", output_types[index]);
	}

	ret = axclrtEngineGetShapeGroupsCount(io_info, &group_count);
    if (0 != ret)
    {
        axclrtEngineUnload(modelId);
		LOG(ERROR) << "[EasyDK] [ax_model] init(): axclrtEngineGetShapeGroupsCount fail";
        return ret;
    }

	printf("group_count[%d]\n", group_count);

	ios.resize(group_count);

	input_dims.resize(inputNum);
	output_dims.resize(outputNum);

	input_sizes.resize(group_count);
	output_sizes.resize(group_count);

	input_layouts.resize(inputNum);
	output_layouts.resize(outputNum);

	for (int grpid = 0; grpid < group_count; grpid++)
    {
        ret = axclrtEngineCreateIO(io_info, &ios[grpid]);
        if (ret != 0)
        {
            axclrtEngineUnload(modelId);
			LOG(ERROR) << "[EasyDK] [ax_model] init(): axclrtEngineCreateIO fail" << ret;
            return ret;
        }

        ret = GetModelInfo(grpid, io_info, ios[grpid], &input_dims[grpid], &output_dims, 
                              &input_sizes[grpid], &output_sizes[grpid], &input_layouts, &output_layouts);
        if (ret != 0)
        {
            axclrtEngineDestroyIO(ios[grpid]);
            axclrtEngineUnload(modelId);

            LOG(ERROR) << "[EasyDK] [ax_model] init(): GetModelInfo fail" << ret;
            return ret;
        }
    }

	return 0;
}


int AX_MModel::GetInputNum(void) noexcept{
	return inputNum;
}

int AX_MModel::GetOutputNum(void) noexcept{
	return outputNum;
}

std::vector<axclrtEngineDataType> AX_MModel::GetInputDataTypes(void) noexcept{
	return input_types;
}

std::vector<axclrtEngineDataType> AX_MModel::GetOutputDataTypes(void) noexcept{
	return output_types;
}

std::vector<axclrtEngineDataLayout> AX_MModel::GetInputDataLayouts(void) noexcept{
	return input_layouts;
}
std::vector<axclrtEngineDataLayout> AX_MModel::GetOutputDataLayouts(void) noexcept{
	return output_layouts;
}

std::vector<std::vector<int64_t>> AX_MModel::GetInputDimensions(void) noexcept{
	return input_dims;
}

std::vector<std::vector<int64_t>> AX_MModel::GetOutputDimensions(void) noexcept{
	return output_dims;
}

AX_Engine* AX_MModel::CreateEngine(void) noexcept{
	if (!engine)
	{
	    engine = (new AX_Engine());
		engine->io_info = io_info;
		engine->io_ = ios[0];
		engine->modelId = modelId;
		engine->context = context;
		
	}
	return engine;
}

int AX_MModel::CreateInputTensors(std::vector<AX_Tensor>* inputs) noexcept{
	inputs->resize(inputNum);
	for (uint32_t index = 0; index < inputNum; index++)
	{
	    AX_Tensor& tensor = (*inputs)[index];
	    tensor.bInputData = true;
		tensor.io_ = ios[0];
		tensor.index_ = index;
		tensor.size_ = input_sizes[0][index];
	}

	return 0;
}

int AX_MModel::CreateOutputTensors(std::vector<AX_Tensor>* outputs) noexcept{
	outputs->resize(outputNum);
	for (uint32_t index = 0; index < outputNum; index++)
	{
		AX_Tensor& tensor = (*outputs)[index];
	    tensor.bInputData = false;
		tensor.io_ = ios[0];
		tensor.index_ = index;
		tensor.size_ = output_sizes[0][index];
	}

	return 0;
}

int AX_Tensor::SetData(void *data) noexcept
{
    if (true == bInputData)
    {
        return axclrtEngineSetInputBufferByIndex(io_, index_, data, size_);
    }
	else
	{
	    return axclrtEngineSetOutputBufferByIndex(io_, index_, data, size_);
	}
}

int AX_Engine::inference() noexcept
{
    return axclrtEngineExecute(modelId, context, 0, io_);
}

AX_MModel *CreateIModel(){return new AX_MModel();}

