#pragma once

// --- Agents specification -------------------------------------------------
// Execution owns only cross-task facilities: stable task registration,
// recording workers, Vulkan queues and locks, command-pool groups, and epoch
// completion. Compiled commands, recipes, payload columns, and operation
// sequences remain task-owned.
// `submit()` first materializes every selected task without queue calls. Only
// after that pass succeeds may it invoke task-local `queue_operation` records.
// A failed materialization never performs a Vulkan queue operation, and a
// failed invocation never marks a task generation successfully submitted.
// Physical command pools are keyed by group, worker, family, create flags, and
// extension fingerprint. All presentation code is guarded by VKTL_HAVE_WINDOW.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	inline constexpr auto FNV_offset = 1469598103934665603ull;

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
		void (*invoke)(VK_ VkQueue, void const*) = nullptr;
		void const* storage = nullptr;
	};

	using command_pool_group_id = uint64_t;
	using command_pool_handle_id = uint64_t;

	struct command_pool_key {
		command_pool_group_id group = 0u;
		uint32_t worker = uint32_t(invalid);
		uint32_t queue_family = uint32_t(invalid);
		VK_ VkCommandPoolCreateFlags flags = VK_ VkCommandPoolCreateFlags(0u);
		uint64_t extension_fingerprint = 0u;

		constexpr bool operator==(command_pool_key const& other) {
			return group == other.group
				&& worker == other.worker
				&& queue_family == other.queue_family
				&& flags == other.flags
				&& extension_fingerprint == other.extension_fingerprint;
		}
	};

	struct command_pool_create_policy {
		VK_ VkCommandPoolCreateFlags flags = VK_ VkCommandPoolCreateFlags(0u);
		uint64_t fingerprint = FNV_offset;

		constexpr command_pool_create_policy() noexcept = default;
		constexpr command_pool_create_policy(auto&&...) noexcept {}
		constexpr void apply(VK_ VkCommandPoolCreateInfo&) noexcept {}
	};

	struct command_pool_policy_view {
		void* object = nullptr;
		VK_ VkCommandPoolCreateFlags flags = VK_ VkCommandPoolCreateFlags(0u);
		uint64_t fingerprint = 0u;
		void (*apply_)(void*, VK_ VkCommandPoolCreateInfo&) noexcept = nullptr;

		constexpr command_pool_policy_view() noexcept = default;

		template<typename Policy>
		explicit command_pool_policy_view(Policy& policy) noexcept
			: object{ ::std::addressof(policy) }
			, flags{ policy.flags }
			, fingerprint{ policy.fingerprint }
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
		case command_pool_policy_kind::individual_reset:
			result.flags = VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			break;
		default:
			break;
		}
		result.fingerprint = command_pool_hash_value(result.fingerprint,
			uint64_t(result.flags));
		return result;
	}

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
				.frame_index_ = [](void const* ptr) noexcept {
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

	struct task {
		template<typename C> struct apply;

		vfn<bool(uint64_t) const noexcept> selected_ = nullptr;
		vfn<void(uint64_t)> materialize_ = nullptr;
		vfn<cspan<detail::queue_operation>() const noexcept> operations_ = nullptr;
		vfn<void(uint64_t) noexcept> submitted_ = nullptr;
		vfn<void(uint64_t) noexcept> complete_ = nullptr;
	};

	template<typename C>
	struct task::apply : C {
		using base = C;

		template<typename T>
		void rebind() noexcept {
			vptr_ = {
				.selected_ = [](void const* ptr, uint64_t epoch) noexcept {
					return static_cast<T const*>(ptr)->selected(epoch);
				},
				.materialize_ = [](void* ptr, uint64_t epoch) {
					static_cast<T*>(ptr)->materialize(epoch);
				},
				.operations_ = [](void const* ptr) noexcept {
					return static_cast<T const*>(ptr)->operations();
				},
				.submitted_ = [](void* ptr, uint64_t epoch) noexcept {
					static_cast<T*>(ptr)->submitted(epoch);
				},
				.complete_ = [](void* ptr, uint64_t epoch) noexcept {
					static_cast<T*>(ptr)->complete(epoch);
				},
			};
		}

		bool selected(uint64_t epoch) const noexcept {
			return vptr_.selected_(C::get_this(), epoch);
		}
		void materialize(uint64_t epoch) { vptr_.materialize_(C::get_this(), epoch); }
		cspan<detail::queue_operation> operations() const noexcept {
			return vptr_.operations_(C::get_this());
		}
		void submitted(uint64_t epoch) noexcept { vptr_.submitted_(C::get_this(), epoch); }
		void complete(uint64_t epoch) noexcept { vptr_.complete_(C::get_this(), epoch); }

		task vptr_;
	};

}

VKTL_EXPORT_ namespace vktl::detail {

	struct registered_task {
		uint64_t id = 0u;
		box<vptr::task> object;
	};

	struct default_command_pool_slot {
		command_pool_key key;
		command_pool_handle_id id = 0u;
		VK_ VkCommandPool handle = VK_NULL_HANDLE;
	};

	struct default_worker_slot {
		::std::jthread thread;
		::std::mutex mutex;
		::std::condition_variable_any cv;
		::std::deque<record_job> jobs;
		::std::mutex pool_mutex;
		list<default_command_pool_slot> command_pools;
	};

	struct default_queue_slot {
		explicit default_queue_slot(queue_declaration value) : declaration{ value } {}

		queue_declaration declaration;
		VK_ VkQueue handle = VK_NULL_HANDLE;
		VK_ VkQueueFamilyProperties properties{};
		::std::mutex submit_lock;
		vector<VK_ VkFence> fences;
		vector<uint8_t> pending;
		bool participating = false;
	};

	namespace exec {
		inline void validate_queue_duties(queue_declaration const& declaration,
			VK_ VkQueueFamilyProperties const& properties) {
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
		static constexpr uint32_t default_frames_in_flight = 2u;

		static_assert(!object_of<N, lockable_>,
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

		void frames_in_flight(uint32_t count) {
			assert(!initialized_ && count != 0u);
			frames_in_flight_ = count;
		}

		uint32_t frames_in_flight() const noexcept { return frames_in_flight_; }
		uint32_t thread_count() const noexcept { return thread_count_; }
		uint64_t current_epoch() const noexcept { return epoch_; }
		uint64_t completed_epoch() const noexcept {
			return has_completed_epoch_ ? completed_epoch_ : uint64_t(invalid);
		}
		bool epoch_complete(uint64_t value) const noexcept {
			return value == uint64_t(invalid)
				|| (has_completed_epoch_ && value <= completed_epoch_);
		}
		queue_dispatch_config const& dispatch() const noexcept { return dispatch_; }

		void init(auto&& get_next = nullptr) try {
			N::init();
			if (initialized_) return;
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
				if (!slot.handle) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Failed to obtain a declared queue." };
				}

				slot.fences.resize(frames_in_flight_, VK_NULL_HANDLE);
				slot.pending.resize(frames_in_flight_, 0u);
				VK_ VkFenceCreateInfo fence_info{ .sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
				for (auto& fence : slot.fences) {
					VK_ vkCreateFence(device_handle, &fence_info, N::allocator(), &fence)
						| popup{ "[EXECUTION] Failed to create an epoch fence." };
				}
			}

#if defined(VK_KHR_synchronization2)
			dispatch_.submit2 = reinterpret_cast<VK_ PFN_vkQueueSubmit2KHR>(
				VK_ vkGetDeviceProcAddr(device_handle, "vkQueueSubmit2KHR"));
#endif
#if defined(VK_KHR_timeline_semaphore)
			dispatch_.timeline_semaphore = VK_ vkGetDeviceProcAddr(
				device_handle, "vkGetSemaphoreCounterValueKHR") != nullptr;
#endif

			for (uint32_t index = 0u; index < thread_count_; ++index) {
				workers_.emplace_back();
			}
			for (auto& worker : workers_) {
				worker.thread = ::std::jthread(&worker_loop, ::std::ref(worker));
			}
			initialized_ = true;
		}
		catch (...) {
			reset();
			throw;
		}

		void reset() noexcept {
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
				for (auto& worker : workers_) {
					for (auto& pool : worker.command_pools) if (pool.handle) {
						VK_ vkDestroyCommandPool(device_handle, pool.handle, N::allocator());
					}
				}
				for (auto& queue : queues_) {
					for (auto fence : queue.fences) if (fence) {
						VK_ vkDestroyFence(device_handle, fence, N::allocator());
					}
				}
			}
			workers_.clear();
			queues_.clear();
			initialized_ = false;
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

		template<typename Task>
		uint64_t attach_task(Task& task_object) {
			::std::lock_guard lock{ task_mutex_ };
			auto id = next_task_id_++;
			tasks_.emplace_back(registered_task{ id, box<vptr::task>{ &task_object } });
			return id;
		}

		void detach_task(uint64_t id) noexcept {
			::std::lock_guard lock{ task_mutex_ };
			for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
				if (it->id == id) {
					tasks_.erase(it);
					return;
				}
			}
			assert(!"unrecognized task registration id");
		}

		template<typename Task>
		void rebind_task(uint64_t id, Task& task_object) noexcept {
			::std::lock_guard lock{ task_mutex_ };
			for (auto& task : tasks_) {
				if (task.id == id) {
					task.object = box<vptr::task>{ &task_object };
					return;
				}
			}
			assert(!"unrecognized task registration id");
		}

		command_pool_group_id acquire_pool_group() noexcept {
			return next_pool_group_++;
		}

		template<typename Policy>
		command_pool_handle_id acquire_command_pool(command_pool_group_id group,
			uint32_t worker_index, uint32_t family, Policy& policy) {
			assert(group != 0u);
			auto& worker = worker_slot(worker_index);
			VK_ VkCommandPoolCreateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = policy.flags,
				.queueFamilyIndex = family,
			};
			policy.apply(info);

			{
				::std::lock_guard lock{ worker.pool_mutex };
				for (auto& pool : worker.command_pools) {
					if (pool.key.group == 0u && pool.key.worker == worker_index
						&& pool.key.queue_family == family && pool.key.flags == info.flags
						&& pool.key.extension_fingerprint == policy.fingerprint) {
						pool.key.group = group;
						return pool.id;
					}
				}
			}

			VK_ VkCommandPool handle = VK_NULL_HANDLE;
			VK_ vkCreateCommandPool(handle_of<vktl::device>(this), &info,
				N::allocator(), &handle)
				| popup{ "[EXECUTION] Failed to create a command pool." };

			::std::lock_guard lock{ worker.pool_mutex };
			auto id = next_pool_handle_++;
			worker.command_pools.emplace_back(default_command_pool_slot{
				.key = command_pool_key{ group, worker_index, family,
					info.flags, policy.fingerprint },
				.id = id,
				.handle = handle,
			});
			return id;
		}

		void release_pool_group(command_pool_group_id group) noexcept {
			if (group == 0u || !initialized_) return;
			auto device_handle = handle_of<vktl::device>(this);
			for (auto& worker : workers_) {
				::std::lock_guard lock{ worker.pool_mutex };
				for (auto& pool : worker.command_pools) {
					if (pool.key.group != group) continue;
					VK_ vkResetCommandPool(device_handle, pool.handle,
						VK_ VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
					pool.key.group = 0u;
				}
			}
		}

		void abandon_pool_group(command_pool_group_id group) noexcept {
			release_pool_group(group);
		}

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

		VK_ VkCommandBuffer allocate_command_buffer(uint32_t worker_index,
			command_pool_handle_id pool_id, VK_ VkCommandBufferLevel level) {
			auto& worker = worker_slot(worker_index);
			VK_ VkCommandPool pool_handle = VK_NULL_HANDLE;
			{
				::std::lock_guard lock{ worker.pool_mutex };
				for (auto const& pool : worker.command_pools) {
					if (pool.id == pool_id) {
						pool_handle = pool.handle;
						break;
					}
				}
			}
			assert(pool_handle != VK_NULL_HANDLE);
			VK_ VkCommandBufferAllocateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = pool_handle,
				.level = level,
				.commandBufferCount = 1u,
			};
			VK_ VkCommandBuffer result = VK_NULL_HANDLE;
			VK_ vkAllocateCommandBuffers(handle_of<vktl::device>(this), &info, &result)
				| popup{ "[EXECUTION] Failed to allocate a command buffer." };
			return result;
		}

		void submit() {
			auto const epoch = current_epoch();

			// Phase 1: task-local writes only. No Vulkan queue operation is allowed.
			{
				::std::lock_guard lock{ task_mutex_ };
				for (auto& task : tasks_) {
					if (task.object.selected(epoch)) task.object.materialize(epoch);
				}
			}

			auto const fence_slot = uint32_t(epoch % frames_in_flight_);
			wait_for_slot(fence_slot);
			if (epoch >= frames_in_flight_) {
				completed_epoch_ = epoch - frames_in_flight_;
				has_completed_epoch_ = true;
				::std::lock_guard lock{ task_mutex_ };
				for (auto& task : tasks_) task.object.complete(completed_epoch_);
			}
			clear_participation();

			try {
				::std::lock_guard task_lock{ task_mutex_ };
				for (auto& task : tasks_) {
					if (!task.object.selected(epoch)) continue;
					for (auto const& operation : task.object.operations()) {
						if (!operation.invoke || !operation.storage) continue;
						auto& queue = queue_slot(operation.queue_index);
						::std::lock_guard queue_lock{ queue.submit_lock };
						operation.invoke(queue.handle, operation.storage);
						queue.participating = true;
					}
				}
				close_participating_queues(fence_slot);
				for (auto& task : tasks_) {
					if (task.object.selected(epoch)) task.object.submitted(epoch);
				}
			}
			catch (...) {
				clear_participation();
				throw;
			}

			++epoch_;
			clear_participation();
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

		void wait_for_slot(uint32_t slot) {
			auto device_handle = handle_of<vktl::device>(this);
			for (auto& queue : queues_) {
				if (!queue.pending[slot]) continue;
				VK_ vkWaitForFences(device_handle, 1u, &queue.fences[slot],
					VK_TRUE, uint64_t(maximum))
					| popup{ "[EXECUTION] Waiting for an epoch fence failed." };
				VK_ vkResetFences(device_handle, 1u, &queue.fences[slot])
					| popup{ "[EXECUTION] Resetting an epoch fence failed." };
				queue.pending[slot] = 0u;
			}
		}

		void close_participating_queues(uint32_t slot) {
			for (auto& queue : queues_) {
				if (!queue.participating) continue;
				::std::lock_guard queue_lock{ queue.submit_lock };
				VK_ VkSubmitInfo close{ .sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO };
				VK_ vkQueueSubmit(queue.handle, 1u, &close, queue.fences[slot])
					| popup{ "[EXECUTION] Failed to close an epoch on a queue." };
				queue.pending[slot] = 1u;
			}
		}

		void clear_participation() noexcept {
			for (auto& queue : queues_) queue.participating = false;
		}

		uint32_t thread_count_ = 0u;
		uint32_t frames_in_flight_ = default_frames_in_flight;
		vector<queue_declaration> queue_declarations_;
		list<default_worker_slot> workers_;
		list<default_queue_slot> queues_;
		list<registered_task> tasks_;
		mutable::std::mutex task_mutex_;
		queue_dispatch_config dispatch_{};
		uint64_t epoch_ = 0u;
		uint64_t completed_epoch_ = 0u;
		uint64_t next_task_id_ = 1u;
		uint64_t next_pool_group_ = 1u;
		uint64_t next_pool_handle_ = 1u;
		bool initialized_ = false;
		bool has_completed_epoch_ = false;
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
