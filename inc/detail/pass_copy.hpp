#pragma once

// --- Agents specification -------------------------------------------------
// This file is a design copy for the pass/render-pass dependency model. It is
// intentionally not included by the library while the design is reviewed.
//
// `m<pass_, N>` only owns the ordered resource-usage declarations. It neither
// detects nor stores render-pass dependencies. Once finalized, no more usages
// may be appended.
//
// `m<render_pass_, N>` derives only the dependencies required by a classic
// Vulkan render pass. It compares every ordered pair of usages with the same
// resource index. Dependencies are kept sorted by source subpass and then by
// destination subpass, and matching entries are extended when discovered.
// There is no final merge pass.
//
// `finalize()` never calls `relocate()`. Relocation remains the responsibility
// of `object` or an explicit caller-selected relocation point.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail::pass_copy {
	struct resource_usage {
		uint16_t index = invalid;
		uint16_t reserved = 0u;
		uint32_t subpass = VK_ VK_SUBPASS_EXTERNAL;
		VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
		VK_ VkPipelineStageFlags stages = VK_ VkPipelineStageFlags(0u);
		VK_ VkAccessFlags access = VK_ VkAccessFlags(0u);
		VK_ VkDependencyFlags dependency = VK_ VkDependencyFlags(0u);
		VK_ VkImageLayout layout = VK_ VK_IMAGE_LAYOUT_UNDEFINED;
	};

	inline constexpr bool writes(VK_ VkAccessFlags access) noexcept {
		constexpr auto write_access = VK_ VK_ACCESS_SHADER_WRITE_BIT
			| VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
			| VK_ VK_ACCESS_TRANSFER_WRITE_BIT
			| VK_ VK_ACCESS_HOST_WRITE_BIT
			| VK_ VK_ACCESS_MEMORY_WRITE_BIT;
		return (access & write_access) != 0u;
	}

	// A pass only records declarations. Dependency compilation belongs to the
	// classic render-pass component because VkSubpassDependency is required only
	// while constructing a VkRenderPass.
	template<typename N>
	struct pass_component : N {
		constexpr pass_component(pass_, auto&&... args)
			: N{ forward_(args)... } {}

		cspan<resource_usage> usages() const noexcept {
			return usages_;
		}

		void finalize() {
			N::finalize();
			assert(!finalized_);
			finalized_ = true;
		}

	protected:
		friend constexpr void append(
			pass_component& self,
			resource_usage const& usage) {
			assert(!self.finalized_);
			assert(usage.index != invalid);
			self.usages_.emplace_back(usage);
		}

	private:
		vector<resource_usage> usages_;
		bool finalized_ = false;
	};

	// This is the dependency-focused part of m<render_pass_, N>. Attachment and
	// subpass create-info collection can stay in their existing focused helpers.
	template<typename N>
	struct render_pass_component : N {
		constexpr render_pass_component(render_pass_, auto&&... args)
			: N{ forward_(args)... }
			, info{ .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO } {}

		void finalize() {
			N::finalize();
			collect_dependencies();

			// Deliberately no relocate(). Object construction/copy/move or the user
			// chooses when all pointer-bearing create infos are rebound.
		}

		void relocate() noexcept {
			N::relocate();
			info.dependencyCount = uint32_t(dependencies_.size());
			info.pDependencies = dependencies_.empty()
				? nullptr : dependencies_.data();
		}

		auto init() {
			N::init();
			if (handle_) return;

			// Dependencies are already ordered and combined as they were added.
			// object::init() relocates immediately before entering this function.
			VK_ vkCreateRenderPass(handle_of<device>(this), &info,
				N::allocator(), &handle_)
				| popup{ "[PASS] Create render pass failure." };
		}

		VK_ VkRenderPass pass() const noexcept {
			return handle_.value;
		}

	private:
		void collect_dependencies() {
			dependencies_.clear();
			auto const values = this->usages();

			// Declaration order defines the source/destination order. Compare every
			// earlier usage, not merely the most recent usage of a resource.
			for (size_t dst_index = 0u; dst_index < values.size(); ++dst_index) {
				auto const& dst = values[dst_index];
				for (size_t src_index = 0u; src_index < dst_index; ++src_index) {
					auto const& src = values[src_index];
					if (src.index != dst.index) continue;
					if (!requires_dependency(src, dst)) continue;

					append_dependency(make_dependency(src, dst));
				}
			}
		}

		static bool requires_dependency(
			resource_usage const& src,
			resource_usage const& dst) noexcept {
			if (writes(src.access) || writes(dst.access)) return true;
			if ((src.dependency | dst.dependency) != 0u) return true;

			return src.resource_type == VK_ VK_OBJECT_TYPE_IMAGE
				&& src.layout != dst.layout;
		}

		static VK_ VkSubpassDependency make_dependency(
			resource_usage const& src,
			resource_usage const& dst) noexcept {
			assert(src.subpass == VK_ VK_SUBPASS_EXTERNAL || src.subpass < 32u);
			assert(dst.subpass == VK_ VK_SUBPASS_EXTERNAL || dst.subpass < 32u);

			return VK_ VkSubpassDependency{
				.srcSubpass = src.subpass,
				.dstSubpass = dst.subpass,
				.srcStageMask = src.stages,
				.dstStageMask = dst.stages,
				.srcAccessMask = source_access(src, dst),
				.dstAccessMask = destination_access(src, dst),
				.dependencyFlags = src.dependency | dst.dependency,
			};
		}

		static VK_ VkAccessFlags source_access(
			resource_usage const& src,
			resource_usage const& dst) noexcept {
			// A read-after-write or write-after-write dependency makes the source
			// write available. Read-before-write only requires execution ordering.
			return writes(src.access) ? src.access : VK_ VkAccessFlags(0u);
		}

		static VK_ VkAccessFlags destination_access(
			resource_usage const& src,
			resource_usage const& dst) noexcept {
			return writes(src.access) ? dst.access : VK_ VkAccessFlags(0u);
		}

		void append_dependency(VK_ VkSubpassDependency dependency) {
			// dependencies_ remains ordered lexicographically by
			// (srcSubpass, dstSubpass). The first search locates the source range;
			// the second search locates the destination inside that range.
			auto src_begin = ::std::ranges::lower_bound(
				dependencies_, dependency.srcSubpass, {},
				&VK_ VkSubpassDependency::srcSubpass);
			auto src_end = ::std::ranges::upper_bound(
				src_begin, dependencies_.end(), dependency.srcSubpass, {},
				&VK_ VkSubpassDependency::srcSubpass);
			auto found = ::std::ranges::lower_bound(
				src_begin, src_end, dependency.dstSubpass, {},
				&VK_ VkSubpassDependency::dstSubpass);

			if (found == src_end || found->dstSubpass != dependency.dstSubpass) {
				dependencies_.insert(found, dependency);
				return;
			}

			// The matching dependency is extended immediately. No later merge pass
			// is needed because there is at most one entry per subpass pair.
			found->srcStageMask |= dependency.srcStageMask;
			found->dstStageMask |= dependency.dstStageMask;
			found->srcAccessMask |= dependency.srcAccessMask;
			found->dstAccessMask |= dependency.dstAccessMask;
			found->dependencyFlags |= dependency.dependencyFlags;
		}

	private:
		VK_ VkRenderPassCreateInfo info{
			.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		};
		vector<VK_ VkSubpassDependency> dependencies_;
		reset_if_copy<VK_ VkRenderPass> handle_{ VK_NULL_HANDLE };
	};
}
