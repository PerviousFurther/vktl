#pragma once

namespace vktl::detail {
	struct bind_descriptor_set {
		void* parent; // maybe remove.
		uint32_t set;
		uint32_t binding;
		uint32_t count;
	};

	template<>
	struct trait<descriptor_set_> {
		using handle_type = VK_ VkDescriptorSet;
		// using create_info_type = VK_ VkDescriptorSetCreateINf
	};

}

namespace vktl::vptr {
	template<typename Trait>
	struct view_need_descriptor {
		using handle_type = typename Trait::handle_type;
		using view_type = typename Trait::view_type;
		using bind_type = detail::bind_descriptor_set;
		using descriptor_type = VK_ VkDescriptorType;
		using layout_type = typename Trait::layout;
		using subresource_range = typename Trait::subresource_range;
		using host_trait = detail::trait<typename Trait::host>;

		template<typename C>
		using base = apply_compose<C, 
			bindable<bind_type>, 
			handle_owner<detail::locked<handle_type>>,
			// child_of<bindable_resource<host_trait>>,
			frame_related>;

		template<typename C>
		struct apply : base<C> {
			using base = base<C>;

			template<typename T>
			void bind() noexcept {
				vptr_ = {
					.type_ = [](void const* ptr) noexcept -> descriptor_type {
						return static_cast<T const*>(ptr)->type();
					},
					.view_ = [](void const* ptr) noexcept -> view_type {
						return static_cast<T const*>(ptr)->view();
					},
					.layout_ = [](void const* ptr) noexcept -> layout_type {
						if constexpr (requires (T const& v) { v.layout(); }) {
							return static_cast<T const*>(ptr)->layout();
						}
						else {
							return nullptr;
						}
					}
					.upload_access_ = [](void* ptr)  {
						static_cast<T*>(ptr)->upload_access();
					}
				};

			}

			descriptor_type type() const noexcept {
				return vptr_.type_(C::get_this());
			}

			view_type view() const noexcept {
				return vptr_.view_(C::get_this());
			}

			layout_type layout() const noexcept {
				return vptr_.layout_(C::get_this());
			}

			void upload_access() noexcept {
				vptr_.upload_access_(C::get_this());
			}

		private:
			view_need_descriptor vptr_;
		};

		vfn<void() const> upload_access_;
		vfn<view_type() const> view_;
		vfn<layout_type() const> layout_;
		vfn<descriptor_type() const> type_;
	};
}

namespace vktl::detail {

	namespace descriptor {
		struct scope {
			uint32_t binding = 0u;
			uint32_t dst_element = 0u;
		};
		// top state.
		struct top {
			scope scope;
			vector<box<vptr::sampler>> samplers;
			vector<VK_ VkDescriptorImageInfo> images;
			vector<VK_ VkDescriptorBufferInfo> buffers;
			vector<VK_ VkBufferView> texels;
		};

		void write(auto& state, VK_ VkImage handle, VK_ VkImageView view, VK_ VkImageLayout layout, VK_ VkImageSubresourceRange const&) {
			top& value = get<top>(state);
			assert(view);	
			value.images.push_back(VK_ VkDescriptorImageInfo{
				.sampler = VK_NULL_HANDLE,
				.imageView = view,
				.imageLayout = layout,
			});
		}

		void write(auto& state, VK_ VkBuffer handle, VK_ VkBufferView view, ::std::nullptr_t, range<VK_ VkDeviceSize> const& range) {
			top& value = get<top>(state);
			assert(handle || view);
			if (view) {
				value.texels.emplace_back(view);
			}
			else {
				value.buffers.emplace_back(VK_ VkDescriptorBufferInfo{
					.buffer = handle,
					.offset = range.offset,
					.range = range.size,
				});
			}
		}
	}

	template<typename N>
	struct m<bind_set_, N> : basic_frame_related<N> {
		constexpr m(bind_set_, auto&&...others)
			: N {forward_(others)...}
		{}

		default_descriptor_set_layout const& layout() const noexcept {
			return layouts_;
		}
		
		void reset() {
			if (bind_.handle) {
				bind_.free();
			}
		}

	protected:
		auto get_state() {
			return::std::tuple(descriptor::top{});
		}

		void bind(bind_descriptors bind) {
			if (bind.handle) {
				assert(bind.handle->type == descriptor::type_set); // test.
				assert(bind.count == this->frame_count()); // test.
				bind_ = bind;
			}
		}
		
		void append(auto&&...others)
			requires(requires{ this->layouts_.add(forward_(others)...); }) {
			this->layouts_.add(forward_(others)...);
		}

		static void write(auto& state, VK_ VkDescriptorSet set) {
			
		}
		static void write(auto& state, VK_ VkDescriptorSet set, auto& child, auto&...others) {
			
		}

	protected:
		VK_ VkDescriptorSet handle() const noexcept {
			if (bind_.handle) {
				return static_cast<descriptor_pool*>(bind_.handle)->sets[bind_.set + this->frame_index()];
			}
			else {
				return nullptr;
			}
		}

	protected:
		default_descriptor_set_layout layouts_;

	private:
		bind_descriptors bind_;
	};

	template<typename T>
	struct default_set_bind_point {
		uint32_t binding;
		VK_ VkDescriptorType type;
		
		box_list<T> childs;
	};

	// N usually is other like `allow_image_` or `allow_tensor_`.
	template<typename N>
		requires(object_of<N, descriptor_set_>)
	struct m<allow_buffer_, N> : N {
		using base = N;

		constexpr m(allow_buffer_, auto&&...others)
			: N{forward_(others)...} 
		{}

		void reserve(object_of<buffer_view> auto& buffer, set_point point) {
			descriptor_scope scope = N::layouts_.add(VK_ VkDescriptorSetLayoutBinding{ point.binding, buffer.descriptor_type(), 1u });
			auto& childs = childs_[scope.index].childs;
		}

		void update(object_of<buffer_view> auto& buffer, set_point point) {

		}

		void bind(object_of<buffer_view> auto& buffer, set_point bind) {
			reserve(buffer, bind);
			childs.insert(childs.begin() + scope.element, buffer);
		}

		void bind(bind_descriptors bind) {
			N::bind(bind);
			
		}

	private:
		vector<default_set_bind_point<vptr::view_need_descriptor<trait<buffer_view>>>> childs_;
	};

}