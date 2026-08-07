#pragma once

namespace VK_NAMESPACE {
#include <spirv-tools/libspirv.h>
#include <glslang/Include/glslang_c_interface.h>
#include "../../external/SPIRV-Reflect/spirv_reflect.h"
}

VKTL_EXPORT_ namespace vktl::detail {

	struct default_shader : shader_handle_tag {
		VK_ VkShaderStageFlagBits stage;
		vector<::std::byte> code;
		vector<::std::byte> compiled;

		::std::string filename;
		::std::string entry_point;

		VK_ VkShaderModule module = nullptr;

		vector<VK_ VkDescriptorSetLayoutBinding> bindings;
	};

	struct default_vertex_shader : default_shader {
		vectors<VK_ VkVertexInputAttributeDescription> location_infos;
	};

	template<typename N>
	struct m<compiler_, N> : N {
		m(compiler_, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		shader_handle append(byte_view code, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main", ::std::string filename = {}) {
			return append_(code, stage, ::std::move(entry_point), ::std::move(filename));
		}
		shader_handle append_compiled(byte_view code, VK_ VkShaderStageFlagBits stage, ::std::string entry_point = "main", ::std::string filename = {}) {
			default_shader& value = append_(code, stage, ::std::move(entry_point), ::std::move(filename));
			value.compiled = ::std::move(value.code);
			return &value;
		}

		template<typename Fn>
		void init(Fn&& func) {
			// N::init();
			for (default_shader& shader : shaders_) {
				if (!shader.compiled.size()) {
					func(shader);
				}
				assert(shader.compiled.size());
				if (!shader.bindings.size()) {
					VK_ SpvReflectShaderModule spv_module;
					VK_ SpvReflectResult reflect_res = VK_ spvReflectCreateShaderModule(
						shader.compiled.size(),
						shader.compiled.data(),
						&spv_module
					);
					if (reflect_res != SPV_REFLECT_RESULT_SUCCESS) {
						throw error{ uint32_t(reflect_res), "[SPIRV-Reflect] Failed to create module." };
					}
					defer reflect_guard{ [&]() { spvReflectDestroyShaderModule(&spv_module); } };

					uint32_t binding_count = 0;
					VK_ spvReflectEnumerateDescriptorBindings(&spv_module, &binding_count, nullptr);

					vector<SpvReflectDescriptorBinding*> reflect_bindings(binding_count);
					VK_ spvReflectEnumerateDescriptorBindings(&spv_module, &binding_count, reflect_bindings.data());

					shader.bindings.clear();
					shader.bindings.reserve(binding_count);

					for (const auto* b : reflect_bindings) {
						VK_ VkDescriptorSetLayoutBinding binding{};
						binding.binding = b->binding;
						binding.descriptorType = static_cast<VK_ VkDescriptorType>(b->descriptor_type);
						binding.descriptorCount = b->count;
						binding.stageFlags = shader.stage;
						binding.pImmutableSamplers = nullptr;

						shader.bindings.push_back(::std::move(binding));
					}
				}
				if (auto hdv = handle_from<N, device>()) {
					assert(hdv);
					VK_ VkShaderModuleCreateInfo cinfo{
						.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
						.codeSize = uint32_t(shader.compiled.size()),
						.pCode = reinterpret_cast<uint32_t const*>(shader.compiled.data()),
					};
					VK_ vkCreateShaderModule(hdv, &cinfo, N::allocator(), &shader.module)
						| popup{ "[COMPILER] Create shader module failure." };
				}
			}
		}

		void reset() { shaders_.clear(); }

	protected:
		default_shader& append_(byte_view code, VK_ VkShaderStageFlagBits stage) {
			switch (stage) {
				case VK_ VK_SHADER_STAGE_VERTEX_BIT :
					return shaders_.emplace_back<default_vertex_shader>(stage, ::std::vector{ code.begin(), code.end() });

				default:
					return shaders_.emplace_back<default_shader>(stage, ::std::vector{ code.begin(), code.end() });
			}
		}

		default_shader& append_(byte_view code, VK_ VkShaderStageFlagBits stage, ::std::string entry_point, ::std::string filename) {
			auto& value = append_(code, stage);
			value.entry_point = ::std::move(entry_point);
			value.filename = ::std::move(filename);
		}

	private:
		template<typename T>
		void enmerate(T val) {}
		template<typename T, typename Fn>
		void enmerate(T val, Fn& fn, auto&...fns) {
			if constexpr (::std::invocable<Fn&, T>) { fn(val); }
			enumerate(val, fns...);
		}

	private:
		poly_list shaders_;
	};

	using namespace compiler_extensions;

	template<typename N>
	struct m<glsl_, N> : N {
		
		constexpr m() : N{} {

		}

		void init() {
			if (!glsl_refc_++) {
				VK_ glslang_initialize_process();
			}
			N::init([&](default_shader& shader) {
				VK_ glslang_stage_t stage = to_glslang_stage(shader.stage);
				enumerate(stage, tuner...);

				auto& code = shader.code;
				auto& name = shader.filename;
				auto& entry_point = shader.entry_point;

				VK_ glslang_input_t input = {
					.language = GLSLANG_SOURCE_GLSL,
					.stage = stage,
					.client = GLSLANG_CLIENT_VULKAN,
					.client_version = GLSLANG_TARGET_VULKAN_1_2,
					.target_language = GLSLANG_TARGET_SPV,
					.target_language_version = GLSLANG_TARGET_SPV_1_5,
					.code = reinterpret_cast<const char*>(code.data()),
					.default_version = 100,
					.default_profile = GLSLANG_NO_PROFILE,
					.force_default_version_and_profile = false,
					.forward_compatible = false,
					.messages = GLSLANG_MSG_DEFAULT_BIT,
					.resource = glslang_default_resource(),
				};

				VK_ glslang_shader_t* c_shader = VK_ glslang_shader_create(&input);
				defer s_defer{ [&]() { VK_ glslang_shader_delete(c_shader); } };

				if (!VK_ glslang_shader_preprocess(c_shader, &input)) {
					throw error{ 1, VK_ glslang_shader_get_info_log(c_shader) };
				}

				if (!glslang_shader_parse(c_shader, &input)) {
					throw error{ 2, VK_ glslang_shader_get_info_log(c_shader) };
				}

				VK_ glslang_program_t* program = VK_ glslang_program_create();
				defer p_defer{ [&]() { VK_ glslang_program_delete(program); } };

				VK_ glslang_program_add_shader(program, c_shader);

				if (!VK_ glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
					throw error{ 3, VK_ glslang_program_get_info_log(program) };
				}

				VK_ glslang_program_SPIRV_generate(program, stage);

				size_t word_count = glslang_program_SPIRV_get_size(program);
				auto begin = reinterpret_cast<const std::byte*>(glslang_program_SPIRV_get_ptr(program));
				size_t byte_count = word_count * sizeof(uint32_t); // 注意：glslang 返回的是 32 位 word 数，需乘以 4 得到字节数

				shader.compiled = { begin, begin + byte_count };
			});
		}

	private:
		static::std::uint32_t glsl_refc_ = 0u;
	};

	template<typename N>
	struct m<hlsl_, N> : N {

	};
}