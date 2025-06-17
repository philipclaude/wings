//
//  wings: web interface for graphics applications
//
//  Copyright 2023 Philip Claude Caplan
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace wings {

enum class TextureFormat : uint8_t {
  kRGB,
  kRGBA,
  kGrayscale,
};

static const std::map<TextureFormat, int> kFormat2Channels = {
    {TextureFormat::kRGB, 3},
    {TextureFormat::kRGBA, 4},
    {TextureFormat::kGrayscale, 1}};

struct TextureOptions {
  TextureFormat format{TextureFormat::kRGB};
  bool flipy{true};  // should the y-component be flipped?
};

/// @brief Represents an image that can be sampled to determine properties of a
/// point on a surface. For example, determining the color or height at a point
/// on a surface mesh.
class Texture {
 public:
  /// @brief Reads an image file
  /// @param filename Path to the image
  /// @param options (see above)
  Texture(const std::string& filename, TextureOptions options);

  /// @brief Returns the number of pixels in the horizontal direction.
  int width() const { return width_; }

  /// @brief Returns the number of pixels in the vertical direction.
  int height() const { return height_; }

  /// @brief Returns the number of channels (components) for each pixel: 1 for
  /// grayscale and 3 for RGB.
  int channels() const { return channels_; }

  /// @brief Returns a pointer to the pixel data.
  const auto* data() const { return data_.data(); }

 private:
  void read(const std::string& filename, bool flipy);
  uint8_t channels_;
  int width_;
  int height_;
  std::vector<unsigned char> data_;
};

}  // namespace wings