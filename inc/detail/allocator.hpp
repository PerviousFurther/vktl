#pragma once

VKTL_EXPORT_ namespace vktl {
	struct memory_handle_tag : detail::poly_list::node {
		using base = detail::poly_list::node;
		template<typename Self, typename T>
		constexpr memory_handle_tag(::std::in_place_type_t<Self> self, T& parent)
			: base{ self } {
		}

		VK_ VkDeviceMemory handle;
		VK_ VkMemoryPropertyFlags property;
	};
}

VKTL_EXPORT_ namespace vktl::vptr {
	template<typename E>
	struct bindable {
		using element_type = E;

		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

			using element_type = E;

			void bind(element_type* element) {
				vptr_.bind_(C::get_this(), element);
			}

			template<typename T>
			void rebind() noexcept {
				C::template rebind<T>();
				vptr_.bind_ = [](void* ptr, element_type* element) {
					static_cast<T*>(ptr)->bind(element);
					};
			}

			template<typename O>
			void rebind(apply<O> const& other) noexcept {
				C::rebind(other);
				vptr_ = other.vptr_;
			}

		private:
			bindable vptr_;
		};

		void(*bind_)(void*, element_type*) = nullptr;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	struct default_memory : memory_handle_tag {
		using base = memory_handle_tag;

		template<typename Self, typename T>
		constexpr default_memory(::std::in_place_type_t<Self> self, T& parent)
			: base{ self, parent } 
		{}

		uint32_t type_id = invalid;
	};

	// tag constraint for resource.

	struct resource; 

	template<typename N>
	struct m<memory_allocator_, N> : N {
		constexpr m(memory_allocator_, auto&&...others)
			: N{forward_(others)...}
		{}

		~m() { reset(); }

		template<object_of<resource> T>
		void append_child(T& value) {
			::std::lock_guard _{N::get_lock()};
			childs_.emplace_back(value);
		}

		void reset() {
			for (auto& child : buffer_childs_) { child.reset(); }
			for (auto& child : binded_buffer_childs_) { child.reset(); }
			::std::lock_guard _{N::get_lock()};
			buffer_childs_.clear();
			buffer_binded_childs_.clear();
			image_childs_.clear();
			image_binded_childs_.clear();
			for (default_memory& memory : memories) {
				VK_ vkFreeMemory(handle_of<device>(this), memory.handle, N::allocator());
			}
			memories.clear();
		}

	protected:
		// first is invoke, ns is pNext setter.
		template<typename Fn = call_duck_, typename Ns = call_duck_>
		auto init(Fn&& fn = call_duck, Ns&& ns) {
			::std::lock_guard _{N::get_lock()};
			for (auto& child : buffer_childs_) {
				locked<VK_ VkBuffer> handle = child.handle();
			#if defined(VK_KHR_get_memory_requirements2)
				get_requirement(
					fn, &VK_ vkGetBufferMemoryRequirements2KHR, &VK_ vkGetBufferMemoryRequirements, handle.value,
					VK_ VkBufferMemoryRequirementsInfo2KHR{
						.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR,
						.buffer = handle.value
					});
			#else
				get_requirement(fn, &VK_ vkGetBufferMemoryRequirements, handle);
			#endif
			}
			::std::move(buffer_childs_.begin(), buffer_childs_.end(), buffer_binded_childs_.end());
			buffer_childs_.clear();

			for (auto& child : image_childs_) {
				locked<VK_ VkImage> handle = child.handle();
			#if defined(VK_KHR_get_memory_requirements2)
				get_requirement(
					fn, &VK_ vkGetImageMemoryRequirements2KHR, &VK_ vkGetImageMemoryRequirements, handle.value,
					VK_ VkImageMemoryRequirementsInfo2KHR{
						.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
						.image = handle.value
					});
			#else
				get_requirement(fn, &VK_ vkGetImageMemoryRequirements, handle);
			#endif
			}
			::std::move(image_childs_.begin(), image_childs_.end(), image_binded_childs_.end());
			image_childs_.clear();
		}

	protected:
		template<typename T>
		using resource = type_box<
			bind_back<vptr::unbind_notifier, memory_allocator_>, 
			bind_back<vptr::handle_owner, locked<T>>>;

	private:
	#if defined(VK_KHR_get_memory_requirements2)
		template<typename F, typename Info, typename MemReq>
		static void get_requirement(F& fn,
			auto getter2, auto getter1, auto handle, Info info, MemReq memory_req) {
			VK_ VkDevice hdv = handle_of<device>(this);
			VK_ VkMemoryRequirements2KHR memory_req{
				.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR
			};
			bool need = false;
			if constexpr (::std::invocable<F, Info&, MemReq&>) {
				fn(info, memory_req);
				need = true;
			}
			if (need && (info.pNext || memory_req.pNext)) {
				getter2(hdv, &info, &memory_req);
			}
			else {
				getter1(hdv, handle, &memory_req.memoryRequirements);
			}
			fn(handle, memory_req);
		}
	#else
		static void get_requirement(auto& fn, auto getter1, auto handle){
			VK_ VkDevice hdv = handle_of<device>(this);
			VK_ VkMemoryRequirements memory_req{};
			getter1(hdv, handle, &memory_req);
			fn(handle, memory_req);
		}
	#endif

	private:
		vector<resource<VK_ VkBuffer>> buffer_childs_, buffer_binded_childs_;
		vector<resource<VK_ VkImage>> image_childs_, image_binded_childs_;
		poly_list memories;
	};

	using namespace memory_allocator_extensions;

	template<typename N>
	struct m<budget_, N> : N {
		constexpr m(budget_, auto&&...others)
			: N{forward_(others)...} {
			parent_of<instance>(this)
				.append_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		}

	protected: 
	
	};
	
	template<typename N>
	struct m<buddy_, N> : N {

	};
}

VKTL_EXPORT_ namespace vktl::detail {

}