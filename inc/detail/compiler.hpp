#pragma once

namespace VK_NAMESPACE {
#include <glslang/Include/glslang_c_interface.h>
#include <spirv-tools/libspirv.h>
} // namespace VK_NAMESPACE

#if VKTL_HAVE_STD_
#include <filesystem>
#include <fstream>
#include <unordered_map>
#endif

VKTL_EXPORT_ namespace vktl::detail {
  struct shader_handle_tag : detail::poly_list::node {
    byte_view compiled;
  };
}

VKTL_EXPORT_ namespace vktl::detail {
  struct default_shader : shader_handle_tag {
    detail::box<vptr::initable> parent;

    VK_ VkShaderStageFlagBits stage = VK_ VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

    vector<::std::byte> code;
    vector<::std::uint32_t> compiled;

    ::std::string entry_point = "main";

    ::std::filesystem::path filepath;
    ::std::filesystem::file_time_type last_timestamp;

    vector<VK_ VkPushConstantRange> push_constants;
    vector<vector<VK_ VkDescriptorSetLayoutBinding>> bindings;

    template <typename Self, typename Parent>
    explicit default_shader(::std::in_place_type_t<Self> tag,
                            Parent &parent_ref, VK_ VkShaderStageFlagBits stage)
        : shader_handle_tag{}, parent(&parent_ref), stage(stage) {}

    template <typename Parent>
    explicit default_shader(Parent &parent_ref, VK_ VkShaderStageFlagBits stage)
        : default_shader(::std::in_place_type<default_shader>, parent_ref,
                         stage) {}
  };

  struct default_vertex_shader : default_shader {
    vector<VK_ VkVertexInputBindingDescription> bindings;
    vector<VK_ VkVertexInputAttributeDescription> locations;

    template <typename Self = default_vertex_shader, typename Parent>
    explicit default_vertex_shader(::std::in_place_type_t<Self> tag,
                                   Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_VERTEX_BIT) {}

    template <typename Parent>
    explicit default_vertex_shader(Parent &parent_ref)
        : default_vertex_shader(::std::in_place_type<default_vertex_shader>,
                                parent_ref) {}
  };

  struct default_fragment_shader : default_shader {
    bool depth_write_enable = true;
    bool depth_test_enable = true;

    template <typename Self = default_fragment_shader, typename Parent>
    explicit default_fragment_shader(::std::in_place_type_t<Self> tag,
                                     Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_FRAGMENT_BIT) {}

    template <typename Parent>
    explicit default_fragment_shader(Parent &parent_ref)
        : default_fragment_shader(::std::in_place_type<default_fragment_shader>,
                                  parent_ref) {}
  };

  struct default_compute_shader : default_shader {
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t z = 1;

    template <typename Self = default_compute_shader, typename Parent>
    explicit default_compute_shader(::std::in_place_type_t<Self> tag,
                                    Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_COMPUTE_BIT) {}
    template <typename Parent>
    explicit default_compute_shader(Parent &parent_ref)
        : default_compute_shader(::std::in_place_type<default_compute_shader>,
                                 parent_ref) {}
  };

  struct default_geometry_shader : default_shader {
    uint32_t max_output_vertices = 0;
    uint32_t invocations = 1;

    template <typename Self = default_geometry_shader, typename Parent>
    explicit default_geometry_shader(::std::in_place_type_t<Self> tag,
                                     Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_GEOMETRY_BIT) {}

    template <typename Parent>
    explicit default_geometry_shader(Parent &parent_ref)
        : default_geometry_shader(::std::in_place_type<default_geometry_shader>,
                                  parent_ref) {}
  };

  struct default_tess_control_shader : default_shader {
    uint32_t patch_control_points = 3;

    template <typename Self = default_tess_control_shader, typename Parent>
    explicit default_tess_control_shader(::std::in_place_type_t<Self> tag,
                                         Parent &parent_ref)
        : default_shader(tag, parent_ref,
                         VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {}

    template <typename Parent>
    explicit default_tess_control_shader(Parent &parent_ref)
        : default_tess_control_shader(
              ::std::in_place_type<default_tess_control_shader>, parent_ref) {}
  };

  struct default_tess_eval_shader : default_shader {
    VK_ VkPrimitiveTopology primitive_mode =
        VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    template <typename Self = default_tess_eval_shader, typename Parent>
    explicit default_tess_eval_shader(::std::in_place_type_t<Self> tag,
                                      Parent &parent_ref)
        : default_shader(tag, parent_ref,
                         VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {}

    template <typename Parent>
    explicit default_tess_eval_shader(Parent &parent_ref)
        : default_tess_eval_shader(
              ::std::in_place_type<default_tess_eval_shader>, parent_ref) {}
  };

  struct default_task_shader : default_shader {
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t z = 1;

    template <typename Self = default_task_shader, typename Parent>
    explicit default_task_shader(::std::in_place_type_t<Self> tag,
                                 Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_TASK_BIT_EXT) {}

    template <typename Parent>
    explicit default_task_shader(Parent &parent_ref)
        : default_task_shader(::std::in_place_type<default_task_shader>,
                              parent_ref) {}
  };

  struct default_mesh_shader : default_shader {
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t z = 1;
    uint32_t max_vertices = 0;
    uint32_t max_primitives = 0;

    template <typename Self = default_mesh_shader, typename Parent>
    explicit default_mesh_shader(::std::in_place_type_t<Self> tag,
                                 Parent &parent_ref)
        : default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_MESH_BIT_EXT) {}

    template <typename Parent>
    explicit default_mesh_shader(Parent &parent_ref)
        : default_mesh_shader(::std::in_place_type<default_mesh_shader>,
                              parent_ref) {}
  };
}

VKTL_EXPORT_ namespace vktl::detail {
  namespace compile {
    inline uint32_t glsl_refc_ = 0u;
    inline bool lock_ = false;
    inline auto acquire() {
      while (::std::exchange(lock_, true)) {}
      if (glsl_refc_ == 0u && !glslang_initialize_process()) {
        lock_ = false;
        throw error{0x42u, "[COMPILER] Initialize compiler failure."};
      }
      auto value = ++glsl_refc_;
      lock_ = false;
      return value;
    }
    inline auto release() noexcept {
      while (::std::exchange(lock_, true)) {}
      if (glsl_refc_ > 0u && glsl_refc_ == 1u) {
        glslang_finalize_process();
      }
      auto value = --glsl_refc_;
      lock_ = false;
      return value;
    }
  }

  template <typename N> struct m<compiler_, N> : N {
    constexpr m(compiler_, auto &&...others) 
      : N{forward_(others)...} {}

    shader_handle append(::std::filesystem::path filepath, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main") {
      return &append_(stage, read_code(filepath), {}, filepath, ::std::move(entry_point));
    }
    shader_handle append_compiled(::std::filesystem::path filepath, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main") {
      return &append_(stage, {}, read_spirv(filepath), filepath, ::std::move(entry_point));
    }
    shader_handle append(byte_view code, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main") {
      return &append_(stage, ::std::vector<::std::byte>(code.begin(), code.end()), {}, {}, ::std::move(entry_point));
    }
    shader_handle append_compiled(byte_view code, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main") {
      return &append_(
          stage, {},
          ::std::vector<::std::uint32_t>(
              reinterpret_cast<const uint32_t *>(code.data()),
              reinterpret_cast<const uint32_t *>(code.data() + code.size())),
          {}, ::std::move(entry_point));
    }

  protected:
    void init() {
      compile::acquire();
      init(call_duck);
    }
    void reset() {
      shaders_.clear();
      compile::release();
    }

  private:
    void init(::std::invocable<default_shader &> auto &&func) {
      ::std::lock_guard _{N::get_lock()};
      for (default_shader &shader : shaders_) {
        if (shader.compiled.empty()) {
          compile_shader_(shader);
        }

        func(shader);
        shader.shader_handle_tag::compiled =
            byte_view{shader.compiled.data(), shader.compiled.size()};
      }
    }

  protected:
    template <typename T> void enumerate(T val) {}
    template <typename T, typename Fn, typename... Fns>
    void enumerate(T val, Fn &fn, Fns &...fns) {
      if constexpr (::std::invocable<Fn &, T>) {
        fn(val);
      }
      enumerate(val, fns...);
    }

  private:
    static auto read_spirv(::std::filesystem::path const &path) {
      const auto size = ::std::filesystem::file_size(path);
      if (size % sizeof(::std::uint32_t) != 0) {
        auto err_str = "Invalid SPIR-V size: " + path.string();
        throw error{0x402, err_str.data()};
      }

      ::std::vector<::std::uint32_t> buffer(size / sizeof(::std::uint32_t));
      ::std::ifstream file(path, ::std::ios::binary);
      if (!file.is_open()) {
        auto err = "Failed to open file: " + path.string();
        throw error{0x403, err.data()};
      }

      file.read(reinterpret_cast<char *>(buffer.data()), size);
      return buffer;
    }
    static auto read_code(::std::filesystem::path const &path) {
      const auto size = ::std::filesystem::file_size(path);

      ::std::vector<::std::byte> buffer(size, ::std::byte(0));
      ::std::ifstream file(path, ::std::ios::binary);
      if (!file.is_open()) {
        auto err = "Failed to open file: " + path.string();
        throw error{0x402, err.data()};
      }

      file.read(reinterpret_cast<char *>(buffer.data()), size);
      return buffer;
    }

    default_shader &append_(VK_ VkShaderStageFlagBits stage) {
      ::std::lock_guard _{N::get_lock()};
      switch (stage) {
      case VK_ VK_SHADER_STAGE_VERTEX_BIT:
        return shaders_.emplace_back<default_vertex_shader>(*this);
      case VK_ VK_SHADER_STAGE_FRAGMENT_BIT:
        return shaders_.emplace_back<default_fragment_shader>(*this);
      case VK_ VK_SHADER_STAGE_COMPUTE_BIT:
        return shaders_.emplace_back<default_compute_shader>(*this);
      case VK_ VK_SHADER_STAGE_GEOMETRY_BIT:
        return shaders_.emplace_back<default_geometry_shader>(*this);
      case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return shaders_.emplace_back<default_tess_control_shader>(*this);
      case VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return shaders_.emplace_back<default_tess_eval_shader>(*this);
      case VK_ VK_SHADER_STAGE_TASK_BIT_EXT:
        return shaders_.emplace_back<default_task_shader>(*this);
      case VK_ VK_SHADER_STAGE_MESH_BIT_EXT:
        return shaders_.emplace_back<default_mesh_shader>(*this);
      default:
        return shaders_.emplace_back<default_shader>(*this, stage);
      }
    }
    default_shader &append_(VK_ VkShaderStageFlagBits stage,
      vector<::std::byte> code, vector<uint32_t> compiled,
      ::std::filesystem::path filepath,
      ::std::string entry_point) {
      default_shader &value = append_(stage);
      value.code = ::std::move(code);
      value.compiled = ::std::move(compiled);
      value.entry_point = ::std::move(entry_point);
      value.filepath = ::std::move(filepath);
      return value;
    }

    void compile_shader_(default_shader &shader) {
      VK_ glslang_stage_t stage = to_glslang_stage(shader.stage);

      auto version_minor = parent_of<instance>(this)->api_version_minor();
      VK_ glslang_input_t shader_input = input;
      shader_input.stage = stage;
      shader_input.code = reinterpret_cast<const char *>(shader.code.data());
      switch (version_minor) {
      case 0:
        shader_input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_0;
        shader_input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_0;
        shader_input.default_version = 450;
        break;
      case 1:
        shader_input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_1;
        shader_input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_3;
        shader_input.default_version = 450;
        break;
      case 2:
        shader_input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_2;
        shader_input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_5;
        shader_input.default_version = 460;
        break;
      case 3:
        shader_input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_3;
        shader_input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_6;
        shader_input.default_version = 460;
        break;
      case 4:
        shader_input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_4;
        shader_input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_6;
        shader_input.default_version = 460;
        break;
      default:
        VKTL_ASSERT(false);
      }

      VK_ glslang_shader_t *c_shader = VK_ glslang_shader_create(&shader_input);
      defer s_defer{[&]() { VK_ glslang_shader_delete(c_shader); }};

      if (shader_input.language == VK_ GLSLANG_SOURCE_HLSL) {
        VK_ glslang_shader_set_options(c_shader,
                                       VK_ GLSLANG_SHADER_AUTO_MAP_BINDINGS);
      }

      if (!VK_ glslang_shader_preprocess(c_shader, &shader_input)) {
        throw error{1, VK_ glslang_shader_get_info_log(c_shader)};
      }

      if (!VK_ glslang_shader_parse(c_shader, &shader_input)) {
        throw error{2, VK_ glslang_shader_get_info_log(c_shader)};
      }

      VK_ glslang_program_t *program = VK_ glslang_program_create();
      defer p_defer{[&]() { VK_ glslang_program_delete(program); }};

      VK_ glslang_program_add_shader(program, c_shader);

      if (!VK_ glslang_program_link(program,
                                    VK_ GLSLANG_MSG_SPV_RULES_BIT |
                                        VK_ GLSLANG_MSG_VULKAN_RULES_BIT)) {
        throw error{3, VK_ glslang_program_get_info_log(program)};
      }

      VK_ glslang_program_SPIRV_generate(program, stage);

      const uint32_t *spirv_ptr = VK_ glslang_program_SPIRV_get_ptr(program);
      size_t word_count = VK_ glslang_program_SPIRV_get_size(program);
      shader.compiled.assign(spirv_ptr, spirv_ptr + word_count);
    }

    static constexpr VK_ glslang_stage_t to_glslang_stage(VK_ VkShaderStageFlagBits stage) {
      switch (stage) {
      case VK_ VK_SHADER_STAGE_VERTEX_BIT:
        return VK_ GLSLANG_STAGE_VERTEX;
      case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return VK_ GLSLANG_STAGE_TESSCONTROL;
      case VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return VK_ GLSLANG_STAGE_TESSEVALUATION;
      case VK_ VK_SHADER_STAGE_GEOMETRY_BIT:
        return VK_ GLSLANG_STAGE_GEOMETRY;
      case VK_ VK_SHADER_STAGE_FRAGMENT_BIT:
        return VK_ GLSLANG_STAGE_FRAGMENT;
      case VK_ VK_SHADER_STAGE_COMPUTE_BIT:
        return VK_ GLSLANG_STAGE_COMPUTE;
      case VK_ VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        return VK_ GLSLANG_STAGE_RAYGEN;
      case VK_ VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return VK_ GLSLANG_STAGE_ANYHIT;
      case VK_ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return VK_ GLSLANG_STAGE_CLOSESTHIT;
      case VK_ VK_SHADER_STAGE_MISS_BIT_KHR:
        return VK_ GLSLANG_STAGE_MISS;
      case VK_ VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return VK_ GLSLANG_STAGE_INTERSECT;
      case VK_ VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        return VK_ GLSLANG_STAGE_CALLABLE;
      case VK_ VK_SHADER_STAGE_TASK_BIT_EXT:
        return VK_ GLSLANG_STAGE_TASK;
      case VK_ VK_SHADER_STAGE_MESH_BIT_EXT:
        return VK_ GLSLANG_STAGE_MESH;
      default:
        return VK_ GLSLANG_STAGE_COUNT;
      }
    }

  protected:
    VK_ glslang_input_t input{
        .language = VK_ GLSLANG_SOURCE_GLSL,
        .client = VK_ GLSLANG_CLIENT_VULKAN,
        .client_version = VK_ GLSLANG_TARGET_VULKAN_1_0,
        .target_language = VK_ GLSLANG_TARGET_SPV,
        .target_language_version = VK_ GLSLANG_TARGET_SPV_1_0,
        .default_version = 430,
        .default_profile = VK_ GLSLANG_NO_PROFILE,
        .messages = VK_ GLSLANG_MSG_DEFAULT_BIT,
    };

    poly_list shaders_;
  };

  using namespace compiler_extensions;

  template <typename N> struct m<glsl_, N> : N {
    m(glsl_, auto &&...others) : N{forward_(others)...} {
      N::input.language = VK_ GLSLANG_SOURCE_GLSL;
    }
  };

  template <typename N> struct m<hlsl_, N> : N {
    constexpr m(hlsl_, auto &&...others) : N{forward_(others)...} {
      N::input.language = VK_ GLSLANG_SOURCE_HLSL;
    }
  };

  template <typename N> struct m<optimize, N> : N {
    constexpr m(optimize, auto &&...others) : N{forward_(others)...} {}
    template <::std::invocable<default_shader &> Fn = call_duck_>
    void init(Fn &&fn = call_duck) {
      N::init([&](default_shader &shader) {
        if (!shader.compiled.empty()) {
          spv_target_env target_env;
          auto minor = parent_of<instance>(this)->api_version_minor();
          switch (minor) {
          case 0:
            target_env = VK_ SPV_ENV_VULKAN_1_0;
            break;
          case 1:
            target_env = VK_ SPV_ENV_VULKAN_1_1;
            break;
          case 2:
            target_env = VK_ SPV_ENV_VULKAN_1_2;
            break;
          case 3:
            target_env = VK_ SPV_ENV_VULKAN_1_3;
            break;
          case 4:
            target_env = VK_ SPV_ENV_VULKAN_1_4;
            break;
          default:
            VKTL_ASSERT(false);
          }
          auto *option = VK_ spvOptimizerOptionsCreate();
          defer _0{[&]() { VK_ spvOptimizerOptionsDestroy(option); }};
          auto *optimizer = VK_ spvOptimizerCreate(target_env);
          if (!optimizer) {
            throw error{0x500, "[COMPILER] Create optmizer failure."};
          }
          defer _1{[&]() { VK_ spvOptimizerDestroy(optimizer); }};
          VK_ spvOptimizerOptionsSetPreserveBindings(
              option, params_.reserve_unused_bindings);
          VK_ spvOptimizerOptionsSetPreserveSpecConstants(
              option, params_.reserve_unused_spec_constants);
          VK_ spvOptimizerOptionsSetRunValidator(option, params_.validate);

          if constexpr (::std::invocable<Fn &, VK_ spv_optimizer_t *>) {
            fn(optimizer);
          }
          VK_ spvOptimizerRegisterPerformancePasses(optimizer);
          VK_ spv_binary optimized_binary = nullptr;
          VK_ spv_result_t result = VK_ spvOptimizerRun(
              optimizer, shader.compiled.data(), shader.compiled.size(),
              &optimized_binary, nullptr);
          if (result == VK_ SPV_SUCCESS && optimized_binary) {
            shader.compiled.assign(optimized_binary->code,
                                   optimized_binary->code +
                                       optimized_binary->wordCount);
            VK_ spvBinaryDestroy(optimized_binary);
          }
        }

        fn(shader);
      });
    }

  private:
    optimize params_;
  };

  template <typename N> struct m<contain_debug_info_, N> : N {
    m(contain_debug_info_, auto &&...others) : N{forward_(others)...} {
      N::input.messages = static_cast<VK_ glslang_messages_t>(
          N::input.messages | VK_ GLSLANG_MSG_DEBUG_INFO_BIT);
    }
  };

  template <typename N> struct m<customize_include_, N> : N {
    constexpr m(customize_include_ tag, auto &&...others)
        : N{forward_(others)...} {
      relocate_this();
    }

    void relocate() {
      N::relocate();
      relocate_this();
    }

    void append(::std::filesystem::path include_dir) {
      search_paths_.push_back(::std::move(include_dir));
    }

    void append(::std::string header_name, ::std::string content) {
      string_files_[::std::move(header_name)] = ::std::move(content);
    }

  private:
    ::std::string load_header(const ::std::string &filename) const {
      if (auto it = string_files_.find(filename); it != string_files_.end()) {
        return it->second;
      }
      for (const auto &path : search_paths_) {
        auto full_path = path / filename;
        if (::std::filesystem::is_regular_file(full_path)) {
          ::std::ifstream file(full_path, ::std::ios::binary);
          if (file) {
            ::std::string content(::std::filesystem::file_size(full_path),
                                  '\0');
            file.read(content.data(), content.size());
            return content;
          }
        }
      }
      return {};
    }

    static void *on_include_resolve(void *user_data, const char *header_name) {
      auto *self = static_cast<m *>(user_data);
      auto content = self->load_header(header_name);
      if (!content.size()) {
        return nullptr;
      } else {
        return content.data();
      }
    }

    void relocate_this() {
      N::input.callback = &callback_;
      N::input.callback_ctx = this;
    }

  private:
    vector<::std::filesystem::path> search_paths_;
    umap<::std::string, ::std::string> string_files_;

    VK_ glsl_include_callbacks_t callback_{};
  };
}
