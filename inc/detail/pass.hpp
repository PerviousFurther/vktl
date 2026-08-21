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
// Graphics and compute records own their live pipeline and shader-module
// handles. Every pointer-bearing Vulkan structure is rebound by `relocate()`.
// 
// use `empty() ? nullptr : data()` for array pointer if there is no 
// independent count for it.
// 
// A classic render pass derives subpass dependencies from repeated attachment
// declarations inside that pass. Cross-pass barriers remain outside this file.
// 
// Descriptor-set layouts are normalized value objects cached permanently by
// the device. A pass and a bind set keep independent schema values/handles;
// neither stores references into the other. Pipeline set numbers map through
// `default_bind_set_schema::set_layout_indices`, whose holes remain `invalid`.
// Pipeline-layout creation materializes an empty layout for every such hole.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	template<typename Trait>
	struct basic_resource_usage_express;
	template<typename Trait>
	struct basic_shader_express;
	template<typename Format>
	struct basic_format_express;

	struct default_bind_set_schema {
		vector<default_descriptor_set_layout> layout_infos;
		vector<uint16_t> set_layout_indices;

		constexpr bool operator==(default_bind_set_schema const& other) const noexcept {
			return layout_infos == other.layout_infos
				&& set_layout_indices == other.set_layout_indices;
		}
	};

	struct default_resource_usage {
		uint16_t index = invalid;
		uint16_t reserved = 0u;
		VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
		VK_ VkDescriptorType type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		VK_ VkShaderStageFlags shader_stages = 0u;
		VK_ VkPipelineStageFlags stages = 0u;
		VK_ VkAccessFlags access = 0u;
		VK_ VkDependencyFlags dependency = 0u;
		VK_ VkImageLayout layout = VK_ VK_IMAGE_LAYOUT_UNDEFINED;

		uint32_t set = invalid;
		uint32_t binding = invalid;
		uint64_t offset = maximum;
		uint64_t usages = 0u;
		uint64_t attributes = 0u;

		constexpr bool uses_descriptor() const noexcept {
			return type != VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	};

	using namespace pass_extensions;

	template<>
	struct is_queryable<render_pass_, pass_extensions::rendering_> : ::std::true_type {};

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
		void append(default_resource_usage const& usage) {
			usages_.emplace_back(usage);
		}

		default_resource_usage& last_usage() noexcept {
			assert(!usages_.empty());
			return usages_.back();
		}

		default_resource_usage const& last_usage() const noexcept {
			assert(!usages_.empty());
			return usages_.back();
		}

	private:
		vector<default_resource_usage> usages_;
	};

	template<typename T>
	constexpr T const* data_or_null(vector<T> const& values) noexcept {
		return values.empty() ? nullptr : values.data();
	}

	template<typename T>
	constexpr T* data_or_null(vector<T>& values) noexcept {
		return values.empty() ? nullptr : values.data();
	}

	template<typename N>
	struct basic_pipe_pass : N {
		using N::append;
		friend struct express<resource_usage_extensions::bind_on_set>;
		friend struct express<resource_usage_extensions::bind_on_heap>;

		constexpr basic_pipe_pass(auto&&... args)
			: N{ forward_(args)... } {}

		~basic_pipe_pass() { reset(); }

		void fill(auto& bind_set) {
			auto _ = locker_of(this);
			bind_set.accept(schema_);
			for (auto const& usage : this->usages()) {
				bind_set.bind(usage);
			}
		}

		VK_ VkPipelineLayout pipeline_layout() const noexcept {
			return pipeline_layout_.value;
		}

		void init() {
			N::init();
			auto _ = locker_of(this);
			if (pipeline_layout_) return;

			auto pdv = parent_of<device>(this);
			auto hdv = pdv->handle();

			VK_ vkCreatePipelineCache(hdv, &cache_info, N::allocator(), &cache_)
				| popup{ "[PASS] Create pipeline cache failure." };

			try {
				auto empty_layout = pdv->create(default_descriptor_set_layout{});
				vector<VK_ VkDescriptorSetLayout> layouts;
				layouts.reserve(schema_.set_layout_indices.size());
				for (auto index : schema_.set_layout_indices) {
					layouts.emplace_back(index == invalid
						? empty_layout
						: pdv->create(schema_.layout_infos[index]));
				}

				default_pipeline_layout layout_info;
				layout_info.layouts = ::std::move(layouts);
				pipeline_layout_.value = pdv->create(::std::move(layout_info));
			}
			catch (...) {
				reset();
				throw;
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			if (cache_) {
				VK_ vkDestroyPipelineCache(handle_of<device>(this),
					::std::exchange(cache_.value, VK_NULL_HANDLE), N::allocator());
			}
			pipeline_layout_.value = VK_NULL_HANDLE;
		}

	protected:
		void finalize() {
			if constexpr (requires { N::finalize(); }) { N::finalize(); }

			vector<default_descriptor_set_layout> layouts_by_set;
			vector<bool> used_sets;
			for (auto const& usage : this->usages()) {
				if (!usage.uses_descriptor()) continue;
				assert(usage.set != invalid);
				assert(usage.binding != invalid);
				assert(usage.shader_stages != 0u);

				if (layouts_by_set.size() <= usage.set) {
					layouts_by_set.resize(size_t(usage.set) + 1u);
					used_sets.resize(size_t(usage.set) + 1u, false);
				}
				used_sets[usage.set] = true;
				layouts_by_set[usage.set].add(VK_ VkDescriptorSetLayoutBinding{
					.binding = usage.binding,
					.descriptorType = usage.type,
					.descriptorCount = 1u,
					.stageFlags = usage.shader_stages,
					});
			}

			default_bind_set_schema result;
			result.set_layout_indices.resize(layouts_by_set.size(), uint16_t(invalid));
			for (auto set = 0u; set < layouts_by_set.size(); ++set) {
				if (!used_sets[set]) continue;

				auto const& layout = layouts_by_set[set];
				auto found = ::std::ranges::find(result.layout_infos, layout);
				if (found == result.layout_infos.end()) {
					result.layout_infos.emplace_back(layout);
					found = result.layout_infos.end() - 1;
				}
				result.set_layout_indices[set] = uint16_t(
					::std::distance(result.layout_infos.begin(), found));
			}
			schema_ = ::std::move(result);
		}

		void append_binding(uint32_t set, uint32_t binding) noexcept {
			auto& usage = this->last_usage();
			assert(usage.uses_descriptor());
			assert(usage.set == invalid || usage.set == set);
			assert(usage.binding == invalid || usage.binding == binding);
			usage.set = set;
			usage.binding = binding;
		}

		void append_binding(uint64_t offset) noexcept {
			auto& usage = this->last_usage();
			assert(usage.offset == maximum || usage.offset == offset);
			usage.offset = offset;
		}

		VK_ VkPipelineCache pipeline_cache() const noexcept {
			return cache_.value;
		}

	protected:
		default_bind_set_schema schema_;
		VK_ VkPipelineCacheCreateInfo cache_info{ .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, };

	private:
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

	struct shader_record {
		VK_ VkPipelineShaderStageCreateInfo stage{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pName = "main",
		};
		VK_ VkShaderModuleCreateInfo module_info{
			.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		};
		shader_handle source = nullptr;
		reset_if_copy<VK_ VkShaderModule> module{ VK_NULL_HANDLE };

		void relocate() noexcept {
			stage.module = module.value;
			if (source) {
				module_info.codeSize = source->compiled.size();
				module_info.pCode = source->compiled.empty() ? nullptr
					: reinterpret_cast<uint32_t const*>(source->compiled.data());
			}
			else {
				module_info.codeSize = 0u;
				module_info.pCode = nullptr;
			}
		}
	};

	struct graphics_pipeline_record {
		VK_ VkGraphicsPipelineCreateInfo info{
			.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		};
		vector<shader_record> shaders;
		vector<VK_ VkPipelineShaderStageCreateInfo> stages;
		VK_ VkPipelineColorBlendStateCreateInfo color_blend = defaultColorBlendState;
		vector<VK_ VkPipelineColorBlendAttachmentState> color_attachments;
		reset_if_copy<VK_ VkPipeline> handle{ VK_NULL_HANDLE };

		void relocate() noexcept {
			for (auto& shader : shaders) shader.relocate();
			stages.resize(shaders.size());
			for (auto index = 0u; index < shaders.size(); ++index) {
				stages[index] = shaders[index].stage;
			}
			info.stageCount = uint32_t(stages.size());
			info.pStages = data_or_null(stages);
			color_blend.attachmentCount = uint32_t(color_attachments.size());
			color_blend.pAttachments = data_or_null(color_attachments);
			info.pColorBlendState = &color_blend;
		}
	};

	template<typename N>
	struct basic_graphics_pass : basic_pipe_pass<N> {
		using base = basic_pipe_pass<N>;
		using base::append;
		friend struct express<pipe_>;
		friend struct express<pipe_extensions::subpass>;
		friend struct express<shader_extensions::customized_entry_point>;
		template<typename>
		friend struct basic_shader_express;
		template<typename>
		friend struct basic_resource_usage_express;
		template<typename>
		friend struct basic_format_express;

		constexpr basic_graphics_pass(auto&&... infos)
			: base{ forward_(infos)... } {}

		~basic_graphics_pass() { reset(); }

		void finalize() {
			base::finalize();
			relocate();
		}

		void relocate() noexcept {
			N::relocate();
			for (auto& pipeline : pipelines_) pipeline.relocate();
		}

		void init(VK_ VkRenderPass render_pass = VK_NULL_HANDLE) {
			base::init();
			auto _ = locker_of(this);
			if (pipelines_.empty() || pipelines_.front().handle) return;

			auto hdv = handle_of<device>(this);
			try {
				for (auto& pipeline : pipelines_) {
					pipeline.info.layout = base::pipeline_layout();
					pipeline.info.renderPass = render_pass;
					for (auto& shader : pipeline.shaders) {
						assert(shader.source);
						assert(!shader.source->compiled.empty());
						assert((shader.source->compiled.size() % sizeof(uint32_t)) == 0u);
						shader.relocate();
						VK_ vkCreateShaderModule(hdv, &shader.module_info,
							N::allocator(), &shader.module)
							| popup{ "[PASS] Failed to create shader module." };
						shader.stage.module = shader.module.value;
					}
					pipeline.relocate();
				}

				vector<VK_ VkGraphicsPipelineCreateInfo> infos;
				vector<VK_ VkPipeline> handles(pipelines_.size(), VK_NULL_HANDLE);
				infos.reserve(pipelines_.size());
				for (auto const& pipeline : pipelines_) infos.emplace_back(pipeline.info);
				try {
					VK_ vkCreateGraphicsPipelines(hdv, base::pipeline_cache(),
						uint32_t(infos.size()), infos.data(), N::allocator(), handles.data())
						| popup{ "[PASS] Failed to create graphics pipelines." };
				}
				catch (...) {
					for (auto handle : handles) {
						if (handle) VK_ vkDestroyPipeline(hdv, handle, N::allocator());
					}
					throw;
				}
				for (auto index = 0u; index < pipelines_.size(); ++index) {
					pipelines_[index].handle.value = handles[index];
				}
			}
			catch (...) {
				reset_unlocked(hdv);
				throw;
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			reset_unlocked(handle_of<device>(this));
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipelines_.size());
			return pipelines_[index].handle.value;
		}

		uint16_t pipe_count() const noexcept {
			assert(pipelines_.size() <= uint16_t(maximum));
			return uint16_t(pipelines_.size());
		}

	protected:
		uint16_t append(VK_ VkGraphicsPipelineCreateInfo info) {
			assert(pipelines_.size() < uint16_t(maximum));
			graphics_pipeline_record record;
			record.info = info;
			if (info.pColorBlendState) {
				record.color_blend = *info.pColorBlendState;
				assert(info.pColorBlendState->attachmentCount == 0u
					|| info.pColorBlendState->pAttachments);
				if (info.pColorBlendState->attachmentCount) {
					record.color_attachments.assign(
						info.pColorBlendState->pAttachments,
						info.pColorBlendState->pAttachments
							+ info.pColorBlendState->attachmentCount);
				}
			}
			record.info.pVertexInputState = info.pVertexInputState
				? info.pVertexInputState : &defaultVertexInputState;
			record.info.pInputAssemblyState = info.pInputAssemblyState
				? info.pInputAssemblyState : &defaultInputAssemblyState;
			record.info.pTessellationState = info.pTessellationState
				? info.pTessellationState : &defaultTessellationState;
			record.info.pViewportState = info.pViewportState
				? info.pViewportState : &defaultViewportState;
			record.info.pRasterizationState = info.pRasterizationState
				? info.pRasterizationState : &defaultRasterizationState;
			record.info.pMultisampleState = info.pMultisampleState
				? info.pMultisampleState : &defaultMultisampleState;
			record.info.pDepthStencilState = info.pDepthStencilState
				? info.pDepthStencilState : &defaultDepthStencilState;
			record.info.pDynamicState = info.pDynamicState
				? info.pDynamicState : &defaultDynamicState;
			pipelines_.emplace_back(::std::move(record));
			pipelines_.back().relocate();
			return uint16_t(pipelines_.size() - 1u);
		}

		void append_shader(shader_handle shader,
			VK_ VkPipelineShaderStageCreateInfo info) {
			assert(shader);
			assert(!pipelines_.empty());
			info.module = VK_NULL_HANDLE;
			pipelines_.back().shaders.emplace_back(shader_record{
				.stage = info,
				.source = shader,
			});
			pipelines_.back().relocate();
		}

		void customize_shader_entry_point(char const* name) noexcept {
			assert(name);
			last_shader_stage().pName = name;
		}

		VK_ VkGraphicsPipelineCreateInfo& last_pipe() noexcept {
			assert(!pipelines_.empty());
			return pipelines_.back().info;
		}

		VK_ VkPipelineShaderStageCreateInfo& last_shader_stage() noexcept {
			assert(!pipelines_.empty());
			assert(!pipelines_.back().shaders.empty());
			return pipelines_.back().shaders.back().stage;
		}

		void append(VK_ VkFormat format) {
			assert(format != VK_ VK_FORMAT_UNDEFINED);
			auto const& usage = this->last_usage();
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

		VK_ VkFormat attachment_format(uint16_t index) const noexcept {
			assert(index < attachment_formats_.size());
			return attachment_formats_[index];
		}

		void configure_color_attachments(uint16_t pipeline, uint32_t count) {
			assert(pipeline < pipelines_.size());
			auto& record = pipelines_[pipeline];
			if (record.color_attachments.empty()) {
				record.color_attachments.resize(count, defaultColorBlendAttachment);
			}
			else {
				assert(record.color_attachments.size() == count);
			}
			record.relocate();
		}

		vector<graphics_pipeline_record>& pipeline_records() noexcept {
			return pipelines_;
		}

	private:
		void reset_unlocked(VK_ VkDevice hdv) noexcept {
			for (auto& pipeline : pipelines_) {
				if (pipeline.handle) {
					VK_ vkDestroyPipeline(hdv,
						::std::exchange(pipeline.handle.value, VK_NULL_HANDLE),
						N::allocator());
				}
			}
			for (auto& pipeline : pipelines_) {
				for (auto& shader : pipeline.shaders) {
					if (shader.module) {
						VK_ vkDestroyShaderModule(hdv,
							::std::exchange(shader.module.value, VK_NULL_HANDLE),
							N::allocator());
						shader.stage.module = VK_NULL_HANDLE;
					}
				}
			}
		}

		vector<graphics_pipeline_record> pipelines_;
		vector<VK_ VkFormat> attachment_formats_;
	};

	constexpr bool attachment_has(uint64_t attributes,
		attachment_attribute::type value) noexcept {
		return (attributes & value) != 0u;
	}

	constexpr auto attachment_role(uint64_t attributes) noexcept {
		return attachment_attribute::type(attributes) & attachment_attribute::type(0x1fu);
	}

	constexpr VK_ VkImageLayout attachment_layout(uint64_t attributes) noexcept {
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

	constexpr bool access_writes(VK_ VkAccessFlags access) noexcept {
		constexpr auto writes = VK_ VK_ACCESS_SHADER_WRITE_BIT
			| VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_TRANSFER_WRITE_BIT
			| VK_ VK_ACCESS_HOST_WRITE_BIT
			| VK_ VK_ACCESS_MEMORY_WRITE_BIT;
		return (access & writes) != 0u;
	}

	struct render_subpass_storage {
		vector<VK_ VkAttachmentReference> inputs;
		vector<VK_ VkAttachmentReference> colors;
		vector<VK_ VkAttachmentReference> resolves;
		vector<VK_ VkAttachmentReference> depth_stencil;
		vector<uint32_t> preserves;
	};

	template<typename N>
	struct m<render_pass_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;
		using base::append;
		friend struct express<pipe_extensions::subpass>;
		template<typename>
		friend struct basic_resource_usage_express;

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
			for (auto index = 0u; index < base::pipeline_records().size(); ++index) {
				auto subpass = base::pipeline_records()[index].info.subpass;
				ensure_subpass(subpass);
				base::configure_color_attachments(uint16_t(index),
					uint32_t(subpass_storage_[subpass].colors.size()));
			}
			relocate();
		}

		void relocate() noexcept {
			base::relocate();
			for (auto index = 0u; index < subpasses_.size(); ++index) {
				auto& subpass = subpasses_[index];
				auto& storage = subpass_storage_[index];
				subpass.inputAttachmentCount = uint32_t(storage.inputs.size());
				subpass.pInputAttachments = data_or_null(storage.inputs);
				subpass.colorAttachmentCount = uint32_t(storage.colors.size());
				subpass.pColorAttachments = data_or_null(storage.colors);
				assert(storage.resolves.empty()
					|| storage.resolves.size() == storage.colors.size());
				subpass.pResolveAttachments = data_or_null(storage.resolves);
				assert(storage.depth_stencil.size() <= 1u);
				subpass.pDepthStencilAttachment = data_or_null(storage.depth_stencil);
				subpass.preserveAttachmentCount = uint32_t(storage.preserves.size());
				subpass.pPreserveAttachments = data_or_null(storage.preserves);
			}
			info.attachmentCount = uint32_t(attachments_.size());
			info.pAttachments = data_or_null(attachments_);
			info.subpassCount = uint32_t(subpasses_.size());
			info.pSubpasses = data_or_null(subpasses_);
			info.dependencyCount = uint32_t(dependencies_.size());
			info.pDependencies = data_or_null(dependencies_);
		}

		void init() {
			basic_pipe_pass<N>::init();
			{
				auto _ = locker_of(this);
				if (!handle_) {
					VK_ vkCreateRenderPass(handle_of<device>(this), &info,
						N::allocator(), &handle_)
						| popup{ "[PASS] Create render pass failure." };
				}
			}
			try {
				base::init(handle_.value);
			}
			catch (...) {
				reset();
				throw;
			}
		}

		void reset() noexcept {
			base::reset();
			auto _ = locker_of(this);
			if (handle_) {
				VK_ vkDestroyRenderPass(handle_of<device>(this),
					::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
			}
		}

		VK_ VkRenderPass handle() const noexcept { return handle_.value; }

	protected:
		void append_subpass(uint16_t subpass) {
			assert(!base::pipeline_records().empty());
			ensure_subpass(subpass);
			base::last_pipe().subpass = subpass;
		}

		void append(default_resource_usage const& usage) {
			base::append(usage);
			if (usage.resource_type != VK_ VK_OBJECT_TYPE_IMAGE
				|| attachment_role(usage.attributes) == 0u) return;

			assert(!base::pipeline_records().empty());
			assert(usage.index != invalid);
			auto subpass = base::last_pipe().subpass;
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
					: attachment_has(usage.attributes, attachment_attribute::load)
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
			auto& storage = subpass_storage_[subpass];
			if (role == attachment_attribute::color) insert_reference(storage.colors, reference);
			else if (role == attachment_attribute::resolve) insert_reference(storage.resolves, reference);
			else if (role == attachment_attribute::input) insert_reference(storage.inputs, reference);
			else {
				assert(storage.depth_stencil.empty()
					|| storage.depth_stencil.front().attachment == usage.index);
				if (storage.depth_stencil.empty()) storage.depth_stencil.emplace_back(reference);
				else storage.depth_stencil.front().layout = usage.layout;
			}

			append_dependency(uint16_t(subpass), usage);
		}

		VK_ VkRenderPassCreateInfo info;

	private:
		struct previous_attachment_usage {
			bool valid = false;
			uint16_t subpass = 0u;
			default_resource_usage usage;
		};

		void ensure_subpass(uint16_t index) {
			if (subpasses_.size() <= index) {
				auto old_size = subpasses_.size();
				subpasses_.resize(size_t(index) + 1u);
				subpass_storage_.resize(size_t(index) + 1u);
				for (; old_size < subpasses_.size(); ++old_size) {
					subpasses_[old_size].pipelineBindPoint =
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

		static void insert_reference(vector<VK_ VkAttachmentReference>& references,
			VK_ VkAttachmentReference reference) {
			auto found = ::std::ranges::find_if(references, [=](auto const& current) {
				return current.attachment >= reference.attachment;
			});
			if (found == references.end() || found->attachment != reference.attachment) {
				references.insert(found, reference);
			}
			else {
				assert(found->layout == reference.layout);
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
			previous = previous_attachment_usage{
				.valid = true,
				.subpass = subpass,
				.usage = usage,
			};
		}

		vector<VK_ VkAttachmentDescription> attachments_;
		vector<bool> attachment_declared_;
		vector<VK_ VkSubpassDescription> subpasses_;
		vector<render_subpass_storage> subpass_storage_;
		vector<VK_ VkSubpassDependency> dependencies_;
		vector<previous_attachment_usage> previous_usages_;
		reset_if_copy<VK_ VkRenderPass> handle_{ VK_NULL_HANDLE };
	};

#if defined(VK_KHR_dynamic_rendering)
	struct dynamic_rendering_record {
		VK_ VkPipelineRenderingCreateInfoKHR info{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		};
		vector<VK_ VkFormat> color_formats;
		void const* next = nullptr;

		void relocate() noexcept {
			info.pNext = next;
			info.colorAttachmentCount = uint32_t(color_formats.size());
			info.pColorAttachmentFormats = data_or_null(color_formats);
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
			assert(renderings_.size() == base::pipeline_records().size());
			for (auto index = 0u; index < renderings_.size(); ++index) {
				base::configure_color_attachments(uint16_t(index),
					uint32_t(renderings_[index].color_formats.size()));
			}
			relocate();
		}

		void relocate() noexcept {
			base::relocate();
			assert(renderings_.size() == base::pipeline_records().size());
			for (auto index = 0u; index < renderings_.size(); ++index) {
				auto& rendering = renderings_[index];
				rendering.relocate();
				base::pipeline_records()[index].info.pNext = &rendering.info;
			}
		}

		void init() { base::init(); }

	protected:
		uint16_t append(VK_ VkGraphicsPipelineCreateInfo info) {
			dynamic_rendering_record rendering;
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
		vector<dynamic_rendering_record> renderings_;
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

	struct compute_pipeline_record {
		VK_ VkComputePipelineCreateInfo info{
			.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = { .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		};
		shader_record shader;
		reset_if_copy<VK_ VkPipeline> handle{ VK_NULL_HANDLE };
	};

	template<typename N>
	struct m<pass_extensions::compute_, N> : basic_pipe_pass<N> {
		using base = basic_pipe_pass<N>;
		using base::append;
		friend struct express<pipe_>;
		friend struct express<shader_extensions::customized_entry_point>;
		template<typename>
		friend struct basic_shader_express;
		template<typename>
		friend struct basic_resource_usage_express;

		m(pass_extensions::compute_, auto&&... infos)
			: base{ forward_(infos)... } {}

		~m() { reset(); }

		void finalize() {
			base::finalize();
			relocate();
		}

		void relocate() noexcept {
			N::relocate();
			for (auto& pipeline : pipelines_) {
				pipeline.shader.relocate();
				pipeline.info.stage = pipeline.shader.stage;
			}
		}

		void init() {
			base::init();
			auto _ = locker_of(this);
			if (pipelines_.empty() || pipelines_.front().handle) return;
			auto hdv = handle_of<device>(this);
			try {
				for (auto& pipeline : pipelines_) {
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
				vector<VK_ VkPipeline> handles(pipelines_.size(), VK_NULL_HANDLE);
				infos.reserve(pipelines_.size());
				for (auto const& pipeline : pipelines_) infos.emplace_back(pipeline.info);
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
				for (auto index = 0u; index < pipelines_.size(); ++index) {
					pipelines_[index].handle.value = handles[index];
				}
			}
			catch (...) {
				reset_unlocked(hdv);
				throw;
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			reset_unlocked(handle_of<device>(this));
		}

		VK_ VkPipeline pipe(uint16_t index) const noexcept {
			assert(index < pipelines_.size());
			return pipelines_[index].handle.value;
		}

		uint16_t pipe_count() const noexcept {
			assert(pipelines_.size() <= uint16_t(maximum));
			return uint16_t(pipelines_.size());
		}

	protected:
		uint16_t append(VK_ VkComputePipelineCreateInfo info) {
			assert(pipelines_.size() < uint16_t(maximum));
			pipelines_.emplace_back(compute_pipeline_record{ .info = info });
			return uint16_t(pipelines_.size() - 1u);
		}

		void append_shader(shader_handle shader,
			VK_ VkPipelineShaderStageCreateInfo info) {
			assert(shader);
			assert(!pipelines_.empty());
			assert(!pipelines_.back().shader.source);
			info.module = VK_NULL_HANDLE;
			pipelines_.back().shader.stage = info;
			pipelines_.back().shader.source = shader;
		}

		void customize_shader_entry_point(char const* name) noexcept {
			assert(name);
			last_shader_stage().pName = name;
		}

		VK_ VkPipelineShaderStageCreateInfo& last_shader_stage() noexcept {
			assert(!pipelines_.empty());
			assert(pipelines_.back().shader.source);
			return pipelines_.back().shader.stage;
		}

	private:
		void reset_unlocked(VK_ VkDevice hdv) noexcept {
			for (auto& pipeline : pipelines_) {
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

		vector<compute_pipeline_record> pipelines_;
	};

	template<>
	struct express<pipe_> {
		static constexpr void invoke(pipe_, auto& base) {
			if constexpr (object_of<decltype(base), pass_extensions::rendering_>) {
				base.append(VK_ VkGraphicsPipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				});
			}
			else if constexpr (object_of<decltype(base), pass_extensions::compute_>) {
				base.append(VK_ VkComputePipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
					.stage = {
						.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					},
				});
			}
			else {
				static_assert(always_false<decltype(base)>,
					"A pipeline requires a graphics or compute pass.");
			}
		}
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

	constexpr uint32_t compress_image_bits(uint16_t bits) noexcept {
		return bits == invalid ? 0u : bits == 8u ? 1u : bits == 16u ? 2u
			: bits == 24u ? 3u : bits == 32u ? 4u : bits == 64u ? 5u : 7u;
	}

	constexpr uint32_t compress_image_type(uint16_t type) noexcept {
		using namespace image_bits_type;
		return type == undefined ? 0u : type == unorm ? 1u : type == uint ? 2u
			: type == snorm ? 3u : type == sint ? 4u : type == sfloat ? 5u : 7u;
	}

	constexpr uint32_t pack_color_format(image_format::format_color const& value) noexcept {
		auto type = [](uint16_t bits, uint16_t kind) {
			return compress_image_type(bits == invalid ? image_bits_type::undefined : kind);
		};
		return (uint32_t(value.order) << 24u)
			| (compress_image_bits(value.rbits) << 20u)
			| (compress_image_bits(value.gbits) << 17u)
			| (compress_image_bits(value.bbits) << 14u)
			| (compress_image_bits(value.abits) << 11u)
			| (type(value.rbits, value.rtype) << 8u)
			| (type(value.gbits, value.gtype) << 5u)
			| (type(value.bbits, value.btype) << 2u)
			| type(value.abits, value.atype);
	}

	constexpr uint32_t color_format_case(uint16_t order, uint16_t r, uint16_t g,
		uint16_t b, uint16_t a, uint16_t type) noexcept {
		return pack_color_format(image_format::format_color{
			.order = order,
			.rbits = r,
			.gbits = g,
			.bbits = b,
			.abits = a,
			.rtype = type,
			.gtype = type,
			.btype = type,
			.atype = type,
		});
	}

	constexpr VK_ VkFormat to_image_format(image_format::format_color const& value) noexcept {
		using namespace image_bits_order;
		using namespace image_bits_type;
		constexpr uint16_t n = invalid;
		switch (pack_color_format(value)) {
		case color_format_case(rgba, 8, n, n, n, unorm): return VK_ VK_FORMAT_R8_UNORM;
		case color_format_case(rgba, 8, n, n, n, snorm): return VK_ VK_FORMAT_R8_SNORM;
		case color_format_case(rgba, 8, n, n, n, uint): return VK_ VK_FORMAT_R8_UINT;
		case color_format_case(rgba, 8, n, n, n, sint): return VK_ VK_FORMAT_R8_SINT;
		case color_format_case(rgba, 16, n, n, n, unorm): return VK_ VK_FORMAT_R16_UNORM;
		case color_format_case(rgba, 16, n, n, n, snorm): return VK_ VK_FORMAT_R16_SNORM;
		case color_format_case(rgba, 16, n, n, n, uint): return VK_ VK_FORMAT_R16_UINT;
		case color_format_case(rgba, 16, n, n, n, sint): return VK_ VK_FORMAT_R16_SINT;
		case color_format_case(rgba, 16, n, n, n, sfloat): return VK_ VK_FORMAT_R16_SFLOAT;
		case color_format_case(rgba, 32, n, n, n, uint): return VK_ VK_FORMAT_R32_UINT;
		case color_format_case(rgba, 32, n, n, n, sint): return VK_ VK_FORMAT_R32_SINT;
		case color_format_case(rgba, 32, n, n, n, sfloat): return VK_ VK_FORMAT_R32_SFLOAT;
		case color_format_case(rgba, 8, 8, n, n, unorm): return VK_ VK_FORMAT_R8G8_UNORM;
		case color_format_case(rgba, 8, 8, n, n, snorm): return VK_ VK_FORMAT_R8G8_SNORM;
		case color_format_case(rgba, 8, 8, n, n, uint): return VK_ VK_FORMAT_R8G8_UINT;
		case color_format_case(rgba, 8, 8, n, n, sint): return VK_ VK_FORMAT_R8G8_SINT;
		case color_format_case(rgba, 16, 16, n, n, unorm): return VK_ VK_FORMAT_R16G16_UNORM;
		case color_format_case(rgba, 16, 16, n, n, snorm): return VK_ VK_FORMAT_R16G16_SNORM;
		case color_format_case(rgba, 16, 16, n, n, uint): return VK_ VK_FORMAT_R16G16_UINT;
		case color_format_case(rgba, 16, 16, n, n, sint): return VK_ VK_FORMAT_R16G16_SINT;
		case color_format_case(rgba, 16, 16, n, n, sfloat): return VK_ VK_FORMAT_R16G16_SFLOAT;
		case color_format_case(rgba, 32, 32, n, n, uint): return VK_ VK_FORMAT_R32G32_UINT;
		case color_format_case(rgba, 32, 32, n, n, sint): return VK_ VK_FORMAT_R32G32_SINT;
		case color_format_case(rgba, 32, 32, n, n, sfloat): return VK_ VK_FORMAT_R32G32_SFLOAT;
		case color_format_case(rgba, 8, 8, 8, n, unorm): return VK_ VK_FORMAT_R8G8B8_UNORM;
		case color_format_case(rgba, 8, 8, 8, n, snorm): return VK_ VK_FORMAT_R8G8B8_SNORM;
		case color_format_case(rgba, 8, 8, 8, n, uint): return VK_ VK_FORMAT_R8G8B8_UINT;
		case color_format_case(rgba, 8, 8, 8, n, sint): return VK_ VK_FORMAT_R8G8B8_SINT;
		case color_format_case(rgba, 16, 16, 16, n, unorm): return VK_ VK_FORMAT_R16G16B16_UNORM;
		case color_format_case(rgba, 16, 16, 16, n, snorm): return VK_ VK_FORMAT_R16G16B16_SNORM;
		case color_format_case(rgba, 16, 16, 16, n, uint): return VK_ VK_FORMAT_R16G16B16_UINT;
		case color_format_case(rgba, 16, 16, 16, n, sint): return VK_ VK_FORMAT_R16G16B16_SINT;
		case color_format_case(rgba, 16, 16, 16, n, sfloat): return VK_ VK_FORMAT_R16G16B16_SFLOAT;
		case color_format_case(rgba, 8, 8, 8, 8, unorm): return VK_ VK_FORMAT_R8G8B8A8_UNORM;
		case color_format_case(rgba, 8, 8, 8, 8, snorm): return VK_ VK_FORMAT_R8G8B8A8_SNORM;
		case color_format_case(rgba, 8, 8, 8, 8, uint): return VK_ VK_FORMAT_R8G8B8A8_UINT;
		case color_format_case(rgba, 8, 8, 8, 8, sint): return VK_ VK_FORMAT_R8G8B8A8_SINT;
		case color_format_case(rgba, 16, 16, 16, 16, unorm): return VK_ VK_FORMAT_R16G16B16A16_UNORM;
		case color_format_case(rgba, 16, 16, 16, 16, snorm): return VK_ VK_FORMAT_R16G16B16A16_SNORM;
		case color_format_case(rgba, 16, 16, 16, 16, uint): return VK_ VK_FORMAT_R16G16B16A16_UINT;
		case color_format_case(rgba, 16, 16, 16, 16, sint): return VK_ VK_FORMAT_R16G16B16A16_SINT;
		case color_format_case(rgba, 16, 16, 16, 16, sfloat): return VK_ VK_FORMAT_R16G16B16A16_SFLOAT;
		case color_format_case(rgba, 32, 32, 32, 32, uint): return VK_ VK_FORMAT_R32G32B32A32_UINT;
		case color_format_case(rgba, 32, 32, 32, 32, sint): return VK_ VK_FORMAT_R32G32B32A32_SINT;
		case color_format_case(rgba, 32, 32, 32, 32, sfloat): return VK_ VK_FORMAT_R32G32B32A32_SFLOAT;
		case color_format_case(bgra, 8, 8, 8, 8, unorm): return VK_ VK_FORMAT_B8G8R8A8_UNORM;
		case color_format_case(bgra, 8, 8, 8, 8, snorm): return VK_ VK_FORMAT_B8G8R8A8_SNORM;
		case color_format_case(bgra, 8, 8, 8, 8, uint): return VK_ VK_FORMAT_B8G8R8A8_UINT;
		case color_format_case(bgra, 8, 8, 8, 8, sint): return VK_ VK_FORMAT_B8G8R8A8_SINT;
		default: return VK_ VK_FORMAT_UNDEFINED;
		}
	}

	constexpr VK_ VkFormat to_image_format(image_format::format_depth const& value) noexcept {
		using namespace image_bits_type;
		if (value.dbits == 16u && value.dtype == unorm && value.sbits == invalid)
			return VK_ VK_FORMAT_D16_UNORM;
		if (value.dbits == 24u && value.dtype == unorm && value.sbits == invalid)
			return VK_ VK_FORMAT_X8_D24_UNORM_PACK32;
		if (value.dbits == 32u && value.dtype == sfloat && value.sbits == invalid)
			return VK_ VK_FORMAT_D32_SFLOAT;
		if (value.dbits == invalid && value.sbits == 8u && value.stype == uint)
			return VK_ VK_FORMAT_S8_UINT;
		if (value.dbits == 16u && value.dtype == unorm && value.sbits == 8u && value.stype == uint)
			return VK_ VK_FORMAT_D16_UNORM_S8_UINT;
		if (value.dbits == 24u && value.dtype == unorm && value.sbits == 8u && value.stype == uint)
			return VK_ VK_FORMAT_D24_UNORM_S8_UINT;
		if (value.dbits == 32u && value.dtype == sfloat && value.sbits == 8u && value.stype == uint)
			return VK_ VK_FORMAT_D32_SFLOAT_S8_UINT;
		return VK_ VK_FORMAT_UNDEFINED;
	}

	template<typename Format>
	struct basic_format_express {
		static constexpr void invoke(Format format, auto& base) {
			auto value = to_image_format(format);
			assert(value != VK_ VK_FORMAT_UNDEFINED);
			base.append(value);
		}
	};

	template<>
	struct express<image_format::format_color>
		: basic_format_express<image_format::format_color> {};
	template<>
	struct express<image_format::format_depth>
		: basic_format_express<image_format::format_depth> {};
}
