/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Project. All rights reserved.                         *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/asset_file.h"

#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/base/string.h" // For xe::utf8::find_name_from_guest_path

namespace xe {
namespace vfs {

AssetFile::AssetFile(const std::string& path, Mode mode, AAsset* asset)
    : path_(path), mode_(mode), asset_(asset) {
  assert_not_null(asset_);
  name_ = xe::utf8::find_name_from_guest_path(path);
}

AssetFile::~AssetFile() { Close(); }

void AssetFile::Close() {
  if (asset_) {
    AAsset_close(asset_);
    asset_ = nullptr;
  }
}

X_STATUS AssetFile::ReadSync(void* buffer, size_t buffer_length,
                             size_t byte_offset, size_t* out_bytes_read) {
  if (!asset_) {
    return X_STATUS_INVALID_HANDLE;
  }
  if (out_bytes_read) {
    *out_bytes_read = 0;
  }

  // AAsset_seek64 returns -1 on error, or the new offset.
  // It's okay if byte_offset is 0 and we are at the beginning.
  if (AAsset_seek64(asset_, static_cast<off64_t>(byte_offset), SEEK_SET) ==
      -1) {
    // TODO(benvanik): map errno?
    XELOGE("AssetFile::ReadSync failed to seek to offset {}", byte_offset);
    return X_STATUS_UNSUCCESSFUL; // Or a more specific error
  }

  int bytes_read = AAsset_read(asset_, buffer, buffer_length);
  if (bytes_read < 0) {
    // Error occurred
    XELOGE("AssetFile::ReadSync AAsset_read failed with error code {}",
           bytes_read);
    return X_STATUS_UNSUCCESSFUL; // Or a more specific error
  }

  if (out_bytes_read) {
    *out_bytes_read = static_cast<size_t>(bytes_read);
  }
  return X_STATUS_SUCCESS;
}

X_STATUS AssetFile::WriteSync(const void* buffer, size_t buffer_length,
                              size_t byte_offset,
                              size_t* out_bytes_written) {
  if (out_bytes_written) {
    *out_bytes_written = 0;
  }
  // Assets are read-only.
  return X_STATUS_ACCESS_DENIED;
}

X_STATUS AssetFile::GetInfo(FileInfo* out_info) {
  if (!asset_) {
    return X_STATUS_INVALID_HANDLE;
  }
  out_info->Reset();
  out_info->name = name_;
  out_info->path = path_; // Full VFS path
  out_info->type = Entry::Type::kFile;
  out_info->total_size = AAsset_getLength64(asset_);
  out_info->attributes = kFileAttributeNormal | kFileAttributeReadOnly;
  // Asset modification times are not available via AAssetManager.
  out_info->create_time = Clock::QueryHostSystemTime(); // Placeholder
  out_info->access_time = out_info->create_time;
  out_info->write_time = out_info->create_time;
  return X_STATUS_SUCCESS;
}

X_STATUS AssetFile::SetLength(size_t length) {
  // Assets are read-only.
  return X_STATUS_ACCESS_DENIED;
}

}  // namespace vfs
}  // namespace xe
