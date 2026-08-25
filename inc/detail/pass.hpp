#pragma once

// --- Agents specification -------------------------------------------------
// `m<pass_, N>` owns one ordered, public read-only resource-usage sequence and
// remains descriptor- and shader-agnostic. Resource instances and cross-pass
// hazards are resolved by bind sets and task compilation; the pass records
// declarations only.
// 
// Use `express<Tag>` for resource, shader, format, and lightweight pipeline
// declarations. Expressed declarations retain no state. Invalid declaration
// order and conflicting declarations are programming errors and use `assert`.
// Each pass expression keeps its own `invoke()` instead of forwarding through a
// `basic_xxx_express`. Express specializations directly access protected pass
// storage and use structured bindings for columnar rows.
// All pipelines in one pass share one descriptor schema and pipeline layout.
// Normalize bindings with `default_descriptor_set_layout::add()`, deduplicate
// equal layouts, preserve sparse Vulkan set numbers, and materialize empty
// layouts for set-number holes.
// 
// Graphics pipelines and render subpasses use columnar `vectors` storage so
// their Vulkan create-info columns can be passed directly to create calls.
// Shader declarations are stored directly in columnar pipeline storage; do not
// reintroduce a shader record type. Every pointer-bearing Vulkan structure is
// rebound by `relocate()`.
// 
// use `empty() ? nullptr : data()` for array pointer if there is no 
// independent count for it.
// 
// `m<pass_, N>` freezes its ordered usage declarations in `finalize()` but does
// not collect dependencies. A classic render pass compares every earlier usage
// with every later usage of the same resource index and inserts dependencies in
// `(srcSubpass, dstSubpass)` order. Cross-pass barriers remain outside this file.
// `default_resource_usage::usages` stores Vulkan buffer/image usage flags
// directly. Attachment kind is derived from `VK_IMAGE_USAGE_*_ATTACHMENT_BIT`;
// `attributes` stores only load/clear/store operation metadata.
// `finalize()` never relocates; object construction or an explicit caller owns
// the relocation point.
// 
// Descriptor-set layouts are normalized value objects cached permanently by
// the device. A pass and a bind set keep independent schema values/handles;
// neither stores references into the other. `default_bind_set_schema` stores
// one entry per public Vulkan set number; an entry without bindings is a hole.
// Pipeline-layout creation materializes the cached empty layout for each hole.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	using default_bind_set_schema = vector<default_descriptor_set_layout>;

	struct default_resource_usage {
		uint16_t index = invalid;
		uint16_t subpass = invalid;
		VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
		VK_ VkDescriptorType type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		VK_ VkPipelineStageFlags stages = VK_ VkPipelineStageFlags(0u);
		VK_ VkAccessFlags access = VK_ VkAccessFlags(0u);
		VK_ VkDependencyFlags dependency = VK_ VkDependencyFlags(0u);
		VK_ VkImageLayout layout = VK_ VK_IMAGE_LAYOUT_UNDEFINED; // prefer layout.

		// for descriptor set.
		uint32_t set = invalid;
		uint32_t binding = invalid;
		// for descriptor heap or descriptor buffer.
		uint64_t offset = invalid;
		uint64_t usages = 0u; // since usage2 is 64 bits.
		uint64_t attributes = 0u;

		constexpr bool uses_descriptor() const noexcept {
			return type != VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	};

	namespace passes {

	}

	struct graphics_pass;
	struct compute_pass;

	using namespace pass_extensions;

	template<>
	struct is_queryable<pass_extensions::render_pass_, graphics_pass>
		: ::std::true_type {};
	template<>
	struct is_queryable<pass_extensions::rendering_, graphics_pass>
		: ::std::true_type {};

	template<>
	struct is_queryable<pass_extensions::compute_, compute_pass>
		: ::std::true_type {};

	template<typename N>
	struct m<pass_, N> : N {
		constexpr m(pass_, auto&&... other)
			: N{ forward_(other)... } {}

		cspan<default_resource_usage> usages() const noexcept {
			return usages_;
		}

		void finalize() {
			N::finalize();
			assert(!finalized_);
			finalized_ = true;
		}

	protected:
		constexpr void append(default_resource_usage const& usage) {
			assert(!finalized_);
			usages_.emplace_back(usage);
		}

		vector<default_resource_usage> usages_;

	private:
		bool finalized_ = false;
	};

	template<typename T>
	constexpr T const* data_or_null(vector<T> const& values) noexcept {
		return values.size() ? values.data() : nullptr;
	}
	template<typename T>
	constexpr T* data_or_null(vector<T>& values) noexcept {
		return values.size() ? values.data() : nullptr;
	}

	inline constexpr VK_ VkShaderStageFlags shader_stages_of(
		VK_ VkPipelineStageFlags stages) noexcept {
		VK_ VkShaderStageFlags result = 0u;
		if (stages & VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_VERTEX_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_GEOMETRY_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_FRAGMENT_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
			result |= VK_ VK_SHADER_STAGE_COMPUTE_BIT;
		if (stages & VK_ VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT)
			result |= VK_ VK_SHADER_STAGE_ALL_GRAPHICS;
		if (stages & VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
			result |= VK_ VK_SHADER_STAGE_ALL;
		return result;
	}

	inline constexpr VK_ VkPipelineStageFlags pipeline_stage_of(
		VK_ VkShaderStageFlagBits stage) noexcept {
		switch (stage) {
		case VK_ VK_SHADER_STAGE_VERTEX_BIT:
			return VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
		case VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
		case VK_ VK_SHADER_STAGE_GEOMETRY_BIT:
			return VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
		case VK_ VK_SHADER_STAGE_FRAGMENT_BIT:
			return VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case VK_ VK_SHADER_STAGE_COMPUTE_BIT:
			return VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		default:
			assert(false);
			return VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}
	}

	template<typename N>
	struct basic_pipe_pass : N {
		constexpr basic_pipe_pass(auto&&... args)
			: N{ forward_(args)... } {}

		~basic_pipe_pass() { reset(); }

		void fill(auto& bind_set) {
			fill(0u, bind_set);
		}

		void fill(uint32_t pass_id, auto& bind_set) {
			auto _ = locker_of(this);
			bind_set.accept(pass_id, schema_);
			for (auto const& usage : this->usages()) bind_set.bind(usage);
		}

		VK_ VkPipelineLayout pipeline_layout() const noexcept {
			return pipeline_layout_.value;
		}

		void init() {
			N::init();
			if (pipeline_layout_) return;

			auto pdv = parent_of<device>(this);
			auto hdv = pdv->handle();
			try {
				VK_ vkCreatePipelineCache(hdv, &cache_info, N::allocator(), &cache_)
					| popup{ "[PASS] Create pipeline cache failure." };

				vector<VK_ VkDescriptorSetLayout> layouts;
				layouts.reserve(schema_.size());
				for (auto const& layout : schema_) {
					layouts.emplace_back(pdv->create(layout));
				}

				pipe_layout_info.layouts = ::std::move(layouts);
				pipeline_layout_.value = pdv->create(pipe_layout_info);
			}
			catch (...) {
				clear();
				throw;
			}
		}

		void reset() {
			clear();
			N::reset();
		}

		VK_ VkPipelineCache pipeline_cache() const noexcept {
			return cache_.value;
		}

	protected:
		void finalize(bool allow_mutable = false) {
			N::finalize();

			auto& layouts_by_set = schema_;
			for (auto const& usage : this->usages()) {
				if (!usage.uses_descriptor()) continue;
				assert(usage.set != invalid);
				assert(usage.binding != invalid);
				auto shader_stages = shader_stages_of(usage.stages);
				assert(shader_stages != 0u);

				if (layouts_by_set.size() <= usage.set) {
					layouts_by_set.resize(size_t(usage.set) + 1u);
				}
				auto result = layouts_by_set[usage.set].add(
					VK_ VkDescriptorSetLayoutBinding{
						.binding = usage.binding,
						.descriptorType = usage.type,
						.descriptorCount = 1u,
						.stageFlags = shader_stages,
					});
				assert(allow_mutable || !result.is_mutable);
			}
		}

	private:
		void clear() noexcept {
			if (cache_) {
				VK_ vkDestroyPipelineCache(handle_of<device>(this),
					::std::exchange(cache_.value, VK_NULL_HANDLE), N::allocator());
			}
			pipeline_layout_.value = VK_NULL_HANDLE;
		}

	protected:
		default_bind_set_schema schema_;
		VK_ VkPipelineCacheCreateInfo cache_info{ .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, };

	private:
		default_pipeline_layout pipe_layout_info;
		reset_if_copy<VK_ VkPipelineCache> cache_{ VK_NULL_HANDLE };
		reset_if_copy<VK_ VkPipelineLayout> pipeline_layout_{ VK_NULL_HANDLE };
	};

#pragma region NOBODY_LIKE_GRAPHICS_PIPELINE
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineVertexInputStateCreateInfo defaultVertexInputState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineInputAssemblyStateCreateInfo defaultInputAssemblyState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineTessellationStateCreateInfo defaultTessellationState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.patchControlPoints = 3u,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineViewportStateCreateInfo defaultViewportState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1u,
		.scissorCount = 1u,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineRasterizationStateCreateInfo defaultRasterizationState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_ VK_POLYGON_MODE_FILL,
		.cullMode = VK_ VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_ VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineMultisampleStateCreateInfo defaultMultisampleState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_ VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1.0f,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthCompareOp = VK_ VK_COMPARE_OP_LESS,
		.front = {
			.failOp = VK_ VK_STENCIL_OP_KEEP,
			.passOp = VK_ VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
			.compareOp = VK_ VK_COMPARE_OP_NEVER,
		},
		.back = {
			.failOp = VK_ VK_STENCIL_OP_KEEP,
			.passOp = VK_ VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
			.compareOp = VK_ VK_COMPARE_OP_NEVER,
		},
		.maxDepthBounds = 1.0f,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineColorBlendAttachmentState defaultColorBlendAttachment{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_ VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_ VK_BLEND_OP_ADD,
		.colorWriteMask = VK_ VK_COLOR_COMPONENT_R_BIT | VK_ VK_COLOR_COMPONENT_G_BIT
			| VK_ VK_COLOR_COMPONENT_B_BIT | VK_ VK_COLOR_COMPONENT_A_BIT,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineColorBlendStateCreateInfo defaultColorBlendState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOp = VK_ VK_LOGIC_OP_COPY,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkDynamicState defaultDynamicStates[]{
		VK_ VK_DYNAMIC_STATE_VIEWPORT,
		VK_ VK_DYNAMIC_STATE_SCISSOR,
	};
	VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineDynamicStateCreateInfo defaultDynamicState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2u,
		.pDynamicStates = defaultDynamicStates,
	};
#pragma endregion

	template<typename N>
	struct basic_graphics_pass : basic_pipe_pass<N> {
		using base = basic_pipe_pass<N>;

		constexpr basic_graphics_pass(auto&&... infos)
			: base{ forward_(infos)... } {}

		~basic_graphics_pass() { reset(); }

		void relocate() noexcept {
			base::relocate();
			relocate_pipelines();
		}

		void init() {
			base::init();
			if (pipe_infos.empty() || pipes.front()) return;
			auto hdv = handle_of<device>(this);
			try {
				for (auto&& [info, shaders, _0, _1] : pipe_infos) {
					info.layout = base::pipeline_layout();
					info.renderPass = render_pass;
					for (auto&& [stage, codes, _2] : shaders) {
						assert(!codes.empty());
						auto module = VK_ VkShaderModuleCreateInfo{
							.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
							.codeSize = codes.size() * sizeof(uint32_t),
							.pCode = codes.data(),
						};
						VK_ vkCreateShaderModule(hdv, &module,
							N::allocator(), &stage.module)
							| popup{ "[PASS] Failed to create shader module." };
					}
				}

				vector<VK_ VkPipeline> handles(pipe_infos.size(), VK_NULL_HANDLE);
				auto result = VK_ vkCreateGraphicsPipelines(hdv, base::pipeline_cache(),
					uint32_t(pipe_infos.size()), pipe_infos.template data<0u>(),
					N::allocator(), handles.data());
				for (auto index = 0u; index < handles.size(); ++index) {
					pipes[index].value = handles[index];
				}
				result | popup{ "[PASS] Failed to create graphics pipelines." };
				for (auto&& [_0, shaders, _1, _2] : pipe_infos) {
					for (auto&& [stage, _3, _4] : shaders) {
						VK_ vkDestroyShaderModule(hdv,
							::std::exchange(stage.module, VK_NULL_HANDLE), N::allocator());
					}
				}
			}
			catch (...) {
				reset();
				throw;
			}
		}

		void reset() {
			auto hdv = handle_of<device>(this);
			for (auto& pipe : pipes) {
				if (pipe) {
					VK_ vkDestroyPipeline(hdv,
						::std::exchange(pipe.value, VK_NULL_HANDLE), N::allocator());
				}
			}
			for (auto&& [_0, shaders, _1, _2] : pipe_infos) {
				for (auto&& [stage, _3, _4] : shaders) {
					if (stage.module) VK_ vkDestroyShaderModule(hdv,
						::std::exchange(stage.module, VK_NULL_HANDLE), N::allocator());
				}
			}
			base::reset();
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipes.size());
			return pipes[index].value;
		}

		uint16_t pipe_count() const noexcept {
			assert(pipe_infos.size() <= uint16_t(maximum));
			return uint16_t(pipe_infos.size());
		}

	protected:
		using base::append;

		uint16_t append(VK_ VkGraphicsPipelineCreateInfo info) {
			info.pVertexInputState = &defaultVertexInputState;
			info.pInputAssemblyState = &defaultInputAssemblyState;
			info.pTessellationState = &defaultTessellationState;
			info.pViewportState = &defaultViewportState;
			info.pRasterizationState = &defaultRasterizationState;
			info.pMultisampleState = &defaultMultisampleState;
			info.pDepthStencilState = &defaultDepthStencilState;
			info.pDynamicState = &defaultDynamicState;
			pipe_infos.emplace_back(::std::move(info), by_default,
				defaultColorBlendState, by_default);
			pipes.emplace_back(reset_if_copy<VK_ VkPipeline>{ VK_NULL_HANDLE });
			return uint16_t(pipe_infos.size() - 1u);
		}

		void append(VK_ VkFormat format) {
			assert(format != VK_ VK_FORMAT_UNDEFINED);
			auto const& usage = this->usages_.back();
			assert(usage.resource_type == VK_ VK_OBJECT_TYPE_IMAGE);
			assert(usage.index != invalid);
			if (attachment_formats_.size() <= usage.index) {
				attachment_formats_.resize(size_t(usage.index) + 1u,
					VK_ VK_FORMAT_UNDEFINED);
			}
			auto& current = attachment_formats_[usage.index];
			assert(current == VK_ VK_FORMAT_UNDEFINED || current == format);
			current = format;
		}

		vectors<VK_ VkGraphicsPipelineCreateInfo
			, vectors<VK_ VkPipelineShaderStageCreateInfo
				, vector<uint32_t>
				, vector<VK_ VkSpecializationInfo>>
			, VK_ VkPipelineColorBlendStateCreateInfo
			, vector<VK_ VkPipelineColorBlendAttachmentState>> pipe_infos;

		vector<reset_if_copy<VK_ VkPipeline>> pipes;
		vector<VK_ VkFormat> attachment_formats_;
		VK_ VkRenderPass render_pass = VK_NULL_HANDLE;

	private:
		void relocate_pipelines() noexcept {
			for (auto&& [info, shaders, color_blend, color_attachments] : pipe_infos) {
				for (auto&& [stage, _0, specializations] : shaders) {
					stage.module = VK_NULL_HANDLE;
					assert(specializations.size() <= 1u);
					stage.pSpecializationInfo = data_or_null(specializations);
				}
				info.stageCount = uint32_t(shaders.size());
				info.pStages = shaders.empty() ? nullptr : shaders.template data<0u>();
				color_blend.attachmentCount = uint32_t(color_attachments.size());
				color_blend.pAttachments = data_or_null(color_attachments);
				info.pColorBlendState = &color_blend;
			}
		}

	};

	namespace attachment {
		inline constexpr bool has(uint64_t attributes, attachment_attribute::type value) noexcept {
			return (attributes & value) != 0u;
		}

		inline constexpr uint64_t depth_operations = attachment_attribute::load
			| attachment_attribute::clear | attachment_attribute::store;
		inline constexpr uint64_t stencil_operations = attachment_attribute::load_stencil
			| attachment_attribute::clear_stencil | attachment_attribute::store_stencil;
	}

	inline constexpr VK_ VkImageUsageFlags attachment_usages =
		VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
		| VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	inline constexpr uint16_t attachment_index(auto const& pass,
		uint16_t requested) noexcept {
		if (requested != invalid) return requested;
		uint32_t result = 0u;
		for (auto const& usage : pass.usages()) {
			if (usage.resource_type != VK_ VK_OBJECT_TYPE_IMAGE
				|| (usage.usages & attachment_usages) == 0u
				|| usage.index == invalid) continue;
			result = ::std::max(result, uint32_t(usage.index) + 1u);
		}
		assert(result < uint32_t(uint16_t(maximum)));
		return uint16_t(result);
	}

	inline constexpr VK_ VkImageLayout attachment_layout(
		VK_ VkImageUsageFlags usage) noexcept {
		switch (usage & attachment_usages) {
		case VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
			return VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
			return VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
			return VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		default:
			assert(false);
			return VK_ VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	inline constexpr VK_ VkPipelineStageFlags attachment_stages(
		VK_ VkImageUsageFlags usage) noexcept {
		switch (usage & attachment_usages) {
		case VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
			return VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
			return VK_ VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
				| VK_ VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
			return VK_ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		default:
			assert(false);
			return VK_ VkPipelineStageFlags(0u);
		}
	}

	inline constexpr VK_ VkAccessFlags attachment_access(
		VK_ VkImageUsageFlags usage) noexcept {
		switch (usage & attachment_usages) {
		case VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
			return VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		case VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
			return VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
				| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
			return VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
				| VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		default:
			assert(false);
			return VK_ VkAccessFlags(0u);
		}
	}

	inline constexpr bool access_writes(VK_ VkAccessFlags access) noexcept {
		constexpr auto writes = VK_ VK_ACCESS_SHADER_WRITE_BIT
			| VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_TRANSFER_WRITE_BIT
			| VK_ VK_ACCESS_HOST_WRITE_BIT
			| VK_ VK_ACCESS_MEMORY_WRITE_BIT;
		return (access & writes) != 0u;
	}

	template<typename N>
	struct m<render_pass_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;

		m(render_pass_, auto&&... infos)
			: base{ forward_(infos)... }
			, info{ .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO } {}

		~m() { reset(); }

		void finalize() {
			base::finalize();
			for (auto index = 0u; index < attachments_.size(); ++index) {
				assert(attachment_declared_[index]);
				assert(index < base::attachment_formats_.size());
				auto format = base::attachment_formats_[index];
				assert(format != VK_ VK_FORMAT_UNDEFINED);
				attachments_[index].format = format;
			}
			for (auto index = 0u; index < base::pipe_count(); ++index) {
				auto&& [pipeline, _0, _1, color_attachments]
					= base::pipe_infos[index];
				ensure_subpass(uint16_t(pipeline.subpass));
				auto&& [_2, _3, colors, _4, _5] = subpasses_[pipeline.subpass];
				auto count = colors.size();
				if (color_attachments.empty()) {
					color_attachments.resize(count, defaultColorBlendAttachment);
				}
				else {
					assert(color_attachments.size() == count);
				}
			}

			dependencies_.clear();
			auto const values = this->usages();
			for (auto dst = 0u; dst < values.size(); ++dst) {
				for (auto src = 0u; src < dst; ++src) {
					if (values[src].index != values[dst].index) continue;
					if (!access_writes(values[src].access)
						&& !access_writes(values[dst].access)
						&& values[src].layout == values[dst].layout
						&& (values[src].dependency | values[dst].dependency) == 0u) continue;
					append_dependency(values[src], values[dst]);
				}
			}
		}

		void relocate() noexcept {
			base::relocate();
			for (auto&& [subpass, inputs, colors, resolves, depth_stencil] : subpasses_) {
				subpass.inputAttachmentCount = uint32_t(inputs.size());
				subpass.pInputAttachments = data_or_null(inputs);
				subpass.colorAttachmentCount = uint32_t(colors.size());
				subpass.pColorAttachments = data_or_null(colors);
				assert(resolves.empty() || resolves.size() == colors.size());
				subpass.pResolveAttachments = data_or_null(resolves);
				assert(depth_stencil.size() <= 1u);
				subpass.pDepthStencilAttachment = data_or_null(depth_stencil);
			}
			info.attachmentCount = uint32_t(attachments_.size());
			info.pAttachments = data_or_null(attachments_);
			info.subpassCount = uint32_t(subpasses_.size());
			info.pSubpasses = subpasses_.empty()
				? nullptr : subpasses_.template data<0u>();
			info.dependencyCount = uint32_t(dependencies_.size());
			info.pDependencies = data_or_null(dependencies_);
		}

		void init() {
			basic_pipe_pass<N>::init();
			if (!handle_) {
				VK_ vkCreateRenderPass(handle_of<device>(this),
					&info, N::allocator(), &handle_)
					| popup{ "[PASS] Create render pass failure." };
			}
			base::render_pass = handle_.value;
			base::init();
		}

		void reset() {
			base::reset();
			base::render_pass = VK_NULL_HANDLE;
			if (handle_) {
				VK_ vkDestroyRenderPass(handle_of<device>(this),
					::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
			}
		}

		VK_ VkRenderPass pass() const noexcept { return handle_.value; }

	protected:
		constexpr void append(default_resource_usage const& usage) {
			base::append(usage);
			if (usage.resource_type != VK_ VK_OBJECT_TYPE_IMAGE) return;

			auto image_usage = VK_ VkImageUsageFlags(usage.usages) & attachment_usages;
			if (image_usage == 0u) return;
			assert(usage.index != invalid);
			assert(usage.subpass != invalid);
			ensure_subpass(usage.subpass);
			if (attachments_.size() <= usage.index) {
				auto size = size_t(usage.index) + 1u;
				attachments_.resize(size);
				attachment_declared_.resize(size, false);
			}

			auto& description = attachments_[usage.index];
			if (!attachment_declared_[usage.index]) {
				attachment_declared_[usage.index] = true;
				description.samples = VK_ VK_SAMPLE_COUNT_1_BIT;
				description.loadOp = attachment::has(usage.attributes, attachment_attribute::clear)
					? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
					: attachment::has(usage.attributes, attachment_attribute::load)
					? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
					: VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencilLoadOp = attachment::has(
					usage.attributes, attachment_attribute::clear_stencil)
					? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
					: attachment::has(usage.attributes, attachment_attribute::load_stencil)
					? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
					: VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout = attachment::has(
					usage.attributes, attachment_attribute::clear)
					|| attachment::has(usage.attributes, attachment_attribute::clear_stencil)
					? VK_ VK_IMAGE_LAYOUT_UNDEFINED : usage.layout;
			}
			description.storeOp = attachment::has(usage.attributes, attachment_attribute::store)
				? VK_ VK_ATTACHMENT_STORE_OP_STORE : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
			description.stencilStoreOp = attachment::has(
				usage.attributes, attachment_attribute::store_stencil)
				? VK_ VK_ATTACHMENT_STORE_OP_STORE : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
			description.finalLayout = usage.layout;

			auto reference = VK_ VkAttachmentReference{
				.attachment = usage.index,
				.layout = usage.layout,
			};
			switch (image_usage) {
			case VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
				{
					auto&& [_0, inputs, _1, _2, _3] = subpasses_[usage.subpass];
					insert_reference(inputs, reference);
				}
				break;
			case VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
				{
					auto&& [_0, _1, colors, _2, _3] = subpasses_[usage.subpass];
					insert_reference(colors, reference);
				}
				break;
			case VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: {
				auto&& [_0, _1, _2, _3, depth_stencil] = subpasses_[usage.subpass];
				assert(depth_stencil.empty()
					|| depth_stencil.front().attachment == usage.index);
				if (depth_stencil.empty()) depth_stencil.emplace_back(reference);
				else depth_stencil.front().layout = usage.layout;
				break;
			}
			default:
				assert(false);
			}
		}

	protected:
		VK_ VkRenderPassCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

	private:
		void ensure_subpass(uint16_t index) {
			assert(index <= subpasses_.size()); // not allow cross subpass.
			if (subpasses_.size() <= index) {
				subpasses_.resize(index + 1u,
					VK_ VkSubpassDescription{ .pipelineBindPoint = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS });
			}
		}

		static void insert_reference(vector<VK_ VkAttachmentReference>& references, VK_ VkAttachmentReference reference) {
			auto it = ::std::ranges::lower_bound(references,
				reference.attachment, {}, &VK_ VkAttachmentReference::attachment);
			if (it == references.end() || it->attachment != reference.attachment) {
				references.insert(it, reference);
			}
			else {
				assert(it->layout == reference.layout);
			}
		}

		void append_dependency(default_resource_usage const& src,
			default_resource_usage const& dst) {
			assert(src.subpass == invalid || src.subpass < 32u);
			assert(dst.subpass == invalid || dst.subpass < 32u);
			auto dependency = VK_ VkSubpassDependency{
				.srcSubpass = src.subpass == invalid
					? VK_SUBPASS_EXTERNAL : uint32_t(src.subpass),
				.dstSubpass = dst.subpass == invalid
					? VK_SUBPASS_EXTERNAL : uint32_t(dst.subpass),
				.srcStageMask = src.stages,
				.dstStageMask = dst.stages,
				.srcAccessMask = src.access,
				.dstAccessMask = dst.access,
				.dependencyFlags = src.dependency | dst.dependency,
			};
			auto src_begin = ::std::ranges::lower_bound(dependencies_,
				dependency.srcSubpass, {}, &VK_ VkSubpassDependency::srcSubpass);
			auto src_end = ::std::ranges::upper_bound(src_begin, dependencies_.end(),
				dependency.srcSubpass, {}, &VK_ VkSubpassDependency::srcSubpass);
			auto found = ::std::ranges::lower_bound(src_begin, src_end,
				dependency.dstSubpass, {}, &VK_ VkSubpassDependency::dstSubpass);
			if (found == src_end || found->dstSubpass != dependency.dstSubpass) {
				dependencies_.insert(found, dependency);
				return;
			}
			found->srcStageMask |= dependency.srcStageMask;
			found->dstStageMask |= dependency.dstStageMask;
			found->srcAccessMask |= dependency.srcAccessMask;
			found->dstAccessMask |= dependency.dstAccessMask;
			found->dependencyFlags |= dependency.dependencyFlags;
		}

	private:
		vector<VK_ VkAttachmentDescription> attachments_;
		vector<bool> attachment_declared_;
		vectors<VK_ VkSubpassDescription,
			vector<VK_ VkAttachmentReference>,
			vector<VK_ VkAttachmentReference>,
			vector<VK_ VkAttachmentReference>,
			vector<VK_ VkAttachmentReference>> subpasses_; // not support preserve attachment.
		vector<VK_ VkSubpassDependency> dependencies_;

		reset_if_copy<VK_ VkRenderPass> handle_{ VK_NULL_HANDLE };
	};

#if defined(VK_KHR_dynamic_rendering)
	template<>
	struct device_querion<pass_extensions::rendering_> {
		static bool query(VK_ VkPhysicalDevice device) noexcept {
			VK_ VkPhysicalDeviceDynamicRenderingFeaturesKHR fea{
				.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
			};
			VK_ VkPhysicalDeviceFeatures2KHR features{
				.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
				.pNext = &fea,
			};
			VK_ vkGetPhysicalDeviceFeatures2KHR(device, &features);
			return fea.dynamicRendering != 0u;
		}
	};

	template<typename N>
	struct m<pass_extensions::rendering_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;

		constexpr m(pass_extensions::rendering_, auto&&... infos)
			: base{ forward_(infos)... } {}

		void finalize() {
			base::finalize();
			assert(renderings_.size() == base::pipe_count());
		}

		void relocate() noexcept {
			base::relocate();
			assert(renderings_.size() == base::pipe_count());
			for (auto index = 0u; index < renderings_.size(); ++index) {
				auto&& [rendering, color_formats] = renderings_[index];
				rendering.colorAttachmentCount = uint32_t(color_formats.size());
				rendering.pColorAttachmentFormats = data_or_null(color_formats);
				auto&& [pipeline, _0, _1, _2] = base::pipe_infos[index];
				pipeline.pNext = &rendering;
			}
		}

		auto init() { return base::init(); }

	protected:
		using base::append;

		void append(VK_ VkGraphicsPipelineCreateInfo info) {
			base::append(::std::move(info));
			renderings_.emplace_back(VK_ VkPipelineRenderingCreateInfoKHR{
				.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			}, by_default);
		}

		void append(VK_ VkFormat format) {
			base::append(format);
			assert(!renderings_.empty());
			auto const& usage = this->usages_.back();
			auto image_usage = VK_ VkImageUsageFlags(usage.usages) & attachment_usages;
			auto&& [rendering, color_formats] = renderings_.back();
			if (image_usage == VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
				if (color_formats.size() <= usage.index) {
					color_formats.resize(usage.index + 1u, VK_ VK_FORMAT_UNDEFINED);
				}
				color_formats[usage.index] = format;
			}
			else if (image_usage == VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				if ((usage.attributes & attachment::depth_operations) != 0u) {
					rendering.depthAttachmentFormat = format;
				}
				if ((usage.attributes & attachment::stencil_operations) != 0u) {
					rendering.stencilAttachmentFormat = format;
				}
			}
		}

	private:
		vectors<VK_ VkPipelineRenderingCreateInfoKHR, 
			vector<VK_ VkFormat>> renderings_;
	};
#else
	template<typename N>
	struct m<pass_extensions::rendering_, N> : N {
		constexpr m(pass_extensions::rendering_, auto&&... infos)
			: N{ forward_(infos)... } {
			static_assert(always_false<N>,
				"Dynamic rendering is not supported by the Vulkan headers.");
		}
	};
#endif

	template<typename N>
	struct m<pass_extensions::compute_, N> : basic_pipe_pass<N> {
		using base = basic_pipe_pass<N>;

		m(pass_extensions::compute_, auto&&...infos)
			: base{ forward_(infos)... } {}

		~m() { reset(); }

		void finalize() {
			base::finalize();
		}

		void relocate() noexcept {
			base::relocate();
			for (auto&& [pipeline, _0] : pipe_infos) {
				pipeline.stage.module = VK_NULL_HANDLE;
			}
		}

		void init() {
			base::init();
			if (pipe_infos.empty() || pipes.front()) return;
			auto hdv = handle_of<device>(this);
			try {
				for (auto&& [pipeline, codes] : pipe_infos) {
					assert(!codes.empty());
					auto module = VK_ VkShaderModuleCreateInfo{
						.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
						.codeSize = codes.size() * sizeof(uint32_t),
						.pCode = codes.data(),
					};
					VK_ vkCreateShaderModule(hdv, &module,
						N::allocator(), &pipeline.stage.module)
						| popup{ "[PASS] Failed to create compute shader module." };
					pipeline.layout = base::pipeline_layout();
				}

				vector<VK_ VkPipeline> handles(pipe_infos.size(), VK_NULL_HANDLE);
				auto result = VK_ vkCreateComputePipelines(hdv, base::pipeline_cache(),
					uint32_t(pipe_infos.size()), pipe_infos.template data<0u>(),
					N::allocator(), handles.data());
				for (auto index = 0u; index < handles.size(); ++index) {
					pipes[index].value = handles[index];
				}
				result | popup{ "[PASS] Failed to create compute pipelines." };
				for (auto&& [pipeline, _0] : pipe_infos) {
					VK_ vkDestroyShaderModule(hdv,
						::std::exchange(pipeline.stage.module, VK_NULL_HANDLE),
						N::allocator());
				}
			}
			catch (...) {
				reset();
				throw;
			}
		}

		void reset() {
			auto hdv = handle_of<device>(this);
			for (auto& pipe : pipes) {
				if (pipe) {
					VK_ vkDestroyPipeline(hdv,
						::std::exchange(pipe.value, VK_NULL_HANDLE),
						N::allocator());
				}
			}
			for (auto&& [pipeline, _0] : pipe_infos) {
				if (pipeline.stage.module) {
					VK_ vkDestroyShaderModule(hdv,
						::std::exchange(pipeline.stage.module, VK_NULL_HANDLE),
						N::allocator());
				}
			}
			base::reset();
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipes.size());
			return pipes[index].value;
		}

		uint16_t pipe_count() const noexcept {
			return uint16_t(pipe_infos.size());
		}

	protected:
		using base::append;

		void append(VK_ VkComputePipelineCreateInfo info) {
			assert(pipe_infos.size() < uint16_t(maximum));
			pipe_infos.emplace_back(info, by_default);
			pipes.emplace_back(reset_if_copy<VK_ VkPipeline>{ VK_NULL_HANDLE });
		}

	protected:
		vectors<VK_ VkComputePipelineCreateInfo, vector<uint32_t>> pipe_infos;
		vector<reset_if_copy<VK_ VkPipeline>> pipes;
	};

	template<typename N>
	struct m<pipe_, N> : N {
		using base = N;
		constexpr m(pipe_, auto&&...others)
			: N{forward_(others)...} {
			if constexpr (requires (VK_ VkGraphicsPipelineCreateInfo const& v) { base::append(v); }) {
				base::append(VK_ VkGraphicsPipelineCreateInfo {
					.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				});
			}
			else if constexpr (requires (VK_ VkComputePipelineCreateInfo const& v) { base::append(v); }) {
				base::append(VK_ VkComputePipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
					.stage = { .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, },
				});
			}
			else {
#if defined(VK_KHR_pipeline_binary)
				base::append(VK_ VkPipelineCreateInfoKHR{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR,
				});
#else
				static_assert(always_false<decltype(base)>,
					"A pipeline requires a graphics or compute pass.");
#endif
			}
		}

	protected:
		static constexpr uint32_t pipe_index = []() constexpr {
			if constexpr (requires { N::pipe_index; }) {
				return N::pipe_index + 1u;
			}
			else {
				return 0u;
			}
		}();
		
		friend constexpr uint32_t get_pipe_index(m&) {
			return m::pipe_index;
		}

	private:
	};

	using namespace pipe_extensions;

	template<>
	struct express<subpass> {
		static constexpr void invoke(subpass value, object_of<render_pass_> auto& base) {
			assert(!base.pipe_infos.empty());
			assert(value.index < 32u);
			auto&& [pipeline, _0, _1, _2] = base.pipe_infos.back();
			pipeline.subpass = value.index;
		}
	};

	template<>
	struct express<vertex_shader> {
		static void invoke(vertex_shader shader, auto& base) {
			assert(shader.handle);
			assert(!base.pipe_infos.empty());
			assert(!shader.handle->compiled.empty());
			assert((shader.handle->compiled.size() % sizeof(uint32_t)) == 0u);
			auto&& [_0, shaders, _1, _2] = base.pipe_infos.back();
			vector<uint32_t> codes(shader.handle->compiled.size() / sizeof(uint32_t));
			::std::memcpy(codes.data(), shader.handle->compiled.data(),
				shader.handle->compiled.size());
			shaders.emplace_back(
				VK_ VkPipelineShaderStageCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_ VK_SHADER_STAGE_VERTEX_BIT,
					.pName = "main",
				}, ::std::move(codes), by_default);
		}
	};

	template<>
	struct express<fragment_shader> {
		static void invoke(fragment_shader shader, auto& base) {
			assert(shader.handle);
			assert(!base.pipe_infos.empty());
			assert(!shader.handle->compiled.empty());
			assert((shader.handle->compiled.size() % sizeof(uint32_t)) == 0u);
			auto&& [_0, shaders, _1, _2] = base.pipe_infos.back();
			vector<uint32_t> codes(shader.handle->compiled.size() / sizeof(uint32_t));
			::std::memcpy(codes.data(), shader.handle->compiled.data(),
				shader.handle->compiled.size());
			shaders.emplace_back(
				VK_ VkPipelineShaderStageCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_ VK_SHADER_STAGE_FRAGMENT_BIT,
					.pName = "main",
				}, ::std::move(codes), by_default);
		}
	};

	template<>
	struct express<compute_shader> {
		static void invoke(compute_shader shader, auto& base) {
			assert(shader.code);
			assert(!base.pipe_infos.empty());
			assert(!shader.code->compiled.empty());
			assert((shader.code->compiled.size() % sizeof(uint32_t)) == 0u);
			auto&& [pipeline, codes] = base.pipe_infos.back();
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

	template<>
	struct express<customized_entry_point> {
		static constexpr void invoke(customized_entry_point entry, auto& base) {
			assert(entry.name);
			assert(!base.pipe_infos.empty());
			if constexpr (requires { base.render_pass; }) {
				auto&& [_0, shaders, _1, _2] = base.pipe_infos.back();
				assert(!shaders.empty());
				auto&& [stage, _3, _4] = shaders.back();
				stage.pName = entry.name;
			}
			else {
				auto&& [pipeline, _0] = base.pipe_infos.back();
				pipeline.stage.pName = entry.name;
			}
		}
	};

	template<>
	struct express<uniform_buffer> {
		static constexpr void invoke(uniform_buffer usage, auto& base) {
			assert(usage.index <= uint32_t(uint16_t(maximum)));
			assert(!base.pipe_infos.empty());
			VK_ VkShaderStageFlagBits stage;
			default_resource_usage result{
				.index = uint16_t(usage.index),
				.resource_type = VK_ VK_OBJECT_TYPE_BUFFER,
				.type = VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.access = VK_ VK_ACCESS_UNIFORM_READ_BIT,
				.usages = VK_ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			};
			if constexpr (requires { base.render_pass; }) {
				auto&& [pipeline, shaders, _0, _1] = base.pipe_infos.back();
				assert(!shaders.empty());
				auto&& [shader, _2, _3] = shaders.back();
				stage = shader.stage;
				result.subpass = uint16_t(pipeline.subpass);
			}
			else {
				auto&& [pipeline, _0] = base.pipe_infos.back();
				stage = pipeline.stage.stage;
			}
			result.stages = pipeline_stage_of(stage);
			base.append(result);
		}
	};

	template<>
	struct express<input_attachment> {
		static constexpr void invoke(input_attachment usage, auto& base) {
			assert(!base.pipe_infos.empty());
			auto&& [pipeline, _0, _1, _2] = base.pipe_infos.back();
			constexpr auto image_usage = VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			base.append(default_resource_usage{
				.index = attachment_index(base, usage.index),
				.subpass = uint16_t(pipeline.subpass),
				.resource_type = VK_ VK_OBJECT_TYPE_IMAGE,
				.stages = attachment_stages(image_usage),
				.access = attachment_access(image_usage),
				.layout = attachment_layout(image_usage),
				.usages = image_usage,
				.attributes = usage.attribute,
			});
		}
	};

	template<>
	struct express<color_attachment> {
		static constexpr void invoke(color_attachment usage, auto& base) {
			assert(!base.pipe_infos.empty());
			auto&& [pipeline, _0, _1, _2] = base.pipe_infos.back();
			assert(!(attachment::has(usage.attribute, attachment_attribute::load)
				&& attachment::has(usage.attribute, attachment_attribute::clear)));
			constexpr auto image_usage = VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			base.append(default_resource_usage{
				.index = attachment_index(base, usage.index),
				.subpass = uint16_t(pipeline.subpass),
				.resource_type = VK_ VK_OBJECT_TYPE_IMAGE,
				.stages = attachment_stages(image_usage),
				.access = attachment_access(image_usage),
				.layout = attachment_layout(image_usage),
				.usages = image_usage,
				.attributes = usage.attribute,
			});
		}
	};

	template<>
	struct express<depth_stencil_attachment> {
		static constexpr void invoke(depth_stencil_attachment usage, auto& base) {
			assert(!base.pipe_infos.empty());
			auto&& [pipeline, _0, _1, _2] = base.pipe_infos.back();
			assert(!(attachment::has(usage.attribute, attachment_attribute::load)
				&& attachment::has(usage.attribute, attachment_attribute::clear)));
			assert(!(attachment::has(usage.attribute, attachment_attribute::load_stencil)
				&& attachment::has(usage.attribute, attachment_attribute::clear_stencil)));
			auto const depth_index = attachment_index(base, usage.index);
			auto const has_stencil = attachment::has(
				usage.attribute, attachment_attribute::load_stencil)
				|| attachment::has(usage.attribute, attachment_attribute::clear_stencil)
				|| attachment::has(usage.attribute, attachment_attribute::store_stencil);
			auto const stencil_index = has_stencil
				? attachment_index(base, usage.stencil_index) : invalid;
			constexpr auto image_usage = VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			auto attributes = uint64_t(usage.attribute) & attachment::depth_operations;
			if (stencil_index == depth_index) {
				attributes = usage.attribute;
			}
			base.append(default_resource_usage{
				.index = depth_index,
				.subpass = uint16_t(pipeline.subpass),
				.resource_type = VK_ VK_OBJECT_TYPE_IMAGE,
				.stages = attachment_stages(image_usage),
				.access = attachment_access(image_usage),
				.layout = attachment_layout(image_usage),
				.usages = image_usage,
				.attributes = attributes,
			});
			if (stencil_index != invalid && stencil_index != depth_index) {
				auto const stencil_attributes = uint64_t(usage.attribute)
					& attachment::stencil_operations;
				base.append(default_resource_usage{
					.index = stencil_index,
					.subpass = uint16_t(pipeline.subpass),
					.resource_type = VK_ VK_OBJECT_TYPE_IMAGE,
					.stages = attachment_stages(image_usage),
					.access = attachment_access(image_usage),
					.layout = attachment_layout(image_usage),
					.usages = image_usage,
					.attributes = stencil_attributes,
				});
			}
		}
	};

	using namespace resource_usage_extensions;

	template<>
	struct express<bind_on_set> {
		static void invoke(bind_on_set value, auto& object) {
			assert(!object.usages_.empty());
			auto& usage = object.usages_.back();
			assert(usage.uses_descriptor());
			assert(usage.set == invalid || usage.set == value.set);
			assert(usage.binding == invalid || usage.binding == value.binding);
			usage.set = value.set;
			usage.binding = value.binding;
		}
	};

	template<>
	struct express<bind_on_heap> {
		static void invoke(bind_on_heap value, auto& object) {
			assert(!object.usages_.empty());
			auto& usage = object.usages_.back();
			assert(usage.offset == invalid || usage.offset == value.offset);
			usage.offset = value.offset;
		}
	};
}
