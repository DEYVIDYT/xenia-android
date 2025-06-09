/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Project. All rights reserved.                         *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/vfs/devices/asset_device.h"

#include <algorithm>

#include "xenia/base/clock.h"
#include "xenia/base/filesystem.h" // For xe::filesystem::path_join
#include "xenia/base/logging.h"
#include "xenia/base/string.h"
#include "xenia/vfs/devices/asset_file.h"

namespace xe {
namespace vfs {

AssetDevice::AssetDevice(const std::string& mount_path,
                         AAssetManager* asset_manager)
    : Device(mount_path), asset_manager_(asset_manager) {
  assert_not_null(asset_manager_);
}

AssetDevice::~AssetDevice() = default;

bool AssetDevice::Initialize() { return true; }

void AssetDevice::Dump(StringBuffer* string_buffer) {
  string_buffer->AppendFormat("AssetDevice @ %s\n", mount_path().c_str());
}

// Helper to remove mount_path and leading slashes.
std::string AssetDevice::NormalizePath(const std::string& path) {
  std::string_view p = path;
  // Remove mount_path prefix if present
  if (xe::utf8::starts_with(p, mount_path_)) {
    p.remove_prefix(mount_path_.length());
  }
  // Remove leading slashes
  while (!p.empty() && (p.front() == '/' || p.front() == '\\')) {
    p.remove_prefix(1);
  }
  return std::string(p);
}

std::unique_ptr<Entry> AssetDevice::ResolvePath(const std::string& path) {
  XELOGVFS("AssetDevice::ResolvePath({})", path);
  FileInfo file_info;
  X_STATUS result = Stat(path, &file_info);
  if (XSUCCEEDED(result)) {
    return std::make_unique<Entry>(this, path, &file_info);
  }
  return nullptr;
}

X_STATUS AssetDevice::OpenFile(const std::string& path, Mode mode,
                               std::unique_ptr<File>* out_file) {
  XELOGVFS("AssetDevice::OpenFile({}, {:08X})", path, uint32_t(mode));

  if (mode != Mode::kRead) {
    // Assets are read-only.
    return X_STATUS_ACCESS_DENIED;
  }

  std::string normalized_path = NormalizePath(path);
  if (normalized_path.empty()) {
    // Cannot open the root of the assets as a file.
    return X_STATUS_INVALID_PARAMETER;
  }

  AAsset* asset = AAssetManager_open(asset_manager_, normalized_path.c_str(),
                                   AASSET_MODE_RANDOM);
  if (!asset) {
    // Could try AAssetManager_openDir to see if it's a directory,
    // but OpenFile is for files.
    return X_STATUS_NO_SUCH_FILE; // Or X_STATUS_OBJECT_PATH_NOT_FOUND
  }

  *out_file = std::make_unique<AssetFile>(path, mode, asset);
  return X_STATUS_SUCCESS;
}

X_STATUS AssetDevice::Stat(const std::string& path, FileInfo* out_info) {
  XELOGVFS("AssetDevice::Stat({})", path);
  std::string normalized_path = NormalizePath(path);

  // Try opening as a file first.
  AAsset* asset = AAssetManager_open(asset_manager_, normalized_path.c_str(),
                                   AASSET_MODE_RANDOM); // RANDOM for getLength
  if (asset) {
    out_info->Reset();
    out_info->name = xe::utf8::find_name_from_guest_path(normalized_path);
    out_info->path = normalized_path;
    out_info->type = Entry::Type::kFile;
    out_info->total_size = AAsset_getLength64(asset);
    out_info->attributes = kFileAttributeNormal | kFileAttributeReadOnly;
    // Asset modification times are not available via AAssetManager.
    // Use a fixed time or current time.
    out_info->create_time = Clock::QueryHostSystemTime();
    out_info->access_time = out_info->create_time;
    out_info->write_time = out_info->create_time;
    AAsset_close(asset);
    return X_STATUS_SUCCESS;
  }

  // Try opening as a directory.
  // Note: normalized_path for openDir should not have a trailing slash.
  // AAssetManager_openDir behavior with empty string (root) is to list root assets.
  AAssetDir* asset_dir =
      AAssetManager_openDir(asset_manager_, normalized_path.c_str());
  if (asset_dir) {
    out_info->Reset();
    out_info->name = xe::utf8::find_name_from_guest_path(normalized_path);
    out_info->path = normalized_path;
    out_info->type = Entry::Type::kDirectory;
    out_info->total_size = 0; // Directories have no size in this context
    out_info->attributes = kFileAttributeDirectory | kFileAttributeReadOnly;
    out_info->create_time = Clock::QueryHostSystemTime();
    out_info->access_time = out_info->create_time;
    out_info->write_time = out_info->create_time;
    AAssetDir_close(asset_dir);
    return X_STATUS_SUCCESS;
  }

  return X_STATUS_NO_SUCH_FILE; // Or X_STATUS_OBJECT_PATH_NOT_FOUND
}

X_STATUS AssetDevice::ListFiles(const std::string& path,
                                std::vector<FileInfo>* out_infos) {
  XELOGVFS("AssetDevice::ListFiles({})", path);
  std::string normalized_path = NormalizePath(path);

  AAssetDir* asset_dir =
      AAssetManager_openDir(asset_manager_, normalized_path.c_str());
  if (!asset_dir) {
    return X_STATUS_NO_SUCH_FILE; // Or X_STATUS_OBJECT_PATH_NOT_FOUND
  }

  out_infos->clear();
  const char* filename_c_str;
  while ((filename_c_str = AAssetDir_getNextFileName(asset_dir)) != nullptr) {
    std::string full_child_path = normalized_path.empty() ?
                                  std::string(filename_c_str) :
                                  xe::filesystem::path_join(normalized_path, filename_c_str);
    FileInfo child_info;
    // Stat each child to determine if it's a file or directory and get its size.
    // This is potentially slow if a directory has many files.
    // AAssetDir_getNextFileName doesn't tell us if it's a file or dir.
    // We need to try opening it as an asset to know.

    AAsset* child_asset = AAssetManager_open(asset_manager_, full_child_path.c_str(), AASSET_MODE_RANDOM);
    if (child_asset) {
      child_info.Reset();
      child_info.name = std::string(filename_c_str);
      child_info.path = full_child_path;
      child_info.type = Entry::Type::kFile;
      child_info.total_size = AAsset_getLength64(child_asset);
      child_info.attributes = kFileAttributeNormal | kFileAttributeReadOnly;
      child_info.create_time = Clock::QueryHostSystemTime(); // Placeholder
      child_info.access_time = child_info.create_time;
      child_info.write_time = child_info.create_time;
      AAsset_close(child_asset);
      out_infos->push_back(child_info);
    } else {
      // If it's not a file, try it as a directory for completeness,
      // though shaders are unlikely to be in nested asset subdirs often.
      // For robust ListFiles, one might need to check if opening as AAssetDir succeeds.
      // For simplicity here, if it's not an asset file, we'll skip or assume it's a dir.
      // This part is less critical for shader loading which uses direct paths.
      // Let's assume if not a file, it *could* be a directory for basic listing.
      AAssetDir* nested_dir = AAssetManager_openDir(asset_manager_, full_child_path.c_str());
      if (nested_dir) {
        child_info.Reset();
        child_info.name = std::string(filename_c_str);
        child_info.path = full_child_path;
        child_info.type = Entry::Type::kDirectory;
        child_info.total_size = 0;
        child_info.attributes = kFileAttributeDirectory | kFileAttributeReadOnly;
        child_info.create_time = Clock::QueryHostSystemTime();
        child_info.access_time = child_info.create_time;
        child_info.write_time = child_info.create_time;
        AAssetDir_close(nested_dir);
        out_infos->push_back(child_info);
      }
    }
  }

  AAssetDir_close(asset_dir);
  return X_STATUS_SUCCESS;
}

}  // namespace vfs
}  // namespace xe
