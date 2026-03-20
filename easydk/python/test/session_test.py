# ==============================================================================
# Copyright (C) [2022] by Cambricon, Inc. All rights reserved
# Copyright (C) [2025] by UNIStream Team. All rights reserved
#
# This file has been modified by UNIStream development team based on
# the original work from Cambricon, Inc.
# The original work is licensed under the Apache License, Version 2.0.
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

"""Session test

This module tests Session related APIs

"""

import os, sys
import numpy as np

sys.path.append(os.path.split(os.path.realpath(__file__))[0] + "/../lib")
import uniis
import utils

class CustomPreprocess(uniis.IPreproc):
  """To use custom preprocess, we define a class CustomPreprocess which inherits from uniis.IPreproc.
  The execute_func API will be called by InferServer to do preprocess.
  """
  def __init__(self):
    super().__init__()

  def on_tensor_params(params):
    return 0

  def on_preproc(self, src, dst, src_rects):
    # doing preprocessing.
    return 0

class CustomPostprocess(uniis.IPostproc):
  """To use custom postprocess, we define a class CustomPostprocess which inherits from uniis.IPostproc.
  The on_postproc API will be called by InferServer to do postprocess.
  """
  def __init__(self):
    super().__init__()

  def on_postproc(self, data_vec, model_output, model_info):
    # doing postprocessing.
    return 0

class TestSession(object):
  """TestSession class provides several APIs for testing Session"""
  @staticmethod
  def test_session():
    infer_server = uniis.InferServer(dev_id=0)
    session_desc = uniis.SessionDesc()
    session_desc.name = "test_session"
    session_desc.model = infer_server.load_model(utils.get_model_dir())
    session_desc.model_input_format = uniis.NetworkInputFormat.RGB

    session_desc.strategy = uniis.BatchStrategy.DYNAMIC
    session_desc.preproc = uniis.Preprocessor()
    preproc_handler = CustomPreprocess()
    uniis.set_preproc_handler(session_desc.model.get_key(), preproc_handler)
    session_desc.postproc = uniis.Postprocessor()
    postproc_handler = CustomPostprocess()
    uniis.set_postproc_handler(session_desc.model.get_key(), postproc_handler)
    session_desc.batch_timeout = 1000
    session_desc.priority = 0
    session_desc.engine_num = 2
    session_desc.show_perf = False

    session = infer_server.create_sync_session(session_desc)
    assert session
    assert infer_server.destroy_session(session)

    class TestObserver(uniis.Observer):
      """To receive results from InferServer, we define a class TestObserver which inherits from uniis.Observer.
      After a request is sent to InferServer and is processed by InferServer, the response API will be called with
      status, results and user data.
      """
      def __init__(self):
        super().__init__()
      def response(self, status, data, user_data):
        # Empty, just for testing session creation
        pass
    obs = TestObserver()
    session = infer_server.create_session(session_desc, obs)
    assert session
    assert infer_server.destroy_session(session)
