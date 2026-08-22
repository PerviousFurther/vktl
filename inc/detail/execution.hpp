#pragma once

// --- Agents specification -------------------------------------------------
// Execution owns only cross-task facilities: recording workers, Vulkan queues,
// per-queue submission locks, dispatch capabilities, and completion services.
// Tasks own compiled generations, payload storage, command buffers and command
// pools. There is no global epoch, task registry, or frames-in-flight ring.
// Queue calls are made through invoke(); poll() is a non-blocking maintenance
// hook. All presentation code is guarded by VKTL_HAVE_WINDOW.
// 
// ## Execution and Task Plans
// -`execution::thread_count` selects CPU command - recording workers; 
//   workers do not own Vulkan queues.
//   Command pools are keyed by worker, queue family, and frame / retirement slot, while queue 
//   operations are issued centrally by `execution::submit()` under a per - queue lock.
// - A task plan contains independently recorded command units.
//   Expand frame variants per command unit from its unique frame scopes; 
//   never form a task - wide Cartesian product or synthesize a primary command buffer merely to join independent units.
// - `task.refresh()` is transactional.Build and record into staging storage, validate captured revisions before recording and publication, 
//   and exchange a stable plan handle only after every required job succeeds.
//   Retire replaced GPU - visible storage by completion serial.
// - Derive command validity from recipe revisions, 
//   stable frame - scope identities, selected per - frame revisions, 
//   and non - frame dependency revisions.Do not retain an authoritative dirty - state enum.
// - `task.submit()` selects recorded frame variants, 
//   fills task - owned preallocated pools, and invokes its compiled queue operations through execution's queue service. 
//   `execution.submit(task)` is only a convenience entry point. 
//   Neither submit path may allocate memory or repeat queue capability validation.
// - Submission and completion are task - local; 
//   execution must not maintain a global epoch, a task registry, or a frames - in - flight fence ring.
//   Cross - queue ordering is expressed with Vulkan synchronization, not task call order.
// - Queue payloads are extensible through specialized traits plus handwritten `vktl::vptr` tables.
//   Keep one typed contiguous pool per payload trait and preserve heterogeneous execution order with a preallocated flat operation sequence; 
//   do not add a central payload `variant` or type switch.
// - Give every built - in payload trait a stable compile - time numeric ID; 
//   do not use static object addresses as runtime type keys. 
//   Aggregate storage counts by sum, but aggregate each payload instance's nested-array capacity by maximum 
//   so every pooled entry is large enough without multiplying total capacity across all entries.
// - Keep preallocated payload columns and their active counts in compiled storage, 
//   but assemble pointer - bearing Vulkan submit / present info structs on the stack at invocation time. 
//   Do not persist create - info - style views whose pointers can be invalidated when pooled columns move or resize.
// - Store non - movable execution queues, workers, and task slots in stable - address containers rather than `vector<unique_ptr<... >> `. 
//   Execution is single - controller state; its per - queue submission locks and task bookkeeping lock remain focused synchronization boundaries.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	inline constexpr auto FNV_bias = 1469598103934665603ull;

	struct queue_declaration : queue {
		queue_duty::type duty = queue_duty::none;
		VK_ VkDeviceQueueCreateFlags flags = VK_ VkDeviceQueueCreateFlags(0u);
	};

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

	struct queue_dispatch_config {
#if defined(VK_KHR_synchronization2)
		VK_ PFN_vkQueueSubmit2KHR submit2 = nullptr;
#endif
		bool timeline_semaphore = false;
	};

	struct queue_operation {
		uint32_t queue_index = uint32_t(invalid);
		void (*invoke)(VK_ VkQueue, void const*, VK_ VkFence) = nullptr;
		void const* storage = nullptr;
		bool fence_capable = false;
		bool user_completion = false;
	};

	struct command_pool_create_policy {
		VK_ VkCommandPoolCreateFlags flags = VK_ VkCommandPoolCreateFlags(0u);
		uint64_t fingerprint = invalid;

		constexpr command_pool_create_policy() noexcept = default;
		constexpr command_pool_create_policy(auto&&...) noexcept {}
		constexpr void apply(VK_ VkCommandPoolCreateInfo&) noexcept {}
	};

	struct command_pool_policy_view {
		void* object = nullptr;
		VK_ VkCommandPoolCreateFlags flags = VK_ VkCommandPoolCreateFlags(0u);
		uint64_t fingerprint = 0u;
		command_pool_policy_kind recycle = command_pool_policy_kind::generation;
		void (*apply_)(void*, VK_ VkCommandPoolCreateInfo&) noexcept = nullptr;

		constexpr command_pool_policy_view() noexcept = default;

		template<typename Policy>
		explicit command_pool_policy_view(Policy& policy) noexcept
			: object{ ::std::addressof(policy) }
			, flags{ policy.flags }
			, fingerprint{ policy.fingerprint }
			, recycle{ [] {
				if constexpr (requires { Policy::recycle_mode(); }) return Policy::recycle_mode();
				else return command_pool_policy_kind::generation;
			}() }
			, apply_{ [](void* pointer, VK_ VkCommandPoolCreateInfo& info) noexcept {
				static_cast<Policy*>(pointer)->apply(info);
			} } {
		}

		explicit operator bool() const noexcept { return object != nullptr; }

		void apply(VK_ VkCommandPoolCreateInfo& info) noexcept {
			assert(apply_);
			apply_(object, info);
		}
	};

	inline constexpr uint64_t command_pool_hash_value(uint64_t hash, uint64_t value) noexcept {
		for (uint32_t index = 0u; index < sizeof(value); ++index) {
			hash ^= uint8_t(value >> (index * 8u));
			hash *= 1099511628211ull;
		}
		return hash;
	}

	template<typename N>
	struct m<command_pool_extensions::transient_, N> : N {
		constexpr m(command_pool_extensions::transient_, auto&&... others)
			: N{ forward_(others)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			this->fingerprint = command_pool_hash_value(this->fingerprint,
				uint64_t(VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT));
		}

		constexpr void apply(VK_ VkCommandPoolCreateInfo& info) noexcept {
			N::apply(info);
		}
	};

	template<typename N>
	struct m<command_pool_extensions::individual_reset_, N> : N {
		constexpr m(command_pool_extensions::individual_reset_, auto&&... others)
			: N{ forward_(others)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			this->fingerprint = command_pool_hash_value(this->fingerprint,
				uint64_t(VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
		}

		constexpr void apply(VK_ VkCommandPoolCreateInfo& info) noexcept {
			N::apply(info);
		}
	};

	inline command_pool_create_policy make_command_pool_policy(
		command_pool_policy_kind kind) noexcept {
		command_pool_create_policy result;
		switch (kind) {
		case command_pool_policy_kind::transient:
			result.flags = VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			break;
		case command_pool_policy_kind::reset_command_buffer:
			result.flags = VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			break;
		default:
			break;
		}
		result.fingerprint = command_pool_hash_value(result.fingerprint,
			uint64_t(result.flags));
		return result;
	}

	template<typename T>
	concept have_command_pool_policy = requires { typename T::command_pool_policy_marker; };

	template<typename N>
	struct m<task_extensions::transient_, N> : N {
		static_assert(!have_command_pool_policy<N>,
			"transient command-pool policy cannot be combined with another policy");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::transient_, auto&&... values)
			: N{ forward_(values)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			this->fingerprint = command_pool_hash_value(this->fingerprint,
				uint64_t(VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT));
		}
		static constexpr VK_ VkCommandPoolCreateFlags command_pool_flags() noexcept {
			return VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		}
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::transient;
		}
	};

	template<typename N>
	struct m<task_extensions::reset_pool_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::reset_pool_, auto&&... values)
			: N{ forward_(values)... } {
			this->fingerprint = command_pool_hash_value(this->fingerprint, 1u);
		}
		static constexpr VK_ VkCommandPoolCreateFlags command_pool_flags() noexcept { return 0u; }
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::generation;
		}
	};

	template<typename N>
	struct m<task_extensions::reset_command_buffer_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::reset_command_buffer_, auto&&... values)
			: N{ forward_(values)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			this->fingerprint = command_pool_hash_value(this->fingerprint,
				uint64_t(VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
		}
		static constexpr VK_ VkCommandPoolCreateFlags command_pool_flags() noexcept {
			return VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		}
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::reset_command_buffer;
		}
	};

	template<typename N>
	struct m<task_extensions::free_command_buffer_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::free_command_buffer_, auto&&... values)
			: N{ forward_(values)... } {
			this->fingerprint = command_pool_hash_value(this->fingerprint, 3u);
		}
		static constexpr VK_ VkCommandPoolCreateFlags command_pool_flags() noexcept { return 0u; }
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::free_command_buffer;
		}
	};

	struct record_job_group {
		void add() noexcept {
			::std::lock_guard lock{ mutex };
			++remaining;
		}

		void finish(::std::exception_ptr failure = {}) noexcept {
			{
				::std::lock_guard lock{ mutex };
				if (failure && !error) error = failure;
				assert(remaining != 0u);
				--remaining;
				if (remaining != 0u) return;
			}
			cv.notify_all();
		}

		void wait() {
			::std::unique_lock lock{ mutex };
			cv.wait(lock, [&] { return remaining == 0u; });
			if (error) ::std::rethrow_exception(::std::exchange(error, {}));
		}

		bool ready() {
			::std::lock_guard lock{ mutex };
			return remaining == 0u;
		}

		uint32_t remaining = 0u;
		::std::mutex mutex;
		::std::condition_variable cv;
		::std::exception_ptr error;
	};

	struct record_job {
		void (*invoke)(void*) = nullptr;
		void* data = nullptr;
		record_job_group* group = nullptr;
	};

}

VKTL_EXPORT_ namespace vktl::vptr {

	struct queue_semaphore {
		template<typename C> struct apply;
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
		template<typename C> struct apply;
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
				.handle_ = [](void const* ptr) noexcept {
					return static_cast<T const*>(ptr)->handle();
				},
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

}

VKTL_EXPORT_ namespace vktl::detail {

	struct default_worker_slot {
		::std::jthread thread;
		::std::mutex mutex;
		::std::condition_variable_any cv;
		::std::deque<record_job> jobs;
	};

	struct default_queue_slot {
		explicit default_queue_slot(queue_declaration value) : declaration{ value } {}

		queue_declaration declaration;
		VK_ VkQueue handle = VK_NULL_HANDLE;
		VK_ VkQueueFamilyProperties properties{};
		::std::mutex submit_lock;
	};

	namespace exec {
		inline void validate_queue_duties(queue_declaration const& declaration, VK_ VkQueueFamilyProperties const& properties) {
			auto require = [&](queue_duty::type duty, VK_ VkQueueFlags flags,
				char const* message) {
				if ((declaration.duty & duty) && !(properties.queueFlags & flags)) {
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
	}

	template<typename N>
	struct m<execution, N> : N {
		using base = N;

		static_assert(!is_lockable<N>,
			"execution owns focused locks and does not use the composition lock");

		constexpr m(execution info, auto&&... others)
			: N{ forward_(others)... }, thread_count_{ info.thread_count } {
			assert(thread_count_ != 0u);
		}

		m(m const&) = delete;
		m& operator=(m const&) = delete;
		m(m&&) = delete;
		m& operator=(m&&) = delete;

		~m() { reset(); }

		void append(vktl::queue declaration) {
			assert(!initialized_);
			assert(::std::ranges::none_of(queue_declarations_, [&](auto const& value) {
				return value.family == declaration.family && value.index == declaration.index;
			}));
			queue_declarations_.emplace_back(queue_declaration{ declaration });
		}

		void append_queue_duty(queue_duty::type duty) {
			assert(!queue_declarations_.empty());
			queue_declarations_.back().duty |= duty;
		}

		uint32_t thread_count() const noexcept { return thread_count_; }
		queue_dispatch_config const& dispatch() const noexcept { return dispatch_; }
		VK_ VkAllocationCallbacks const* allocation_callbacks() const noexcept {
			return N::allocator();
		}

		template<typename GetNext = ::std::nullptr_t>
		auto init(GetNext&& get_next = nullptr) try {
			N::init();
			auto locker = nullptr;
			if (initialized_) return locker;
			if (queue_declarations_.empty()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] At least one queue must be declared." };
			}

			auto* device_object = parent_of<device>(this);
			auto device_handle = device_object->handle();
			auto physical_device = device_object->physical_device();

			vector<VK_ VkQueueFamilyProperties> families;
			vkget(families, VK_ vkGetPhysicalDeviceQueueFamilyProperties, physical_device);


			for (auto const& declaration : queue_declarations_) {
				if (declaration.family >= families.size()) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Queue family index is out of range." };
				}
				exec::validate_queue_duties(declaration, families[declaration.family]);
				auto& slot = queues_.emplace_back(declaration);
				slot.properties = families[declaration.family];
				if (declaration.flags) {
					VK_ VkDeviceQueueInfo2 info{
						.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
						.flags = declaration.flags,
						.queueFamilyIndex = declaration.family,
						.queueIndex = declaration.index,
					};
					if constexpr (requires { get_next(info); }) get_next(info);
					VK_ vkGetDeviceQueue2(device_handle, &info, &slot.handle);
				}
				else {
					VK_ vkGetDeviceQueue(device_handle, declaration.family,
						declaration.index, &slot.handle);
				}
				if (!slot.handle) VKTL_UNLIKELY {
					// AGENTS SPECIFICATION: Do not remove this commit and VKTL_UNLIKELY.
					// see note of: https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceQueueInfo2.html
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Failed to obtain a declared queue." };
				}

			}

			for (uint32_t index = 0u; index < thread_count_; ++index) {
				workers_.emplace_back();
			}
			for (auto& worker : workers_) {
				worker.thread = ::std::jthread(&worker_loop, ::std::ref(worker));
			}
			initialized_ = true;
			return locker;
		}
		catch (...) {
			reset();
			throw;
		}

		auto reset() noexcept {
			N::reset();
			auto locker = nullptr;

			for (auto& worker : workers_) {
				worker.thread.request_stop();
				worker.cv.notify_all();
			}
			for (auto& worker : workers_) {
				if (worker.thread.joinable()) worker.thread.join();
			}

			auto device_handle = queues_.empty()
				? VK_NULL_HANDLE : handle_of<vktl::device>(this);
			if (device_handle) {
				for (auto& queue : queues_) if (queue.handle) VK_ vkQueueWaitIdle(queue.handle);
			}
			workers_.clear();
			queues_.clear();
			initialized_ = false;

			return locker;
		}

		uint32_t resolve_queue(queue_duty::type duty) const {
			uint32_t index = 0u;
			for (auto const& queue : queue_declarations_) {
				if ((queue.duty & duty) == duty) return index;
				++index;
			}
			throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
				"[EXECUTION] No declared queue satisfies the requested duty." };
		}

		uint32_t queue_family(uint32_t queue_index) const noexcept {
			return queue_slot(queue_index).declaration.family;
		}

		queue_duty::type queue_duties(uint32_t queue_index) const noexcept {
			return queue_slot(queue_index).declaration.duty;
		}

#if VKTL_HAVE_WINDOW
		void validate_present(uint32_t queue_index, VK_ VkSurfaceKHR surface) {
			auto const& queue = queue_slot(queue_index);
			if (!(queue.declaration.duty & queue_duty::present)) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[EXECUTION] Presentation requires a present-capable queue." };
			}
			VK_ VkBool32 supported = VK_FALSE;
			VK_ vkGetPhysicalDeviceSurfaceSupportKHR(
				parent_of<vktl::device>(this)->physical_device(),
				queue.declaration.family, surface, &supported)
				| popup{ "[EXECUTION] Failed to query presentation support." };
			if (!supported) {
				throw error{ int(VK_ VK_ERROR_FEATURE_NOT_PRESENT),
					"[EXECUTION] Queue family does not support the surface." };
			}
		}
#endif

		void enqueue(uint32_t worker_index, record_job job) {
			assert(job.invoke && job.group);
			auto& worker = worker_slot(worker_index);
			job.group->add();
			{
				::std::lock_guard lock{ worker.mutex };
				worker.jobs.emplace_back(job);
			}
			worker.cv.notify_one();
		}

		void invoke(queue_operation const& operation,
			VK_ VkFence completion = VK_NULL_HANDLE) {
			assert(operation.invoke && operation.storage);
			auto& queue = queue_slot(operation.queue_index);
			::std::lock_guard lock{ queue.submit_lock };
			operation.invoke(queue.handle, operation.storage, completion);
		}

		void close_queue(uint32_t queue_index, VK_ VkFence completion) {
			auto& queue = queue_slot(queue_index);
			::std::lock_guard lock{ queue.submit_lock };
			VK_ VkSubmitInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO };
			VK_ vkQueueSubmit(queue.handle, 1u, &info, completion)
				| popup{ "[EXECUTION] Failed to establish recovery completion." };
		}

		VK_ VkFence create_completion_fence(bool signaled = false) {
			VK_ VkFence result = VK_NULL_HANDLE;
			VK_ VkFenceCreateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = signaled ? VK_ VK_FENCE_CREATE_SIGNALED_BIT : 0u,
			};
			VK_ vkCreateFence(handle_of<vktl::device>(this), &info,
				N::allocator(), &result)
				| popup{ "[EXECUTION] Failed to create a completion fence." };
			return result;
		}

		void reset_completion_fence(VK_ VkFence fence) {
			VK_ vkResetFences(handle_of<vktl::device>(this), 1u, &fence)
				| popup{ "[EXECUTION] Failed to reset a completion fence." };
		}

		bool completion_ready(VK_ VkFence fence) const {
			auto result = VK_ vkGetFenceStatus(handle_of<vktl::device>(this), fence);
			if (result == VK_ VK_NOT_READY) return false;
			result | popup{ "[EXECUTION] Failed to query a completion fence." };
			return true;
		}

		void wait_completion(VK_ VkFence fence,
			uint64_t timeout = uint64_t(maximum)) const {
			VK_ vkWaitForFences(handle_of<vktl::device>(this), 1u, &fence,
				VK_TRUE, timeout)
				| popup{ "[EXECUTION] Failed to wait for task completion." };
		}

		void destroy_completion_fence(VK_ VkFence fence) noexcept {
			if (fence) VK_ vkDestroyFence(handle_of<vktl::device>(this), fence, N::allocator());
		}

		void poll() noexcept {}
		template<typename Task>
		void poll(Task& task_object) { task_object.poll(); }

		template<typename Task>
		auto submit(Task& task_object) {
			poll(task_object);
			return task_object.submit();
		}

	protected:
		static constexpr auto queue_flags() noexcept {
			return VK_ VkDeviceQueueCreateFlags(0u);
		}

		void finalize() {
			if constexpr (requires { N::finalize(); }) N::finalize();
			assert(!queue_declarations_.empty());
		}

	private:
		static void worker_loop(::std::stop_token stop, default_worker_slot& worker) noexcept {
			for (;;) {
				record_job job;
				{
					::std::unique_lock lock{ worker.mutex };
					worker.cv.wait(lock, stop, [&] { return !worker.jobs.empty(); });
					if (worker.jobs.empty()) {
						if (stop.stop_requested()) return;
						continue;
					}
					job = worker.jobs.front();
					worker.jobs.pop_front();
				}
				if (stop.stop_requested()) {
					job.group->finish(::std::make_exception_ptr(error{
						int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Recording cancelled during shutdown." }));
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

		default_worker_slot& worker_slot(uint32_t index) noexcept {
			assert(index < workers_.size());
			auto it = workers_.begin();
			::std::advance(it, index);
			return *it;
		}

		default_queue_slot& queue_slot(uint32_t index) noexcept {
			assert(index < queues_.size());
			auto it = queues_.begin();
			::std::advance(it, index);
			return *it;
		}

		default_queue_slot const& queue_slot(uint32_t index) const noexcept {
			assert(index < queues_.size());
			auto it = queues_.begin();
			::std::advance(it, index);
			return *it;
		}

		uint32_t thread_count_ = 0u;
		vector<queue_declaration> queue_declarations_;
		list<default_worker_slot> workers_;
		list<default_queue_slot> queues_;
		queue_dispatch_config dispatch_{};
		bool initialized_ = false;
	};

	using namespace queue_extensions;

	template<>
	struct express<queue> {
		static void invoke(queue declaration, auto& object) {
			parent_of<device>(object)->append(declaration);
			object.append(declaration);
		}
	};

	template<>
	struct express<queue_extensions::priority> {
		static void invoke(priority value, auto& object) {
			auto& priorities = get<1u>(parent_of<device>(object)->last_queue());
			priorities.back() = value.value;
		}
	};

	template<queue_duty::type Duty>
	struct basic_queue_duty_express {
		static void invoke(auto, auto& object) { object.append_queue_duty(Duty); }
	};

	template<> struct express<queue_extensions::graphics_>
		: basic_queue_duty_express<queue_duty::graphics> {};
	template<> struct express<queue_extensions::compute_>
		: basic_queue_duty_express<queue_duty::compute> {};
	template<> struct express<queue_extensions::transfer_>
		: basic_queue_duty_express<queue_duty::transfer> {};
#if VKTL_HAVE_WINDOW
	template<> struct express<queue_extensions::present_>
		: basic_queue_duty_express<queue_duty::present> {};
#endif
	template<> struct express<queue_extensions::bind_sparse_>
		: basic_queue_duty_express<queue_duty::bind_sparse> {};

	template<>
	struct express<execution_extensions::sync2_> {
		static void invoke(auto, auto& object) {
#if defined(VK_KHR_synchronization2)
			parent_of<device>(object)->append_extensions(
				VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, 3u);
#else
			static_assert(always_false<decltype(object)>,
				"VK_KHR_synchronization2 declarations are unavailable.");
#endif
		}
	};

}
