/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Project. All rights reserved.                         *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_ASSET_DEVICE_H_
#define XENIA_VFS_DEVICES_ASSET_DEVICE_H_

#include <android/asset_manager.h>
#include <string>
#include <vector>
#include <memory>

#include "xenia/vfs/device.h"

namespace xe {
namespace vfs {

class AssetDevice : public Device {
 public:
  AssetDevice(const std::string& mount_path, AAssetManager* asset_manager);
  ~AssetDevice() override;

  bool Initialize() override;
  void Dump(StringBuffer* string_buffer) override;
  bool is_read_only() const override { return true; }

  std::unique_ptr<Entry> ResolvePath(const std::string& path) override;

  X_STATUS OpenFile(const std::string& path, Mode mode,
                    std::unique_ptr<File>* out_file) override;
  X_STATUS Stat(const std::string& path, FileInfo* out_info) override;
  X_STATUS ListFiles(const std::string& path,
                       std::vector<FileInfo>* out_infos) override;

 private:
  std::string NormalizePath(const std::string& path);

  AAssetManager* asset_manager_ = nullptr;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_ASSET_DEVICE_H_
