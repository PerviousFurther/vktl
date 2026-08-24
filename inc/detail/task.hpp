#pragma once

// --- Agents specification -------------------------------------------------
// A task owns one compiled state. refresh() waits for its previous submissions,
// clears that state, packages every command unit as a record_job, and waits
// for all recording jobs. submit() only prepares the recorded payload data and
// invokes queue operations. Task command-pool policy specializations live here.
// Command units do not retain frame-scope state. No fingerprint, revision, or
// generation tracking is involved.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	enum class submission_state : uint8_t {
		complete, preparing, submitting, in_flight, partial_submission
	};

	struct task_command_pool : poly_list::node {
		uint32_t worker = 0u;
		uint32_t family = uint32_t(invalid);
		VK_ VkCommandPoolCreateFlags flags = 0u;
		VK_ VkCommandPool handle = VK_NULL_HANDLE;
		vector<VK_ VkCommandBuffer> buffers;
		VK_ VkDevice device = VK_NULL_HANDLE;
		VK_ VkAllocationCallbacks const* allocator = nullptr;
	};

	struct dependency_edge {
		uint32_t source_command = uint32_t(invalid);
		uint32_t destination_command = uint32_t(invalid);
		uint32_t source_queue = uint32_t(invalid);
		uint32_t destination_queue = uint32_t(invalid);
	};

	struct default_command_unit : poly_list::node {
		task_command_pool* pool = nullptr; 
		uint32_t worker_index = 0u;
		uint32_t queue_index = uint32_t(invalid);
		uint32_t queue_family = uint32_t(invalid);
		VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u);
		VK_ VkCommandBuffer handle = VK_NULL_HANDLE;

		poly_list recipes;
	};

	struct compiled_task;

	struct compiled_payload : poly_list::node {
		using prepare_fn = void(*)(compiled_payload&);
		using prepare_submit_fn = void(*)(compiled_payload&, compiled_task const&);

		void prepare() { prepare_(*this); }
		void prepare_submit(compiled_task const& task) { prepare_submit_(*this, task); }
		bool enabled() const noexcept { return enabled_; }
		void request_completion() noexcept { completion_requested_ = true; }

	protected:
		template<typename T>
		void bind_payload() noexcept {
			prepare_ = [](compiled_payload& base) { static_cast<T&>(base).prepare_storage(); };
			prepare_submit_ = [](compiled_payload& base, compiled_task const& task) {
				static_cast<T&>(base).prepare_submit_storage(task);
			};
			// operation.invoke = &T::invoke;
			// operation.storage = static_cast<T const*>(this);
			// operation.fence_capable = T::fence_capable;
		}

		prepare_fn prepare_ = nullptr;
		prepare_submit_fn prepare_submit_ = nullptr;
		bool enabled_ = false;
		bool completion_requested_ = false;
	};

	struct submit_semaphore_value {
		VK_ VkSemaphore handle = VK_NULL_HANDLE;
		uint64_t value = invalid;
	};

	struct semaphore_operand {
		box<vptr::queue_semaphore> object;
		uint64_t value = invalid;
	};

	struct base_submit_payload : compiled_payload {
		uint32_t queue = uint32_t(invalid);
		vector<uint32_t> commands;
		vector<semaphore_operand> waits;
		vector<semaphore_operand> signals;
		vector<VK_ VkSemaphore> legacy_semaphores;
		vector<VK_ VkPipelineStageFlags> legacy_stages;
		vector<uint64_t> legacy_values;
	};

	struct submit_payload : base_submit_payload {
		static constexpr bool fence_capable = true;
		submit_payload() { bind_payload<submit_payload>(); }

		vector<VK_ VkCommandBuffer> command_handles;
		vector<submit_semaphore_value> semaphore_values;

		void prepare_storage() {
			enabled_ = !commands.empty() || !waits.empty() || !signals.empty();
			command_handles.resize(commands.size());
			semaphore_values.resize(waits.size() + signals.size());
			legacy_semaphores.resize(waits.size() + signals.size());
			legacy_stages.resize(waits.size());
			legacy_values.resize(waits.size() + signals.size());
		}

		void prepare_submit_storage(compiled_task const& task);

		static void invoke(VK_ VkQueue queue_handle, void const* storage,
			VK_ VkFence completion) {
			auto& self = *static_cast<submit_payload const*>(storage);
				VK_ VkSubmitInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = uint32_t(self.waits.size()),
					.pWaitSemaphores = self.legacy_semaphores.data(),
					.pWaitDstStageMask = self.legacy_stages.data(),
					.commandBufferCount = uint32_t(self.command_handles.size()),
					.pCommandBuffers = self.command_handles.data(),
					.signalSemaphoreCount = uint32_t(self.signals.size()),
					.pSignalSemaphores = self.legacy_semaphores.data() + self.waits.size(),
				};
#if defined(VK_KHR_timeline_semaphore)
				VK_ VkTimelineSemaphoreSubmitInfoKHR timeline{
					.sType = VK_ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
					.waitSemaphoreValueCount = uint32_t(self.waits.size()),
					.pWaitSemaphoreValues = self.legacy_values.data(),
					.signalSemaphoreValueCount = uint32_t(self.signals.size()),
					.pSignalSemaphoreValues = self.legacy_values.data() + self.waits.size(),
				};
				info.pNext = &timeline;
#endif
				VK_ vkQueueSubmit(queue_handle, 1u, &info, completion)
					| popup{ "[EXECUTION] Queue submission failed." };


		}
	};

#if defined(VK_KHR_synchronization2)
	struct submit_payload2 : base_submit_payload {
		static constexpr bool fence_capable = true;
		vectors<VK_ VkCommandBufferSubmitInfoKHR, vector<VK_ VkCommandBuffer>> command_infos;
		vectors<VK_ VkSemaphoreSubmitInfoKHR, vector<VK_ VkSemaphore>> semaphore_infos;

		static void invoke(VK_ VkQueue queue_handle, void const* storage,
			VK_ VkFence completion) {
			auto& self = *static_cast<submit_payload2 const*>(storage);
			VK_ VkSubmitInfo2KHR info{
				.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
				.waitSemaphoreInfoCount = uint32_t(self.waits.size()),
				.pWaitSemaphoreInfos = self.semaphore_infos.data<0u>(),
				.commandBufferInfoCount = uint32_t(self.command_infos.size()),
				.pCommandBufferInfos = self.command_infos.data<0u>(),
				.signalSemaphoreInfoCount = uint32_t(self.signals.size()),
				.pSignalSemaphoreInfos = self.semaphore_infos.data<0u>() + self.waits.size(),
			};
			VK_ vkQueueSubmit2KHR(queue_handle, 1u, &info, completion)
				| popup{ "[EXECUTION] Queue submission failed." };
			return;
		}
	};
#endif


#if VKTL_HAVE_WINDOW
	struct present_payload : compiled_payload {
		static constexpr bool fence_capable = false;
		present_payload() { bind_payload<present_payload>(); }

		uint32_t queue = uint32_t(invalid);
		vector<box<vptr::presentable>> swapchains;
		vector<box<vptr::queue_semaphore>> waits;
		vector<VK_ VkSwapchainKHR> handles;
		vector<uint32_t> image_indices;
		vector<VK_ VkSemaphore> wait_handles;
		vector<VK_ VkResult> results;

		void prepare_storage() {
			enabled_ = !swapchains.empty();
			handles.resize(swapchains.size());
			image_indices.resize(swapchains.size());
			wait_handles.resize(waits.size());
			results.resize(swapchains.size());
		}

		void prepare_submit_storage(compiled_task const&) {
			for (uint32_t index = 0u; index < uint32_t(swapchains.size()); ++index) {
				handles[index] = swapchains[index].handle();
				image_indices[index] = swapchains[index].frame_index();
			}
			for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
				wait_handles[index] = waits[index].handle();
			}
		}

		static void invoke(VK_ VkQueue queue_handle, void const* storage,
			VK_ VkFence) {
			auto& self = *const_cast<present_payload*>(
				static_cast<present_payload const*>(storage));
			VK_ VkPresentInfoKHR info{
				.sType = VK_ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = uint32_t(self.wait_handles.size()),
				.pWaitSemaphores = self.wait_handles.data(),
				.swapchainCount = uint32_t(self.handles.size()),
				.pSwapchains = self.handles.data(),
				.pImageIndices = self.image_indices.data(),
				.pResults = self.results.data(),
			};
			
			auto result = VK_ vkQueuePresentKHR(queue_handle, &info);
			if (result != VK_ VK_SUCCESS) { 
				for (auto&& [result, swapchain] : spans{ self.results, self.swapchains }) {
					if (result != VK_ VK_SUCCESS) {
						swapchain.handle_error(result);
					}
				}
			}

			// switch () {
			// case VK_ VK_SUCCESS:break;
			// case VK_ VK_SUBOPTIMAL_KHR:
			// case VK_ VK_ERROR_OUT_OF_DATE_KHR:
			// 	// self.swapchains
			// 	break;
			// }
		}
	};
#endif

	struct sparse_bind_payload : compiled_payload {
		static constexpr bool fence_capable = true;
		sparse_bind_payload() { bind_payload<sparse_bind_payload>(); }

		uint32_t queue = uint32_t(invalid);
		vector<VK_ VkSparseImageMemoryBindInfo> image_binds;
		vector<VK_ VkSparseImageOpaqueMemoryBindInfo> image_opaque_binds;
		vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
		vector<box<vptr::queue_semaphore>> waits;
		vector<box<vptr::queue_semaphore>> signals;
		vector<VK_ VkSemaphore> wait_handles;
		vector<VK_ VkSemaphore> signal_handles;

		void prepare_storage() {
			enabled_ = !buffer_binds.empty();
			wait_handles.resize(waits.size());
			signal_handles.resize(signals.size());
		}

		void prepare_submit_storage(compiled_task const&) {
			for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
				wait_handles[index] = waits[index].handle();
			}
			for (uint32_t index = 0u; index < uint32_t(signals.size()); ++index) {
				signal_handles[index] = signals[index].handle();
			}
		}

		static void invoke(VK_ VkQueue queue_handle, void const* storage,
			VK_ VkFence completion) {
			auto& self = *static_cast<sparse_bind_payload const*>(storage);
			VK_ VkBindSparseInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
				.waitSemaphoreCount = uint32_t(self.wait_handles.size()),
				.pWaitSemaphores = self.wait_handles.data(),
				.bufferBindCount = uint32_t(self.buffer_binds.size()),
				.pBufferBinds = self.buffer_binds.data(),
				.signalSemaphoreCount = uint32_t(self.signal_handles.size()),
				.pSignalSemaphores = self.signal_handles.data(),
			};
			VK_ vkQueueBindSparse(queue_handle, 1u, &info, completion)
				| popup{ "[EXECUTION] Sparse queue binding failed." };
		}
	};

	// struct submission_slot {
	// 	submission_state state = submission_state::complete;
	// 	vector<VK_ VkFence> completions; // why multiple?
	// 	vector<uint8_t> completion_submitted;
	// 	uint32_t successful_operations = 0u;
	// };
	//
	// struct task_submission {
	// 	// A submission refers to the task's current state. refresh() waits for it
	// 	// and invalidates this lightweight view.
	// 	bool accepted = false;
	// 	void* owner = nullptr;
	// 	compiled_task* state = nullptr;
	// 	submission_slot* slot = nullptr;
	// 	bool (*ready_)(void*, compiled_task&, submission_slot&) = nullptr;
	// 	void (*wait_)(void*, compiled_task&, submission_slot&) = nullptr;
	// 	explicit operator bool() const noexcept { return accepted; }
	// 	bool ready() const {
	// 		return !accepted || ready_(owner, *state, *slot);
	// 	}
	// 	void wait() const {
	// 		if (accepted) wait_(owner, *state, *slot);
	// 	}
	// };

	struct compiled_task : poly_list::node {
		// Everything needed to submit one recorded task lives here.
		poly_list commands;
		poly_list policies;
		poly_list payloads;
		poly_list command_pools;
		vector<default_command_unit*> command_index;
		// vector<queue_operation> operations;
		vector<uint32_t> completion_operations;
		// vector<submission_slot> submission_slots;
		uint32_t in_flight_submissions = 0u;

		void prepare_submit() {
			for (auto& node : payloads) {
				auto& payload = node.as<compiled_payload>();
				if (!payload.enabled()) continue;
				payload.prepare_submit(*this);
			}
		}
	};

	inline void submit_payload::prepare_submit_storage(compiled_task const& task) {
		for (uint32_t index = 0u; index < uint32_t(commands.size()); ++index) {
			auto command_index = commands[index];
			assert(command_index < task.command_index.size());
			auto handle = task.command_index[command_index]->handle;
			assert(handle != VK_NULL_HANDLE);
			command_handles[index] = handle;
		}

		auto write = [&](auto const& source, uint32_t first) {
			for (uint32_t index = 0u; index < uint32_t(source.size()); ++index) {
				auto const& operand = source[index];
				semaphore_values[first + index] = submit_semaphore_value{
					operand.object.handle(), operand.value, operand.stage };
			}
		};
		write(waits, 0u);
		write(signals, uint32_t(waits.size()));

		for (uint32_t index = 0u; index < uint32_t(semaphore_values.size()); ++index) {
			legacy_semaphores[index] = semaphore_values[index].handle;
			legacy_values[index] = semaphore_values[index].value;
			if (index < waits.size()) legacy_stages[index] = legacy_stage(semaphore_values[index].stage);
		}
	}

	template<typename Execution>
	struct refresh_builder;
	template<typename Builder>
	struct refresh_context;
	template<typename Builder>
	struct worker_context;
	template<typename Builder>
	struct command_context;
	template<typename Builder, typename Pass>
	struct pass_context;
	template<typename Builder, typename Pass>
	struct pipe_context;
	template<typename Builder>
	struct submit_context;

	template<typename Execution>
	struct refresh_builder {
		struct record_request {
			Execution* execution = nullptr;
			default_command_unit* command = nullptr;

			static void invoke(void* data) {
				auto& self = *static_cast<record_request*>(data);
				assert(self.command->pool && self.command->pool->handle);
				VK_ VkCommandBufferAllocateInfo allocation{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = self.command->pool->handle,
					.level = self.command->level,
					.commandBufferCount = 1u,
				};
				VK_ VkCommandBuffer handle = VK_NULL_HANDLE;
				VK_ vkAllocateCommandBuffers(handle_of<vktl::device>(self.execution),
					&allocation, &handle)
					| popup{ "[TASK] Failed to allocate a command buffer." };
				self.command->pool->buffers.emplace_back(handle);
				VK_ VkCommandBufferBeginInfo begin{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = self.command->usage,
				};
				VK_ vkBeginCommandBuffer(handle, &begin)
					| popup{ "[TASK] Failed to begin command recording." };
				command_record_context context{ handle };
				for (auto const& node : self.command->recipes) {
					node.template as<compiled_recipe>().record(context);
				}
				VK_ vkEndCommandBuffer(handle)
					| popup{ "[TASK] Failed to end command recording." };
				self.command->handle = handle;
			}
		};

		refresh_builder(Execution& execution, compiled_task& state)
			: execution_{ execution }, state_{ state } {}

		uint32_t add_command(uint32_t worker, queue_duty::type duty,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u),
			command_pool_policy_kind policy = command_pool_policy_kind::reset_pool) {
			assert(worker < execution_.thread_count()); // worker out of range.
			auto queue = execution_.resolve_queue(duty);
			auto& command = state_.commands.template emplace_back<default_command_unit>();
			command.worker_index = worker;
			command.queue_index = queue;
			command.queue_family = execution_.queue_family(queue);
			command.level = level;
			command.usage = usage;
			command.policy = policy;
			state_.command_index.emplace_back(::std::addressof(command));
			return uint32_t(state_.command_index.size() - 1u);
		}

		void set_policy(uint32_t command, command_pool_policy_kind policy) noexcept {
			state_.command_index[command]->policy = policy;
			state_.command_index[command]->policy_view = {};
		}

		template<typename Policy>
		struct policy_holder : poly_list::node {
			Policy value;

			template<typename... Infos>
			explicit policy_holder(Infos&&... infos)
				: value{ use_base<command_pool_create_policy>{},
					static_cast<Infos&&>(infos)... } {}
		};

		template<typename... Infos>
		void set_policy_components(uint32_t command, Infos&&... infos) {
			using policy_type = decltype(object{ use_base<command_pool_create_policy>{},
				static_cast<Infos&&>(infos)... });
			auto& holder = state_.policies.template emplace_back<policy_holder<policy_type>>(
				static_cast<Infos&&>(infos)...);
			state_.command_index[command]->policy_view =
				command_pool_policy_view{ holder.value };
		}

		void set_usage(uint32_t command, VK_ VkCommandBufferUsageFlags usage) noexcept {
			state_.command_index[command]->usage = usage;
		}

		template<typename Object>
		void add_dependency(uint32_t, Object& object) {
			object.init(); // TODO: Should not directly init.
		}

		template<typename... Infos>
		void append_recipe(uint32_t command, Infos&&... infos) {
			auto& unit = *state_.command_index[command];
			emplace_recipe(unit.recipes, static_cast<Infos&&>(infos)...);
		}

		uint32_t add_submit(uint32_t command) {
			auto& payload = state_.payloads.template emplace_back<submit_payload>();
			payload.queue = state_.command_index[command]->queue_index;
			payload.commands.emplace_back(command);
			submits_.emplace_back(::std::addressof(payload));
			sparse_.emplace_back(nullptr);
#if VKTL_HAVE_WINDOW
			presents_.emplace_back(nullptr);
#endif
			return uint32_t(submits_.size() - 1u);
		}

		void request_completion(uint32_t submit) noexcept {
			assert(submit < submits_.size());
			submits_[submit]->request_completion();
		}

		void add_command_to_submit(uint32_t submit, uint32_t command) {
			auto& payload = *submits_[submit];
			if (payload.queue != state_.command_index[command]->queue_index) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] A submit cannot contain commands from different queues." };
			}
			payload.commands.emplace_back(command);
		}

		template<typename Semaphore>
		void add_wait(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->waits.emplace_back(semaphore_operand{
				box<vptr::queue_semaphore>{ semaphore }, value, stage });
		}

		template<typename Semaphore>
		void add_signal(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->signals.emplace_back(semaphore_operand{
				box<vptr::queue_semaphore>{ semaphore }, value, stage });
		}

#if VKTL_HAVE_WINDOW
		template<typename Swapchain>
		void add_present(uint32_t submit, Swapchain& swapchain) {
			swapchain.init();
			auto& payload = present_for(submit);
			auto object = box<vptr::presentable>{ swapchain };
			execution_.validate_present(payload.queue, object.surface());
			payload.swapchains.emplace_back(::std::move(object));
		}

		template<typename Semaphore>
		void add_present_wait(uint32_t submit, Semaphore& semaphore) {
			semaphore.init();
			auto& payload = present_for(submit);
			payload.waits.emplace_back(box<vptr::queue_semaphore>{ semaphore });
		}
#endif

		void add_sparse_buffer_binds(uint32_t submit,
			cspan<VK_ VkSparseBufferMemoryBindInfo> binds) {
			auto& payload = sparse_for(submit);
			payload.buffer_binds.insert(payload.buffer_binds.end(), binds.begin(), binds.end());
		}

		void record() {
			acquire_command_pools();
			record_commands();
			prepare_operation_storage();
		}

	private:
#if VKTL_HAVE_WINDOW
		present_payload& present_for(uint32_t submit) {
			if (!presents_[submit]) {
				auto& payload = state_.payloads.template emplace_back<present_payload>();
				payload.queue = submits_[submit]->queue;
				presents_[submit] = ::std::addressof(payload);
			}
			return *presents_[submit];
		}
#endif

		sparse_bind_payload& sparse_for(uint32_t submit) {
			if (!sparse_[submit]) {
				auto& payload = state_.payloads.template emplace_back<sparse_bind_payload>();
				payload.queue = submits_[submit]->queue;
				sparse_[submit] = ::std::addressof(payload);
			}
			return *sparse_[submit];
		}

		void acquire_command_pools() {
			for (auto* command : state_.command_index) {
				auto policy = make_command_pool_policy(command->policy);
				auto flags = command->policy_view ? command->policy_view.flags : policy.flags;
				// One pool per command keeps policy ownership and recording simple.
				auto& pool = state_.command_pools.template emplace_back<task_command_pool>();
				pool.worker = command->worker_index;
				pool.family = command->queue_family;
				pool.flags = flags;
				pool.recycle = command->policy_view
					? command->policy_view.recycle : command->policy;
				pool.device = handle_of<vktl::device>(&execution_);
				pool.allocator = execution_.allocator();
				VK_ VkCommandPoolCreateInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.flags = flags,
					.queueFamilyIndex = command->queue_family,
				};
				if (command->policy_view) command->policy_view.apply(info);
				else policy.apply(info);
				VK_ vkCreateCommandPool(handle_of<vktl::device>(&execution_), &info,
					execution_.allocator(), &pool.handle)
					| popup{ "[TASK] Failed to create a command pool." };
				command->pool = ::std::addressof(pool);
			}
		}

		void record_commands() {
			vector<record_request> requests;
			for (auto* command : state_.command_index) {
				requests.emplace_back(record_request{ &execution_, command });
			}

			// record_job is intentionally small; requests own the typed job data.
			vector<record_job> jobs;
			jobs.reserve(requests.size());
			for (auto& request : requests) {
				jobs.emplace_back(record_job{ &record_request::invoke, &request });
			}
			record_job_group group;
			for (uint32_t index = 0u; index < uint32_t(jobs.size()); ++index) {
				jobs[index].group = &group;
				execution_.enqueue(requests[index].command->worker_index,
					jobs[index]);
			}
			group.wait();
		}

		void prepare_operation_storage() {
			state_.operations.clear();
			for (auto& node : state_.payloads) {
				auto& payload = node.as<compiled_payload>();
				payload.prepare();
				if (payload.enabled()) state_.operations.emplace_back(payload.operation);
			}

			for (uint32_t operation = 0u;
				operation < uint32_t(state_.operations.size()); ++operation) {
				auto const& value = state_.operations[operation];
				if (!value.user_completion) continue;
				if (!value.fence_capable) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[TASK] Requested completion payload cannot accept a fence." };
				}
				state_.completion_operations.emplace_back(operation);
			}
			for (uint32_t operation = 0u;
				operation < uint32_t(state_.operations.size()); ++operation) {
				auto const& value = state_.operations[operation];
				if (!value.fence_capable) continue;
				bool later_on_queue = false;
				for (uint32_t later = operation + 1u;
					later < uint32_t(state_.operations.size()); ++later) {
					if (state_.operations[later].fence_capable
						&& state_.operations[later].queue_index == value.queue_index) {
						later_on_queue = true;
						break;
					}
				}
				if (!later_on_queue && ::std::ranges::find(
					state_.completion_operations, operation)
					== state_.completion_operations.end()) {
					state_.completion_operations.emplace_back(operation);
				}
			}
			if (!state_.operations.empty() && state_.completion_operations.empty()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] Submission has no completion-capable payload." };
			}

			state_.submission_slots.resize(2u);
			for (auto& slot : state_.submission_slots) {
				slot.completions.reserve(state_.completion_operations.size());
				slot.completion_submitted.resize(state_.completion_operations.size());
				for (uint32_t index = 0u;
					index < uint32_t(state_.completion_operations.size()); ++index) {
					slot.completions.emplace_back(execution_.create_completion_fence(true));
				}
			}
		}

		Execution& execution_;
		compiled_task& state_;
		poly_list payloads_;
		vector<submit_payload*> submits_;
		vector<sparse_bind_payload*> sparse_;
#if VKTL_HAVE_WINDOW
		vector<present_payload*> presents_;
#endif
	};

	// BEGIN INTERFACE.

	template<typename Builder>
	struct command_context {
		command_context(Builder& builder, uint32_t command) noexcept
			: builder_{ &builder }, command_{ command } {}

		uint32_t index() const noexcept { return command_; }

		template<typename Object>
		command_context& depends(Object& object) {
			builder_->add_dependency(command_, object);
			return *this;
		}

		template<typename Source, typename Destination>
		command_context& depends(Source& source, Destination& destination) {
			builder_->add_dependency(command_, source);
			builder_->add_dependency(command_, destination);
			return *this;
		}

		command_context& pool_policy(command_pool_policy_kind policy) noexcept {
			builder_->set_policy(command_, policy);
			return *this;
		}

		template<typename... Infos>
		command_context& pool(Infos&&... infos) {
			builder_->set_policy_components(command_, static_cast<Infos&&>(infos)...);
			return *this;
		}

		command_context& usage(VK_ VkCommandBufferUsageFlags value) noexcept {
			builder_->set_usage(command_, value);
			return *this;
		}

		template<typename Fn>
		command_context& record(Fn&& function) {
			using recipe = callable_recipe<::std::decay_t<Fn>>;
			builder_->append_recipe(command_, recipe{
				static_cast<Fn&&>(function) });
			return *this;
		}

		template<typename Pass, typename...Scopes>
		auto begin(Pass& pass, Scopes&...scopes) {
			builder_->add_dependency(command_, pass);
			(builder_->add_dependency(command_, scopes), ...);
			return pass_context<Builder, Pass>{ *builder_, command_, pass };
		}

	private:
		Builder* builder_;
		uint32_t command_;
	};

	template<typename Builder>
	struct worker_context {
		worker_context(Builder& builder, uint32_t worker) noexcept
			: builder_{ &builder }, worker_{ worker } {}

		command_context<Builder> commands(queue_duty::type duty,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return { *builder_, builder_->add_command(worker_, duty, level) };
		}

		command_context<Builder> commands(queue_duty::type duty,
			command_pool_policy_kind policy,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return { *builder_, builder_->add_command(worker_, duty, level,
				VK_ VkCommandBufferUsageFlags(0u), policy) };
		}

		command_context<Builder> commands(queue_extensions::graphics_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::graphics, level);
		}
		command_context<Builder> commands(queue_extensions::compute_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::compute, level);
		}
		command_context<Builder> commands(queue_extensions::transfer_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::transfer, level);
		}

	private:
		Builder* builder_;
		uint32_t worker_;
	};

	template<typename Builder, typename Pass>
	struct pass_context {
		pass_context(Builder& builder, uint32_t command, Pass& pass) noexcept
			: builder_{ &builder }, command_{ command }, pass_{ &pass } {}

		auto pipe(uint16_t index) {
			return pipe_context<Builder, Pass>{ *builder_, command_, *pass_, index };
		}
		void end() noexcept {}

	private:
		Builder* builder_;
		uint32_t command_;
		Pass* pass_;
	};

	template<typename Builder, typename Pass>
	struct pipe_context {
		pipe_context(Builder& builder, uint32_t command, Pass& pass, uint16_t index)
			: builder_{ &builder }, command_{ command }, pass_{ &pass }, index_{ index } {
			bind_point_ = object_of<Pass, vktl::pass_extensions::compute_>
				? VK_ VK_PIPELINE_BIND_POINT_COMPUTE : VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
			builder_->append_recipe(command_, bind_pipeline_recipe<Pass>{
				pass_, index_, bind_point_ });
		}

		template<object_of<bind_set_> BindSet>
		pipe_context& bind(BindSet& bind_set, uint32_t set = 0u) {
			builder_->add_dependency(command_, bind_set);
			builder_->append_recipe(command_, bind_descriptor_recipe<BindSet>{
				&bind_set, bind_point_, pass_->pipeline_layout(), set });
			return *this;
		}

		template<object_of<buffer> Buffer>
		pipe_context& bind_vertex_buffer(uint32_t binding, Buffer& buffer,
			VK_ VkDeviceSize offset = 0u) {
			builder_->add_dependency(command_, buffer);
			builder_->append_recipe(command_, bind_vertex_recipe<Buffer>{
				&buffer, binding, offset });
			return *this;
		}

		template<object_of<buffer> Buffer>
		pipe_context& bind_index_buffer(Buffer& buffer,
			VK_ VkDeviceSize offset = 0u,
			VK_ VkIndexType type = VK_ VK_INDEX_TYPE_UINT32) {
			builder_->add_dependency(command_, buffer);
			builder_->append_recipe(command_, bind_index_recipe<Buffer>{
				&buffer, offset, type });
			return *this;
		}

		pipe_context& draw(uint32_t vertices, uint32_t instances = 1u,
			uint32_t first_vertex = 0u, uint32_t first_instance = 0u) {
			builder_->append_recipe(command_, draw_recipe{
				vertices, instances, first_vertex, first_instance });
			return *this;
		}

		pipe_context& dispatch(uint32_t x, uint32_t y = 1u, uint32_t z = 1u) {
			builder_->append_recipe(command_, dispatch_recipe{ x, y, z });
			return *this;
		}

	private:
		Builder* builder_;
		uint32_t command_;
		Pass* pass_;
		uint16_t index_;
		VK_ VkPipelineBindPoint bind_point_;
	};

	template<typename Builder>
	struct submit_context {
		submit_context(Builder& builder, uint32_t submit) noexcept
			: builder_{ &builder }, submit_{ submit } {}

		submit_context& commands(command_context<Builder> const& command) {
			builder_->add_command_to_submit(submit_, command.index());
			return *this;
		}

		template<typename Semaphore>
		submit_context& wait(Semaphore& semaphore, uint64_t value = 0u,
			queue_stage_flags stage = queue_all_commands_stage) {
			builder_->add_wait(submit_, semaphore, value, stage);
			return *this;
		}

		template<typename Semaphore>
		submit_context& signal(Semaphore& semaphore, uint64_t value = 0u,
			queue_stage_flags stage = queue_all_commands_stage) {
			builder_->add_signal(submit_, semaphore, value, stage);
			return *this;
		}

		// submit_context& completion() noexcept {
		// 	builder_->request_completion(submit_);
		// 	return *this;
		// }

#if VKTL_HAVE_WINDOW
		template<typename Swapchain>
		submit_context& present(Swapchain& swapchain) {
			builder_->add_present(submit_, swapchain);
			return *this;
		}

		template<typename Semaphore>
		submit_context& present_wait(Semaphore& semaphore) {
			builder_->add_present_wait(submit_, semaphore);
			return *this;
		}
#endif

		submit_context& bind_sparse(cspan<VK_ VkSparseBufferMemoryBindInfo> binds) {
			builder_->add_sparse_buffer_binds(submit_, binds);
			return *this;
		}

	private:
		Builder* builder_;
		uint32_t submit_;
	};

	template<typename Builder>
	struct refresh_context {
		explicit refresh_context(Builder& builder) noexcept : builder_{ &builder } {}

		worker_context<Builder> worker(uint32_t index) { return { *builder_, index }; }

		submit_context<Builder> submit(command_context<Builder> const& command) {
			return { *builder_, builder_->add_submit(command.index()) };
		}

	private:
		Builder* builder_;
	};

	// END INTERFACE.

	template<typename Fn, typename N>
	struct m<task<Fn>, N> : N {
		using base = N;
		static_assert(!object_of<N, extensions::allocate_from>, 
			"task object use execution's allocator, standalone allocator is not allowed.");

		template<similiar_to<task<Fn>> Info>
		constexpr m(Info&& info, auto&&... others)
			: N{ forward_(others)... }, fn_{ info.func } {}

		m(m const&) = delete;
		m& operator=(m const&) = delete;

		m(m&& other) 
			noexcept(::std::is_nothrow_move_constructible_v<N> && ::std::is_nothrow_move_constructible_v<Fn>)
			requires(::std::move_constructible<N>)
			: N{ static_cast<N&&>(other) }
			, fn_{ ::std::move(other.fn_) }
			, compiled_{ ::std::move(other.compiled_) }
			, active_{ ::std::exchange(other.active_, nullptr) } {
			assert(!other.refreshing_);
		}

		m& operator=(m&&) = delete;

		~m() { release_all(); }

		void refresh() {
			auto* exec = parent_of<vktl::execution>(this);
			exec->init();

			auto _ = locker_of(this);
			assert(!refreshing_);

			refreshing_ = true;
			auto flag = defer{ [&](){ refreshing_ = false; } };

			// Rebuilding is explicit: finish using the old state before replacing it.
			release_all();
			auto& state = compiled_.template emplace_back<compiled_task>();
			try {
				refresh_builder builder{ *exec, state };
				fn_(refresh_context{ builder });
				builder.record();
				active_ = ::std::addressof(state);
			}
			catch (...) {
				destroy_state(state);
				compiled_.erase(state);
				throw;
			}
		}

		task_submission submit(submit_policy policy = {}) {
			auto* execution = parent_of<vktl::execution>(this);
			execution->init();
			if (!active_) {
				if (!policy.allow_refresh) return {};
				refresh();
			}
			auto _ = locker_of(this);
			auto& state = *active_;

			if (state.in_flight_submissions != 0u
				&& !all_commands_allow_simultaneous_use(state)) {
				if (policy.busy == busy_policy::skip) return {};
				wait_oldest(state);
			}
			auto* slot = claim_slot(state);
			if (!slot) {
				if (policy.busy == busy_policy::skip) return {};
				wait_oldest(state);
				slot = claim_slot(state);
				assert(slot);
			}

			state.prepare_submit();
			slot->state = submission_state::preparing;
			slot->successful_operations = 0u;
			::std::ranges::fill(slot->completion_submitted, uint8_t(0u));
			for (auto fence : slot->completions) execution->reset_completion_fence(fence);
			slot->state = submission_state::submitting;
			try {
				for (uint32_t index = 0u;
					index < uint32_t(state.operations.size()); ++index) {
					VK_ VkFence completion = VK_NULL_HANDLE;
					for (uint32_t sink = 0u;
						sink < uint32_t(state.completion_operations.size()); ++sink) {
						if (state.completion_operations[sink] == index) {
							completion = slot->completions[sink];
						}
					}
					execution->invoke(state.operations[index], completion);
					for (uint32_t sink = 0u;
						sink < uint32_t(state.completion_operations.size()); ++sink) {
						if (state.completion_operations[sink] == index)
							slot->completion_submitted[sink] = 1u;
					}
					++slot->successful_operations;
				}
			}
			catch (...) {
				auto failure = ::std::current_exception();
				if (slot->successful_operations == 0u) {
					slot->state = submission_state::complete;
				}
				else {
					slot->state = submission_state::partial_submission;
					establish_recovery_completion(state, *slot);
					++state.in_flight_submissions;
				}
				::std::rethrow_exception(failure);
			}
			if (state.operations.empty()) slot->state = submission_state::complete;
			else {
				slot->state = submission_state::in_flight;
				++state.in_flight_submissions;
			}
			return {
				true, this, ::std::addressof(state), slot,
				[](void* owner, compiled_task& state, submission_slot& value) {
					return static_cast<m*>(owner)->submission_ready(state, value);
				},
				[](void* owner, compiled_task& state, submission_slot& value) {
					static_cast<m*>(owner)->wait_submission(state, value);
				},
			};
		}

		void poll() { collect_completed_states(); }
		void reset() noexcept { release_all(); }

	protected:
		void finalize() { if constexpr (requires { N::finalize(); }) N::finalize(); }
		void relocate() noexcept { N::relocate(); }

	private:
		void collect_completed_states() {
			if (!active_) return;
			for (submission_slot& slot : active_->submission_slots)
				submission_ready(*active_, slot);
		}

		bool submission_ready(compiled_task& state, submission_slot& slot) {
			if (slot.state == submission_state::complete) return true;
			if (slot.state != submission_state::in_flight
				&& slot.state != submission_state::partial_submission) return false;
			auto* exec = parent_of<execution>(this);
			for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
				if (slot.completion_submitted[sink]
					&& !exec->completion_ready(slot.completions[sink])) return false;
			}
			slot.state = submission_state::complete;
			assert(state.in_flight_submissions != 0u);
			--state.in_flight_submissions;
			return true;
		}

		void wait_submission(compiled_task& state, submission_slot& slot) {
			if (slot.state == submission_state::complete) return;
			auto* execution = parent_of<vktl::execution>(this);
			for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
				if (slot.completion_submitted[sink])
					execution->wait_completion(slot.completions[sink]);
			}
			if (slot.state == submission_state::in_flight
				|| slot.state == submission_state::partial_submission) {
				slot.state = submission_state::complete;
				assert(state.in_flight_submissions != 0u);
				--state.in_flight_submissions;
			}
		}

		static bool all_commands_allow_simultaneous_use(compiled_task const& task) noexcept {
			return ::std::ranges::all_of(task.command_index, [](default_command_unit const* command) {
				return bool(command->usage & VK_ VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
			});
		}

		submission_slot* claim_slot(compiled_task& state) noexcept {
			for (auto& slot : state.submission_slots) {
				if (slot.state == submission_state::complete) return ::std::addressof(slot);
			}
			return nullptr;
		}

		void wait_oldest(compiled_task& state) {
			auto* exec = parent_of<execution>(this);
			for (auto& slot : state.submission_slots) {
				if (slot.state != submission_state::in_flight
					&& slot.state != submission_state::partial_submission) continue;
				for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
					if (slot.completion_submitted[sink]) {
						exec->wait_completion(slot.completions[sink]);
					}
				}
				slot.state = submission_state::complete;
				assert(state.in_flight_submissions != 0u);
				--state.in_flight_submissions;
				return;
			}
		}

		void establish_recovery_completion(compiled_task const& state,
			submission_slot& slot) {
			auto* execution = parent_of<vktl::execution>(this);
			for (uint32_t sink = 0u;
				sink < uint32_t(state.completion_operations.size()); ++sink) {
				if (slot.completion_submitted[sink]) continue;
				auto operation = state.completion_operations[sink];
				execution->close_queue(state.operations[operation].queue_index,
					slot.completions[sink]);
				slot.completion_submitted[sink] = 1u;
			}
		}

		void destroy_state(compiled_task& state) noexcept {
			for (auto& slot : state.submission_slots) {
				for (auto fence : slot.completions) {
					parent_of<execution>(this)->destroy_completion_fence(fence);
				}
				slot.completions.clear();
			}
			for (auto& node : state.command_pools) {
				auto& pool = node.template as<task_command_pool>();
				if (pool.handle) reclaim_task_command_pool(::std::addressof(pool));
			}
		}

		void release_all() noexcept {
			// refresh() also uses this path before installing the replacement state.
			for (compiled_task& state : compiled_) {
				while (state.in_flight_submissions != 0u) { wait_oldest(state); }
				destroy_state(state);
			}
			compiled_.clear();
			active_ = nullptr;
		}

	private:
		Fn fn_;
		poly_list compiled_;
		compiled_task* active_ = nullptr;
		bool refreshing_ = false;
	};

	template<typename N>
	struct m<task_extensions::transient_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::transient_, auto&&... values) : N{ forward_(values)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		}
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::transient;
		}
	};

	template<typename N>
	struct m<task_extensions::reset_pool_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::reset_pool_, auto&&... values) : N{ forward_(values)... } {}
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::reset_pool;
		}
	};

	template<typename N>
	struct m<task_extensions::reset_command_buffer_, N> : N {
		static_assert(!have_command_pool_policy<N>, "command-pool policies are mutually exclusive");
		using command_pool_policy_marker = void;
		constexpr m(task_extensions::reset_command_buffer_, auto&&... values)
			: N{ forward_(values)... } {
			this->flags |= VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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
		}
		static constexpr command_pool_policy_kind recycle_mode() noexcept {
			return command_pool_policy_kind::free_command_buffer;
		}
	};

}
