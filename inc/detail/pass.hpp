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
// All pipelines in one pass share one descriptor schema and pipeline layout.
// Normalize bindings with `default_descriptor_set_layout::add()`, deduplicate
// equal layouts, preserve sparse Vulkan set numbers, and materialize empty
// layouts for set-number holes.
// 
// Graphics pipelines and render subpasses use columnar `vectors` storage so
// their Vulkan create-info columns can be passed directly to create calls.
// Live pipeline and shader-module handles remain reset-on-copy. Every
// pointer-bearing Vulkan structure is rebound by `relocate()`.
// 
// use `empty() ? nullptr : data()` for array pointer if there is no 
// independent count for it.
// 
// A classic render pass derives subpass dependencies from repeated attachment
// declarations inside that pass. Cross-pass barriers remain outside this file.
// 
// Descriptor-set layouts are normalized value objects cached permanently by
// the device. A pass and a bind set keep independent schema values/handles;
// neither stores references into the other. `default_bind_set_schema` stores
// one entry per public Vulkan set number; an entry without bindings is a hole.
// Pipeline-layout creation materializes the cached empty layout for each hole.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	template<typename Trait>
	struct basic_resource_usage_express;
	template<typename Trait>
	struct basic_shader_express;
	template<typename Format>
	struct basic_format_express;

	using default_bind_set_schema = vector<default_descriptor_set_layout>;

	struct default_resource_usage {
		uint16_t index = invalid;
		uint16_t reserved = 0u;
		VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
		VK_ VkDescriptorType type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		VK_ VkPipelineStageFlags stages = VK_ VkPipelineStageFlags(0u);
		VK_ VkAccessFlags access = VK_ VkAccessFlags(0u);
		VK_ VkDependencyFlags dependency = VK_ VkDependencyFlags(0u);
		VK_ VkImageLayout layout = VK_ VK_IMAGE_LAYOUT_UNDEFINED;

		// for descriptor set.
		uint32_t set = invalid;
		uint32_t binding = invalid;
		// for descriptor heap or descriptor buffer.
		uint64_t offset = invalid;
		uint64_t usages = 0u; // since usage2 is 64 bits.

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
		template<typename>
		friend struct basic_resource_usage_express;

		constexpr m(pass_, auto&&... other)
			: N{ forward_(other)... } {}

		cspan<default_resource_usage> usages() const noexcept {
			return usages_;
		}

	protected:
		friend constexpr void append(m& self, default_resource_usage const& usage) {
			self.usages_.emplace_back(usage);
		}

		friend constexpr auto& usages(m& self) noexcept {
			return self.usages_;
		}

	private:
		vector<default_resource_usage> usages_;
	};

	template<typename T>
	constexpr T const* data_or_null(vector<T> const& values) noexcept {
		return values.size() ? values.data() : nullptr;
	}
	template<typename T>
	constexpr T* data_or_null(vector<T>& values) noexcept {
		return values.size() ? values.data() : nullptr;
	}

	template<typename N>
	struct basic_pipe_pass : N {
		using N::append;

		constexpr basic_pipe_pass(auto&&... args)
			: N{ forward_(args)... } {}

		~basic_pipe_pass() { reset(); }

		void fill(auto& bind_set) {
			fill(0u, bind_set);
		}

		VK_ VkPipelineLayout pipeline_layout() const noexcept {
			return pipeline_layout_.value;
		}

		auto init() {
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
				reset();
				throw;
			}
		}

		auto reset() {
			if (cache_) {
				VK_ vkDestroyPipelineCache(handle_of<device>(this),
					::std::exchange(cache_.value, VK_NULL_HANDLE), N::allocator());
			}
			pipeline_layout_.value = VK_NULL_HANDLE;
		}

	protected:
		void finalize(bool allow_mutable = false) {
			N::finalize();

			default_bind_set_schema const& layouts_by_set = schema_;
			for (auto const& usage : this->usages()) {
				if (!usage.uses_descriptor()) continue;
				assert(usage.set != invalid);
				assert(usage.binding != invalid);
				assert(usage.shader_stages != 0u);

				if (layouts_by_set.size() <= usage.set) {
					layouts_by_set.resize(size_t(usage.set) + 1u);
				}
				auto result = layouts_by_set[usage.set].add(VK_ VkDescriptorSetLayoutBinding{
					.binding = usage.binding,
					.descriptorType = usage.type,
					.descriptorCount = 1u,
					.stageFlags = usage.shader_stages,
				});
				assert(allow_mutable || !result.is_mutable);
			}
		}

		friend constexpr void append_binding(uint32_t set, uint32_t binding) noexcept {
			auto& usage = this->last_usage();
			assert(usage.uses_descriptor());
			assert(usage.set == invalid || usage.set == set);
			assert(usage.binding == invalid || usage.binding == binding);
			usage.set = set;
			usage.binding = binding;
		}

		friend constexpr void append_binding(uint64_t offset) noexcept {
			auto& usage = this->last_usage();
			assert(usage.offset == maximum || usage.offset == offset);
			usage.offset = offset;
		}

		friend constexpr auto& pipe_layout_info(basic_pipe_pass& self) noexcept {
			return self.pipe_layout_info;
		}

		VK_ VkPipelineCache pipeline_cache() const noexcept {
			return cache_.value;
		}
		VK_ VkPipelineLayout pipeline_layout() const noexcept {
			return pipeline_layout_.value;
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

		template<typename>
		friend struct basic_shader_express;
		template<typename>
		friend struct basic_resource_usage_express;
		template<typename>
		friend struct basic_format_express;

		constexpr basic_graphics_pass(auto&&... infos)
			: base{ forward_(infos)... } {}

		~basic_graphics_pass() { reset(); }

		void relocate() noexcept {
			N::relocate();
			relocate_pipelines();
		}

		void init() {
			N::init();
			if (pipe_infos.size()) return;
			auto hdv = handle_of<device>(this);
			try {
				for (auto&& [info, shaders] : pipe_infos) {
					info.layout = base::pipeline_layout();
					for (auto& [stage, module, codes] : shaders) {
						assert(codes.size()); // Pass not allow reset but init again.
						module.codeSize = codes.size();
						module.pCode = codes.data();
						VK_ vkCreateShaderModule(hdv, 
							&module, N::allocator(), &stage.module)
							| popup{ "[PASS] Failed to create shader module." };
						codes = {}; // free all memory.
					}
				}

				VK_ vkCreateGraphicsPipelines(hdv,
					base::pipeline_cache(), uint32_t(pipe_infos.size()), pipe_infos.template data<0u>(), 
					N::allocator(), pipe_handles_.data())
					| popup{ "[PASS] Failed to create graphics pipelines." };
				clear_shaders();
			}
			catch (...) {
				reset();
				throw;
			}
		}

		void reset() {
			N::reset();
			auto hdv = handle_of<device>(this);
			for (auto pipe : pipe_handles_) {
				if (pipe != VK_NULL_HANDLE) {
					VK_ vkDestroyPipeline(hdv, ::std::exchange(pipe, VK_NULL_HANDLE), N::allocator());
				}
				else {
					break;
				}
			}
			clear_shaders();
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipe_infos.size());
			return pipe_infos.template get<5u>(index).value;
		}

		uint16_t pipe_count() const noexcept {
			assert(pipe_infos.size() <= uint16_t(maximum));
			return uint16_t(pipe_infos.size());
		}

	protected:
		void append(VK_ VkGraphicsPipelineCreateInfo info) {
			info.pColorBlendState = &defaultColorBlendState;
			info.pVertexInputState = &defaultVertexInputState;
			info.pInputAssemblyState = &defaultInputAssemblyState;
			info.pTessellationState = &defaultTessellationState;
			info.pViewportState = &defaultViewportState;
			info.pRasterizationState = &defaultRasterizationState;
			info.pMultisampleState = &defaultMultisampleState;
			info.pDepthStencilState = &defaultDepthStencilState;
			info.pDynamicState = &defaultDynamicState;
			pipe_infos.emplace_back(::std::move(info), by_default);
		}

		friend constexpr auto& pipeline_info(basic_pipe_pass& self) noexcept {
			assert(!self.pipe_infos.empty());
			return self.pipe_infos;
		}

	private:
		void clear_shaders() {
			for (auto&& [_, vecs] : pipe_infos) {
				for (auto&& [stage, module, codes] : vecs) {
					if (stage.module) {
						VK_ vkDestroyShaderModule(hdv, ::std::exchange(shader.module, VK_NULL_HANDLE), N::allocator());
					}
					else {
						break;
					}
				}
			}
		}
		void relocate_pipelines() noexcept {
			for (auto index = 0u; index < pipe_infos.size(); ++index) {
				relocate_pipeline(index);
			}
		}

		void relocate_pipeline(size_t index) noexcept {
			auto& info = pipe_infos.template get<0u>(index);
			auto& shaders = pipe_infos.template get<1u>(index);
			info.pStages = shaders.template column<0u>().data();
		}

	protected:
		vectors<VK_ VkGraphicsPipelineCreateInfo,
			vectors<VK_ VkPipelineShaderStageCreateInfo,
			VK_ VkShaderModuleCreateInfo,
			vector<uint32_t>>> pipe_infos;

		vector<VK_ VkPipeline> pipes;
	};

	inline constexpr bool is_attachment(uint64_t usage) {
		
	}

	inline constexpr bool attachment_has(uint64_t attributes, attachment_attribute::type value) noexcept {
		return (attributes & value) != 0u;
	}

	inline constexpr auto attachment_role(uint64_t attributes) noexcept {
		return attachment_attribute::type(attributes) & attachment_attribute::type(0x3fu);
	}

	inline constexpr VK_ VkImageLayout attachment_layout(uint64_t attributes) noexcept {
		auto role = attachment_role(attributes);
		if (role == attachment_attribute::color
			|| role == attachment_attribute::resolve) {
			return VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (role == attachment_attribute::depth
			|| role == attachment_attribute::stencil
			|| role == (attachment_attribute::depth | attachment_attribute::stencil)) {
			return VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		assert(role == attachment_attribute::input);
		return VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
		using base::append;

		m(render_pass_, auto&&... infos)
			: base{ forward_(infos)... }
			, info{ .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO } {}

		~m() { reset(); }

		void finalize() {
			base::finalize();
			for (auto index = 0u; index < attachments_.size(); ++index) {
				assert(attachment_declared_[index]);
				auto format = base::attachment_format(uint16_t(index));
				assert(format != VK_ VK_FORMAT_UNDEFINED);
				attachments_[index].format = format;
			}
			for (auto index = 0u; index < base::pipe_count(); ++index) {
				auto subpass = base::pipeline_info(index).subpass;
				ensure_subpass(subpass);
				base::configure_color_attachments(uint16_t(index),
					uint32_t(subpasses_.template get<2u>(subpass).size()));
			}
			relocate();
		}

		void relocate() noexcept {
			base::relocate();
			for (auto index = 0u; index < subpasses_.size(); ++index) {
				auto& subpass = subpasses_.template get<0u>(index);
				auto& inputs = subpasses_.template get<1u>(index);
				auto& colors = subpasses_.template get<2u>(index);
				auto& resolves = subpasses_.template get<3u>(index);
				auto& depth_stencil = subpasses_.template get<4u>(index);
				auto& preserves = subpasses_.template get<5u>(index);
				subpass.inputAttachmentCount = uint32_t(inputs.size());
				subpass.pInputAttachments = data_or_null(inputs);
				subpass.colorAttachmentCount = uint32_t(colors.size());
				subpass.pColorAttachments = data_or_null(colors);
				assert(resolves.empty() || resolves.size() == colors.size());
				subpass.pResolveAttachments = data_or_null(resolves);
				assert(depth_stencil.size() <= 1u);
				subpass.pDepthStencilAttachment = data_or_null(depth_stencil);
				// subpass.preserveAttachmentCount = uint32_t(preserves.size());
				// subpass.pPreserveAttachments = data_or_null(preserves);
			}
			info.attachmentCount = uint32_t(attachments_.size());
			info.pAttachments = data_or_null(attachments_);
			info.subpassCount = uint32_t(subpasses_.size());
			info.pSubpasses = subpasses_.template data<0u>();
			info.dependencyCount = uint32_t(dependencies_.size());
			info.pDependencies = data_or_null(dependencies_);
		}

		auto init() {
			auto locker = basic_pipe_pass<N>::init();
			if (!handle_) {
				VK_ vkCreateRenderPass(handle_of<device>(this), &info,
					N::allocator(), &handle_)
					| popup{ "[PASS] Create render pass failure." };
			}
			try {
				base::init_unlocked(handle_.value);
			}
			catch (...) {
				if (handle_) {
					VK_ vkDestroyRenderPass(handle_of<device>(this),
						::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
				}
				throw;
			}
			return locker;
		}

		auto reset() {
			auto locker = base::reset();
			if (handle_) {
				VK_ vkDestroyRenderPass(handle_of<device>(this),
					::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
			}
			return locker;
		}

		VK_ VkRenderPass pass() const noexcept { return handle_.value; }

	protected:
		friend constexpr void append(m& self, default_resource_usage const& usage) {
			append(static_cast<base&>(self), usage);
			if (usage.resource_type != VK_ VK_OBJECT_TYPE_IMAGE || attachment_role(usage.attributes) == 0u) return;

			assert(base::pipe_count() != 0u);
			assert(usage.index != invalid);
			auto subpass = .subpass;
			ensure_subpass(uint16_t(subpass));
			ensure_attachment(usage.index);

			auto role = attachment_role(usage.attributes);
			assert(role == attachment_attribute::color
				|| role == attachment_attribute::depth
				|| role == attachment_attribute::stencil
				|| role == (attachment_attribute::depth | attachment_attribute::stencil)
				|| role == attachment_attribute::resolve
				|| role == attachment_attribute::input);

			auto& description = attachments_[usage.index];
			if (!attachment_declared_[usage.index]) {
				attachment_declared_[usage.index] = true;
				description.samples = VK_ VK_SAMPLE_COUNT_1_BIT;
				description.loadOp = attachment_has(usage.attributes, attachment_attribute::clear)
					? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
					: attachment_has(usage.attributes, attachment_attribute::load)
					? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
					: VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencilLoadOp = attachment_has(usage.attributes, attachment_attribute::clear_stencil)
					? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
					: attachment_has(usage.attributes, attachment_attribute::load_stencil)
					? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
					: VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout = (attachment_has(usage.attributes, attachment_attribute::clear)
					|| attachment_has(usage.attributes, attachment_attribute::clear_stencil))
					? VK_ VK_IMAGE_LAYOUT_UNDEFINED : usage.layout;
			}
			description.storeOp = attachment_has(usage.attributes, attachment_attribute::store)
				? VK_ VK_ATTACHMENT_STORE_OP_STORE : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
			description.stencilStoreOp = attachment_has(usage.attributes, attachment_attribute::store_stencil)
				? VK_ VK_ATTACHMENT_STORE_OP_STORE : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
			description.finalLayout = usage.layout;

			auto reference = VK_ VkAttachmentReference{
				.attachment = usage.index,
				.layout = usage.layout,
			};
			// TODO: switch.
			if (role == attachment_attribute::color) {
				insert_reference(subpasses_.template get<2u>(subpass), reference);
			}
			else if (role == attachment_attribute::resolve) {
				insert_reference(subpasses_.template get<3u>(subpass), reference);
			}
			else if (role == attachment_attribute::input) {
				insert_reference(subpasses_.template get<1u>(subpass), reference);
			}
			else {
				auto& depth_stencil = subpasses_.template get<4u>(subpass);
				assert(depth_stencil.empty()
					|| depth_stencil.front().attachment == usage.index);
				if (depth_stencil.empty()) depth_stencil.emplace_back(reference);
				else depth_stencil.front().layout = usage.layout;
			}

			append_dependency(uint16_t(subpass), usage);
		}

	protected:
		VK_ VkRenderPassCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

	private:
		void ensure_subpass(uint16_t index) {
			if (subpasses_.size() <= index) {
				auto old_size = subpasses_.size();
				subpasses_.resize(size_t(index) + 1u);
				for (; old_size < subpasses_.size(); ++old_size) {
					subpasses_.template get<0u>(old_size).pipelineBindPoint =
						VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
				}
			}
		}

		void ensure_attachment(uint16_t index) {
			if (attachments_.size() <= index) {
				auto size = size_t(index) + 1u;
				attachments_.resize(size);
				attachment_declared_.resize(size, false);
				previous_usages_.resize(size);
			}
		}

		static void insert_reference(vector<VK_ VkAttachmentReference>& references, VK_ VkAttachmentReference reference) {
			auto it = ::std::ranges::lower_bound(references, [=](auto const& current) {
				return current.attachment < reference.attachment;
			});
			if (it == references.end() || it->attachment != reference.attachment) {
				references.insert(it, reference);
			}
			else {
				assert(it->layout == reference.layout);
			}
		}

		void append_dependency(uint16_t subpass, default_resource_usage const& usage) {
			auto& previous = previous_usages_[usage.index];
			if (previous.valid && previous.subpass != subpass
				&& (access_writes(previous.usage.access)
					|| access_writes(usage.access)
					|| previous.usage.layout != usage.layout)) {
				auto found = ::std::ranges::find_if(dependencies_, [&](auto const& value) {
					return value.srcSubpass == previous.subpass
						&& value.dstSubpass == subpass;
				});
				if (found == dependencies_.end()) {
					dependencies_.emplace_back(VK_ VkSubpassDependency{
						.srcSubpass = previous.subpass,
						.dstSubpass = subpass,
					});
					found = dependencies_.end() - 1;
				}
				found->srcStageMask |= previous.usage.stages;
				found->srcAccessMask |= previous.usage.access;
				found->dstStageMask |= usage.stages;
				found->dstAccessMask |= usage.access;
				found->dependencyFlags |= previous.usage.dependency | usage.dependency;
			}
		}

	private:
		vector<VK_ VkAttachmentDescription> attachments_;
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
		using base::append;
		friend struct express<pipe_>;
		template<typename>
		friend struct basic_format_express;

		constexpr m(pass_extensions::rendering_, auto&&... infos)
			: base{ forward_(infos)... } {}

		void finalize() {
			base::finalize();
			assert(renderings_.size() == base::pipe_count());
			for (auto index = 0u; index < renderings_.size(); ++index) {
				base::configure_color_attachments(uint16_t(index),
					uint32_t(renderings_[index].color_formats.size()));
			}
			relocate();
		}

		void relocate() noexcept {
			base::relocate();
			assert(renderings_.size() == base::pipe_count());
			for (auto index = 0u; index < renderings_.size(); ++index) {
				auto& rendering = renderings_[index];
				rendering.relocate();
				base::pipeline_info(index).pNext = &rendering.info;
			}
		}

		auto init() { return base::init(); }

	protected:
		uint16_t append(VK_ VkGraphicsPipelineCreateInfo info) {
			//dynamic_rendering_record rendering;
			rendering.next = info.pNext;
			auto result = base::append(info);
			renderings_.emplace_back(::std::move(rendering));
			relocate();
			return result;
		}

		void append(VK_ VkFormat format) {
			base::append(format);
			assert(!renderings_.empty());
			auto const& usage = this->last_usage();
			auto role = attachment_role(usage.attributes);
			auto& rendering = renderings_.back();
			if (role == attachment_attribute::color) {
				if (rendering.color_formats.size() <= usage.index) {
					rendering.color_formats.resize(size_t(usage.index) + 1u,
						VK_ VK_FORMAT_UNDEFINED);
				}
				rendering.color_formats[usage.index] = format;
			}
			else if (attachment_has(role, attachment_attribute::depth)) {
				rendering.info.depthAttachmentFormat = format;
			}
			if (attachment_has(role, attachment_attribute::stencil)) {
				rendering.info.stencilAttachmentFormat = format;
			}
			rendering.relocate();
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
			relocate();
		}

		void relocate() noexcept {
			N::relocate();
			for (auto& pipeline : pipe_infos) {
				pipeline.shader.relocate();
				pipeline.info.stage = pipeline.shader.stage;
			}
		}

		auto init() {
			auto locker = base::init();
			if (pipe_infos.empty() || pipe_infos.front().handle) return locker;
			auto hdv = handle_of<device>(this);
			try {
				for (auto& pipeline : pipe_infos) {
					assert(pipeline.shader.source);
					assert(!pipeline.shader.source->compiled.empty());
					assert((pipeline.shader.source->compiled.size() % sizeof(uint32_t)) == 0u);
					pipeline.shader.relocate();
					VK_ vkCreateShaderModule(hdv, &pipeline.shader.module_info,
						N::allocator(), &pipeline.shader.module)
						| popup{ "[PASS] Failed to create compute shader module." };
					pipeline.shader.stage.module = pipeline.shader.module.value;
					pipeline.info.stage = pipeline.shader.stage;
					pipeline.info.layout = base::pipeline_layout();
				}

				vector<VK_ VkComputePipelineCreateInfo> infos;
				vector<VK_ VkPipeline> handles(pipe_infos.size(), VK_NULL_HANDLE);
				infos.reserve(pipe_infos.size());
				for (auto const& pipeline : pipe_infos) infos.emplace_back(pipeline.info);
				try {
					VK_ vkCreateComputePipelines(hdv, base::pipeline_cache(),
						uint32_t(infos.size()), infos.data(), N::allocator(), handles.data())
						| popup{ "[PASS] Failed to create compute pipelines." };
				}
				catch (...) {
					for (auto handle : handles) {
						if (handle) VK_ vkDestroyPipeline(hdv, handle, N::allocator());
					}
					throw;
				}
				for (auto index = 0u; index < pipe_infos.size(); ++index) {
					pipe_infos[index].handle.value = handles[index];
				}
			}
			catch (...) {
				reset_unlocked(hdv);
				throw;
			}
			return locker;
		}

		auto reset() {
			auto locker = base::reset();
			reset_unlocked(handle_of<device>(this));
			return locker;
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipe_infos.size());
			return pipe_infos[index].handle.value;
		}

		uint16_t pipe_count() const noexcept {
			assert(pipe_infos.size() <= uint16_t(maximum));
			return uint16_t(pipe_infos.size());
		}

	protected:
		uint16_t append(VK_ VkComputePipelineCreateInfo info) {
			assert(pipe_infos.size() < uint16_t(maximum));
			pipe_infos.emplace_back(compute_pipeline_record{ .info = info });
			return uint16_t(pipe_infos.size() - 1u);
		}

		void append_shader(shader_handle shader,
			VK_ VkPipelineShaderStageCreateInfo info) {
			assert(shader);
			assert(!pipe_infos.empty());
			assert(!pipe_infos.back().shader.source);
			info.module = VK_NULL_HANDLE;
			pipe_infos.back().shader.stage = info;
			pipe_infos.back().shader.source = shader;
		}

		void customize_shader_entry_point(char const* name) noexcept {
			assert(name);
			last_shader_stage().pName = name;
		}

		VK_ VkPipelineShaderStageCreateInfo& last_shader_stage() noexcept {
			assert(!pipe_infos.empty());
			assert(pipe_infos.back().shader.source);
			return pipe_infos.back().shader.stage;
		}

	private:
		void reset_unlocked(VK_ VkDevice hdv) noexcept {
			for (auto& pipeline : pipe_infos) {
				if (pipeline.handle) {
					VK_ vkDestroyPipeline(hdv,
						::std::exchange(pipeline.handle.value, VK_NULL_HANDLE),
						N::allocator());
				}
				if (pipeline.shader.module) {
					VK_ vkDestroyShaderModule(hdv,
						::std::exchange(pipeline.shader.module.value, VK_NULL_HANDLE),
						N::allocator());
					pipeline.shader.stage.module = VK_NULL_HANDLE;
				}
			}
		}
		// [create info, shader codes].
		vectors<VK_ VkComputePipelineCreateInfo, vector<uint32_t>> pipe_infos;
	};

	template<typename N>
	struct m<pipe_, N> : N {
		using base = N;
		constexpr m(pipe_, auto&&...others)
			: N{forward_(others)...} {
			if constexpr (requires (VK_ VkGraphicsPipelineCreateInfo const& v) { base::append(v); }) {
				base::append(VK_ VkGraphicsPipelineCreateInfo {
					.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					// TODO: 
				});
			}
			else if constexpr (requires (VK_ VkGraphicsPipelineCreateInfo const& v) { base::append(v); }) {
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
			base.append_subpass(value.index);
		}
	};

	template<>
	struct trait<vertex_shader> {
		using type = vertex_shader;
		static constexpr VK_ VkShaderStageFlagBits stage = VK_ VK_SHADER_STAGE_VERTEX_BIT;
		static constexpr shader_handle handle(type shader) noexcept { return shader.bytes; }
	};

	template<>
	struct trait<fragment_shader> {
		using type = fragment_shader;
		static constexpr VK_ VkShaderStageFlagBits stage = VK_ VK_SHADER_STAGE_FRAGMENT_BIT;
		static constexpr shader_handle handle(type shader) noexcept { return shader.bytes; }
	};

	template<>
	struct trait<compute_shader> {
		using type = compute_shader;
		static constexpr VK_ VkShaderStageFlagBits stage = VK_ VK_SHADER_STAGE_COMPUTE_BIT;
		static constexpr shader_handle handle(type shader) noexcept { return shader.code; }
	};

	template<typename Trait>
	struct basic_shader_express {
		using shader_type = typename Trait::type;

		static void invoke(shader_type shader, auto& base) {
			base.append_shader(Trait::handle(shader),
				VK_ VkPipelineShaderStageCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = Trait::stage,
					.pName = "main",
				});
		}
	};

	template<>
	struct express<vertex_shader> : basic_shader_express<trait<vertex_shader>> {};
	template<>
	struct express<fragment_shader> : basic_shader_express<trait<fragment_shader>> {};
	template<>
	struct express<compute_shader> : basic_shader_express<trait<compute_shader>> {};

	using namespace shader_extensions;

	template<>
	struct express<customized_entry_point> {
		static constexpr void invoke(customized_entry_point entry, auto& base) {
			base.customize_shader_entry_point(entry.name);
		}
	};

	constexpr VK_ VkPipelineStageFlags shader_pipeline_stage(
		VK_ VkShaderStageFlags shader_stage) noexcept {
		VK_ VkPipelineStageFlags result = 0u;
		if (shader_stage & VK_ VK_SHADER_STAGE_VERTEX_BIT)
			result |= VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		if (shader_stage & VK_ VK_SHADER_STAGE_FRAGMENT_BIT)
			result |= VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		if (shader_stage & VK_ VK_SHADER_STAGE_COMPUTE_BIT)
			result |= VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		return result ? result : VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	template<>
	struct trait<uniform_buffer> {
		using type = uniform_buffer;
		using resource_type = buffer;
		static constexpr VK_ VkObjectType resource_object_type = VK_ VK_OBJECT_TYPE_BUFFER;
		static constexpr bool uses_descriptor = true;
		static constexpr VK_ VkDescriptorType descriptor_type = VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		static constexpr uint64_t usage(type) noexcept {
			return VK_ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		static constexpr VK_ VkAccessFlags access(type) noexcept {
			return VK_ VK_ACCESS_UNIFORM_READ_BIT;
		}
	};

	template<>
	struct trait<attachment> {
		using type = attachment;
		using resource_type = image;
		static constexpr VK_ VkObjectType resource_object_type = VK_ VK_OBJECT_TYPE_IMAGE;
		static constexpr bool uses_descriptor = false;
		static constexpr VK_ VkDescriptorType descriptor_type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;

		static constexpr uint64_t usage(type value) noexcept {
			assert(!(attachment_has(value.attribute, attachment_attribute::load)
				&& (attachment_has(value.attribute, attachment_attribute::clear)
					|| attachment_has(value.attribute, attachment_attribute::clear_stencil))));
			assert(!attachment_has(value.attribute, attachment_attribute::clear_stencil)
				|| attachment_has(value.attribute,
					attachment_attribute::depth | attachment_attribute::stencil));
			assert(!attachment_has(value.attribute, attachment_attribute::store_stencil)
				|| attachment_has(value.attribute,
					attachment_attribute::depth | attachment_attribute::stencil));
			auto role = attachment_role(value.attribute);
			if (role == attachment_attribute::input)
				return VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			if (role == attachment_attribute::depth
				|| role == attachment_attribute::stencil
				|| role == (attachment_attribute::depth | attachment_attribute::stencil)) {
				return VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}
			assert(role == attachment_attribute::color
				|| role == attachment_attribute::resolve);
			return VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		static constexpr VK_ VkPipelineStageFlags stages(type value) noexcept {
			auto role = attachment_role(value.attribute);
			if (role == attachment_attribute::input)
				return VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (role == attachment_attribute::depth
				|| role == attachment_attribute::stencil
				|| role == (attachment_attribute::depth | attachment_attribute::stencil)) {
				return VK_ VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
					| VK_ VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			}
			return VK_ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}

		static constexpr VK_ VkAccessFlags access(type value) noexcept {
			auto role = attachment_role(value.attribute);
			if (role == attachment_attribute::input)
				return VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			if (role == attachment_attribute::depth
				|| role == attachment_attribute::stencil
				|| role == (attachment_attribute::depth | attachment_attribute::stencil)) {
				return VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
					| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			}
			return VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		static constexpr VK_ VkImageLayout layout(type value) noexcept {
			return attachment_layout(value.attribute);
		}

		static constexpr uint64_t attributes(type value) noexcept {
			return value.attribute;
		}
	};

	template<typename Trait>
	struct basic_resource_usage_express {
		using resource_usage_type = typename Trait::type;

		static constexpr void invoke(resource_usage_type usage, auto& base) {
			default_resource_usage result{
				.index = uint16_t(usage.index),
				.resource_type = Trait::resource_object_type,
				.type = Trait::descriptor_type,
				.usages = Trait::usage(usage),
			};

			if constexpr (Trait::uses_descriptor) {
				result.shader_stages = base.last_shader_stage().stage;
				result.stages = shader_pipeline_stage(result.shader_stages);
			}
			if constexpr (requires { Trait::stages(usage); })
				result.stages = Trait::stages(usage);
			if constexpr (requires { Trait::access(usage); })
				result.access = Trait::access(usage);
			if constexpr (requires { Trait::layout(usage); })
				result.layout = Trait::layout(usage);
			if constexpr (requires { Trait::attributes(usage); })
				result.attributes = Trait::attributes(usage);

			base.append(result);
		}
	};

	template<>
	struct express<uniform_buffer> : basic_resource_usage_express<trait<uniform_buffer>> {};
	template<>
	struct express<attachment> : basic_resource_usage_express<trait<attachment>> {};

	using namespace resource_usage_extensions;

	template<>
	struct express<bind_on_set> {
		static void invoke(bind_on_set value, auto& object) {
			object.append_binding(value.set, value.binding);
		}
	};

	template<>
	struct express<bind_on_heap> {
		static void invoke(bind_on_heap value, auto& object) {
			object.append_binding(value.offset);
		}
	};
}
