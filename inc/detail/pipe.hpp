#pragma once

// Interface style: pipeline descriptors compose graphics/compute state one
// focused layer at a time, including subpass and shader declarations.
// Implementation: each layer mutates the owning pass's stable create-info
// storage so Vulkan pointers remain valid until pipeline creation.

VKTL_EXPORT_ namespace vktl::detail {

#pragma region NOBODY_LIKE_GRAPHICS_PIPELINE
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineVertexInputStateCreateInfo defaultVertexInputState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = 0,
			.pVertexBindingDescriptions = nullptr,
			.vertexAttributeDescriptionCount = 0,
			.pVertexAttributeDescriptions = nullptr
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineInputAssemblyStateCreateInfo defaultInputAssemblyState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineTessellationStateCreateInfo defaultTessellationState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.patchControlPoints = 3
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineViewportStateCreateInfo defaultViewportState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineRasterizationStateCreateInfo defaultRasterizationState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_ VK_POLYGON_MODE_FILL,
			.cullMode = VK_ VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_ VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineMultisampleStateCreateInfo defaultMultisampleState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = VK_ VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 1.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
			.depthCompareOp = VK_ VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {
				.failOp = VK_ VK_STENCIL_OP_KEEP,
				.passOp = VK_ VK_STENCIL_OP_KEEP,
				.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
				.compareOp = VK_ VK_COMPARE_OP_NEVER,
				.compareMask = 0,
				.writeMask = 0,
				.reference = 0
			},
			.back = {
				.failOp = VK_ VK_STENCIL_OP_KEEP,
				.passOp = VK_ VK_STENCIL_OP_KEEP,
				.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
				.compareOp = VK_ VK_COMPARE_OP_NEVER,
				.compareMask = 0,
				.writeMask = 0,
				.reference = 0
			},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineColorBlendAttachmentState defaultColorBlendAttachment{
			.blendEnable = VK_FALSE,
			.srcColorBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
			.colorBlendOp = VK_ VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_ VK_BLEND_OP_ADD,
			.colorWriteMask = VK_ VK_COLOR_COMPONENT_R_BIT | VK_ VK_COLOR_COMPONENT_G_BIT |
							  VK_ VK_COLOR_COMPONENT_B_BIT | VK_ VK_COLOR_COMPONENT_A_BIT
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineColorBlendStateCreateInfo defaultColorBlendState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_ VK_LOGIC_OP_COPY,
			.attachmentCount = 0,
			.pAttachments = nullptr,
			.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};
	VKTL_MAYBE_UNUSED
		inline constexpr VK_ VkPipelineDynamicStateCreateInfo defaultDynamicState{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = 0,
			.pDynamicStates = nullptr
	};
#pragma endregion

	template<typename N>
	struct m<pipe_, N> : N {
		constexpr m(pipe_, auto&&...others)
			: N{ forward_(others)... } {
			if constexpr (N::template query<graphics_>()) {
				pipe_index_ = N::append(VK_ VkGraphicsPipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.pStages = nullptr,
					.pVertexInputState = &defaultVertexInputState,
					.pInputAssemblyState = &defaultInputAssemblyState,
					.pTessellationState = &defaultTessellationState,
					.pViewportState = &defaultViewportState,
					.pRasterizationState = &defaultRasterizationState,
					.pMultisampleState = &defaultMultisampleState,
					.pDepthStencilState = &defaultDepthStencilState,
					.pColorBlendState = &defaultColorBlendState,
					.pDynamicState = &defaultDynamicState,
					});
			}
			else if constexpr (N::template query<compute_>()) {
				pipe_index_ = N::append(VK_ VkComputePipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
					.stage = {.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
					});
			}
		}

	protected:
		uint16_t pipe_index() const noexcept { return pipe_index_; }

	private:
		uint16_t pipe_index_;
	};

	using namespace pipe_extensions;

	template<typename N>
		requires(N::template query<render_pass_>())
	struct m<subpass, N> : N {
		constexpr m(subpass subpass, auto&&...others)
			: N{ forward_(others)... }
			, subpass_{ subpass.index } {
		}

	protected:
		uint16_t subpass() const noexcept { return subpass_; }

	private:
		uint16_t subpass_;
	};

}

