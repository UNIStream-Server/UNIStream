# ==============================================================================
# Copyright (C) [2022] by Cambricon, Inc. All rights reserved
# Copyright (C) [2025] by UNIStream Team. All rights reserved 
#  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
#  The original work is licensed under the Apache License, Version 2.0
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ==============================================================================

import os, sys
sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
import unistream

# for test stream msg observer keep alive with pipeline
smo_del_called = False

class CustomSMO(unistream.StreamMsgObserver):
  def __init__(self):
    unistream.StreamMsgObserver.__init__(self)

  def __del__(self):
    global smo_del_called
    smo_del_called = True

  def update(self, msg):
    # Empty, only for testing create and destroy StreamMsgObserver
    pass

def create_config_with_source_module():
  config = unistream.CNGraphConfig()
  config.name = "test_pipeline"
  config.profiler_config.enable_profiling = True
  mconfig = unistream.CNModuleConfig()
  mconfig.name = "test_module"
  mconfig.class_name = "unistream::DataSource"
  mconfig.parameters = {"device_id": "0"}
  config.module_configs = [mconfig]
  return config

def test_name():
  pipeline = unistream.Pipeline("test_pipeline")
  assert "test_pipeline" == pipeline.get_name()

def test_build_pipeline():
  # TODO(liumingxuan): wait for config py api merge
  pipeline = unistream.Pipeline("test_pipeline")
  config = create_config_with_source_module()
  assert pipeline.build_pipeline(config)
  source_module = pipeline.get_source_module("test_module")
  assert source_module

def test_start():
  pipeline = unistream.Pipeline("test_pipeline")
  assert pipeline.start()
  assert pipeline.is_running()

def test_get_source_module():
  pipeline = unistream.Pipeline("test_pipeline")
  config = create_config_with_source_module()
  assert pipeline.build_pipeline(config)
  assert None != pipeline.get_source_module("test_module")
  assert None == pipeline.get_source_module("test_none")

def test_get_module_config():
  pipeline = unistream.Pipeline("test_pipeline")
  config = create_config_with_source_module()
  assert pipeline.build_pipeline(config)
  mconfig = pipeline.get_module_config("test_module")
  assert "unistream::DataSource" == mconfig.class_name

def test_is_profiling_enabled():
  pipeline = unistream.Pipeline("test_pipeline")
  config = unistream.CNGraphConfig()
  config.profiler_config.enable_profiling = True
  assert pipeline.build_pipeline(config)
  assert pipeline.is_profiling_enabled()

def test_is_tracing_enabled():
  pipeline = unistream.Pipeline("test_pipeline")
  config = unistream.CNGraphConfig()
  config.profiler_config.enable_tracing = True
  assert pipeline.build_pipeline(config)
  assert pipeline.is_tracing_enabled()

def test_provide_data():
  pipeline = unistream.Pipeline("test_pipeline")
  config = create_config_with_source_module()
  assert pipeline.build_pipeline(config)
  assert pipeline.start()
  source_module = pipeline.get_source_module("test_module")
  assert pipeline.provide_data(source_module, unistream.CNFrameInfo("test_stream"))
  pipeline.stop()

def test_stream_msg_observer():
  smo = CustomSMO()
  pipeline = unistream.Pipeline("test_pipeline")
  pipeline.stream_msg_observer = smo
  # test smo keep alive with pipeline
  smo = None
  global smo_del_called
  assert not smo_del_called
  # TODO(liumingxuan): test stream message update

def test_is_root_node():
  pipeline = unistream.Pipeline("test_pipeline")
  config = create_config_with_source_module()
  assert pipeline.build_pipeline(config)
  assert pipeline.is_root_node("test_module")
  assert not pipeline.is_root_node("test_not")

def test_register_frame_done_cb():
  # TODO(liumingxuan): wait for data handler api
  pass
