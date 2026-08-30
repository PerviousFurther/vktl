#pragma once

VKTL_EXPORT_ namespace vktl::detail {

  template<typename T>
  constexpr VK_ VkShaderStageFlagBits stage_of = VK_ VK_SHADER_STAGE_ALL;

	using namespace pipe_extensions;

  namespace pipe_shader {
     inline constexpr char default_entry[] = "main";
  } // namespace pipe_shader

  template<typename N> struct basic_shader : N {
    template<typename S>
    constexpr basic_shader(S shader, auto &&...others) : N{forward_(others)...} {
      static_assert(requires { N::pipe_index; }, "Must append shader at pipe's bottom.");
      auto compiled = shader.handle->compiled;
      VKTL_ASSERT(compiled.size()); // Not allow empty handle.
      auto &shaders = N::pipe_infos.template back<1u>();
      vector<uint32_t> codes(compiled.size() / sizeof(uint32_t));
      ::std::memcpy(codes.data(), compiled.data(), compiled.size());
      shaders.emplace_back(
          VK_ VkPipelineShaderStageCreateInfo{
              .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = stage_of<S>,
              .pName = pipe_shader::default_entry,
          }, ::std::move(codes));
    }
  };
  template<>
  inline constexpr VK_ VkShaderStageFlagBits stage_of<vertex_shader> =
      VK_ VK_SHADER_STAGE_VERTEX_BIT;
  template <typename N> struct m<vertex_shader, N> : basic_shader<N> {
    using base = basic_shader<N>;
    constexpr m(vertex_shader shader, auto &&...others)
        : base{shader, forward_(others)...} {
    }
  };
  

  template <typename N> struct m<fragment_shader, N> : basic_shader<N> {
    using base = basic_shader<N>;
    constexpr m(fragment_shader shader, auto &&...others)
        : base{shader, forward_(others)...} {}
  };

  template<>
  inline constexpr VK_ VkShaderStageFlagBits stage_of<fragment_shader> =
      VK_ VK_SHADER_STAGE_FRAGMENT_BIT;

  template<> struct express<compute_shader> {
    static void invoke(compute_shader shader, auto &base) {
      VKTL_ASSERT(shader.code);
      VKTL_ASSERT(!base.pipe_infos.empty());
      VKTL_ASSERT(!shader.code->compiled.empty());
      VKTL_ASSERT((shader.code->compiled.size() % sizeof(uint32_t)) == 0u);
      auto &&[pipeline, codes] = base.pipe_infos.back();
      pipeline.stage = VK_ VkPipelineShaderStageCreateInfo{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_ VK_SHADER_STAGE_COMPUTE_BIT,
          .pName = "main",
      };
      codes.resize(shader.code->compiled.size() / sizeof(uint32_t));
      ::std::memcpy(codes.data(), shader.code->compiled.data(),
                    shader.code->compiled.size());
    }
  };

  using namespace shader_extensions;

	template<typename N> struct m<input_assembly, N> : N {
    constexpr m(input_assembly v, auto&&...others) 
			: N{forward_(others)...} {
			using namespace input_assembly_attributes;

			// TODO:
      
		}

		constexpr void relocate() {
      VK_ VkGraphicsPipelineCreateInfo &info 
				= N::pipe_infos.template get<0u>(N::pipe_index);
      info.pInputAssemblyState = &input_assembly_;
		}

  protected:
		auto& input_assembly() noexcept { return input_assembly_; }

	private:
    VK_ VkPipelineInputAssemblyStateCreateInfo input_assembly_ {
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		};
	};

	template <typename N> struct m<vertex_input_, N> : N {
		constexpr m(vertex_input_, auto&&...others) 
			: N{forward_(others)...} {}

		void relocate() noexcept {
      	   VK_ VkGraphicsPipelineCreateInfo &info 
		   			= N::pipe_infos.template get<0u>(N::pipe_index);
      	   info.pVertexInputState = &vertex_input_;
		}

    protected:
	  auto& vertex_input() noexcept { return vertex_input_; }

	private:
      VK_ VkPipelineVertexInputStateCreateInfo vertex_input_ {
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	  };
	};

}