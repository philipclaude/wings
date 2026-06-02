//
//  wings: web interface for graphics applications
//
//  Copyright 2023 - 2026 Philip Claude Caplan
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
#include "texture.h"

#include <fmt/format.h>

#include <algorithm>

#include "log.h"

#if WINGS_COMPILE_STB

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#pragma GCC diagnostic pop
#endif

// TODO move these to header file
extern "C" {
unsigned char* stbi_load(char const* filename, int* x, int* y,
                         int* channels_in_file, int desired_channels);
void stbi_image_free(void* retval_from_stbi_load);
int stbi_write_jpg(char const* filename, int x, int y, int comp,
                   const void* data, int quality);
void stbi_set_flip_vertically_on_load(int flag);
}

namespace wings {

Texture::Texture(const std::string& filename, TextureOptions options)
    : channels_(kFormat2Channels.at(options.format)) {
  read(filename, options.flipy);
}

void Texture::read(const std::string& filename, bool flipy) {
  // From the stb documentation (N is the number of desired channels):
  // An output image with N components has the following components interleaved
  // in this order in each pixel:
  //
  //     N=#comp     components
  //       1           grey
  //       2           grey, alpha
  //       3           red, green, blue
  //       4           red, green, blue, alpha
  int n;
  unsigned char* pixels;
  stbi_set_flip_vertically_on_load(flipy);
  pixels = stbi_load(filename.c_str(), &width_, &height_, &n, channels_);
  ASSERT(pixels);

  // save the pixel data
  data_.resize(width_ * height_ * channels_);
  for (int i = 0; i < width_ * height_ * channels_; i++) data_[i] = pixels[i];
  stbi_image_free(pixels);

  LOG << fmt::format("read {} x {} image with {} channels (saved {})", width_,
                     height_, n, channels_);
}

}  // namespace wings