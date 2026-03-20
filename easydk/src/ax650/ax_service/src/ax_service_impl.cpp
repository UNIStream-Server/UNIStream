#include "ax_service_impl.hpp"
#include <mutex>
#include "glog/logging.h"
#include "axcl_rt.h"
#include "axcl.h"

namespace uniedk {

int MpsServiceImpl::Init(const MpsServiceConfig &config) {
  // check config, TODO(gaoyujia)
  mps_config_ = config;

  // Due to the mps-sdk design that vpps must use public common-vbpool,
  //    we have to config common-VB-buffers first ...
  //
  if (mps_vdec_->Config(config) != 0) {
    LOG(ERROR) << "[EasyDK] [MpsServiceImpl] Init(): Config Vdec failed";
    goto err_exit;
  }
  
  if (mps_venc_->Config(config) != 0) {
    LOG(ERROR) << "[EasyDK] [MpsServiceImpl] Init(): Config Venc failed";
    goto err_exit;
  }

  AX_S32 ret;
  ret = InitSys();
  if (ret) {
    goto err_exit;
  }

  return 0;

err_exit:
  Destroy();
  return -1;
}

void MpsServiceImpl::Destroy() {
	
}

void MpsServiceImpl::OnVBInfo(AX_U64 blkSize, int blkCount) {
  if (pool_cfgs_.find(blkSize) != pool_cfgs_.end()) {
    pool_cfgs_[blkSize] += blkCount;
  } else {
    pool_cfgs_[blkSize] = blkCount;
  }
}

AX_S32 MpsServiceImpl::InitSys() {
  AX_S32 ret = AX_SUCCESS;
  axclrtDeviceList stDeviceList;

  ret = axclInit(NULL);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [Mps] Create(): axclInit failed, ret = " << ret;
    return ret;
  }

  memset(&stDeviceList, 0, sizeof(stDeviceList));
  ret = axclrtGetDeviceList(&stDeviceList);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [Mps] Create(): axclrtGetDeviceList failed, ret = " << ret;
    return ret;
  }

  ret = axclrtSetDevice(stDeviceList.devices[0]);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [Mps] Create(): axclrtSetDevice failed, ret = " << ret;
    return ret;
  }

  ret = AXCL_SYS_Init();
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [Mps] Create(): AXCL_SYS_Init failed, ret = " << ret;
    return ret;
  }
  
  return AX_SUCCESS;
}
};  // namespace uniedk
