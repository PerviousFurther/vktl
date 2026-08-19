#pragma once

// --- Agents specification -------------------------------------------------
// Recording workers and Vulkan queues are independent. Execution owns queue
// handles, per-queue locks, typed payload pools, stable task slots, epochs,
// completion fences, and retired plans. `submit()` invokes only precompiled
// operation records and must not allocate or repeat capability checks.
//
// Queue payload extension uses `detail::trait<Payload>` plus the handwritten
// `vptr::queue_payload` table, never a central payload variant or type switch.
// A payload trait owns three things: an index-only `storage_type`, a flat
// `column_type` holding the actual Vulkan values for the whole queue, and the
// `invoke()` which assembles pointer-bearing Vulkan info structs on the stack
// (or in preallocated scratch) at invocation time. Pool entries therefore never
// carry per-entry worst-case arrays.
//
// Plan-visible GPU storage is retired by completion epoch. Every queue owns one
// fence per in-flight slot; `submit()` closes each participating queue with an
// empty fenced submission, which both throttles the host and defines the
// completion epoch used to recycle command pools and retired plans.
//
// `VK_KHR_synchronization2` and `VK_KHR_timeline_semaphore` are selected at
// runtime through `vkGetDeviceProcAddr`, not by preprocessor alone; the legacy
// path stays reachable on the same binary.
//
// VkResult failures use `| popup{...}`. Prefer extension-suffixed Vulkan APIs,
// and keep all presentation declarations behind `VKTL_HAVE_WINDOW`.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	// -----------------------------------------------------------------------
	// Queue duties and declarations
	// -----------------------------------------------------------------------

	namespace queue_duty {
		using type = uint16_t;
		inline constexpr type none = 0x0;
		inline constexpr type compute = 0x1 << 0;
		inline constexpr type transfer = 0x1 << 1;
		inline constexpr type graphics = 0x1 << 2;
#if VKTL_HAVE_WINDOW
		inline constexpr type present = 0x1 << 3;
#endif
		inline constexpr type bind_sparse = 0x1 << 4;
	}

	struct queue_declaration : queue {
		queue_duty::type duty = queue_duty::none;
	};

	// -----------------------------------------------------------------------
	// Neutral synchronization vocabulary
	// -----------------------------------------------------------------------

	// The neutral stage type is always 64-bit. For every stage bit that exists in
	// both generations the numeric values are identical, so the legacy path only
	// has to narrow; a mask that survives narrowing as zero falls back to
	// ALL_COMMANDS rather than silently waiting on nothing.
#if defined(VK_KHR_synchronization2)
	using queue_stage_flags = VK_ VkPipelineStageFlags2KHR;
	inline constexpr queue_stage_flags queue_all_commands_stage =
		VK_ VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
#else
	using queue_stage_flags = uint64_t;
	inline constexpr queue_stage_flags queue_all_commands_stage =
		queue_stage_flags(VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
#endif

	inline constexpr VK_ VkPipelineStageFlags legacy_stage(queue_stage_flags stage) noexcept {
		auto narrowed = VK_ VkPipelineStageFlags(stage & uint32_t(maximum));
		return narrowed ? narrowed : VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	// Runtime-resolved dispatch selection. Filled once by `execution::init()`.
	struct queue_dispatch_config {
		bool synchronization2 = false;
		bool timeline_semaphore = false;
	};

	// -----------------------------------------------------------------------
	// Payload identity and capacity
	// -----------------------------------------------------------------------

	// Stable compile-time numeric IDs. Object addresses are never used as keys.
	namespace payload_type {
		using type = uint32_t;
		inline constexpr type submit = 0u;
		inline constexpr type present = 1u;
		inline constexpr type sparse_bind = 2u;
		inline constexpr type first_user = 16u;
	}

	// Two distinct groups with two distinct aggregation rules.
	//
	//   *sum group*  - flat column lengths and record counts. One payload
	//                  instance consumes its own slice of the queue-wide column,
	//                  so totals add.
	//   *peak group* - the largest single payload instance. Only used to size the
	//                  per-queue scratch that `invoke()` writes into, so totals
	//                  take the maximum instead of multiplying.
	struct submit_capacity {
		uint32_t operations = 0u;
		uint32_t submits = 0u;
		uint32_t command_buffers = 0u;
		uint32_t semaphores = 0u;
#if VKTL_HAVE_WINDOW
		uint32_t presents = 0u;
		uint32_t present_swapchains = 0u;
		uint32_t present_waits = 0u;
#endif
		uint32_t sparse_binds = 0u;
		uint32_t sparse_buffer_binds = 0u;
		uint32_t sparse_semaphores = 0u;

		uint32_t peak_command_buffers = 0u;
		uint32_t peak_semaphores = 0u;
#if VKTL_HAVE_WINDOW
		uint32_t peak_present_swapchains = 0u;
		uint32_t peak_present_waits = 0u;
#endif
		uint32_t peak_sparse_buffer_binds = 0u;
		uint32_t peak_sparse_semaphores = 0u;

		constexpr submit_capacity& operator+=(submit_capacity const& other) noexcept {
			operations += other.operations;
			submits += other.submits;
			command_buffers += other.command_buffers;
			semaphores += other.semaphores;
#if VKTL_HAVE_WINDOW
			presents += other.presents;
			present_swapchains += other.present_swapchains;
			present_waits += other.present_waits;
#endif
			sparse_binds += other.sparse_binds;
			sparse_buffer_binds += other.sparse_buffer_binds;
			sparse_semaphores += other.sparse_semaphores;

			peak_command_buffers = (::std::max)(peak_command_buffers, other.peak_command_buffers);
			peak_semaphores = (::std::max)(peak_semaphores, other.peak_semaphores);
#if VKTL_HAVE_WINDOW
			peak_present_swapchains = (::std::max)(peak_present_swapchains, other.peak_present_swapchains);
			peak_present_waits = (::std::max)(peak_present_waits, other.peak_present_waits);
#endif
			peak_sparse_buffer_binds = (::std::max)(peak_sparse_buffer_binds, other.peak_sparse_buffer_binds);
			peak_sparse_semaphores = (::std::max)(peak_sparse_semaphores, other.peak_sparse_semaphores);
			return *this;
		}

		// Field-wise maximum. Used when an already sized pool must be widened:
		// adding the new total to the old one would make capacity grow without
		// bound across repeated refreshes.
		constexpr submit_capacity& grow(submit_capacity const& other) noexcept {
			operations = (::std::max)(operations, other.operations);
			submits = (::std::max)(submits, other.submits);
			command_buffers = (::std::max)(command_buffers, other.command_buffers);
			semaphores = (::std::max)(semaphores, other.semaphores);
#if VKTL_HAVE_WINDOW
			presents = (::std::max)(presents, other.presents);
			present_swapchains = (::std::max)(present_swapchains, other.present_swapchains);
			present_waits = (::std::max)(present_waits, other.present_waits);
#endif
			sparse_binds = (::std::max)(sparse_binds, other.sparse_binds);
			sparse_buffer_binds = (::std::max)(sparse_buffer_binds, other.sparse_buffer_binds);
			sparse_semaphores = (::std::max)(sparse_semaphores, other.sparse_semaphores);

			peak_command_buffers = (::std::max)(peak_command_buffers, other.peak_command_buffers);
			peak_semaphores = (::std::max)(peak_semaphores, other.peak_semaphores);
#if VKTL_HAVE_WINDOW
			peak_present_swapchains = (::std::max)(peak_present_swapchains, other.peak_present_swapchains);
			peak_present_waits = (::std::max)(peak_present_waits, other.peak_present_waits);
#endif
			peak_sparse_buffer_binds = (::std::max)(peak_sparse_buffer_binds, other.peak_sparse_buffer_binds);
			peak_sparse_semaphores = (::std::max)(peak_sparse_semaphores, other.peak_sparse_semaphores);
			return *this;
		}

		constexpr bool fits_in(submit_capacity const& other) const noexcept {
			return operations <= other.operations
				&& submits <= other.submits
				&& command_buffers <= other.command_buffers
				&& semaphores <= other.semaphores
#if VKTL_HAVE_WINDOW
				&& presents <= other.presents
				&& present_swapchains <= other.present_swapchains
				&& present_waits <= other.present_waits
#endif
				&& sparse_binds <= other.sparse_binds
				&& sparse_buffer_binds <= other.sparse_buffer_binds
				&& sparse_semaphores <= other.sparse_semaphores
				&& peak_command_buffers <= other.peak_command_buffers
				&& peak_semaphores <= other.peak_semaphores
#if VKTL_HAVE_WINDOW
				&& peak_present_swapchains <= other.peak_present_swapchains
				&& peak_present_waits <= other.peak_present_waits
#endif
				&& peak_sparse_buffer_binds <= other.peak_sparse_buffer_binds
				&& peak_sparse_semaphores <= other.peak_sparse_semaphores;
		}
	};

	struct queue_payload_pool_base;
	template<typename Trait>
	struct queue_payload_pool;
	struct task_plan;

	// The compiled hot-path record. No capability test, no type switch, no
	// allocation: one indirect call per contiguous batch of one payload type.
	struct queue_operation {
		void (*invoke)(VK_ VkQueue, queue_payload_pool_base&, uint32_t, uint32_t) = nullptr;
		queue_payload_pool_base* pool = nullptr;
		uint32_t first = 0u;
		uint32_t count = 0u;
	};

	struct payload_requirement {
		payload_type::type type = payload_type::type(invalid);
		uint32_t queue_index = uint32_t(invalid);
		queue_duty::type required_duty = queue_duty::none;
		queue_payload_pool_base& (*create)(poly_list&) = nullptr;
		submit_capacity capacity;
	};

	// Type-erased handle onto the owning `execution`. `task.submit()` writes
	// through this; it performs cursor arithmetic only.
	struct queue_payload_writer {
		void* execution = nullptr;
		queue_payload_pool_base& (*pool_)(void*, uint32_t, payload_type::type) noexcept = nullptr;
		void (*append_)(void*, uint32_t, queue_operation) noexcept = nullptr;

		template<typename Trait>
		queue_payload_pool<Trait>& pool(uint32_t queue_index) const noexcept {
			auto& base = pool_(execution, queue_index, Trait::payload_id);
			assert(base.type == Trait::payload_id);
			return static_cast<queue_payload_pool<Trait>&>(base);
		}

		void append(uint32_t queue_index, queue_operation operation) const noexcept {
			append_(execution, queue_index, operation);
		}
	};

	// Everything a payload declaration needs in order to materialize itself.
	struct payload_context {
		task_plan const* plan = nullptr;
		queue_payload_writer writer;
	};

}

VKTL_EXPORT_ namespace vktl::vptr {

	struct queue_semaphore {
		template<typename C>
		struct apply;
		vfn<VK_ VkSemaphore() const noexcept> handle_ = nullptr;
	};

	template<typename C>
	struct queue_semaphore::apply : C {
		using base = C;
		template<typename T>
		void rebind() noexcept {
			vptr_ = { .handle_ = [](void const* ptr) noexcept {
				return static_cast<T const*>(ptr)->handle();
			} };
		}
		VK_ VkSemaphore handle() const noexcept { return vptr_.handle_(C::get_this()); }
		queue_semaphore vptr_;
	};

#if VKTL_HAVE_WINDOW
	struct presentable {
		template<typename C>
		struct apply;
		vfn<VK_ VkSwapchainKHR() const noexcept> handle_ = nullptr;
		vfn<uint32_t() const noexcept> frame_index_ = nullptr;
		vfn<VK_ VkSurfaceKHR() const noexcept> surface_ = nullptr;
	};

	template<typename C>
	struct presentable::apply : C {
		using base = C;
		template<typename T>
		void rebind() noexcept {
			vptr_ = {
				.handle_ = [](void const* ptr) noexcept { return static_cast<T const*>(ptr)->handle(); },
				.frame_index_ = [](void const* ptr) noexcept -> uint32_t {
					return static_cast<T const*>(ptr)->frame_index();
				},
				.surface_ = [](void const* ptr) noexcept {
					return static_cast<T const*>(ptr)->surface();
				},
			};
		}
		VK_ VkSwapchainKHR handle() const noexcept { return vptr_.handle_(C::get_this()); }
		uint32_t frame_index() const noexcept { return vptr_.frame_index_(C::get_this()); }
		VK_ VkSurfaceKHR surface() const noexcept { return vptr_.surface_(C::get_this()); }
		presentable vptr_;
	};
#endif

	// Handwritten queue-payload table.
	//
	// It is used during refresh (`count_`) and during `task.submit()`
	// (`materialize_`) only; the execution hot path runs off compiled
	// `queue_operation` records instead. `apply` is provided so an `objects`
	// instance can also serve as a payload declaration, but plan-owned
	// declarations live in the task arena and are bound through
	// `detail::payload_ref`, because arena storage is not reference counted and
	// must not be adopted by `box`.
	struct queue_payload {
		template<typename C>
		struct apply;

		vfn<uint32_t() const noexcept> queue_index_ = nullptr;
		vfn<detail::payload_requirement() const> count_ = nullptr;
		vfn<void(detail::payload_context const&) const> materialize_ = nullptr;
	};

	template<typename C>
	struct queue_payload::apply : C {
		using base = C;

		template<typename T>
		void rebind() noexcept {
			vptr_ = {
				.queue_index_ = [](void const* ptr) noexcept {
					return detail::trait<T>::queue_index(*static_cast<T const*>(ptr));
				},
				.count_ = [](void const* ptr) {
					return detail::trait<T>::count(*static_cast<T const*>(ptr));
				},
				.materialize_ = [](void const* ptr, detail::payload_context const& context) {
					detail::trait<T>::materialize(*static_cast<T const*>(ptr), context);
				},
			};
		}

		uint32_t queue_index() const noexcept { return vptr_.queue_index_(C::get_this()); }
		detail::payload_requirement count() const { return vptr_.count_(C::get_this()); }
		void materialize(detail::payload_context const& context) const {
			vptr_.materialize_(C::get_this(), context);
		}

		queue_payload vptr_;
	};

}

VKTL_EXPORT_ namespace vktl::detail {

	// One static table per payload declaration type.
	template<typename T>
	inline constexpr vptr::queue_payload queue_payload_table_v{
		.queue_index_ = [](void const* ptr) noexcept {
			return trait<T>::queue_index(*static_cast<T const*>(ptr));
		},
		.count_ = [](void const* ptr) {
			return trait<T>::count(*static_cast<T const*>(ptr));
		},
		.materialize_ = [](void const* ptr, payload_context const& context) {
			trait<T>::materialize(*static_cast<T const*>(ptr), context);
		},
	};

	// Non-owning reference to an arena-resident payload declaration.
	struct payload_ref {
		void const* value = nullptr;
		vptr::queue_payload const* table = nullptr;

		constexpr payload_ref() noexcept = default;

		template<typename T>
		explicit constexpr payload_ref(T const& declaration) noexcept
			: value{ ::std::addressof(declaration) }
			, table{ ::std::addressof(queue_payload_table_v<T>) } {
		}

		explicit constexpr operator bool() const noexcept { return value != nullptr; }

		uint32_t queue_index() const noexcept { return table->queue_index_(value); }
		payload_requirement count() const { return table->count_(value); }
		void materialize(payload_context const& context) const { table->materialize_(value, context); }
	};

	// -----------------------------------------------------------------------
	// Typed pools
	// -----------------------------------------------------------------------

	struct queue_payload_pool_base : poly_list::node {
		payload_type::type type = payload_type::type(invalid);
		queue_dispatch_config dispatch{};
		submit_capacity capacity{};

		void (*reserve_)(queue_payload_pool_base&, submit_capacity const&) = nullptr;
		void (*rewind_)(queue_payload_pool_base&) noexcept = nullptr;

		void reserve(submit_capacity const& value) { reserve_(*this, value); }
		void rewind() noexcept { rewind_(*this); }
	};

	template<typename Trait>
	struct queue_payload_pool : queue_payload_pool_base {
		using storage_type = typename Trait::storage_type;
		using column_type = typename Trait::column_type;

		queue_payload_pool() {
			this->type = Trait::payload_id;
			this->reserve_ = [](queue_payload_pool_base& pool, submit_capacity const& capacity) {
				auto& typed = static_cast<queue_payload_pool&>(pool);
				typed.values.resize(Trait::storage_count(capacity));
				Trait::reserve(typed.columns, capacity);
				typed.capacity = capacity;
				typed.rewind();
			};
			this->rewind_ = [](queue_payload_pool_base& pool) noexcept {
				auto& typed = static_cast<queue_payload_pool&>(pool);
				typed.cursor = 0u;
				typed.columns.rewind();
			};
		}

		// Claims one storage entry. Never grows: refresh already sized the pool.
		storage_type& claim(uint32_t& index) noexcept {
			assert(cursor < values.size() && "payload pool was not sized during refresh");
			index = cursor++;
			return values[index];
		}

		vector<storage_type> values;
		column_type columns;
		uint32_t cursor = 0u;
	};

	template<typename Trait>
	inline constexpr auto payload_pool_factory_v =
		+[](poly_list& pools) -> queue_payload_pool_base& {
			return pools.emplace_back<queue_payload_pool<Trait>>();
		};

	// -----------------------------------------------------------------------
	// Frame selection and command units
	// -----------------------------------------------------------------------

	struct frame_selection {
		span<box<vptr::frame_related> const> scopes;
		span<uint32_t const> frames;

		uint32_t frame(frame_scope_id scope) const noexcept {
			for (uint32_t index = 0u; index < uint32_t(scopes.size()); ++index) {
				if (scopes[index].frame_scope_identity() == scope) return frames[index];
			}
			// A recipe may only ask for a scope its command unit depends on.
			assert(!"requested frame scope is not a dependency of this command unit");
			return 0u;
		}
	};

	inline uint32_t command_variant_count(span<box<vptr::frame_related> const> scopes) {
		uint32_t count = 1u;
		for (auto const& scope : scopes) {
			auto frames = scope.frame_count();
			if (frames == 0u || count > uint32_t(maximum) / frames) {
				throw error{ int(VK_ VK_ERROR_OUT_OF_HOST_MEMORY),
					"[TASK] Command frame-variant count overflowed." };
			}
			count *= frames;
		}
		return count;
	}

	struct recipe_operation {
		void const* payload = nullptr;
		void (*record)(void const*, VK_ VkCommandBuffer, frame_selection const&) = nullptr;

		uint64_t fingerprint = 0u;
	};

	struct command_variant {
		vector<uint32_t> frames;
		vector<uint64_t> revisions;

		VK_ VkCommandBuffer handle = VK_NULL_HANDLE;
		uint64_t generation = 0u;
	};

	struct command_unit {
		uint32_t worker_index = 0u;
		uint32_t queue_index = uint32_t(invalid);
		uint32_t queue_family = uint32_t(invalid);

		VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		uint64_t recipe_revision = 0u;
		vector<recipe_operation> recipe;
		vector<box<vptr::frame_related>> frame_scopes;
		vector<uint32_t> strides;
		vector<command_variant> variants;
	};

	// Mixed-radix selection of the variant matching every scope's current frame.
	inline uint32_t current_variant_index(command_unit const& command) noexcept {
		uint32_t result = 0u;
		for (uint32_t scope = 0u; scope < uint32_t(command.frame_scopes.size()); ++scope) {
			auto frame = command.frame_scopes[scope].frame_index();
			assert(frame < command.frame_scopes[scope].frame_count());
			result += frame * command.strides[scope];
		}
		assert(result < command.variants.size());
		return result;
	}

	inline command_variant const& current_variant(command_unit const& command) noexcept {
		return command.variants[current_variant_index(command)];
	}

	// -----------------------------------------------------------------------
	// Reusable bump arena
	// -----------------------------------------------------------------------

	// Whole arenas are recycled after their retirement epoch completes; `clear()`
	// keeps the blocks and only unwinds the destructor records and cursors.
	class recipe_arena {
		struct block {
			::std::unique_ptr<::std::byte[]> bytes;
			size_t size = 0u;
			size_t cursor = 0u;
		};
		struct destructor_record {
			void* object = nullptr;
			void (*destroy)(void*) noexcept = nullptr;
		};

		static constexpr size_t default_block_size = 8192u;

	public:
		recipe_arena() = default;
		recipe_arena(recipe_arena const&) = delete;
		recipe_arena& operator=(recipe_arena const&) = delete;
		recipe_arena(recipe_arena&&) noexcept = default;
		recipe_arena& operator=(recipe_arena&&) noexcept = default;
		~recipe_arena() { clear(); }

		template<typename T, typename...Args>
		T& emplace(Args&&...args) {
			auto* storage = allocate(sizeof(T), alignof(T));
			auto* value = ::new(storage) T(static_cast<Args&&>(args)...);
			if constexpr (!::std::is_trivially_destructible_v<T>) {
				try {
					destructors_.emplace_back(destructor_record{
						value, [](void* ptr) noexcept { static_cast<T*>(ptr)->~T(); } });
				}
				catch (...) {
					value->~T();
					throw;
				}
			}
			return *value;
		}

		// Unwinds contents but keeps the blocks so the arena can be reused.
		void clear() noexcept {
			for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
				it->destroy(it->object);
			}
			destructors_.clear();
			for (auto& value : blocks_) value.cursor = 0u;
			active_ = blocks_.empty() ? size_t(invalid) : 0u;
		}

	private:
		void* allocate(size_t size, size_t alignment) {
			// Walk from the active block forward so a large allocation does not
			// permanently abandon the space left in earlier blocks.
			for (auto index = (active_ == size_t(invalid) ? 0u : active_); index < blocks_.size(); ++index) {
				if (auto* result = try_allocate(blocks_[index], size, alignment)) {
					active_ = index;
					return result;
				}
			}
			auto block_size = (::std::max)(size + alignment, default_block_size);
			blocks_.emplace_back(block{ ::std::make_unique<::std::byte[]>(block_size), block_size, 0u });
			active_ = blocks_.size() - 1u;
			auto* result = try_allocate(blocks_.back(), size, alignment);
			assert(result);
			return result;
		}

		static void* try_allocate(block& value, size_t size, size_t alignment) noexcept {
			auto aligned = (value.cursor + alignment - 1u) & ~(alignment - 1u);
			if (aligned > value.size || size > value.size - aligned) return nullptr;
			value.cursor = aligned + size;
			return value.bytes.get() + aligned;
		}

		vector<block> blocks_;
		vector<destructor_record> destructors_;
		size_t active_ = size_t(invalid);
	};

	// -----------------------------------------------------------------------
	// Built-in payload: normal submission.
	// -----------------------------------------------------------------------

	// Neutral (generation-agnostic) semaphore operand.
	struct submit_semaphore_value {
		VK_ VkSemaphore handle = VK_NULL_HANDLE;
		uint64_t value = 0u;
		queue_stage_flags stage = queue_all_commands_stage;
	};

	// Index-only pool entry. All Vulkan values live in the queue-wide columns.
	struct compiled_submit {
		uint32_t command_first = 0u;
		uint32_t command_count = 0u;
		uint32_t wait_first = 0u;
		uint32_t wait_count = 0u;
		uint32_t signal_first = 0u;
		uint32_t signal_count = 0u;
	};

	struct submit_columns {
		vector<VK_ VkCommandBuffer> command_buffers;
		vector<submit_semaphore_value> semaphores;
		uint32_t command_cursor = 0u;
		uint32_t semaphore_cursor = 0u;

		// Scratch used by `invoke()`. Sized to the largest single submission, not
		// to the sum, and never resized outside refresh.
#if defined(VK_KHR_synchronization2)
		vector<VK_ VkCommandBufferSubmitInfoKHR> command_scratch;
		vector<VK_ VkSemaphoreSubmitInfoKHR> semaphore_scratch;
#endif
		vector<VK_ VkSemaphore> legacy_semaphores;
		vector<VK_ VkPipelineStageFlags> legacy_stages;
		vector<uint64_t> legacy_values;

		void rewind() noexcept { command_cursor = 0u; semaphore_cursor = 0u; }
	};

	struct submit_payload;
	struct semaphore_operand;

	template<>
	struct trait<submit_payload> {
		using type = submit_payload;
		using storage_type = compiled_submit;
		using column_type = submit_columns;

		static constexpr payload_type::type payload_id = payload_type::submit;
		static constexpr queue_duty::type required_duty = queue_duty::none;

		static uint32_t storage_count(submit_capacity const& capacity) noexcept {
			return capacity.submits;
		}

		static void reserve(column_type& columns, submit_capacity const& capacity) {
			columns.command_buffers.resize(capacity.command_buffers);
			columns.semaphores.resize(capacity.semaphores);
#if defined(VK_KHR_synchronization2)
			columns.command_scratch.resize(capacity.peak_command_buffers);
			columns.semaphore_scratch.resize(capacity.peak_semaphores);
#endif
			columns.legacy_semaphores.resize(capacity.peak_semaphores);
			columns.legacy_stages.resize(capacity.peak_semaphores);
			columns.legacy_values.resize(capacity.peak_semaphores);
			columns.rewind();
		}

		static uint32_t queue_index(submit_payload const& declaration) noexcept;
		static payload_requirement count(submit_payload const& declaration);
		static void materialize(submit_payload const& declaration, payload_context const& context);

		static void invoke(VK_ VkQueue queue, queue_payload_pool_base& base,
			uint32_t first, uint32_t count) {
			auto& pool = static_cast<queue_payload_pool<trait>&>(base);
			auto& columns = pool.columns;
			for (uint32_t index = 0u; index < count; ++index) {
				auto const& value = pool.values[first + index];
#if defined(VK_KHR_synchronization2)
				if (base.dispatch.synchronization2) {
					invoke_synchronization2(queue, columns, value);
					continue;
				}
#endif
				invoke_legacy(queue, columns, value, base.dispatch.timeline_semaphore);
			}
		}

	private:
#if defined(VK_KHR_synchronization2)
		static void invoke_synchronization2(VK_ VkQueue queue, column_type& columns,
			storage_type const& value) {
			for (uint32_t index = 0u; index < value.command_count; ++index) {
				columns.command_scratch[index] = VK_ VkCommandBufferSubmitInfoKHR{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
					.commandBuffer = columns.command_buffers[value.command_first + index],
				};
			}
			auto write = [&](uint32_t target, submit_semaphore_value const& operand) {
				columns.semaphore_scratch[target] = VK_ VkSemaphoreSubmitInfoKHR{
					.sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
					.semaphore = operand.handle,
					.value = operand.value,
					.stageMask = operand.stage,
				};
			};
			for (uint32_t index = 0u; index < value.wait_count; ++index) {
				write(index, columns.semaphores[value.wait_first + index]);
			}
			for (uint32_t index = 0u; index < value.signal_count; ++index) {
				write(value.wait_count + index, columns.semaphores[value.signal_first + index]);
			}
			VK_ VkSubmitInfo2KHR info{
				.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
				.waitSemaphoreInfoCount = value.wait_count,
				.pWaitSemaphoreInfos = columns.semaphore_scratch.data(),
				.commandBufferInfoCount = value.command_count,
				.pCommandBufferInfos = columns.command_scratch.data(),
				.signalSemaphoreInfoCount = value.signal_count,
				.pSignalSemaphoreInfos = columns.semaphore_scratch.data() + value.wait_count,
			};
			VK_ vkQueueSubmit2KHR(queue, 1u, &info, VK_NULL_HANDLE)
				| popup{ "[EXECUTION] Queue submission failed." };
		}
#endif

		static void invoke_legacy(VK_ VkQueue queue, column_type& columns,
			storage_type const& value, bool timeline) {
			// Waits occupy [0, wait_count), signals [wait_count, wait+signal).
			for (uint32_t index = 0u; index < value.wait_count; ++index) {
				auto const& operand = columns.semaphores[value.wait_first + index];
				columns.legacy_semaphores[index] = operand.handle;
				columns.legacy_stages[index] = legacy_stage(operand.stage);
				columns.legacy_values[index] = operand.value;
			}
			for (uint32_t index = 0u; index < value.signal_count; ++index) {
				auto const& operand = columns.semaphores[value.signal_first + index];
				columns.legacy_semaphores[value.wait_count + index] = operand.handle;
				columns.legacy_values[value.wait_count + index] = operand.value;
			}

			VK_ VkSubmitInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = value.wait_count,
				.pWaitSemaphores = columns.legacy_semaphores.data(),
				.pWaitDstStageMask = columns.legacy_stages.data(),
				.commandBufferCount = value.command_count,
				.pCommandBuffers = columns.command_buffers.data() + value.command_first,
				.signalSemaphoreCount = value.signal_count,
				.pSignalSemaphores = columns.legacy_semaphores.data() + value.wait_count,
			};
#if defined(VK_KHR_timeline_semaphore)
			VK_ VkTimelineSemaphoreSubmitInfoKHR timeline_info{
				.sType = VK_ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
				.waitSemaphoreValueCount = value.wait_count,
				.pWaitSemaphoreValues = columns.legacy_values.data(),
				.signalSemaphoreValueCount = value.signal_count,
				.pSignalSemaphoreValues = columns.legacy_values.data() + value.wait_count,
			};
			if (timeline) info.pNext = &timeline_info;
#else
			(void)timeline;
#endif
			VK_ vkQueueSubmit(queue, 1u, &info, VK_NULL_HANDLE)
				| popup{ "[EXECUTION] Queue submission failed." };
		}
	};

	// -----------------------------------------------------------------------
	// Built-in payload: presentation
	// -----------------------------------------------------------------------

#if VKTL_HAVE_WINDOW
	struct compiled_present {
		uint32_t wait_first = 0u;
		uint32_t wait_count = 0u;
		uint32_t swapchain_first = 0u;
		uint32_t swapchain_count = 0u;
	};

	struct present_columns {
		vector<VK_ VkSemaphore> waits;
		vector<VK_ VkSwapchainKHR> swapchains;
		vector<uint32_t> image_indices;
		uint32_t wait_cursor = 0u;
		uint32_t swapchain_cursor = 0u;

		vector<VK_ VkResult> result_scratch;

		void rewind() noexcept { wait_cursor = 0u; swapchain_cursor = 0u; }
	};

	// Non-fatal presentation outcomes are reported rather than thrown: the host
	// loop must be able to recreate a swapchain without unwinding the frame.
	struct present_status {
		::std::atomic<uint64_t> out_of_date{ 0u };
		::std::atomic<uint64_t> suboptimal{ 0u };
	};

	inline present_status& global_present_status() noexcept {
		static present_status value;
		return value;
	}

	struct present_payload;

	template<>
	struct trait<present_payload> {
		using type = present_payload;
		using storage_type = compiled_present;
		using column_type = present_columns;

		static constexpr payload_type::type payload_id = payload_type::present;
		static constexpr queue_duty::type required_duty = queue_duty::present;

		static uint32_t storage_count(submit_capacity const& capacity) noexcept {
			return capacity.presents;
		}

		static void reserve(column_type& columns, submit_capacity const& capacity) {
			columns.waits.resize(capacity.present_waits);
			columns.swapchains.resize(capacity.present_swapchains);
			columns.image_indices.resize(capacity.present_swapchains);
			columns.result_scratch.resize(capacity.peak_present_swapchains);
			columns.rewind();
		}

		static uint32_t queue_index(present_payload const& declaration) noexcept;
		static payload_requirement count(present_payload const& declaration);
		static void materialize(present_payload const& declaration, payload_context const& context);

		static void invoke(VK_ VkQueue queue, queue_payload_pool_base& base,
			uint32_t first, uint32_t count) {
			auto& pool = static_cast<queue_payload_pool<trait>&>(base);
			auto& columns = pool.columns;
			for (uint32_t index = 0u; index < count; ++index) {
				auto const& value = pool.values[first + index];
				VK_ VkPresentInfoKHR info{
					.sType = VK_ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.waitSemaphoreCount = value.wait_count,
					.pWaitSemaphores = columns.waits.data() + value.wait_first,
					.swapchainCount = value.swapchain_count,
					.pSwapchains = columns.swapchains.data() + value.swapchain_first,
					.pImageIndices = columns.image_indices.data() + value.swapchain_first,
					.pResults = columns.result_scratch.data(),
				};
				auto result = VK_ vkQueuePresentKHR(queue, &info);
				report(result);
				for (uint32_t swapchain = 0u; swapchain < value.swapchain_count; ++swapchain) {
					report(columns.result_scratch[swapchain]);
				}
				if (result != VK_ VK_SUCCESS
					&& result != VK_ VK_SUBOPTIMAL_KHR
					&& result != VK_ VK_ERROR_OUT_OF_DATE_KHR) {
					result | popup{ "[EXECUTION] Queue presentation failed." };
				}
			}
		}

	private:
		static void report(VK_ VkResult result) noexcept {
			if (result == VK_ VK_ERROR_OUT_OF_DATE_KHR) {
				global_present_status().out_of_date.fetch_add(1u, ::std::memory_order_relaxed);
			}
			else if (result == VK_ VK_SUBOPTIMAL_KHR) {
				global_present_status().suboptimal.fetch_add(1u, ::std::memory_order_relaxed);
			}
		}
	};
#endif // VKTL_HAVE_WINDOW

	// -----------------------------------------------------------------------
	// Built-in payload: sparse binding
	// -----------------------------------------------------------------------

	struct compiled_sparse_bind {
		uint32_t bind_first = 0u;
		uint32_t bind_count = 0u;
		uint32_t wait_first = 0u;
		uint32_t wait_count = 0u;
		uint32_t signal_first = 0u;
		uint32_t signal_count = 0u;
	};

	struct sparse_bind_columns {
		vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
		vector<VK_ VkSemaphore> semaphores;
		uint32_t bind_cursor = 0u;
		uint32_t semaphore_cursor = 0u;

		void rewind() noexcept { bind_cursor = 0u; semaphore_cursor = 0u; }
	};

	struct sparse_bind_payload;

	template<>
	struct trait<sparse_bind_payload> {
		using type = sparse_bind_payload;
		using storage_type = compiled_sparse_bind;
		using column_type = sparse_bind_columns;

		static constexpr payload_type::type payload_id = payload_type::sparse_bind;
		static constexpr queue_duty::type required_duty = queue_duty::bind_sparse;

		static uint32_t storage_count(submit_capacity const& capacity) noexcept {
			return capacity.sparse_binds;
		}

		static void reserve(column_type& columns, submit_capacity const& capacity) {
			columns.buffer_binds.resize(capacity.sparse_buffer_binds);
			columns.semaphores.resize(capacity.sparse_semaphores);
			columns.rewind();
		}

		static uint32_t queue_index(sparse_bind_payload const& declaration) noexcept;
		static payload_requirement count(sparse_bind_payload const& declaration);
		static void materialize(sparse_bind_payload const& declaration, payload_context const& context);

		static void invoke(VK_ VkQueue queue, queue_payload_pool_base& base,
			uint32_t first, uint32_t count) {
			auto& pool = static_cast<queue_payload_pool<trait>&>(base);
			auto& columns = pool.columns;
			for (uint32_t index = 0u; index < count; ++index) {
				auto const& value = pool.values[first + index];
				VK_ VkBindSparseInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
					.waitSemaphoreCount = value.wait_count,
					.pWaitSemaphores = columns.semaphores.data() + value.wait_first,
					.bufferBindCount = value.bind_count,
					.pBufferBinds = columns.buffer_binds.data() + value.bind_first,
					.signalSemaphoreCount = value.signal_count,
					.pSignalSemaphores = columns.semaphores.data() + value.signal_first,
				};
				VK_ vkQueueBindSparse(queue, 1u, &info, VK_NULL_HANDLE)
					| popup{ "[EXECUTION] Sparse queue binding failed." };
			}
		}
	};

	// -----------------------------------------------------------------------
	// Payload declarations
	// -----------------------------------------------------------------------

	struct semaphore_operand {
		box<vptr::queue_semaphore> object;
		uint64_t value = 0u;
		queue_stage_flags stage = queue_all_commands_stage;
	};

	struct submit_payload {
		uint32_t queue = uint32_t(invalid);
		vector<uint32_t> commands;   // indices into `task_plan::commands`
		vector<semaphore_operand> waits;
		vector<semaphore_operand> signals;

		bool empty() const noexcept {
			return commands.empty() && waits.empty() && signals.empty();
		}
	};

#if VKTL_HAVE_WINDOW
	struct present_payload {
		uint32_t queue = uint32_t(invalid);
		vector<box<vptr::presentable>> swapchains;
		vector<box<vptr::queue_semaphore>> waits;

		bool empty() const noexcept { return swapchains.empty(); }
	};
#endif

	struct sparse_bind_payload {
		uint32_t queue = uint32_t(invalid);
		vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
		vector<box<vptr::queue_semaphore>> waits;
		vector<box<vptr::queue_semaphore>> signals;

		bool empty() const noexcept { return buffer_binds.empty(); }
	};

	// -----------------------------------------------------------------------
	// Task plan
	// -----------------------------------------------------------------------

	struct task_plan {
		uint64_t key = 0u;              // command-pool ownership key
		uint64_t generation = 0u;
		uint64_t last_epoch = invalid;  // invalid == never submitted
		vector<command_unit> commands;
		vector<payload_ref> payloads;   // heterogeneous, submission order
		vector<payload_requirement> requirements;
		recipe_arena arena;
	};

	// -----------------------------------------------------------------------
	// Payload trait implementations
	// -----------------------------------------------------------------------

	inline uint32_t trait<submit_payload>::queue_index(submit_payload const& declaration) noexcept {
		return declaration.queue;
	}

	inline payload_requirement trait<submit_payload>::count(submit_payload const& declaration) {
		auto commands = uint32_t(declaration.commands.size());
		auto waits = uint32_t(declaration.waits.size());
		auto signals = uint32_t(declaration.signals.size());
		payload_requirement requirement{
			.type = payload_id,
			.queue_index = declaration.queue,
			.required_duty = required_duty,
			.create = payload_pool_factory_v<trait<submit_payload>>,
		};
		requirement.capacity.operations = 1u;
		requirement.capacity.submits = 1u;
		requirement.capacity.command_buffers = commands;
		requirement.capacity.semaphores = waits + signals;
		requirement.capacity.peak_command_buffers = commands;
		requirement.capacity.peak_semaphores = waits + signals;
		return requirement;
	}

	inline void trait<submit_payload>::materialize(submit_payload const& declaration,
		payload_context const& context) {
		auto& pool = context.writer.pool<trait<submit_payload>>(declaration.queue);
		auto& columns = pool.columns;
		uint32_t index = 0u;
		auto& value = pool.claim(index);

		value.command_first = columns.command_cursor;
		value.command_count = uint32_t(declaration.commands.size());
		for (auto command : declaration.commands) {
			assert(columns.command_cursor < columns.command_buffers.size());
			auto handle = current_variant(context.plan->commands[command]).handle;
			assert(handle != VK_NULL_HANDLE);
			columns.command_buffers[columns.command_cursor++] = handle;
		}

		auto write = [&](vector<semaphore_operand> const& operands,
			uint32_t& first, uint32_t& count) {
			first = columns.semaphore_cursor;
			count = uint32_t(operands.size());
			for (auto const& operand : operands) {
				assert(columns.semaphore_cursor < columns.semaphores.size());
				columns.semaphores[columns.semaphore_cursor++] = submit_semaphore_value{
					.handle = operand.object.handle(),
					.value = operand.value,
					.stage = operand.stage,
				};
			}
		};
		write(declaration.waits, value.wait_first, value.wait_count);
		write(declaration.signals, value.signal_first, value.signal_count);

		context.writer.append(declaration.queue, queue_operation{
			.invoke = &trait<submit_payload>::invoke,
			.pool = ::std::addressof(pool),
			.first = index,
			.count = 1u,
		});
	}

#if VKTL_HAVE_WINDOW
	inline uint32_t trait<present_payload>::queue_index(present_payload const& declaration) noexcept {
		return declaration.queue;
	}

	inline payload_requirement trait<present_payload>::count(present_payload const& declaration) {
		auto swapchains = uint32_t(declaration.swapchains.size());
		auto waits = uint32_t(declaration.waits.size());
		payload_requirement requirement{
			.type = payload_id,
			.queue_index = declaration.queue,
			.required_duty = required_duty,
			.create = payload_pool_factory_v<trait<present_payload>>,
		};
		requirement.capacity.operations = 1u;
		requirement.capacity.presents = 1u;
		requirement.capacity.present_swapchains = swapchains;
		requirement.capacity.present_waits = waits;
		requirement.capacity.peak_present_swapchains = swapchains;
		requirement.capacity.peak_present_waits = waits;
		return requirement;
	}

	inline void trait<present_payload>::materialize(present_payload const& declaration,
		payload_context const& context) {
		// Presentation references no recorded command buffer, so the plan is not
		// consulted here; only the writer is used.
		auto& pool = context.writer.pool<trait<present_payload>>(declaration.queue);
		auto& columns = pool.columns;
		uint32_t index = 0u;
		auto& value = pool.claim(index);

		value.wait_first = columns.wait_cursor;
		value.wait_count = uint32_t(declaration.waits.size());
		for (auto const& wait : declaration.waits) {
			assert(columns.wait_cursor < columns.waits.size());
			columns.waits[columns.wait_cursor++] = wait.handle();
		}

		value.swapchain_first = columns.swapchain_cursor;
		value.swapchain_count = uint32_t(declaration.swapchains.size());
		for (auto const& swapchain : declaration.swapchains) {
			assert(columns.swapchain_cursor < columns.swapchains.size());
			columns.swapchains[columns.swapchain_cursor] = swapchain.handle();
			columns.image_indices[columns.swapchain_cursor] = swapchain.frame_index();
			++columns.swapchain_cursor;
		}

		context.writer.append(declaration.queue, queue_operation{
			.invoke = &trait<present_payload>::invoke,
			.pool = ::std::addressof(pool),
			.first = index,
			.count = 1u,
		});
	}
#endif

	inline uint32_t trait<sparse_bind_payload>::queue_index(sparse_bind_payload const& declaration) noexcept {
		return declaration.queue;
	}

	inline payload_requirement trait<sparse_bind_payload>::count(sparse_bind_payload const& declaration) {
		auto binds = uint32_t(declaration.buffer_binds.size());
		auto waits = uint32_t(declaration.waits.size());
		auto signals = uint32_t(declaration.signals.size());
		payload_requirement requirement{
			.type = payload_id,
			.queue_index = declaration.queue,
			.required_duty = required_duty,
			.create = payload_pool_factory_v<trait<sparse_bind_payload>>,
		};
		requirement.capacity.operations = 1u;
		requirement.capacity.sparse_binds = 1u;
		requirement.capacity.sparse_buffer_binds = binds;
		requirement.capacity.sparse_semaphores = waits + signals;
		requirement.capacity.peak_sparse_buffer_binds = binds;
		requirement.capacity.peak_sparse_semaphores = waits + signals;
		return requirement;
	}

	inline void trait<sparse_bind_payload>::materialize(sparse_bind_payload const& declaration,
		payload_context const& context) {
		auto& pool = context.writer.pool<trait<sparse_bind_payload>>(declaration.queue);
		auto& columns = pool.columns;
		uint32_t index = 0u;
		auto& value = pool.claim(index);

		value.bind_first = columns.bind_cursor;
		value.bind_count = uint32_t(declaration.buffer_binds.size());
		for (auto const& bind : declaration.buffer_binds) {
			assert(columns.bind_cursor < columns.buffer_binds.size());
			columns.buffer_binds[columns.bind_cursor++] = bind;
		}

		auto write = [&](vector<box<vptr::queue_semaphore>> const& operands,
			uint32_t& first, uint32_t& count) {
			first = columns.semaphore_cursor;
			count = uint32_t(operands.size());
			for (auto const& operand : operands) {
				assert(columns.semaphore_cursor < columns.semaphores.size());
				columns.semaphores[columns.semaphore_cursor++] = operand.handle();
			}
		};
		write(declaration.waits, value.wait_first, value.wait_count);
		write(declaration.signals, value.signal_first, value.signal_count);

		context.writer.append(declaration.queue, queue_operation{
			.invoke = &trait<sparse_bind_payload>::invoke,
			.pool = ::std::addressof(pool),
			.first = index,
			.count = 1u,
		});
	}

	// -----------------------------------------------------------------------
	// Recording jobs and worker slots
	// -----------------------------------------------------------------------

	struct record_job_group {
		void add() noexcept { remaining.fetch_add(1u, ::std::memory_order_relaxed); }

		void finish(::std::exception_ptr failure = {}) noexcept {
			if (failure) {
				::std::lock_guard lock{ mutex };
				if (!error) error = failure;
			}
			if (remaining.fetch_sub(1u, ::std::memory_order_acq_rel) == 1u) {
				::std::lock_guard lock{ mutex };
				cv.notify_all();
			}
		}

		void wait() {
			::std::unique_lock lock{ mutex };
			cv.wait(lock, [&] { return remaining.load(::std::memory_order_acquire) == 0u; });
			if (error) ::std::rethrow_exception(::std::exchange(error, {}));
		}

		::std::atomic<uint32_t> remaining = 0u;
		::std::mutex mutex;
		::std::condition_variable cv;
		::std::exception_ptr error;
	};

	struct record_job {
		using invoke_type = void(*)(void*);

		invoke_type invoke = nullptr;
		void* data = nullptr;
		record_job_group* group = nullptr;
	};

	// A command pool belongs to exactly one worker and one plan key. Only the
	// owning worker records into it; the controller thread may reset it after the
	// plan's retirement epoch has completed.
	struct command_pool_slot {
		uint32_t family = uint32_t(invalid);
		uint64_t plan_key = 0u;   // 0 == free for reuse
		VK_ VkCommandPool handle = VK_NULL_HANDLE;
	};

	struct execution_worker_slot {
		::std::jthread thread;
		::std::mutex mutex;
		::std::condition_variable_any cv;
		::std::deque<record_job> jobs;

		::std::mutex pool_mutex;
		vector<command_pool_slot> command_pools;
	};

	struct execution_queue_slot {
		queue_declaration declaration{};
		VK_ VkQueue handle = VK_NULL_HANDLE;
		VK_ VkQueueFamilyProperties properties{};

		::std::mutex submit_lock;
		vector<queue_operation> operations;
		uint32_t operation_cursor = 0u;

		poly_list payload_pools;
		vector<queue_payload_pool_base*> payload_index;

		// One fence per in-flight slot. `pending[slot]` is true between the
		// fenced submission and the wait which consumes it.
		vector<VK_ VkFence> fences;
		vector<uint8_t> pending;
	};

	struct execution_task_slot {
		uint64_t id = 0u;
		::std::shared_ptr<task_plan> active_plan;
		vector<::std::shared_ptr<task_plan>> retired_plans;
		uint64_t submitted_epoch = uint64_t(invalid);
		bool attached = true;   // false once the task detached
		bool active = true;     // false for an explicit inactive submission
	};

#if VKTL_HAVE_WINDOW
	struct present_support_entry {
		uint32_t family = uint32_t(invalid);
		VK_ VkSurfaceKHR surface = VK_NULL_HANDLE;
		bool supported = false;
	};
#endif

	// -----------------------------------------------------------------------
	// execution component
	// -----------------------------------------------------------------------

	template<typename N>
	struct m<execution, N> : N {
		using base = N;

		static_assert(!object_of<N, lockable_>, "execution owns focused locks and does not use the composition lock.");

		static constexpr uint32_t default_frames_in_flight = 2u;

		constexpr m(execution const& info, auto&&...others)
			: base{ forward_(others)... }
			, thread_count_{ info.thread_count } {
			if (thread_count_ == 0u) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] thread_count must be greater than zero." };
			}
		}

		~m() { reset(); }

		// --- declaration phase ---------------------------------------------

		void append(vktl::queue declaration) {
			assert(queues_.empty() && "queue declarations must precede initialization");
			queue_declarations_.emplace_back(queue_declaration{ declaration, queue_duty::none });
		}

		void append_queue_duty(queue_duty::type duty) {
			assert(queue_declarations_.size()); // queue must at front of any other queue_extensions::.
			queue_declarations_.back().duty |= duty;
		}

		void require_synchronization2() noexcept { require_synchronization2_ = true; }

		void frames_in_flight(uint32_t count) {
			assert(queues_.empty()); // frames_in_flight must be selected before initialization.
			assert(count); // Not allow zero.
			frames_in_flight_ = count;
		}

		uint32_t frames_in_flight() const noexcept { return frames_in_flight_; }
		uint32_t thread_count() const noexcept { return thread_count_; }
		queue_dispatch_config const& dispatch_config() const noexcept { return dispatch_; }

		// --- lifecycle ------------------------------------------------------

		void init() {
			N::init();
			if (!workers_.empty()) return;
			if (queue_declarations_.empty()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] At least one queue must be declared." };
			}

			auto device = handle_of<vktl::device>(this);
			auto physical_device = parent_of<vktl::device>(this)->physical_device();

			resolve_dispatch(device);

			vector<VK_ VkQueueFamilyProperties> families;
			invoke(families, VK_ vkGetPhysicalDeviceQueueFamilyProperties, physical_device);
			for (auto const& declaration : queue_declarations_) {
				assert(declaration.duty != queue_duty::none); // Every declared queue needs at least one duty.
				assert(parent_of<device>(this)->contain_queue(declaration.family, declaration.index));

				if (declaration.value.family >= family_count
					|| declaration.value.index >= families[declaration.value.family].queueCount) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Queue family or queue index is out of range." };
				}
				validate_queue_duties(declaration, families[declaration.value.family]);
				if (::std::ranges::any_of(queues_, [&](auto const& existing) {
					return existing.declaration.value.family == declaration.value.family
						&& existing.declaration.value.index == declaration.value.index;
				})) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] The same Vulkan queue cannot be declared twice." };
				}

				auto& slot = queues_.emplace_back();
				slot.declaration = declaration;
				slot.properties = families[declaration.value.family];
				VK_ vkGetDeviceQueue(device, declaration.value.family,
					declaration.value.index, &slot.handle);
				if (!slot.handle) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] vkGetDeviceQueue returned a null queue." };
				}

				slot.fences.resize(frames_in_flight_, VK_NULL_HANDLE);
				slot.pending.resize(frames_in_flight_, 0u);
				VK_ VkFenceCreateInfo fence_info{ .sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
				for (auto& fence : slot.fences) {
					VK_ vkCreateFence(device, &fence_info, N::allocator(), &fence)
						| popup{ "[EXECUTION] Failed to create a completion fence." };
				}
			}

			for (uint32_t worker_index = 0u; worker_index < thread_count_; ++worker_index) {
				auto& worker = workers_.emplace_back();
				auto* worker_ptr = ::std::addressof(worker);
				worker.thread = ::std::jthread([worker_ptr](::std::stop_token stop) {
					worker_main(*worker_ptr, stop);
				});
			}
		}

		void reset() noexcept {
			if (queues_.empty() && workers_.empty()) return;

			for (auto& worker : workers_) {
				worker.thread.request_stop();
				worker.cv.notify_all();
			}
			for (auto& worker : workers_) {
				if (worker.thread.joinable()) worker.thread.join();
			}

			for (auto& queue : queues_) {
				if (queue.handle) {
					::std::lock_guard queue_lock{ queue.submit_lock };
					VK_ vkQueueWaitIdle(queue.handle);
				}
			}

			// The device parent outlives this component: `reset()` runs from the
			// execution layer's destructor, which is above `device` in the chain.
			auto device = handle_of<vktl::device>(this);

			for (auto& worker : workers_) {
				for (auto& pool : worker.command_pools) {
					if (pool.handle) VK_ vkDestroyCommandPool(device, pool.handle, N::allocator());
				}
				worker.command_pools.clear();
			}
			for (auto& queue : queues_) {
				for (auto fence : queue.fences) {
					if (fence) VK_ vkDestroyFence(device, fence, N::allocator());
				}
				queue.fences.clear();
				queue.pending.clear();
			}

			workers_.clear();
			tasks_.clear();
			queues_.clear();
#if VKTL_HAVE_WINDOW
			present_support_.clear();
#endif
		}

		// --- queue routing --------------------------------------------------

		uint32_t resolve_queue(queue_duty::type duty) const {
			for (uint32_t index = 0u; index < uint32_t(queues_.size()); ++index) {
				if ((queues_[index].declaration.duty & duty) == duty) return index;
			}
			throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
				"[EXECUTION] No declared queue satisfies the requested duties." };
		}

		uint32_t queue_family(uint32_t queue_index) const noexcept {
			assert(queue_index < queues_.size());
			return queues_[queue_index].declaration.value.family;
		}

		queue_duty::type queue_duties(uint32_t queue_index) const noexcept {
			assert(queue_index < queues_.size());
			return queues_[queue_index].declaration.duty;
		}

#if VKTL_HAVE_WINDOW
		// Presentation support is surface specific, so routing must consider the
		// surface instead of the duty bit alone. Results are cached per
		// (family, surface) pair and never re-queried on the frame path.
		uint32_t resolve_present_queue(VK_ VkSurfaceKHR surface) {
			for (uint32_t index = 0u; index < uint32_t(queues_.size()); ++index) {
				if ((queues_[index].declaration.duty & queue_duty::present) == 0u) continue;
				if (query_present_support(index, surface)) return index;
			}
			throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
				"[EXECUTION] No declared present queue supports this surface." };
		}

		void validate_present(uint32_t queue_index, VK_ VkSurfaceKHR surface) {
			assert(queue_index < queues_.size());
			if ((queues_[queue_index].declaration.duty & queue_duty::present) == 0u) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] The selected queue was not declared with a present duty." };
			}
			if (!query_present_support(queue_index, surface)) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] Queue family does not support presentation to this surface." };
			}
		}
#endif

		// --- task registry --------------------------------------------------

		uint64_t attach_task() {
			::std::lock_guard lock{ task_mutex_ };
			auto& slot = tasks_.emplace_back();
			slot.id = next_task_id_++;
			slot.submitted_epoch = uint64_t(invalid);
			return slot.id;
		}

		// Detach retires GPU visible state instead of freeing it immediately.
		void detach(uint64_t task_id) noexcept {
			::std::lock_guard lock{ task_mutex_ };
			auto* slot = find_task(task_id);
			if (!slot) return;
			slot->attached = false;
			slot->active = false;
			if (slot->active_plan) slot->retired_plans.emplace_back(::std::move(slot->active_plan));
		}

		uint64_t acquire_plan_key() {
			::std::lock_guard lock{ task_mutex_ };
			return next_plan_key_++;
		}

		// Releases the command pools a plan owned. Safe only once the plan can no
		// longer be referenced by in-flight GPU work.
		void release_plan_key(uint64_t plan_key) noexcept {
			if (plan_key == 0u || workers_.empty()) return;
			auto device = handle_of<vktl::device>(this);
			for (auto& worker : workers_) {
				::std::lock_guard lock{ worker.pool_mutex };
				for (auto& pool : worker.command_pools) {
					if (pool.plan_key != plan_key) continue;
					VK_ vkResetCommandPool(device, pool.handle,
						VK_ VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
					pool.plan_key = 0u;
				}
			}
		}

		void publish(uint64_t task_id, ::std::shared_ptr<task_plan> plan) {
			::std::lock_guard lock{ task_mutex_ };
			auto epoch = current_epoch();
			for (auto const& task : tasks_) {
				if (task.attached && task.submitted_epoch == epoch) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Tasks cannot refresh after submission materialization has begun." };
				}
			}
			auto* slot = find_task(task_id);
			if (!slot || !slot->attached) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] Cannot publish a plan for a detached task." };
			}
			if (slot->active_plan) slot->retired_plans.emplace_back(::std::move(slot->active_plan));
			slot->active_plan = ::std::move(plan);
			slot->active = true;
			resize_operation_storage();
		}

		::std::shared_ptr<task_plan> active_plan(uint64_t task_id) const {
			::std::lock_guard lock{ task_mutex_ };
			auto const* slot = find_task(task_id);
			return slot ? slot->active_plan : nullptr;
		}

		uint64_t current_epoch() const noexcept {
			return epoch_.load(::std::memory_order_acquire);
		}

		// --- submission filling ---------------------------------------------

		// `task.submit()` fills execution-owned pools through cursors. The lock is
		// a focused boundary around cursor arithmetic; it allocates nothing.
		[[nodiscard]] ::std::unique_lock<::std::mutex> begin_fill() {
			return ::std::unique_lock<::std::mutex>{ fill_mutex_ };
		}

		void mark_submitted(uint64_t task_id, bool active, task_plan* plan) {
			::std::lock_guard lock{ task_mutex_ };
			auto* slot = find_task(task_id);
			if (!slot || !slot->attached) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] Cannot submit a detached task." };
			}
			auto epoch = current_epoch();
			if (slot->submitted_epoch == epoch) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] A task may submit at most once per execution epoch." };
			}
			slot->submitted_epoch = epoch;
			slot->active = active;
			if (active && plan) plan->last_epoch = epoch;
		}

		queue_payload_writer make_payload_writer() noexcept {
			return queue_payload_writer{
				.execution = this,
				.pool_ = [](void* self, uint32_t queue_index, payload_type::type type) noexcept
					-> queue_payload_pool_base& {
					return static_cast<m*>(self)->pool_of(queue_index, type);
				},
				.append_ = [](void* self, uint32_t queue_index, queue_operation operation) noexcept {
					static_cast<m*>(self)->append_operation(queue_index, operation);
				},
			};
		}

		queue_payload_pool_base& pool_of(uint32_t queue_index, payload_type::type type) noexcept {
			assert(queue_index < queues_.size());
			auto& queue = queues_[queue_index];
			for (auto* pool : queue.payload_index) {
				if (pool->type == type) return *pool;
			}
			assert(false && "payload pool was not registered during refresh");
			// Unreachable in a correctly refreshed plan; returning the first pool
			// keeps the contract total instead of forming a null reference.
			return *queue.payload_index.front();
		}

		// Adjacent operations sharing a trait and pool are coalesced into one
		// contiguous batch, which is why `count` is not always one.
		void append_operation(uint32_t queue_index, queue_operation operation) noexcept {
			assert(queue_index < queues_.size());
			auto& queue = queues_[queue_index];
			if (queue.operation_cursor != 0u) {
				auto& previous = queue.operations[queue.operation_cursor - 1u];
				if (previous.invoke == operation.invoke && previous.pool == operation.pool
					&& previous.first + previous.count == operation.first) {
					previous.count += operation.count;
					return;
				}
			}
			assert(queue.operation_cursor < queue.operations.size());
			queue.operations[queue.operation_cursor++] = operation;
		}

		// --- execution submission -------------------------------------------

		void submit() {
			auto epoch = current_epoch();
			auto slot = uint32_t(epoch % frames_in_flight_);

			verify_participation(epoch);
			wait_for_slot(slot);

			// Whatever happens below, the epoch must advance and every cursor must
			// return to zero; otherwise a single failed submission would wedge the
			// state machine permanently.
			auto guard = defer{ [&] {
				for (auto& queue : queues_) {
					queue.operation_cursor = 0u;
					for (auto* pool : queue.payload_index) pool->rewind();
				}
				epoch_.fetch_add(1u, ::std::memory_order_release);
				finish_epoch(epoch);
			} };

			for (auto& queue : queues_) {
				if (queue.operation_cursor == 0u) continue;
				::std::lock_guard queue_lock{ queue.submit_lock };
				for (uint32_t index = 0u; index < queue.operation_cursor; ++index) {
					auto const& operation = queue.operations[index];
					assert(operation.invoke && operation.pool && operation.count);
					operation.invoke(queue.handle, *operation.pool, operation.first, operation.count);
				}
				// One empty fenced submission closes the queue for this epoch. It
				// costs a single submit, works without any extension, and defines
				// the completion point used for retirement and host throttling.
				VK_ VkSubmitInfo close{ .sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO };
				VK_ vkQueueSubmit(queue.handle, 1u, &close, queue.fences[slot])
					| popup{ "[EXECUTION] Failed to close the epoch on a queue." };
				queue.pending[slot] = 1u;
			}
		}

		// --- worker services ------------------------------------------------

		void enqueue(uint32_t worker_index, record_job job) {
			assert(worker_index < workers_.size() && job.invoke && job.group);
			job.group->add();
			auto& worker = workers_[worker_index];
			{
				::std::lock_guard lock{ worker.mutex };
				worker.jobs.emplace_back(job);
			}
			worker.cv.notify_one();
		}

		VK_ VkCommandBuffer allocate_command_buffer(uint32_t worker_index,
			uint32_t family, uint64_t plan_key, VK_ VkCommandBufferLevel level) {
			assert(worker_index < workers_.size());
			auto& worker = workers_[worker_index];
			auto pool = find_or_create_pool(worker, family, plan_key);
			VK_ VkCommandBufferAllocateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = pool,
				.level = level,
				.commandBufferCount = 1u,
			};
			VK_ VkCommandBuffer result = VK_NULL_HANDLE;
			VK_ vkAllocateCommandBuffers(handle_of<vktl::device>(this), &info, &result)
				| popup{ "[EXECUTION] Failed to allocate a command buffer." };
			return result;
		}

	private:
		// --- worker loop ----------------------------------------------------

		static void worker_main(execution_worker_slot& worker, ::std::stop_token stop) noexcept {
			for (;;) {
				record_job job;
				{
					::std::unique_lock lock{ worker.mutex };
					worker.cv.wait(lock, stop, [&] { return !worker.jobs.empty(); });
					if (worker.jobs.empty()) {
						// Woken only by the stop request and nothing left to do.
						if (stop.stop_requested()) return;
						continue;
					}
					job = worker.jobs.front();
					worker.jobs.pop_front();
				}

				if (stop.stop_requested()) {
					// Stopping must never strand a waiter: every accepted job is
					// completed exactly once, with a failure if it cannot run.
					job.group->finish(::std::make_exception_ptr(error{
						int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Recording was cancelled because execution is shutting down." }));
					continue;
				}

				try {
					job.invoke(job.data);
					job.group->finish();
				}
				catch (...) {
					job.group->finish(::std::current_exception());
				}
			}
		}

		// --- initialization helpers -----------------------------------------

		void resolve_dispatch(VK_ VkDevice device) {
			dispatch_ = {};
#if defined(VK_KHR_synchronization2)
			// A compiled-in header macro only proves the SDK knows the extension.
			// Whether the device enabled it is a runtime question.
			dispatch_.synchronization2 =
				VK_ vkGetDeviceProcAddr(device, "vkQueueSubmit2KHR") != nullptr
				|| VK_ vkGetDeviceProcAddr(device, "vkQueueSubmit2") != nullptr;
#endif
#if defined(VK_KHR_timeline_semaphore)
			dispatch_.timeline_semaphore =
				VK_ vkGetDeviceProcAddr(device, "vkWaitSemaphoresKHR") != nullptr
				|| VK_ vkGetDeviceProcAddr(device, "vkWaitSemaphores") != nullptr;
#endif
			if (require_synchronization2_ && !dispatch_.synchronization2) {
				throw error{ int(VK_ VK_ERROR_EXTENSION_NOT_PRESENT),
					"[EXECUTION] VK_KHR_synchronization2 was requested but is not enabled on the device." };
			}
		}

		void validate_queue_duties(queue_declaration const& declaration,
			VK_ VkQueueFamilyProperties const& properties) const {
			auto require = [&](queue_duty::type duty, VK_ VkQueueFlags flag, const char* message) {
				if ((declaration.duty & duty) && !(properties.queueFlags & flag)) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED), message };
				}
			};
			require(queue_duty::graphics, VK_ VK_QUEUE_GRAPHICS_BIT,
				"[EXECUTION] Queue family does not support graphics.");
			require(queue_duty::compute, VK_ VK_QUEUE_COMPUTE_BIT,
				"[EXECUTION] Queue family does not support compute.");
			require(queue_duty::transfer, VK_ VK_QUEUE_TRANSFER_BIT,
				"[EXECUTION] Queue family does not support transfer.");
			require(queue_duty::bind_sparse, VK_ VK_QUEUE_SPARSE_BINDING_BIT,
				"[EXECUTION] Queue family does not support sparse binding.");
		}

#if VKTL_HAVE_WINDOW
		bool query_present_support(uint32_t queue_index, VK_ VkSurfaceKHR surface) {
			auto const family = queues_[queue_index].declaration.value.family;
			for (auto const& cached : present_support_) {
				if (cached.family == family && cached.surface == surface) return cached.supported;
			}
			VK_ VkBool32 supported = VK_FALSE;
			VK_ vkGetPhysicalDeviceSurfaceSupportKHR(
				parent_of<vktl::device>(this)->physical_device(), family, surface, &supported)
				| popup{ "[EXECUTION] Failed to query queue present support." };
			present_support_.emplace_back(present_support_entry{ family, surface, supported == VK_TRUE });
			return supported == VK_TRUE;
		}
#endif

		// --- task bookkeeping -----------------------------------------------

		execution_task_slot* find_task(uint64_t id) noexcept {
			for (auto& task : tasks_) if (task.id == id) return ::std::addressof(task);
			return nullptr;
		}

		execution_task_slot const* find_task(uint64_t id) const noexcept {
			for (auto const& task : tasks_) if (task.id == id) return ::std::addressof(task);
			return nullptr;
		}

		void verify_participation(uint64_t epoch) const {
			::std::lock_guard lock{ task_mutex_ };
			for (auto const& task : tasks_) {
				if (!task.attached || !task.active_plan) continue;
				if (task.submitted_epoch != epoch) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Every active task must submit exactly once per epoch." };
				}
			}
		}

		void wait_for_slot(uint32_t slot) {
			auto device = handle_of<vktl::device>(this);
			for (auto& queue : queues_) {
				if (!queue.pending[slot]) continue;
				VK_ vkWaitForFences(device, 1u, &queue.fences[slot], VK_TRUE, uint64_t(maximum))
					| popup{ "[EXECUTION] Waiting for a completion fence failed." };
				VK_ vkResetFences(device, 1u, &queue.fences[slot])
					| popup{ "[EXECUTION] Resetting a completion fence failed." };
				queue.pending[slot] = 0u;
			}
		}

		// Everything submitted at or before `epoch - frames_in_flight` has now
		// completed on every queue, so its plans and pools can be recycled.
		void finish_epoch(uint64_t epoch) noexcept {
			if (epoch + 1u < frames_in_flight_) return;
			auto completed = epoch + 1u - frames_in_flight_;

			::std::lock_guard lock{ task_mutex_ };
			for (auto it = tasks_.begin(); it != tasks_.end();) {
				auto& task = *it;
				for (auto plan = task.retired_plans.begin(); plan != task.retired_plans.end();) {
					auto last = (*plan)->last_epoch;
					if (last == uint64_t(invalid) || last < completed) {
						release_plan_key((*plan)->key);
						plan = task.retired_plans.erase(plan);
					}
					else {
						++plan;
					}
				}
				if (!task.attached && !task.active_plan && task.retired_plans.empty()) {
					it = tasks_.erase(it);
				}
				else {
					++it;
				}
			}
		}

		// --- storage sizing --------------------------------------------------

		void resize_operation_storage() {
			struct queue_total {
				uint32_t operations = 0u;
				vector<payload_requirement> payloads;
			};
			vector<queue_total> totals(queues_.size());

			for (auto const& task : tasks_) {
				if (!task.attached || !task.active_plan) continue;
				for (auto const& requirement : task.active_plan->requirements) {
					assert(requirement.queue_index < totals.size());
					auto& total = totals[requirement.queue_index];
					total.operations += requirement.capacity.operations;
					auto found = ::std::ranges::find_if(total.payloads,
						[&](payload_requirement const& value) { return value.type == requirement.type; });
					if (found == total.payloads.end()) {
						total.payloads.emplace_back(requirement);
					}
					else {
						found->capacity += requirement.capacity;
					}
				}
			}

			for (uint32_t index = 0u; index < uint32_t(queues_.size()); ++index) {
				auto& slot = queues_[index];
				auto const& total = totals[index];
				if (slot.operations.size() < total.operations) slot.operations.resize(total.operations);
				for (auto const& requirement : total.payloads) {
					auto found = ::std::ranges::find_if(slot.payload_index,
						[&](auto* pool) { return pool->type == requirement.type; });
					queue_payload_pool_base* pool = nullptr;
					if (found == slot.payload_index.end()) {
						pool = ::std::addressof(requirement.create(slot.payload_pools));
						pool->dispatch = dispatch_;
						slot.payload_index.emplace_back(pool);
					}
					else {
						pool = *found;
					}
					// Only re-reserve when the existing storage cannot hold the new
					// requirement; refresh is not obliged to rebuild every column.
					if (!requirement.capacity.fits_in(pool->capacity)) {
						auto grown = pool->capacity;
						grown.grow(requirement.capacity);
						pool->reserve(grown);
					}
					else {
						pool->rewind();
					}
				}
			}
		}

		VK_ VkCommandPool find_or_create_pool(execution_worker_slot& worker,
			uint32_t family, uint64_t plan_key) {
			{
				::std::lock_guard lock{ worker.pool_mutex };
				for (auto& pool : worker.command_pools) {
					if (pool.family == family && pool.plan_key == plan_key) return pool.handle;
				}
				// Reuse a pool whose plan has already been retired instead of
				// creating a new one for every refresh generation.
				for (auto& pool : worker.command_pools) {
					if (pool.family == family && pool.plan_key == 0u) {
						pool.plan_key = plan_key;
						return pool.handle;
					}
				}
			}

			VK_ VkCommandPoolCreateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = family,
			};
			VK_ VkCommandPool pool = VK_NULL_HANDLE;
			VK_ vkCreateCommandPool(handle_of<vktl::device>(this), &info, N::allocator(), &pool)
				| popup{ "[EXECUTION] Failed to create a command pool." };

			::std::lock_guard lock{ worker.pool_mutex };
			worker.command_pools.emplace_back(command_pool_slot{ family, plan_key, pool });
			return pool;
		}

	private:
		uint32_t thread_count_ = 0u;
		uint32_t frames_in_flight_ = default_frames_in_flight;
		bool require_synchronization2_ = false;
		queue_dispatch_config dispatch_{};

		vector<queue_declaration> queue_declarations_;

		::std::deque<execution_queue_slot> queues_;
		::std::deque<execution_worker_slot> workers_;
		::std::list<execution_task_slot> tasks_;

#if VKTL_HAVE_WINDOW
		vector<present_support_entry> present_support_;
#endif
		mutable ::std::mutex task_mutex_;
		::std::mutex fill_mutex_;

		::std::atomic<uint64_t> epoch_ = 0u;
		uint64_t next_task_id_ = 1u;
		uint64_t next_plan_key_ = 1u;
	};

	// -----------------------------------------------------------------------
	// Expressed declarations
	// -----------------------------------------------------------------------

	template<>
	struct express<vktl::queue> {
		static void invoke(vktl::queue declaration, auto& object) {
			object.append(declaration);
		}
	};

	template<queue_duty::type Duty>
	struct basic_queue_duty_express {
		static void invoke(auto, auto& object) {
			object.append_queue_duty(Duty);
		}
	};

	template<> struct express<vktl::queue_extensions::graphics_>
		: basic_queue_duty_express<queue_duty::graphics> {};
	template<> struct express<vktl::queue_extensions::compute_>
		: basic_queue_duty_express<queue_duty::compute> {};
	template<> struct express<vktl::queue_extensions::transfer_>
		: basic_queue_duty_express<queue_duty::transfer> {};
#if VKTL_HAVE_WINDOW
	template<> struct express<vktl::queue_extensions::present_>
		: basic_queue_duty_express<queue_duty::present> {};
#endif
	template<> struct express<vktl::queue_extensions::bind_sparse_>
		: basic_queue_duty_express<queue_duty::bind_sparse> {};

	template<>
	struct express<vktl::execution_extensions::sync2_> {
		static void invoke(auto, auto& object) {

			// parent_of<device>(object)->append_extensions();
			object.require_synchronization2();
		}
	};

}
