#pragma once

// --- Agents specification -------------------------------------------------
// Every runtime recipe is one independently owned `object` inheritance chain
// stored in a command unit's `poly_list`. `compiled_recipe` owns only the
// record trampoline and fingerprint; recipe components retain their own data.
// This header must not depend on the task component or task compiled state.
// Recipe fingerprints describe recorded command content and explicit
// revisions; they are never generated from a refresh counter.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	inline constexpr uint64_t recipe_hash_basis = FNV_bias;
	inline constexpr uint64_t recipe_hash_prime = 1099511628211ull;

	inline constexpr uint64_t recipe_hash_bytes(
		uint64_t hash, void const* data, size_t size) noexcept {
		auto const* bytes = static_cast<::std::byte const*>(data);
		for (size_t index = 0u; index < size; ++index) {
			hash ^= uint64_t(bytes[index]);
			hash *= recipe_hash_prime;
		}
		return hash;
	}

	template<typename T>
	uint64_t recipe_hash_value(uint64_t hash, T const& value) noexcept
		requires(::std::is_trivially_copyable_v<T>) {
		return recipe_hash_bytes(hash, ::std::addressof(value), sizeof(T));
	}

	inline uint64_t recipe_hash_pointer(uint64_t hash, void const* value) noexcept {
		auto pointer = reinterpret_cast<::std::uintptr_t>(value);
		return recipe_hash_value(hash, pointer);
	}

	inline uint64_t allocate_recipe_type_key() noexcept {
		static ::std::atomic<uint64_t> next{ 1u };
		return next.fetch_add(1u, ::std::memory_order_relaxed);
	}

	template<typename T>
	inline uint64_t const recipe_type_key_v = allocate_recipe_type_key();

	template<typename T>
	uint64_t recipe_hash_type(uint64_t hash) noexcept {
		return recipe_hash_value(hash, recipe_type_key_v<T>);
	}

	struct frame_selection {
		span<box<vptr::frame_related> const> scopes;
		span<uint32_t const> frames;

		uint32_t frame(frame_scope_id scope) const noexcept {
			for (uint32_t index = 0u; index < uint32_t(scopes.size()); ++index) {
				if (scopes[index].frame_scope_identity() == scope) return frames[index];
			}
			assert(!"requested frame scope is not a command dependency");
			return 0u;
		}
	};

	struct command_record_context {
		VK_ VkCommandBuffer command = VK_NULL_HANDLE;
		frame_selection const* frames = nullptr;
	};

	struct compiled_recipe : poly_list::node {
		using record_fn = void(*)(compiled_recipe const&, command_record_context const&);

		void record(command_record_context const& context) const {
			assert(record_);
			record_(*this, context);
		}

		uint64_t fingerprint() const noexcept { return fingerprint_; }

	protected:
		template<typename T>
		void bind_record() noexcept {
			record_ = [](compiled_recipe const& base,
				command_record_context const& context) {
				static_cast<T const&>(base).record_chain(context);
			};
		}

		void record_chain(command_record_context const&) const noexcept {}

		record_fn record_ = nullptr;
		uint64_t fingerprint_ = recipe_hash_basis;
	};

	template<typename T>
	uint32_t selected_frame(T const& object, frame_selection const& selection) noexcept {
		if constexpr (requires { object.frame_scope_identity(); }) {
			return selection.frame(object.frame_scope_identity());
		}
		else {
			return 0u;
		}
	}

	template<typename Fn>
	struct callable_recipe {
		Fn function;
		uint64_t revision = 0u;
	};

	struct draw_recipe {
		uint32_t vertices = 0u;
		uint32_t instances = 1u;
		uint32_t first_vertex = 0u;
		uint32_t first_instance = 0u;
	};

	struct dispatch_recipe {
		uint32_t x = 1u;
		uint32_t y = 1u;
		uint32_t z = 1u;
	};

	template<typename Pass>
	struct bind_pipeline_recipe {
		Pass* pass = nullptr;
		uint16_t index = 0u;
		VK_ VkPipelineBindPoint bind_point = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
	};

	template<typename BindSet>
	struct bind_descriptor_recipe {
		BindSet* bind_set = nullptr;
		VK_ VkPipelineBindPoint bind_point = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
		VK_ VkPipelineLayout layout = VK_NULL_HANDLE;
		uint32_t set = 0u;
	};

	template<typename Buffer>
	struct bind_vertex_recipe {
		Buffer* buffer = nullptr;
		uint32_t binding = 0u;
		VK_ VkDeviceSize offset = 0u;
	};

	template<typename Buffer>
	struct bind_index_recipe {
		Buffer* buffer = nullptr;
		VK_ VkDeviceSize offset = 0u;
		VK_ VkIndexType type = VK_ VK_INDEX_TYPE_UINT32;
	};

	// Compatibility spellings for the command-context API used before recipes
	// moved to this focused header.
	template<typename Fn>
	using callable_command_recipe = callable_recipe<Fn>;
	using draw_command_recipe = draw_recipe;
	using dispatch_command_recipe = dispatch_recipe;
	template<typename Pass>
	using bind_pipeline_command_recipe = bind_pipeline_recipe<Pass>;
	template<typename BindSet>
	using bind_descriptor_command_recipe = bind_descriptor_recipe<BindSet>;
	template<typename Buffer>
	using bind_vertex_buffer_command_recipe = bind_vertex_recipe<Buffer>;
	template<typename Buffer>
	using bind_index_buffer_command_recipe = bind_index_recipe<Buffer>;

	template<typename Fn, typename N>
	struct m<callable_recipe<Fn>, N> : N {
		callable_recipe<Fn> info_;

		template<similiar_to<callable_recipe<Fn>> Info>
		constexpr m(Info&& info, auto&&... others)
			: N{ forward_(others)... }, info_{ forward_(info) } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_type<callable_recipe<Fn>>(this->fingerprint_);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.revision);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			if constexpr (::std::invocable<Fn const&, VK_ VkCommandBuffer, frame_selection const&>) {
				assert(context.frames);
				info_.function(context.command, *context.frames);
			}
			else if constexpr (::std::invocable<Fn const&, VK_ VkCommandBuffer>) {
				info_.function(context.command);
			}
			else {
				static_assert(always_false<Fn>,
					"A command recipe must accept VkCommandBuffer, optionally with frame_selection.");
			}
		}
	};

	template<typename N>
	struct m<draw_recipe, N> : N {
		draw_recipe info_;

		constexpr m(draw_recipe info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.vertices);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.instances);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.first_vertex);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.first_instance);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			VK_ vkCmdDraw(context.command, info_.vertices, info_.instances,
				info_.first_vertex, info_.first_instance);
		}
	};

	template<typename N>
	struct m<dispatch_recipe, N> : N {
		dispatch_recipe info_;

		constexpr m(dispatch_recipe info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.x);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.y);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.z);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			VK_ vkCmdDispatch(context.command, info_.x, info_.y, info_.z);
		}
	};

	template<typename Pass, typename N>
	struct m<bind_pipeline_recipe<Pass>, N> : N {
		bind_pipeline_recipe<Pass> info_;

		constexpr m(bind_pipeline_recipe<Pass> info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_pointer(this->fingerprint_, info_.pass);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.index);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.bind_point);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			auto pipeline = info_.pass->pipe(info_.index);
			assert(pipeline != VK_NULL_HANDLE);
			VK_ vkCmdBindPipeline(context.command, info_.bind_point, pipeline);
		}
	};

	template<typename BindSet, typename N>
	struct m<bind_descriptor_recipe<BindSet>, N> : N {
		bind_descriptor_recipe<BindSet> info_;

		constexpr m(bind_descriptor_recipe<BindSet> info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_pointer(this->fingerprint_, info_.bind_set);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.bind_point);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.layout);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.set);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			assert(context.frames);
			auto frame = selected_frame(*info_.bind_set, *context.frames);
			auto descriptor = info_.bind_set->descriptor_set(frame, info_.set);
			assert(descriptor != VK_NULL_HANDLE && info_.layout != VK_NULL_HANDLE);
			VK_ vkCmdBindDescriptorSets(context.command, info_.bind_point, info_.layout,
				info_.set, 1u, &descriptor, 0u, nullptr);
		}
	};

	template<typename Buffer, typename N>
	struct m<bind_vertex_recipe<Buffer>, N> : N {
		bind_vertex_recipe<Buffer> info_;

		constexpr m(bind_vertex_recipe<Buffer> info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_pointer(this->fingerprint_, info_.buffer);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.binding);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.offset);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			assert(context.frames);
			auto frame = selected_frame(*info_.buffer, *context.frames);
			auto locked_handle = info_.buffer->handle(frame);
			auto handle = locked_handle.value;
			VK_ vkCmdBindVertexBuffers(context.command, info_.binding, 1u,
				&handle, &info_.offset);
		}
	};

	template<typename Buffer, typename N>
	struct m<bind_index_recipe<Buffer>, N> : N {
		bind_index_recipe<Buffer> info_;

		constexpr m(bind_index_recipe<Buffer> info, auto&&... others)
			: N{ forward_(others)... }, info_{ info } {
			this->template bind_record<m>();
			this->fingerprint_ = recipe_hash_pointer(this->fingerprint_, info_.buffer);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.offset);
			this->fingerprint_ = recipe_hash_value(this->fingerprint_, info_.type);
		}

		void record_chain(command_record_context const& context) const {
			N::record_chain(context);
			assert(context.frames);
			auto frame = selected_frame(*info_.buffer, *context.frames);
			auto locked_handle = info_.buffer->handle(frame);
			VK_ vkCmdBindIndexBuffer(context.command, locked_handle.value,
				info_.offset, info_.type);
		}
	};

	template<typename... Infos>
	auto& emplace_recipe(poly_list& recipes, Infos&&... infos) {
		using object_type = decltype(object{
			use_base<compiled_recipe>{}, static_cast<Infos&&>(infos)... });
		return recipes.template emplace_back<object_type>(
			use_base<compiled_recipe>{}, static_cast<Infos&&>(infos)...);
	}

}
