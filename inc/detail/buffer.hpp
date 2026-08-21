#pragma once

// --- Agents specification -------------------------------------------------
// Buffer views upload pass-declared creation usage through
// `parent_of<buffer>(this)` when bound. The buffer exposes this focused
// operation publicly; access-state and barrier insertion remain separate
// future work.
// --------------------------------------------------------------------------

// Interface style: buffer and buffer-view descriptors compose resource
// ownership, memory binding, mapping, and descriptor-view behavior.
// Implementation: Vulkan operations are selected through compact traits while
// the generic resource layers provide frame and allocator integration.

VKTL_EXPORT_ namespace vktl::vptr {
}

VKTL_EXPORT_ namespace vktl::detail {
	
	template<>
	struct trait<buffer> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_BUFFER;

		using type = buffer;
		using view = buffer_view_;
		using handle_type = VK_ VkBuffer;
		using create_info_type = VK_ VkBufferCreateInfo;
		using create_flags_bits_type = VK_ VkBufferCreateFlagBits;
		using usage_flags_type = VK_ VkBufferUsageFlags;
		using view_type = VK_ VkBufferView;
		using view_create_info_type = VK_ VkBufferViewCreateInfo;

		static constexpr auto create = &VK_ vkCreateBuffer;
		static constexpr auto destroy = &VK_ vkDestroyBuffer;

		static constexpr auto bind_memory = &VK_ vkBindBufferMemory;

#if defined(VK_KHR_bind_memory2)
		using bind_memory_info = VK_ VkBindBufferMemoryInfoKHR;
		bind_memory_info memory_info(void* ptr, handle_type handle, VK_ VkDeviceMemory memory, size_t offset) {
			return {
				.sType = VK_ VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR,
				.pNext = ptr,
				.buffer = handle,
				.memory = memory,
				.memoryOffset = offset,
			};
		}
		static constexpr auto bind_memory_2 = &VK_ vkBindBufferMemory2KHR;
#endif
	};
	// template<>
	// struct trait<allow_buffer_> : trait<buffer> {};
	template<>
	struct trait<VK_ VkBuffer> : trait<buffer> {};

	template<>
	struct trait<buffer_view_> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_BUFFER_VIEW;

		using type = buffer_view_;
		using host = buffer;
		using handle_type = VK_ VkBufferView;
		using view_type = handle_type;
		using create_info_type = VK_ VkBufferViewCreateInfo;
		using create_flags_bits_type = VK_ VkBufferViewCreateFlags;
		using subresource_range = range<VK_ VkDeviceSize>;
		using layout = ::std::nullptr_t;

		static constexpr auto create = &VK_ vkCreateBufferView;
		static constexpr auto destroy = &VK_ vkDestroyBufferView;
	};
	template<>
	struct trait<VK_ VkBufferView> : trait<buffer_view_> {};


	template<typename N>
	struct m<buffer, N> : basic_resource<N, trait<buffer>> {
		using base = basic_resource<N, trait<buffer>>;

		constexpr m(buffer buffer, auto&&...others)
			: base{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.size = buffer.size;
		}

		~m() { reset(); }

		void init() {
			N::init();
			auto _ = locker_of(this);
			if (base::is_null()) {
				this->generate(info, "[BUFFER] Create buffer failure.");
			}
		}

		void reset() {
			auto _ = locker_of(this);
			if (!base::is_null()) {
				this->destroy();
			}
		}

		void append_usage(VK_ VkBufferUsageFlags usages) noexcept {
			auto _ = locker_of(this);
			assert(base::is_null());
			info.usage |= usages;
		}

		VK_ VkMemoryBarrier append_access(default_buffer_access const& access) {
			// access_.insert(access);
		}

	protected:
		VK_ VkBufferCreateInfo info {
			.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		};

	private:
		access_list<default_buffer_access> access_;
	};


	template<typename N>
	struct m<buffer_view_, N> : N {
		constexpr m(buffer_view_ buffer_view, auto&&...others)
			: N{ forward_(others)... }
			, info{
				.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
				.offset = 0u,
				.range = maximum,
			}
		{
		}

		~m() { reset(); }

		void init() {
			N::init();

			auto _ = locker_of(this);
			// if (handle_) {
			// 	// this->generate(info, "[BUFFER_VIEW] Create buffer failure.");
			// 	VK_ vkCreateBufferView(handle_of<device>(this), &info, N::allocator(), &handle_)
			// 		| popup{ "[BUFFER_VIEW] Create buffer view failure." };
			// }
		}

		void reset() {
			auto _ = locker_of(this);
			// if (handle_) {
			// 	for (auto child : childs_) {
			// 		child.reset();
			// 	}
			// 
			// 	::std::lock_guard _{ N::get_lock() };
			// 	VK_ vkDestroyBufferView(handle_of<device>(this),
			// 		exchange(handle_, VK_NULL_HANDLE), N::allocator());
			// }
		}

		void upload_access() {
			// parent_of<buffer>(this)->
		}

		// void bind(bind_memory bind) {
		// 
		// }

		uint32_t frame_count() const noexcept {
			return parent_of<buffer>(this)->frame_count();
		}
		auto handle(uint32_t frame) const noexcept {
			auto value = parent_of<buffer>(this)->handle(frame);
			return value.value;
		}
		auto handle() const noexcept { return handle(parent_of<buffer>(this)->frame_index()); }
		auto view(uint32_t) const noexcept { return handle_.value; }
		auto view() const noexcept { return view(0u); }
		auto layout() const noexcept { return nullptr; }
		auto subresource_range() const noexcept {
			return range<VK_ VkDeviceSize>{ info.offset, info.range };
		}

		void upload_usage(VK_ VkBufferUsageFlags usages) {
			parent_of<buffer>(this)->append_usage(usages);
		}

		void append(VK_ VkFormat format) noexcept {
			assert(!handle_);
			assert(format != VK_ VK_FORMAT_UNDEFINED);
			info.format = format;
		}

	protected:
		VK_ VkBufferViewCreateInfo info;

	private:
		reset_if_copy<VK_ VkBufferView> handle_;
		descriptor_handle descriptor_;
	};


}
