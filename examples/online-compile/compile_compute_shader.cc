// Copyright 2016 The Shaderc Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The program demonstrates basic shader compilation using the Shaderc C++ API.
// For clarity, each method is deliberately self-contained.
//
// Techniques demonstrated:
//  - Preprocessing GLSL source text
//  - Compiling a shader to SPIR-V assembly text
//  - Compliing a shader to a SPIR-V binary module
//  - Performing optimization with compilation
//  - Setting basic options: setting a preprocessor symbol.
//  - Checking compilation status and extracting an error message.

// #include "glslc/src/file_includer.h"
// #include <shaderc/libshaderc_util/include/libshaderc_util/file_finder.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <shaderc/shaderc.hpp>
#include <string>
#include <vector>

#include "glslc/src/file_compiler.h"
#include "glslc/src/file_includer.h"
#include "libshaderc_util/file_finder.h"
#include "libshaderc_util/io_shaderc.h"
#include "libshaderc_util/resources.h"
#include "third_party/glslang/glslang/Public/ShaderLang.h"

std::string preprocesse_shader(std::string const& source_name,
                               shaderc_shader_kind kind) {
  auto inputfile =
      glslc::InputFileSpec{source_name.c_str(), shaderc_compute_shader,
                           shaderc_source_language_glsl, "main"};

  // vulkan-shaders-gen execute command:
  // /Users/brooksli/.local/VulkanSDK/1.4.309.0/macOS/bin/glslc
  // -fshader-stage=compute --target-env=vulkan1.2 -O
  // /Users/brooksli/github/llama.cpp/ggml/src/ggml-vulkan/vulkan-shaders/mul_mm.comp
  // -o
  // /Users/brooksli/github/llama.cpp/build/ggml/src/ggml-vulkan/vulkan-shaders.spv/matmul_f32_f16_aligned_fp32.spv
  // -DACC_TYPE=float -DALIGNED=1 -DB_TYPE=f16vec4 -DDATA_A_F32=1 -DD_TYPE=float
  // -DFLOAT_TYPE=float -DFLOAT_TYPE_VEC2=vec2 -DLOAD_VEC_A=4 -DLOAD_VEC_B=4

  shaderc::CompileOptions options;
  shaderc::Compiler compiler;
  shaderc_util::FileFinder finder;

  finder.search_path().push_back("/Users/brooksli/github/shaderc/");
  options.AddMacroDefinition("ACC_TYPE", "float");
  options.AddMacroDefinition("ALIGNED", "1");
  options.AddMacroDefinition("B_TYPE", "f16vec4");
  options.AddMacroDefinition("DATA_A_F32", "1");
  options.AddMacroDefinition("D_TYPE", "float");
  options.AddMacroDefinition("FLOAT_TYPE", "float");
  options.AddMacroDefinition("FLOAT_TYPE_VEC2", "vec2");
  options.AddMacroDefinition("LOAD_VEC_A", "4");
  options.AddMacroDefinition("LOAD_VEC_B", "4");
  options.SetSourceLanguage(shaderc_source_language_glsl);

  options.SetIncluder(std::make_unique<glslc::FileIncluder>(&finder));

  std::vector<char> input_data;
  std::string path = inputfile.name;
  if (!shaderc_util::ReadFile(path, &input_data)) {
    return "";
  }

  shaderc_util::string_piece source_string = "";
  if (!input_data.empty()) {
    source_string = {&input_data.front(),
                     &input_data.front() + input_data.size()};
  }

  auto result =
      compiler.PreprocessGlsl(source_string.data(), source_string.size(),
                              inputfile.stage, path.c_str(), options);

  return {result.begin(), result.end()};
#if 0
  result = compiler.CompileGlslToSpvAssembly(
      preprocessed_source.data(), preprocessed_source.size(), inputfile.stage,
      path.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << result.GetErrorMessage() << std::endl;
    return "";
  }

  return {result.begin(), result.end()};
#endif
}

glslang::TIntermediate* parse_ast(
    glslang::TShader& shader, shaderc_util::string_piece input_source_string) {
  // Parsing requires its own Glslang symbol tables.
  auto used_shader_stage = EShLangCompute;
  std::string error_tag = "glsld";
  const char* shader_strings = input_source_string.data();
  const int shader_lengths = static_cast<int>(input_source_string.size());
  const char* string_names = error_tag.c_str();
  shader.setStringsWithLengthsAndNames(&shader_strings, &shader_lengths,
                                       &string_names, 1);
  // shader.setPreamble(preamble.c_str());
  shader.setEntryPoint("main");
  bool auto_bind_uniforms_ = false;
  auto auto_combined_image_sampler_ = false;
  auto auto_map_locations_ = false;

  shader.setAutoMapBindings(auto_bind_uniforms_);
  if (auto_combined_image_sampler_) {
    shader.setTextureSamplerTransformMode(
        EShTexSampTransUpgradeTextureRemoveSampler);
  }
  shader.setAutoMapLocations(auto_map_locations_);

  shader.setShiftImageBinding(0);
  shader.setShiftSamplerBinding(0);
  shader.setShiftTextureBinding(0);
  shader.setShiftUboBinding(0);
  shader.setShiftSsboBinding(0);
  shader.setShiftUavBinding(0);

  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
  glslang::EShSource language = glslang::EShSourceGlsl;
  // This option will only be used if the Vulkan client is used.
  // If new versions of GL_KHR_vulkan_glsl come out, it would make sense to
  // let callers specify which version to use. For now, just use 100.
  shader.setEnvInput(language, used_shader_stage, glslang::EShClientVulkan,
                     100);
  shader.setEnvInputVulkanRulesRelaxed();
  shader.setInvertY(false);
  shader.setNanMinMaxClamp(false);

#if 1
  const EShMessages rules = static_cast<EShMessages>(
      EShMsgCascadingErrors | EShMsgSpvRules | EShMsgVulkanRules);

  auto default_version_ = 110;
  auto default_profile_ = ENoProfile;
  auto force_version_profile_ = false;

  glslang::TShader::ForbidIncluder includer;
  bool success = shader.parse(&shaderc_util::kDefaultTBuiltInResource,
                              default_version_, default_profile_,
                              force_version_profile_, false, rules, includer);

  if (!success) return nullptr;

  auto ast = shader.getIntermediate();
  return ast;
#endif
}

int main(const int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << ": <compute shader file>" << std::endl;
    return -1;
  }

  // std::cerr << "file source: " << std::endl << source << std::endl;
  auto preprocessed_source = preprocesse_shader(
      argv[1], shaderc_shader_kind::shaderc_glsl_compute_shader);

  std::cerr << "preprocessed source: " << std::endl
            << preprocessed_source << std::endl;

  glslang::TShader shader(EShLangCompute);
  glslang::TIntermediate* ast = parse_ast(shader, preprocessed_source);
  if (!ast) {
    std::cerr << "parse ast failed." << std::endl;
    return -1;
  }

  return 0;
  // std::cerr << "compute shader preprocessed: " << std::endl
  //           <<
  //           << std::endl;
}
