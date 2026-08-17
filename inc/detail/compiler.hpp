#pragma once

// Interface style: compiler objects collect source or SPIR-V shaders and add
// language/optimization behavior through composable extension tags.
// Implementation: heterogeneous shader records live in a polymorphic list;
// compilation and reflection are deferred until initialization.

#if !defined(VKTL_NO_COMPILER)

//#if !defined(VKTL_NO_PRA_LIB)
//// # pragma comment(lib, "glslc")
//#endif

namespace VK_NAMESPACE {
#define SPIRV_REFLECT_DISABLE_CPP_BINDINGS
#include <spirv-tools/libspirv.h>
#include <glslang/Include/glslang_c_interface.h>
#include "../../external/SPIRV-Reflect/spirv_reflect.h"
}

#if VKTL_HAVE_STD_
# include <filesystem>
# include <fstream>
# include <unordered_map>
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

		template<typename Self, typename Parent>
		explicit default_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref, VK_ VkShaderStageFlagBits stage)
			: shader_handle_tag{}, parent(&parent_ref), stage(stage) {
		}

		template<typename Parent>
		explicit default_shader(Parent& parent_ref, VK_ VkShaderStageFlagBits stage)
			: default_shader(::std::in_place_type<default_shader>, parent_ref, stage) {
		}
	};

	struct default_vertex_shader : default_shader {
		vector<VK_ VkVertexInputBindingDescription> bindings;
		vector<VK_ VkVertexInputAttributeDescription> locations;

		template<typename Self = default_vertex_shader, typename Parent>
		explicit default_vertex_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_VERTEX_BIT) {
		}

		template<typename Parent>
		explicit default_vertex_shader(Parent& parent_ref)
			: default_vertex_shader(::std::in_place_type<default_vertex_shader>, parent_ref) {
		}

	};

	struct default_fragment_shader : default_shader {
		bool depth_write_enable = true;
		bool depth_test_enable = true;

		template<typename Self = default_fragment_shader, typename Parent>
		explicit default_fragment_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_FRAGMENT_BIT) {
		}

		template<typename Parent>
		explicit default_fragment_shader(Parent& parent_ref)
			: default_fragment_shader(::std::in_place_type<default_fragment_shader>, parent_ref) {
		}
	};

	struct default_compute_shader : default_shader {
		uint32_t x = 1;
		uint32_t y = 1;
		uint32_t z = 1;

		template<typename Self = default_compute_shader, typename Parent>
		explicit default_compute_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_COMPUTE_BIT) {
		}
		template<typename Parent>
		explicit default_compute_shader(Parent& parent_ref)
			: default_compute_shader(::std::in_place_type<default_compute_shader>, parent_ref) {
		}
	};

	struct default_geometry_shader : default_shader {
		uint32_t max_output_vertices = 0;
		uint32_t invocations = 1;

		template<typename Self = default_geometry_shader, typename Parent>
		explicit default_geometry_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_GEOMETRY_BIT) {
		}

		template<typename Parent>
		explicit default_geometry_shader(Parent& parent_ref)
			: default_geometry_shader(::std::in_place_type<default_geometry_shader>, parent_ref) {
		}
	};

	struct default_tess_control_shader : default_shader {
		uint32_t patch_control_points = 3;

		template<typename Self = default_tess_control_shader, typename Parent>
		explicit default_tess_control_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
		}

		template<typename Parent>
		explicit default_tess_control_shader(Parent& parent_ref)
			: default_tess_control_shader(::std::in_place_type<default_tess_control_shader>, parent_ref) {
		}
	};

	struct default_tess_eval_shader : default_shader {
		VK_ VkPrimitiveTopology primitive_mode = VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		template<typename Self = default_tess_eval_shader, typename Parent>
		explicit default_tess_eval_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
		}

		template<typename Parent>
		explicit default_tess_eval_shader(Parent& parent_ref)
			: default_tess_eval_shader(::std::in_place_type<default_tess_eval_shader>, parent_ref) {
		}
	};

	struct default_task_shader : default_shader {
		uint32_t x = 1;
		uint32_t y = 1;
		uint32_t z = 1;

		template<typename Self = default_task_shader, typename Parent>
		explicit default_task_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_TASK_BIT_EXT) {
		}

		template<typename Parent>
		explicit default_task_shader(Parent& parent_ref)
			: default_task_shader(::std::in_place_type<default_task_shader>, parent_ref) {
		}
	};

	struct default_mesh_shader : default_shader {
		uint32_t x = 1;
		uint32_t y = 1;
		uint32_t z = 1;
		uint32_t max_vertices = 0;
		uint32_t max_primitives = 0;

		template<typename Self = default_mesh_shader, typename Parent>
		explicit default_mesh_shader(::std::in_place_type_t<Self> tag, Parent& parent_ref)
			: default_shader(tag, parent_ref, VK_ VK_SHADER_STAGE_MESH_BIT_EXT) {
		}

		template<typename Parent>
		explicit default_mesh_shader(Parent& parent_ref)
			: default_mesh_shader(::std::in_place_type<default_mesh_shader>, parent_ref) {
		}
	};
}

VKTL_EXPORT_ namespace vktl::detail {

	namespace compile {
		inline auto glsl_refc_ = 0u;
		inline auto acquire() {
			if (glsl_refc_ == 0u && !glslang_initialize_process()) {
				throw error{ 0x42u, "[COMPILER] Initialize compiler failure." };
			}
			return ++glsl_refc_;
		}
		inline auto release() noexcept {
			if (glsl_refc_ > 0u && glsl_refc_ == 1u) {
				glslang_finalize_process();
			}
			return --glsl_refc_;
		}

		inline void duck_invoker_(default_shader&) {}
	}

	template<typename N>
	struct m<compiler_, N> : N {
		m(compiler_, auto&&...others)
			: N{ forward_(others)... } {
			compile::acquire();
		}

		~m() { compile::release(); }

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
			return &append_(stage, {}, 
				::std::vector<::std::uint32_t>(reinterpret_cast<const uint32_t*>(code.data()), reinterpret_cast<const uint32_t*>(code.data() + code.size())), 
				{}, ::std::move(entry_point));
		}

		void init() { init(&compile::duck_invoker_); }

		void init(::std::invocable<default_shader&> auto&& func) {
			::std::lock_guard _{ N::get_lock() };
			for (default_shader& shader : shaders_) {
				if (shader.compiled.empty()) {
					compile_shader_(shader);
				}
				if (shader.bindings.empty()) {
					reflect_shader_(shader);
				}

				func(shader);
			}
		}

		void reset() { shaders_.clear(); }

	protected:
		template<typename T>
		void enumerate(T val) {}

		template<typename T, typename Fn, typename... Fns>
		void enumerate(T val, Fn& fn, Fns&... fns) {
			if constexpr (::std::invocable<Fn&, T>) { fn(val); }
			enumerate(val, fns...);
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

	private:
		static auto read_spirv(::std::filesystem::path const& path) {
			const auto size = ::std::filesystem::file_size(path);
			if (size % sizeof(::std::uint32_t) != 0) {
				auto err_str = "Invalid SPIR-V size: " + path.string();
				throw error{ 0x402, err_str.data() };
			}

			::std::vector<::std::uint32_t> buffer(size / sizeof(::std::uint32_t));
			::std::ifstream file(path, ::std::ios::binary);
			if (!file.is_open()) {
				auto err = "Failed to open file: " + path.string();
				throw error{ 0x403, err.data() };
			}

			file.read(reinterpret_cast<char*>(buffer.data()), size);
			return buffer;
		}
		static auto read_code(::std::filesystem::path const& path) {
			const auto size = ::std::filesystem::file_size(path);

			::std::vector<::std::byte> buffer(size, ::std::byte(0));
			::std::ifstream file(path, ::std::ios::binary);
			if (!file.is_open()) {
				auto err = "Failed to open file: " + path.string();
				throw error{ 0x402, err.data() };
			}

			file.read(reinterpret_cast<char*>(buffer.data()), size);
			return buffer;
		}

		default_shader& append_(VK_ VkShaderStageFlagBits stage) {
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
		default_shader& append_(VK_ VkShaderStageFlagBits stage, vector<::std::byte> code, vector<uint32_t> compiled, 
			::std::filesystem::path filepath, ::std::string entry_point) {
			default_shader& value = append_(stage);
			value.code = ::std::move(code);
			value.compiled = ::std::move(compiled);
			value.entry_point = ::std::move(entry_point);
			value.filepath = ::std::move(filepath);
			return value;
		}

		void compile_shader_(default_shader& shader) {
			VK_ glslang_stage_t stage = to_glslang_stage(shader.stage);

			auto version_minor = parent_of<instance>(this)->api_version_minor();
			VK_ glslang_input_t shader_input = input;
			shader_input.stage = stage;
			shader_input.code = reinterpret_cast<const char*>(shader.code.data());
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
			default:assert(false);
			}

			VK_ glslang_shader_t* c_shader = VK_ glslang_shader_create(&shader_input);
			defer s_defer{ [&]() { VK_ glslang_shader_delete(c_shader); } };

			if (shader_input.language == VK_ GLSLANG_SOURCE_HLSL) {
				VK_ glslang_shader_set_options(c_shader, VK_ GLSLANG_SHADER_AUTO_MAP_BINDINGS);
			}

			if (!VK_ glslang_shader_preprocess(c_shader, &shader_input)) {
				throw error{ 1, VK_ glslang_shader_get_info_log(c_shader) };
			}

			if (!VK_ glslang_shader_parse(c_shader, &shader_input)) {
				throw error{ 2, VK_ glslang_shader_get_info_log(c_shader) };
			}

			VK_ glslang_program_t* program = VK_ glslang_program_create();
			defer p_defer{ [&]() { VK_ glslang_program_delete(program); } };

			VK_ glslang_program_add_shader(program, c_shader);

			if (!VK_ glslang_program_link(program, VK_ GLSLANG_MSG_SPV_RULES_BIT | VK_ GLSLANG_MSG_VULKAN_RULES_BIT)) {
				throw error{ 3, VK_ glslang_program_get_info_log(program) };
			}

			VK_ glslang_program_SPIRV_generate(program, stage);

			const uint32_t* spirv_ptr = VK_ glslang_program_SPIRV_get_ptr(program);
			size_t word_count = VK_ glslang_program_SPIRV_get_size(program);
			shader.compiled.assign(spirv_ptr, spirv_ptr + word_count);
		}

		static void reflect_shader_(default_shader& shader) {
			VK_ SpvReflectShaderModule spv_module;
			VK_ SpvReflectResult reflect_res = VK_ spvReflectCreateShaderModule(
				shader.compiled.size() * sizeof(uint32_t),
				shader.compiled.data(),
				&spv_module
			);
			if (reflect_res != VK_ SPV_REFLECT_RESULT_SUCCESS) {
				throw error{ reflect_res, "[SPIRV-Reflect] Failed to create module." };
			}
			defer reflect_guard{ [&]() { VK_ spvReflectDestroyShaderModule(&spv_module); } };

			uint32_t set_count = 0;
			VK_ spvReflectEnumerateDescriptorSets(&spv_module, &set_count, nullptr);
			vector<VK_ SpvReflectDescriptorSet*> reflect_sets(set_count);
			VK_ spvReflectEnumerateDescriptorSets(&spv_module, &set_count, reflect_sets.data());

			shader.bindings.clear();
			for (const auto* set : reflect_sets) {
				if (set->set >= shader.bindings.size()) {
					shader.bindings.resize(set->set + 1);
				}
				for (uint32_t i = 0; i < set->binding_count; ++i) {
					const auto* b = set->bindings[i];
					VK_ VkDescriptorSetLayoutBinding binding{};
					binding.binding = b->binding;
					binding.descriptorType = static_cast<VK_ VkDescriptorType>(b->descriptor_type);
					binding.descriptorCount = b->count;
					binding.stageFlags = shader.stage;
					binding.pImmutableSamplers = nullptr;

					shader.bindings[set->set].push_back(binding);
				}
			}

			uint32_t pc_count = 0;
			VK_ spvReflectEnumeratePushConstantBlocks(&spv_module, &pc_count, nullptr);
			vector<VK_ SpvReflectBlockVariable*> pc_blocks(pc_count);
			VK_ spvReflectEnumeratePushConstantBlocks(&spv_module, &pc_count, pc_blocks.data());

			shader.push_constants.clear();
			for (const auto* pc : pc_blocks) {
				VK_ VkPushConstantRange range{};
				range.stageFlags = shader.stage;
				range.offset = pc->offset;
				range.size = pc->size;
				shader.push_constants.push_back(range);
			}

			switch (shader.stage) {
				case VK_ VK_SHADER_STAGE_VERTEX_BIT : {
					auto& vert_shader = static_cast<default_vertex_shader&>(shader);
					if (vert_shader.locations.empty()) {
						uint32_t input_count = 0;
						VK_ spvReflectEnumerateInputVariables(&spv_module, &input_count, nullptr);
						vector<SpvReflectInterfaceVariable*> input_vars(input_count);
						VK_ spvReflectEnumerateInputVariables(&spv_module, &input_count, input_vars.data());

						for (const auto* input_var : input_vars) {
							if (input_var->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;

							VK_ VkVertexInputAttributeDescription attr{};
							attr.location = input_var->location;
							attr.binding = 0;
							attr.format = static_cast<VK_ VkFormat>(input_var->format);
							attr.offset = 0;

							vert_shader.locations.push_back(attr);
						}
					}
				} break;

				case VK_ VK_SHADER_STAGE_FRAGMENT_BIT : {
					// auto& frag_shader = static_cast<default_fragment_shader&>(shader);
					// uint32_t output_count = 0;
					// VK_ spvReflectEnumerateOutputVariables(&spv_module, &output_count, nullptr);
					// vector<VK_ SpvReflectInterfaceVariable*> output_vars(output_count);
					// VK_ spvReflectEnumerateOutputVariables(&spv_module, &output_count, output_vars.data());

					// frag_shader.blend_attachments.clear();
					// for (const auto* out_var : output_vars) {
					// 	if (out_var->decoration_flags & VK_ SPV_REFLECT_DECORATION_BUILT_IN) continue;
					// 
					// 	blend.colorWriteMask = VK_ VK_COLOR_COMPONENT_R_BIT | VK_ VK_COLOR_COMPONENT_G_BIT |
					// 		VK_ VK_COLOR_COMPONENT_B_BIT | VK_ VK_COLOR_COMPONENT_A_BIT;
					// 	blend.blendEnable = VK_FALSE;
					// 	frag_shader.blend_attachments.push_back(blend);
					// }
				} break;

				case VK_ VK_SHADER_STAGE_COMPUTE_BIT : {
					auto& comp_shader = static_cast<default_compute_shader&>(shader);
					if (spv_module.entry_point_count > 0) {
						comp_shader.x = spv_module.entry_points[0].local_size.x;
						comp_shader.y = spv_module.entry_points[0].local_size.y;
						comp_shader.z = spv_module.entry_points[0].local_size.z;
					}
				} break;

				case VK_ VK_SHADER_STAGE_GEOMETRY_BIT : {
					// auto& geom_shader = static_cast<default_geometry_shader&>(shader);
					
					// if (spv_module.entry_point_count > 0) {
					// 	geom_shader.invocations = spv_module.entry_points[0].geometry.invocations;
					// 	geom_shader.max_output_vertices = spv_module.entry_points[0].geometry.vertices;
					// }
				} break;

				case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT : {
					// auto& tess_ctrl = static_cast<default_tess_control_shader&>(shader);
					// if (spv_module.entry_point_count > 0) {
					// 	tess_ctrl.patch_control_points = spv_module.entry_points[0].tessellation.output_vertices;
					// }
				} break;

				case VK_ VK_SHADER_STAGE_TASK_BIT_EXT : {
					// auto& task_shader = static_cast<default_task_shader&>(shader);
					// if (spv_module.entry_point_count > 0) {
					// 	task_shader.x = spv_module.entry_points[0].local_size.x;
					// 	task_shader.y = spv_module.entry_points[0].local_size.y;
					// 	task_shader.z = spv_module.entry_points[0].local_size.z;
					// }
				} break;

				case VK_ VK_SHADER_STAGE_MESH_BIT_EXT : {
					// auto& mesh_shader = static_cast<default_mesh_shader&>(shader);
					// if (spv_module.entry_point_count > 0) {
					// 	mesh_shader.x = spv_module.entry_points[0].local_size.x;
					// 	mesh_shader.y = spv_module.entry_points[0].local_size.y;
					// 	mesh_shader.z = spv_module.entry_points[0].local_size.z;
					// 	mesh_shader.max_vertices = spv_module.entry_points[0].mesh.max_vertices;
					// 	mesh_shader.max_primitives = spv_module.entry_points[0].mesh.max_primitives;
					// }
				} break;

				default: break;
			}
		}

		static constexpr VK_ glslang_stage_t to_glslang_stage(VK_ VkShaderStageFlagBits stage) {
			switch (stage) {
				case VK_ VK_SHADER_STAGE_VERTEX_BIT : return VK_ GLSLANG_STAGE_VERTEX;
					case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT : return VK_ GLSLANG_STAGE_TESSCONTROL;
						case VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT : return VK_ GLSLANG_STAGE_TESSEVALUATION;
							case VK_ VK_SHADER_STAGE_GEOMETRY_BIT : return VK_ GLSLANG_STAGE_GEOMETRY;
								case VK_ VK_SHADER_STAGE_FRAGMENT_BIT : return VK_ GLSLANG_STAGE_FRAGMENT;
									case VK_ VK_SHADER_STAGE_COMPUTE_BIT : return VK_ GLSLANG_STAGE_COMPUTE;
										case VK_ VK_SHADER_STAGE_RAYGEN_BIT_KHR : return VK_ GLSLANG_STAGE_RAYGEN;
											case VK_ VK_SHADER_STAGE_ANY_HIT_BIT_KHR : return VK_ GLSLANG_STAGE_ANYHIT;
												case VK_ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR : return VK_ GLSLANG_STAGE_CLOSESTHIT;
													case VK_ VK_SHADER_STAGE_MISS_BIT_KHR : return VK_ GLSLANG_STAGE_MISS;
														case VK_ VK_SHADER_STAGE_INTERSECTION_BIT_KHR : return VK_ GLSLANG_STAGE_INTERSECT;
															case VK_ VK_SHADER_STAGE_CALLABLE_BIT_KHR : return VK_ GLSLANG_STAGE_CALLABLE;
																case VK_ VK_SHADER_STAGE_TASK_BIT_EXT : return VK_ GLSLANG_STAGE_TASK;
																	case VK_ VK_SHADER_STAGE_MESH_BIT_EXT : return VK_ GLSLANG_STAGE_MESH;
																	default: return VK_ GLSLANG_STAGE_COUNT;
			}
		}
	};

	using namespace compiler_extensions;

	template<typename N>
	struct m<glsl_, N> : N {
		m(glsl_, auto&&...others) : N{ forward_(others)... } {
			N::input.language = VK_ GLSLANG_SOURCE_GLSL;
		}
	};

	template<typename N>
	struct m<hlsl_, N> : N {
		constexpr m(hlsl_, auto&&...others) : N{ forward_(others)... } {
			N::input.language = VK_ GLSLANG_SOURCE_HLSL;
		}
	};

	template<typename N>
	struct m<optimize, N> : N {
		constexpr m(optimize, auto&&...others)
			: N{ forward_(others)... } {
		}
		template<::std::invocable<default_shader&>  Fn = call_duck_>
		void init(Fn&& fn = call_duck) {
			N::init([&](default_shader& shader) {
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
					default:assert(false);
					}
					auto* option = VK_ spvOptimizerOptionsCreate();
					defer _0{ [&]() { VK_ spvOptimizerOptionsDestroy(option); } };
					auto* optimizer = VK_ spvOptimizerCreate(target_env);
					if (!optimizer) {
						throw error{ 0x500, "[COMPILER] Create optmizer failure." };
					}
					defer _1{ [&]() { VK_ spvOptimizerDestroy(optimizer); } };
					VK_ spvOptimizerOptionsSetPreserveBindings(option, params_.reserve_unused_bindings);
					VK_ spvOptimizerOptionsSetPreserveSpecConstants(option, params_.reserve_unused_spec_constants);
					VK_ spvOptimizerOptionsSetRunValidator(option, params_.validate);
					
					if constexpr (::std::invocable<Fn&, VK_ spv_optimizer_t*>) {
						fn(optimizer);
					}
					VK_ spvOptimizerRegisterPerformancePasses(optimizer);
					VK_ spv_binary optimized_binary = nullptr;
					VK_ spv_result_t result = VK_ spvOptimizerRun(
						optimizer,
						shader.compiled.data(),
						shader.compiled.size(),
						&optimized_binary,
						nullptr
					);
					if (result == VK_ SPV_SUCCESS && optimized_binary) {
						shader.compiled.assign(optimized_binary->code, optimized_binary->code + optimized_binary->wordCount);
						VK_ spvBinaryDestroy(optimized_binary);
					}
				}

				fn(shader);
			});
		}
	private:
		optimize params_;
	};

	// template<uint32_t Major, uint32_t Minor, typename N>
	// struct m<vulkan_version<Major, Minor>, N> : N {
	// 	m(vulkan_version<Major, Minor>, auto&&...others) : N{ forward_(others)... } {
	// 		if constexpr (Major == 1 && Minor == 1) {
	// 			N::input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_1;
	// 			N::input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_3;
	// 		}
	// 		else if constexpr (Major == 1 && Minor == 2) {
	// 			N::input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_2;
	// 			N::input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_5;
	// 		}
	// 		else if constexpr (Major == 1 && Minor == 3) {
	// 			N::input.client_version = VK_ GLSLANG_TARGET_VULKAN_1_3;
	// 			N::input.target_language_version = VK_ GLSLANG_TARGET_SPV_1_6;
	// 		}
	// 	}
	// };

	template<typename N>
	struct m<contain_debug_info_, N> : N {
		m(contain_debug_info_, auto&&...others) : N{ forward_(others)... } {
			N::input.messages = static_cast<VK_ glslang_messages_t>(N::input.messages | VK_ GLSLANG_MSG_DEBUG_INFO_BIT);
		}
	};

	template<typename N>
	struct m<customize_include_, N> : N {
		constexpr m(customize_include_ tag, auto&&... others)
			: N{ forward_(others)... } {
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
		::std::string load_header(const::std::string& filename) const {
			if (auto it = string_files_.find(filename); it != string_files_.end()) {
				return it->second;
			}
			for (const auto& path : search_paths_) {
				auto full_path = path / filename;
				if (::std::filesystem::is_regular_file(full_path)) {
					::std::ifstream file(full_path, ::std::ios::binary);
					if (file) {
						::std::string content(::std::filesystem::file_size(full_path), '\0');
						file.read(content.data(), content.size());
						return content;
					}
				}
			}
			return {};
		}

		static void* on_include_resolve(void* user_data, const char* header_name) {
			auto* self = static_cast<m*>(user_data);
			auto content = self->load_header(header_name);
			if (!content.size()) { return nullptr; }
			else { return content.data(); }
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

	// template<typename N>
	// struct m<defines, N> : N {
	// 	using N::N;
	// 
	// 	void add_define(::std::string key, ::std::string value = "1") {
	// 		defines_[::std::move(key)] = ::std::move(value);
	// 	}
	// 
	// 	void init(::std::invocable<default_shader&> auto&& fn = compile::duck_invoker_) {
	// 		if (!defines_.empty()) {
	// 			::std::string preamble;
	// 			for (const auto& [k, v] : defines_) {
	// 				preamble += "#define " + k + " " + v + "\n";
	// 			}
	// 
	// 			for (default_shader& shader : N::shaders_) {
	// 				if (!shader.code.empty()) {
	// 					::std::string code_str(reinterpret_cast<const char*>(shader.code.data()), shader.code.size());
	// 					code_str = preamble + code_str;
	// 					shader.code.assign(
	// 						reinterpret_cast<const ::std::byte*>(code_str.data()),
	// 						reinterpret_cast<const ::std::byte*>(code_str.data() + code_str.size())
	// 					);
	// 				}
	// 			}
	// 		}
	// 
	// 		N::init(forward_(fn));
	// 	}
	// 
	// private:
	// 	::std::unordered_map<::std::string, ::std::string> defines_;
	// };

	// template<typename N>
	// struct m<disassembler, N> : N {
	// 	using N::N;
	// 
	// 	::std::string disassemble(const default_shader& shader) {
	// 		if (shader.compiled.empty()) return {};
	// 
	// 		VK_ spv_context context = VK_ spvContextCreate(VK_ SPV_ENV_VULKAN_1_0);
	// 		defer _{ [&]() { VK_ spvContextDestroy(context); } };
	// 
	// 		VK_ spv_text text = nullptr;
	// 		VK_ spv_diagnostic diagnostic = nullptr;
	// 
	// 		VK_ spv_result_t result = VK_ spvBinaryToText(
	// 			context,
	// 			shader.compiled.data(),
	// 			shader.compiled.size(),
	// 			SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES,
	// 			&text,
	// 			&diagnostic
	// 		);
	// 
	// 		if (result != VK_ SPV_SUCCESS || !text) {
	// 			if (diagnostic) VK_ spvDiagnosticDestroy(diagnostic);
	// 			return {};
	// 		}
	// 
	// 		::std::string disasm_str(text->str, text->length);
	// 		VK_ spvTextDestroy(text);
	// 		return disasm_str;
	// 	}
	// };
}

#endif
