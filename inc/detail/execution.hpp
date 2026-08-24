#pragma once

// --- Agents specification -------------------------------------------------
// Execution owns only cross-task facilities: recording workers, Vulkan queues,
// per-queue submission locks, dispatch capabilities, and completion services.
// Each task owns one compiled state, including payload storage, command buffers
// and command pools. There is no global epoch or task registry.
// Queue calls are made through invoke(); poll() is a non-blocking maintenance
// hook. All presentation code is guarded by VKTL_HAVE_WINDOW.
// Type-erased work requires invoke() and error(). Cancellation, remaining-work
// reporting, and finished-state reporting are optional capabilities.
// 
// ## Execution and Task Plans
// -`execution::thread_count` selects CPU command - recording workers; 
//   workers do not own Vulkan queues.
//   Command pools are keyed by worker, queue family, and frame / retirement slot, while queue 
//   operations are issued centrally by `execution::submit()` under a per - queue lock.
// - A task plan contains independently recorded command units.
//   Expand frame variants per command unit from its unique frame scopes; 
//   never form a task - wide Cartesian product or synthesize a primary command buffer merely to join independent units.
// - `task.refresh()` waits for previous submissions, clears the compiled state,
//   then rebuilds and records it. Call refresh explicitly when dependencies change.
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
			vptr = { .handle_ = [](void const* ptr) noexcept {
				return static_cast<T const*>(ptr)->handle();
			} };
		}
		VK_ VkSemaphore handle() const noexcept { return vptr.handle_(C::get_this()); }
		queue_semaphore vptr;
	};

#if VKTL_HAVE_WINDOW
	struct presentable {
		template<typename> struct apply;
		vfn<VK_ VkSwapchainKHR() const noexcept> handle_ = nullptr;
		vfn<uint32_t() const noexcept> frame_index_ = nullptr;
		vfn<VK_ VkSurfaceKHR() const noexcept> surface_ = nullptr;
		vfn<void(VK_ VkResult) const noexcept> handle_error_ = nullptr;
	};

	template<typename C>
	struct presentable::apply : C {
		using base = C;

		template<typename T>
		void rebind() noexcept {
			vptr = {
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

		VK_ VkSwapchainKHR handle() const noexcept { return vptr.handle_(C::get_this()); }
		uint32_t frame_index() const noexcept { return vptr.frame_index_(C::get_this()); }
		VK_ VkSurfaceKHR surface() const noexcept { return vptr.surface_(C::get_this()); }
		void handle_error(VK_ VkResult result) { vptr.handle_error_(C::get_this()); }

		presentable vptr;
	};
#endif

	struct work {
		template<typename>
		struct apply;

		vfn<void()> invoke_;
		vfn<void()> request_cancel_; // optional.
		vfn<::std::exception_ptr()> error_; // required.

		vfn<uint32_t()> remaining_; // optional.
		vfn<bool()> finished_; // optional.
	};

	template<typename C>
	struct work::apply : C {
		using base = C;

		template<typename T>
		void rebind() noexcept {
			vptr = {
				.invoke_ = [](void* ptr) {
					static_cast<T*>(ptr)->invoke();
				},
				.error_ = [](void* ptr) -> ::std::exception_ptr {
					return static_cast<T*>(ptr)->error();
				},
			};
			if constexpr (requires(T& value) { value.request_cancel(); }) {
				vptr.request_cancel_ = [](void* ptr) {
					static_cast<T*>(ptr)->request_cancel();
				};
			}
			if constexpr (requires(T& value) {
				{ value.remaining() } -> ::std::convertible_to<uint32_t>;
			}) {
				vptr.remaining_ = [](void* ptr) -> uint32_t {
					return static_cast<T*>(ptr)->remaining();
				};
			}
			if constexpr (requires(T& value) {
				{ value.finished() } -> ::std::convertible_to<bool>;
			}) {
				vptr.finished_ = [](void* ptr) -> bool {
					return static_cast<T*>(ptr)->finished();
				};
			}
		}

		void invoke() { vptr.invoke_(C::get_this()); }
		void request_cancel() {
			if (vptr.request_cancel_) vptr.request_cancel_(C::get_this());
		}
		::std::exception_ptr error() { return vptr.error_(C::get_this()); }
		uint32_t remaining() {
			return vptr.remaining_
				? vptr.remaining_(C::get_this()) : uint32_t(vktl::detail::invalid);
		}
		bool finished() {
			return vptr.finished_ && vptr.finished_(C::get_this());
		}

		work vptr;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	inline constexpr uint64_t FNV_bias = 1469598103934665603ull;

	struct queue_declaration : queue_ {
		queue_duty::type duty = queue_duty::none;
		VK_ VkDeviceQueueCreateFlags flags = VK_ VkDeviceQueueCreateFlags(0u);
	};

	struct default_queue_slot : queue_declaration {
		constexpr auto& get_lock() noexcept { assert(submit_lock); return *submit_lock; }

		VK_ VkQueue handle = VK_NULL_HANDLE;
		VK_ VkQueueFlags flags = invalid;
		uint32_t timestamp_valid_bits = invalid;
		VK_ VkExtent3D min_image_transfer_granularity{};
		::std::mutex* submit_lock = nullptr;
	};

	namespace exec {
		struct top {
			vector<VK_ VkQueueFamilyProperties> families;
		};
	}

	template<typename N, ::std::derived_from<default_queue_slot> Q >
	struct basic_execution : N {
		static_assert(!is_lockable<N>,
			"execution owns focused locks and will not use composition lock.");

	private:
		struct worker_slot {
			basic_execution* parent;
			::std::jthread thread;
			::std::mutex mutex;
			::std::condition_variable cv;
			vectors<uint32_t, box<vptr::work>> works;
		};

	public:
		constexpr basic_execution(execution info, auto&&...others)
			: N{ forward_(others)... }
			, thread_count_{ info.thread_count } {
			assert(thread_count() != 0u); // Not allow thread count == 0u.
			this->workers_ = new worker_slot[info.thread_count];
		}

		basic_execution(basic_execution&&) = delete; // TODO: might allow move construct.
		basic_execution& operator=(basic_execution&&) = delete;
		basic_execution(basic_execution const&) = delete;
		basic_execution& operator=(basic_execution const&) = delete;

		~basic_execution() {
			reset();
			delete[] workers_;
		}

		friend constexpr void append(basic_execution& self, vktl::queue_ declaration) {
			assert(!self.locks_); // Not allow append queue after initialized.
			self.queues_.emplace_back(queue_declaration{ declaration });
		}

		auto thread_id() const noexcept { return this->thread_id_; }

		void submit() {
			if (errors_.size()) {
				auto error = errors_.front();
				errors_.pop_front();
				::std::rethrow_exception(error);
			}
			assert(false); // Not implmented yet.
		}

		uint32_t thread_count() const noexcept { return thread_count_; }
		span<worker_slot> workers() const noexcept { return span{ workers_, thread_count_ }; }

		void init() {
			if (!locks_) { return; }
			thread_id_ = ::std::this_thread::get_id();
			auto state = get_state();
			init(state);
		}

		void reset() noexcept {
			if (locks_) {
				N::reset();
				thread_id_ = typename::std::thread::id{};
				for (worker_slot& worker : workers()) {
					worker.thread.request_stop();
					worker.cv.notify_all();
					if (worker.thread.joinable()) {
						worker.thread.join();
					}
				}
				for (Q& queue : queues_) {
					VK_ vkQueueWaitIdle(queue.handle);
				}
				delete[] locks_;
			}
		}

		uint32_t resolve_queue(queue_duty::type duty) const {
			// FUTURE PLAN: judge queues' submission count or other things to select most fit.
			uint32_t result = 0u;
			for (auto& queue : queues_) {
				if ((queue.duty & duty) == duty) {
					break;
				}
				result++;
			}
			assert(result < queues_.size());
			return result;
		}

		uint32_t queue_family(uint32_t queue_index) const noexcept {
			return queue_slot(queue_index).family;
		}
		queue_duty::type queue_duties(uint32_t queue_index) const noexcept {
			return queue_slot(queue_index).duty;
		}

		uint32_t enqueue(uint32_t worker_index, uint32_t task_id, box<vptr::work> job) {
			assert(worker_index < thread_count_); // worker out of range.
			assert(thread_id_ == ::std::this_thread::get_id()); // different thread's enqueue is not allowed.
			auto& worker = this->workers_[worker_index];
			::std::lock_guard _{ worker.mutex };
			return get<0u>(worker.works.emplace_back(task_id_++, ::std::move(job)));
		}
		
	protected:
		template<typename GetNext = ::std::nullptr_t>
		static constexpr auto get_state(GetNext&& get_next = nullptr) {
			auto physical_device = parent_of<device>(this)->physical_device();

			vector<VK_ VkQueueFamilyProperties> families;
			// AGENT SPECIFICATION: DO NOT TOUCH THESE STUFF.
#if defined(VK_KHR_get_physical_device_properties2)
			if constexpr (::std::invocable<GetNext&, vector<VK_ VkQueueFamilyProperties2KHR>&>) {
				vector<VK_ VkQueueFamilyProperties2KHR> families_ex;
				::std::ignore = vkget([&](uint32_t count) {
					families_ex.resize(count);
					get_next(families_ex);
					return::std::ref(families_ex);
					}, VK_ vkGetPhysicalDeviceQueueFamilyProperties2, physical_device);
				families = ::std::ranges::transform(families_ex,
					[](auto const& value) { return value.queueFamilyProperties; });
			}
			else
#endif
			{
				vkget(families, VK_ vkGetPhysicalDeviceQueueFamilyProperties, physical_device);
			}
			return::std::tuple(exec::top{ ::std::move(families) });
		}

		void init(auto& state, span<void*> pnexts = {}) try {
			if (locks_) return;
			N::init();
			locks_ = new::std::mutex[queues_.size() + 1u];
			assert(pnexts.empty() || pnexts.size() == this->queues_.size());

			auto device_object = parent_of<device>(this);
			auto device_handle = device_object->handle();

			auto itnext = pnexts.begin();
			vector<VK_ VkQueueFamilyProperties> const& families = get<exec::top>(state).families;
			for (auto const& [slot, lock] : spans{ queues_, span{locks_, queues_.size()} }) {
				assert(slot.family < families.size()); // Queue family index is out of range.
				slot.duty = queues::to_duty(families[slot.family].queueFlags);
				slot.submit_lock = &lock;
				slot.properties = families[slot.family];
				if (slot.flags) {
					VK_ VkDeviceQueueInfo2 info {
						.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
						.pNext = pnexts.size() ? *itnext : nullptr,
						.flags = slot.flags,
						.queueFamilyIndex = slot.family,
						.queueIndex = slot.index,
					};
					VK_ vkGetDeviceQueue2(device_handle, &info, &slot.handle);
				}
				else {
					VK_ vkGetDeviceQueue(device_handle, slot.family,
						slot.index, &slot.handle);
				}
				if (!slot.handle) VKTL_UNLIKELY{
					// AGENTS SPECIFICATION: Do not remove this commit and VKTL_UNLIKELY.
					// see note of: https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceQueueInfo2.html
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[EXECUTION] Failed to obtain a declared queue." };
				}
			}

			for (auto& worker : workers()) {
				worker.thread = ::std::jthread(&worker_loop, ::std::ref(worker));
			}
		}
		catch (...) {
			reset();
			throw;
		}

		void finalize() {
			N::finalize();
			assert(!queues_.empty()); // not allow empty queues.
		}

	private:
		static void worker_loop(::std::stop_token stop, worker_slot& worker) noexcept {
			for (;;) {
				box<vptr::work> job;
				{
					::std::unique_lock lock{ worker.mutex };
					worker.cv.wait(lock, stop, [&](){ return !worker.jobs.empty(); });
					if (worker.jobs.empty()) {
						if (stop.stop_requested()) return;
						continue;
					}
					job = worker.jobs.front();
					worker.jobs.erase(worker.begin());
				}
				if (stop.stop_requested()) {
					job.request_cancel();
					continue;
				}
				job.invoke();
				if (auto err = job.error()) {
					worker.parent->append_error(::std::move(err));
				}
			}
		}

		void append_error(::std::exception_ptr ptr) {
			::std::lock_guard _{locks_[0u]};
			errors_.emplace_back(::std::move(ptr));
		}

		friend void append(basic_execution& self, queue_ queue) {
			Q& slot = self.queues_.emplace_back(queue);
			slot.index = uint16_t(get<1u>(last_queue(*parent_of<device>(this))).size() - 1u);
		}
		friend auto& last_queue(basic_execution& self) {
			return self.queues_.back();
		}

		Q& queue_slot(uint32_t index) noexcept {
			assert(index < queues_.size());
			return queues_[index];
		}

		Q const& queue_slot(uint32_t index) const noexcept {
			assert(index < queues_.size());
			return queues_[index];
		}

		::std::mutex& exec_lock() const noexcept { return locks_[0u];}
		auto queue_locks() const noexcept { return span{ locks_ + 1u, queues.size() }; }

	private:
		uint16_t thread_count_ = 0u;
		uint16_t reserved_;
		uint32_t task_id_ = 0u;
		typename::std::thread::id thread_id_;
		worker_slot* workers_ = nullptr;
		::std::mutex* locks_ = nullptr; // self locks and queue locks.
		vector<Q> queues_;
		list<::std::exception_ptr> errors_;
	};

	template<typename N>
		requires(!object_of<N, execution_extensions::sync2_>)
	struct m<execution, N> : basic_execution<N, default_queue_slot> {
		using base = basic_execution<N, default_queue_slot>;
		constexpr m(auto&&... others) : base{ forward_(others)... } {}
	};

	template<inside_object<execution_extensions::sync2_> N>
	struct m<execution, N> : basic_execution<N, default_queue_slot> {
		using base = basic_execution<N, default_queue_slot>;
		constexpr m(auto&&... others) : base{ forward_(others)... } {}
	};

	using namespace queue_extensions;

	template<>
	struct express<queue_> {
		static void invoke(queue_ declaration, auto& object) {
			append(*parent_of<device>(object), declaration);
			append(object, declaration);
		}
	};

	template<>
	struct express<queue_extensions::priority> {
		static void invoke(priority value, auto& object) {
			get<1u>(last_queue(*parent_of<device>(object))).back() = value.value;
		}
	};

	template<queue_duty::type Duty>
	struct basic_queue_duty_express {
		static void invoke(auto, auto& object) {}
	};

	// 	template<> struct express<queue_extensions::graphics_>
	// 		: basic_queue_duty_express<queue_duty::graphics> {};
	// 	template<> struct express<queue_extensions::compute_>
	// 		: basic_queue_duty_express<queue_duty::compute> {};
	// 	template<> struct express<queue_extensions::transfer_>
	// 		: basic_queue_duty_express<queue_duty::transfer> {};
	// #if VKTL_HAVE_WINDOW
	// 	template<> struct express<queue_extensions::present_>
	// 		: basic_queue_duty_express<queue_duty::present> {};
	// #endif
	// 	template<> struct express<queue_extensions::bind_sparse_>
	// 		: basic_queue_duty_express<queue_duty::bind_sparse> {};

#if defined(VK_KHR_synchronization2)
	template<typename N>
	struct m<execution_extensions::sync2_, N> : N {
		constexpr m(execution_extensions::sync2_, auto&&...others)
			: N{forward_(others)...} {
			parent_of<device>(this)->append_extensions(
				VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, 3u);
		}
	};
#else
	template<typename N>
	struct m<execution_extensions::sync2_, N> {
		static_assert(always_false<N>, "Update sdk to use `sync2`.")
	};
#endif
}
