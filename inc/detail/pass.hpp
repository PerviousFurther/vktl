#pragma once

// --- Agents specification -------------------------------------------------
// Use `express<Tag>` for resource and shader declarations that are collected by
// the pass and do not need a position in the inheritance chain. Keep backend
// metadata in focused `trait<Tag>` specializations and share invocation logic
// through a `basic_xxx_express<Trait>` helper.
// A pass stores every resource kind in one declaration sequence. Public usage
// indices are possibly sparse positions in the matching bind-set resource
// layer, not indices in that declaration sequence; resource traits identify
// kinds without a central variant. All pipelines in a pass share one layout.
// Normalize its descriptor bindings into unique layouts, keep Vulkan set-number
// mapping in `set_layout_indices`, and materialize empty layouts for set-number
// holes.
// Descriptor usages take their shader stages from the latest shader in the
// current pipeline. A new pipeline clears that state; usage without a prior
// shader is an error, and repeated bindings union stages across pipelines.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	struct default_resource_usage {
		uint16_t index = invalid;
		uint16_t reserved = 0u;
		VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
		VK_ VkDescriptorType type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		VK_ VkPipelineStageFlags stages = 0u;
		VK_ VkAccessFlags access = 0u;
		VK_ VkDependencyFlags dependency = 0u;

		uint32_t set = invalid;
		uint32_t binding = invalid;
		uint64_t offset = maximum;
		uint64_t usages = 0u; // since usage2 is 64bits.

		constexpr bool uses_descriptor() const noexcept {
			return type != VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	};

	using namespace pass_extensions;

	template<>
	struct is_queryable<render_pass_, graphics_> : ::std::true_type {};

	template<typename N>
	struct m<pass_, N> : N {
		constexpr m(pass_, auto&&... other)
			: N{ forward_(other)... }
		{
		}

	protected:
		void append(default_resource_usage const& usage) {
			usages_.emplace_back(usage);
		}

		auto& last_usage() noexcept { 
			assert(usages_.size());
			return usages_.back(); 
		}

	private:
		vector<default_resource_usage> usages_;
	};


	template<typename N>
	struct basic_pipe_pass : N {
		constexpr basic_pipe_pass(auto&&... args)
			: N{ forward_(args)... }
		{
		}

		void fill(auto& bind_set) {
			bind_set.accept(schema_);
			for (auto const& usage : usages_) {
				bind_set.bind(usage);
			}
		}

	protected:
		void init() {
			N::init();
			if (!pipeline_layout_) {
				auto pdv = parent_of<vktl::device>(this);
				auto hdv = pdv->handle();

				VK_ VkPipelineCacheCreateInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
				};
				VK_ vkCreatePipelineCache(hdv, &info, N::allocator(), &cache_)
					| popup{ "[PASS] Create pipeline cache failure." };

				auto empty_handle = pdv->create(default_descriptor_set_layout{});
				vector<VK_ VkDescriptorSetLayout> unique_layouts;
				unique_layouts.reserve(schema_.size());
				for (default_descriptor_set_layout const& layout : schema_) {
					unique_layouts.emplace_back(pdv->create(layout));
				}

				default_pipeline_layout info;
				info.layouts = ::std::move(unique_layouts);
				pipeline_layout_ = pdv->create(::std::move(info));
			}
		}

		auto pipeline_layout() {
			return pipeline_layout_.value;
		}

		void finalize() {
			default_bind_set_schema result;
			auto& layouts_by_set = result.layout_infos;
			for (auto const& usage : usages_) {
				if (!usage.uses_descriptor()) continue;
				assert(usage.set != invalid && usage.binding != invalid);
				if (layouts_by_set.size() <= usage.set) {
					layouts_by_set.resize(size_t(usage.set) + 1u);
					used_sets.resize(size_t(usage.set) + 1u, false);
				}
				layouts_by_set[usage.set].add(VK_ VkDescriptorSetLayoutBinding{
					.binding = usage.binding,
					.descriptorType = usage.type,
					.descriptorCount = 1u,
					.stageFlags = usage.stages,
				});
			}

			auto& set_layout_indices = result.set_layout_indices;
			set_layout_indices.resize(layouts_by_set.size(), uint16_t(invalid));
			for (auto set = 0u; set < layouts_by_set.size(); ++set) {
				auto const& layout = layouts_by_set[set];
				auto it = ::std::ranges::find(result.layout_infos, layout);
				if (it == result.layout_infos.end()) {
					it = result.layout_infos.insert(it, layout);
				}
				set_layout_indices[set] = uint16_t(::std::distance(result.layout_infos.begin(), it));
			}

			schema_ = ::std::move(result);
		}

	private:
		vector<default_descriptor_set_layout> schema_;
		default_pipeline_layout pipe_;

		copyable_if_null<VK_ VkPipelineCache> cache_{ VK_NULL_HANDLE };
		copyable_if_null<VK_ VkPipelineLayout> pipeline_layout_{ VK_NULL_HANDLE };
	};

	template<typename N>
	struct basic_graphics_pass : basic_pipe_pass<N> {
		using base = N;

		constexpr basic_graphics_pass(auto&&...infos)
			: N{ forward_(infos)... } {
		}

		~basic_graphics_pass() { reset(); }

		void relocate() {
			N::relocate();
			for (auto& [pipeline, shaders, handles] : pipelines_) {
				pipeline.stageCount = uint32_t(shaders.size());
				pipeline.pStages = shaders.data();
			}
		}

		void init() {
			N::init();
			if (this->modules.size()) {
				VK_ VkDevice hdv = handle_of<device>(this);
				vector<VK_ VkPipelineShaderStageCreateInfo>& stages = pipelines_.column<1u>();
				vector<VK_ VkShaderModuleCreateInfo>& module_infos = pipelines_.column<2u>();

				auto itp = stages.begin();
				auto iti = module_infos.begin();
				try {
					for (; iti != module_infos.end(); ++iti, ++itp) {
						VK_ vkCreateShaderModule(hdv, &(*iti), N::allocator(), &itp->module)
							| popup{ "[PASS] Failed to create shader module." };
						
					}
				}
				catch (...) {
					reset_shader_module();
					throw;
				}
			}
		}

		void reset() {
			reset_shader_module();
			if (this->pipes.size()) {
				for (auto pipe : pipes) { 
					
				}
			}
		}

		auto pipe(uint16_t index) noexcept { 
			return pipes_[index];
		}

		uint16_t pipe_count() const noexcept { 
			return uint16_t(pipeslines_.size()); 
		}

	protected:
		void reset_shader_module() {
			for (auto col : pipelines.column<2u>()) {
				if (*it != VK_NULL_HANDLE) {
					VK_ vkDestroyShaderModule(hdv, *it, N::allocator());
				}
				else {
					break;
				}
			}
		}

		// these function trigger on childs' 
		uint16_t append(VK_ VkGraphicsPipelineCreateInfo const& info) {
			pipelines_.emplace_back(info,
				vector<VK_ VkPipelineShaderStageCreateInfo>{},
				vector<VK_ VkShaderModuleCreateInfo>{});
			return uint16_t(pipelines_.size() - 1u);
		}

		uint16_t append(VK_ VkPipelineShaderStageCreateInfo const& info) {
			assert(pipelines_.column<1u>().size());
			auto& shaders = pipelines_.column<1u>().back();
			shaders.emplace_back(info);
			pipelines_.column<2u>().back().emplace_back(nullptr);
			return uint16_t(shaders.size() - 1u);
		}

		uint16_t append_shader(VK_ VkPipelineShaderStageCreateInfo const& info, VK_ VkShaderModuleCreateInfo const& shader) {
			assert(shader);
			assert(!pipelines_.empty());

			auto& shaders = pipelines_.column<1u>().back();
			auto& handles = pipelines_.column<2u>().back();
			shaders.emplace_back(info);
			handles.emplace_back(VK_ VkShaderModuleCreateInfo {
				.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = shader->compiled.size(),
				.pCode = shader->compiled.data(),
			});
			return uint16_t(shaders.size() - 1u);
		}

		auto& last_pipe() noexcept {
			assert(!pipelines_.empty());
			return pipelines_.column<0u>().back();
		}

		auto& last_shader_stage() noexcept {
			assert(!pipelines_.empty()); // not allow non pipe inside.
			assert(!pipelines_.column<1u>().back()); // not allow non shader inside.
			return pipelines_.column<1u>().back().back();
		}

		auto& last_shader_module() noexcept {
			assert(!pipelines_.empty()); // not allow non pipe inside.
			assert(!pipelines_.column<2u>().back()); // not allow non shader inside.
			return pipelines_.column<2u>().back().back();
		}

		
		auto pipe_infos() noexcept { return pipelines_.column<0>(); }
		auto shader_stage_infos() noexcept { return pipelines_.column<1>(); }
		auto shader_module_infos() noexcept { return pipelines_.column<2>(); }

	protected:
		vectors<VK_ VkGraphicsPipelineCreateInfo,
			vector<VK_ VkPipelineShaderStageCreateInfo>,
			vector<VK_ VkShaderModuleCreateInfo>> pipelines;

		vector<VK_ VkPipeline> pipes;

	private:
		void allocate(VK_ VkShaderModuleCreateInfo& info) {
			uint32_t* stages;
			if constexpr (object_of<N, allocate_from>) {
				stages = new uint32_t[info.codeSize / sizeof(uint32_t)];
			}
			else {
				stages = N::allocator_impl().allocate(info.codeSize, sizeof(uint32_t));
			}
			::std::memcpy(stages, info.pCode, info.codeSize);
			info.pCode = stages;
		}
		void free(VK_ VkShaderModuleCreateInfo& info) {
			if constexpr (object_of<N, allocate_from>) {
				delete[] info.pCode;
			}
			else {
				N::allocator_impl().free(info.pCode);
			}
		}
	};

	template<typename N>
	struct m<render_pass_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;
		using base::append;

		m(render_pass_, auto&&...infos)
			: base{ forward_(infos)... }
		{}

		~m() { reset(); }

		void init() {
			base::init();
			if (!handle_) {
				auto hdv = handle_of<device>(this);
				try {
					VK_ vkCreateRenderPass(hdv, &info, N::allocator(), &handle_)
						| popup{ "[PASS] Create pass failure." };
					VK_ vkCreateGraphicsPipelines(hdv, base::cache_.value, N::allocator(), pipes_.data())
						| popup{ "[PASS] Create pipeline failure." };
				}
				catch (...) {
					reset();
					throw;
				}
			}
		}

		// TODO: rewrite append(resource_usage const&) to add subpass dependency.


		void reset() {
			if (handle_) {
				for ()
				VK_ vkDestroyPipeline();


				VK_ vkDestroyRenderPass(handle_of<device>(this), handle_, N::allocator());
			}
		}

		void fill(object_of<bind_set_> auto& bind_set) {
			N::fill(bind_set);
		}

	protected:
		void append(VK_ VkAttachmentDescription const& attachment) {

		}

		void append(VK_ VkSubpassDescription const& description) {
			// subpasses_
		}

		uint16_t last_subpass() {
			return uint16_t(subpasses_.size());
		}

		// void append() {
		// 
		// }

	protected:
		VK_ VkRenderPassCreateInfo info;

	private:
		vector<VK_ VkAttachmentDescription> attachments_;
		vectors<VK_ VkSubpassDescription,
			array<vector<VK_ VkAttachmentReference>, 4>> subpasses_;
		vector<VK_ VkSubpassDependency> dependencies_;

		copyable_if_null<VK_ VkRenderPass> handle_{ VK_NULL_HANDLE };
	};

#if defined(VK_KHR_dynamic_rendering)
	template<typename N>
	struct m<graphics_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;
		constexpr m(graphics_, auto&&...infos)
			: base{ forward_(infos)... }
		{
		}

		void init

	private:
		VK_ VkPipelineRenderingCreateInfo rendering_;
	};
#else
	template<typename N>
	struct m<graphics_, N> : N {
		constexpr m(graphics_, auto&&...infos)
			: base{ forward_(infos)... }
		{}

		static_assert(always_false<N>, 
			"Header not support dynamic rendering but try using graphics to extend pipe.");
	};
#endif

	template<typename N>
	struct m<compute_, N> : N {
		using base = N;
		using N::append;
		m(compute_, auto&&...infos)
			: base{ forward_(infos)... }
		{
		}

	public:
		uint16_t append(VK_ VkComputePipelineCreateInfo const& info) {
			N::reset_shader_stage();
			pipelines_.emplace_back(info);
			shaders_.emplace_back(nullptr);
			return uint16_t(pipelines_.size() - 1u);
		}

		void append_shader(shader_handle shader,
			VK_ VkPipelineShaderStageCreateInfo const& info) {
			assert(shader);
			assert(!pipelines_.empty());
			assert(!shaders_.back());
			pipelines_.back().stage = info;
			shaders_.back() = shader;
			N::current_shader_stage(info.stage);
		}

		void customize_shader_entry_point(const char* name) {
			assert(name);
			assert(!pipelines_.empty());
			assert(shaders_.back());
			pipelines_.back().stage.pName = name;
		}

		void relocate() {
			N::relocate();
		}

		void init() {
			N::init();
			auto layout = N::pipeline_layout();
			for (auto& pipeline : pipelines_) pipeline.layout = layout;
		}

	private:
		vector<VK_ VkComputePipelineCreateInfo> pipelines_;
		vector<shader_handle> shaders_;

		vector<VK_ VkPipeline> pipes_;
	};

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

	template<>
	struct express<pipe_> {
		template<typename B>
		static constexpr void invoke(pipe_, B& base) {
			if constexpr (object_of<B, graphics_>) {
				base.append(VK_ VkGraphicsPipelineCreateInfo{
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
			else if constexpr (object_of<B, compute_>) {
				base.append(VK_ VkComputePipelineCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
					.stage = { .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
				});
			}
			else {
				static_assert(always_false<decltype(base)>, "Not allow apppend pipe in not graphics or compute pass.");
			}
		}
	};

	using namespace pipe_extensions;

	template<>
	struct express<subpass> {
		template<typename B>
		static constexpr void invoke(subpass subpass, B& base) requires(object_of<B, graphics_>) {
			base.append_subpass(subpass.index);
		}
	};


		template<>
	struct trait<vertex_shader> {
		using type = vertex_shader;
		static constexpr VK_ VkShaderStageFlagBits stage =
			VK_ VK_SHADER_STAGE_VERTEX_BIT;

		static constexpr shader_handle handle(type shader) noexcept {
			return shader.bytes;
		}
	};

	template<>
	struct trait<fragment_shader> {
		using type = fragment_shader;
		static constexpr VK_ VkShaderStageFlagBits stage =
			VK_ VK_SHADER_STAGE_FRAGMENT_BIT;

		static constexpr shader_handle handle(type shader) noexcept {
			return shader.bytes;
		}
	};

	template<>
	struct trait<compute_shader> {
		using type = compute_shader;
		static constexpr VK_ VkShaderStageFlagBits stage =
			VK_ VK_SHADER_STAGE_COMPUTE_BIT;

		static constexpr shader_handle handle(type shader) noexcept {
			return shader.code;
		}
	};

	template<typename Trait>
	struct basic_shader_express {
		using shader_type = typename Trait::type;

		static void invoke(shader_type shader, auto& base) {
			base.append_shader(Trait::handle(shader),
				VK_ VkPipelineShaderStageCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = Trait::stage,
					.module = VK_NULL_HANDLE,
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


		template<>
	struct trait<uniform_buffer> {
		using type = uniform_buffer;
		using resource_type = buffer;
		static constexpr VK_ VkObjectType resource_object_type =
			VK_ VK_OBJECT_TYPE_BUFFER;
		using usage_type = VK_ VkBufferUsageFlags;
		static constexpr usage_type usage(uniform_buffer) {
			return VK_ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		static constexpr bool uses_descriptor = true;
		static constexpr VK_ VkDescriptorType descriptor_type =
			VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	};

	template<>
	struct trait<attachment> {
		using type = attachment;
		using resource_type = image;
		static constexpr VK_ VkObjectType resource_object_type =
			VK_ VK_OBJECT_TYPE_IMAGE;

		using usage_type = VK_ VkImageUsageFlags;
		static constexpr usage_type usage(attachment const&) {
			return VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		static constexpr bool uses_descriptor = false;
		static constexpr VK_ VkDescriptorType descriptor_type =
			VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
	};

	template<typename Trait>
	struct basic_resource_usage_express {
		using resource_usage_type = typename Trait::type;
		using resource_type = typename Trait::resource_type;

		static constexpr void invoke(resource_usage_type usage, auto& base) {
			default_resource_usage append{
				.index = uint16_t(usage.index),
				.resource_type = Trait::resource_object_type,
				.type = Trait::descriptor_type,
				.stages = Trait::uses_descriptor
					? VK_ VkShaderStageFlags(base.last_shader_stage()) : 0u,
				.usages = uint32_t(Trait::usage(usage)),
			};
			base.append(append);
		}
	};

	template<>
	struct express<uniform_buffer> : basic_resource_usage_express<trait<uniform_buffer>> {};
	template<>
	struct express<attachment> : basic_resource_usage_express<trait<attachment>> {};

	using namespace resource_usage_extensions;

	template<>
	struct express<bind_on_set> {
		static void invoke(bind_on_set set, auto& object) {
			object.append_binding(set.set, set.binding);
		}
	};


	template<>
	struct express<bind_on_heap> {
		static void invoke(bind_on_heap bind, auto& object) {
			object.append_binding(bind.offset);
		}
	};

}
