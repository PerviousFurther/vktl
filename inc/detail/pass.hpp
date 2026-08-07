#pragma once

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct c<pass_, N> : N {
		constexpr c(pass_, auto&&...other)
			: N{ forward_(other)... }
		{
		}

	protected:
		umap<uint16_t, access_list<default_buffer_access>> buffers;
		umap<uint16_t, access_list<default_image_access>> images;
	};

	using namespace pass_extensions;

	template<>
	struct is_queryable<render_pass_, graphics_> : ::std::true_type {};

	template<typename N>
	struct basic_graphics_pass : N {
		using base = N;

		constexpr basic_graphics_pass(auto&&...infos)
			: N{ forward_(infos)... }
		{
		}

	protected:
		uint16_t append(VK_ VkGraphicsPipelineCreateInfo const& info) {
			pipelines_.emplace_back(info, vector<VK_ VkPipelineShaderStageCreateInfo>{});
			return uint16_t(pipelines_.size() - 1u);
		}

		uint16_t append(::std::uint16_t pipe, VK_ VkPipelineShaderStageCreateInfo const& info) {
			auto& shaders = pipelines_.column<1u>()[pipe];
			shaders.emplace_back(info);
			return uint16_t(shaders.size() - 1u);
		}

		auto& pipe_info(uint16_t index) noexcept { return pipelines_.column<0>()[index]; }
		auto& shader_stage_info(uint16_t pipe, uint16_t index) noexcept { return pipelines_.column<1>()[index]; }

		auto& pipe(uint16_t index) noexcept {}


	private:
		vectors<VK_ VkGraphicsPipelineCreateInfo,
			vector<VK_ VkPipelineShaderStageCreateInfo>> pipelines_;
		vector<VK_ VkPipeline> pipes_;
	};

	template<typename N>
	struct c<render_pass_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;

		c(render_pass_, auto&&...infos)
			: base{ forward_(infos)... }
		{

		}

		~c() { reset(); }

		void init() {
			if (!handle_) {
				VK_ vkDestroyRenderPass(handle_from<N, device>(), handle_, N::allocator());
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroyRenderPass(handle_from<N, device>(), handle_, N::allocator());
			}
		}

	protected:
		void append(VK_ VkAttachmentDescription const& attachment) {

		}

		void append(uint16_t subpass, VK_ VkSubpassDescription const& description) {

		}

	protected:
		VK_ VkRenderPassCreateInfo info;

	private:
		vector<VK_ VkAttachmentDescription> attachments_;
		vectors<VK_ VkSubpassDescription,
			array<vector<VK_ VkAttachmentReference>, 4>> subpasses_;
		vector<VK_ VkSubpassDependency> dependencies_;

		copyable_if_null<VK_ VkRenderPass> handle_{ VK_NULL_HANDLE };
	};

	template<typename N>
	struct c<graphics_, N> : basic_graphics_pass<N> {
		using base = basic_graphics_pass<N>;
		constexpr c(graphics_, auto&&...infos)
			: base{ forward_(infos)... }
		{
		}

	};

	template<typename N>
	struct c<compute_, N> : N {
		using base = N;
		c(compute_, auto&&...infos)
			: base{ forward_(infos)... }
		{
		}

	protected:
		uint16_t append(VK_ VkComputePipelineCreateInfo const& info) {
			pipelines_.emplace_back(info);
			return uint16_t(pipelines_.size() - 1u);
		}

	private:
		vector<VK_ VkComputePipelineCreateInfo> pipelines_;

		vector<VK_ VkPipeline> pipes_;
	};
}