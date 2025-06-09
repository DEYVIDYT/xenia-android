/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Project. All rights reserved.                         *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_VFS_DEVICES_ASSET_FILE_H_
#define XENIA_VFS_DEVICES_ASSET_FILE_H_

#include <android/asset_manager.h>
#include <string>

#include "xenia/vfs/file.h"

namespace xe {
namespace vfs {

class AssetFile : public File {
 public:
  AssetFile(const std::string& path, Mode mode, AAsset* asset);
  ~AssetFile() override;

  void Close() override;
  const std::string& path() const override { return path_; }
  const std::string& name() const override { return name_; }
  Mode mode() const override { return mode_; }

  X_STATUS ReadSync(void* buffer, size_t buffer_length, size_t byte_offset,
                    size_t* out_bytes_read) override;
  X_STATUS WriteSync(const void* buffer, size_t buffer_length,
                     size_t byte_offset, size_t* out_bytes_written) override;

  X_STATUS GetInfo(FileInfo* out_info) override;
  X_STATUS SetLength(size_t length) override;

 private:
  std::string path_;
  std::string name_; // Just the filename part of path_
  Mode mode_;
  AAsset* asset_ = nullptr;
};

}  // namespace vfs
}  // namespace xe

#endif  // XENIA_VFS_DEVICES_ASSET_FILE_H_
