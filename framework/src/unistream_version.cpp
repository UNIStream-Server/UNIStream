/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
 *  The original work is licensed under the Apache License, Version 2.0
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

#include "unistream_version.hpp"
#include <cstdio>

#define VERSION_STRING_LENGTH 10

namespace unistream {

const char* VersionString() {
  static char version[VERSION_STRING_LENGTH];
  snprintf(version, VERSION_STRING_LENGTH, "v%d.%d.%d", UNISTREAM_MAJOR_VERSION, UNISTREAM_MINOR_VERSION,
           UNISTREAM_PATCH_VERSION);
  return version;
}

const int MajorVersion() { return UNISTREAM_MAJOR_VERSION; }
const int MinorVersion() { return UNISTREAM_MINOR_VERSION; }
const int PatchVersion() { return UNISTREAM_PATCH_VERSION; }

}  // namespace unistream
