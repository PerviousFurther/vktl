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

	struct default_command_pool : poly_list::node {
		uint32_t worker = 0u;
		uint32_t family = uint32_t(invalid);
		VK_ VkCommandPool handle = VK_NULL_HANDLE;
		VK_ VkCommandPoolCreateFlags flags = 0u;
	};

	struct dependency_edge {
		uint32_t source_command = uint32_t(invalid);
		uint32_t destination_command = uint32_t(invalid);
		uint32_t source_queue = uint32_t(invalid);
		uint32_t destination_queue = uint32_t(invalid);
	};

	struct default_command_unit : poly_list::node {
		default_command_pool* pool = nullptr;
		uint32_t worker_index = 0u;
		uint32_t queue_index = uint32_t(invalid);
		uint32_t queue_family = uint32_t(invalid);
		VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u);
		vector<VK_ VkCommandBuffer> handles;

		poly_list recipes;
	};

	struct compiled_task;

	struct compiled_payload : poly_list::node {

	};

	struct submit_payload : compiled_payload {
		using submit_type = VK_ VkSubmitInfo;

		vector<VK_ VkCommandBuffer> command_handles;
		vector<VK_ VkSemaphore> emit_semaphores;
		vectors<VK_ VkSemaphore, VK_ VkPipelineStageFlags> wait_semaphores;

		void fill(vector<VK_ VkSubmitInfo>& infos) {
			infos.emplace_back(VK_ VkSubmitInfo{
				.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
				.pWaitSemaphores = wait_semaphores.data<0u>(),
				.pWaitDstStageMask = wait_semaphores.data<1u>(),
				.commandBufferCount = uint32_t(command_handles.size()),
				.pCommandBuffers = command_handles.data(),
				.signalSemaphoreCount = uint32_t(emit_semaphores.size()),
				.pSignalSemaphores = emit_semaphores.data(),
			});
		}
	};

#if defined(VK_KHR_synchronization2)
	struct submit_payload2 : compiled_payload {
		vectors<VK_ VkCommandBufferSubmitInfoKHR, vector<VK_ VkCommandBuffer>> command_infos;
		vectors<VK_ VkSemaphoreSubmitInfoKHR, vector<VK_ VkSemaphore>> emit_semaphore_infos;
		vectors<VK_ VkSemaphoreSubmitInfoKHR, vector<VK_ VkSemaphore>> wait_semaphore_infos;
		
		void fill(vector<VK_ VkSubmitInfo2KHR>& infos) {
			infos.emplace_back(VK_ VkSubmitInfo2KHR {
				.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
				.waitSemaphoreInfoCount = uint32_t(wait_semaphore_infos.size()),
				.pWaitSemaphoreInfos = wait_semaphore_infos.data<0u>(),
				.commandBufferInfoCount = uint32_t(command_infos.size()),
				.pCommandBufferInfos = command_infos.data<0u>(),
				.signalSemaphoreInfoCount = uint32_t(emit_semaphore_infos.size()),
				.pSignalSemaphoreInfos = emit_semaphore_infos.data<0u>(),
			});
		}
	};
#endif


#if VKTL_HAVE_WINDOW
	struct present_payload : compiled_payload {
		uint32_t queue = uint32_t(invalid);
		vector<box<vptr::presentable>> swapchains;
		vector<box<vptr::queue_semaphore>> waits;
		vector<VK_ VkSwapchainKHR> handles;
		vector<uint32_t> image_indices;
		vector<VK_ VkSemaphore> wait_handles;
		vector<VK_ VkResult> results;

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

	//struct compiled_task : poly_list::node {
	//	// Everything needed to submit one recorded task lives here.
	//	poly_list commands;
	//	poly_list policies;
	//	poly_list payloads;
	//	poly_list command_pools;
	//	vector<default_command_unit*> command_index;
	//	// vector<queue_operation> operations;
	//	vector<uint32_t> completion_operations;
	//	// vector<submission_slot> submission_slots;
	//	uint32_t in_flight_submissions = 0u;
	//};

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

//	template<typename Execution>
//	struct refresh_builder {
//		struct record_request {
//			Execution* execution = nullptr;
//			default_command_unit* command = nullptr;
//
//			static void invoke(void* data) {
//				auto& self = *static_cast<record_request*>(data);
//				assert(self.command->pool && self.command->pool->handle);
//				VK_ VkCommandBufferAllocateInfo allocation{
//					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
//					.commandPool = self.command->pool->handle,
//					.level = self.command->level,
//					.commandBufferCount = 1u,
//				};
//				VK_ VkCommandBuffer handle = VK_NULL_HANDLE;
//				VK_ vkAllocateCommandBuffers(handle_of<vktl::device>(self.execution),
//					&allocation, &handle)
//					| popup{ "[TASK] Failed to allocate a command buffer." };
//				self.command->pool->buffers.emplace_back(handle);
//				VK_ VkCommandBufferBeginInfo begin{
//					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//					.flags = self.command->usage,
//				};
//				VK_ vkBeginCommandBuffer(handle, &begin)
//					| popup{ "[TASK] Failed to begin command recording." };
//				command_record_context context{ handle };
//				for (auto const& node : self.command->recipes) {
//					node.template as<compiled_recipe>().record(context);
//				}
//				VK_ vkEndCommandBuffer(handle)
//					| popup{ "[TASK] Failed to end command recording." };
//				self.command->handle = handle;
//			}
//		};
//
//		refresh_builder(Execution& execution, compiled_task& state)
//			: execution_{ execution }, state_{ state } {}
//
//		// uint32_t add_command(uint32_t worker, queue_duty::type duty,
//		// 	VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
//		// 	VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u)) {
//		// 	assert(worker < execution_.thread_count()); // worker out of range.
//		// 	auto queue = execution_.resolve_queue(duty);
//		// 	auto& command = state_.commands.template emplace_back<default_command_unit>();
//		// 	command.worker_index = worker;
//		// 	command.queue_index = queue;
//		// 	command.queue_family = execution_.queue_family(queue);
//		// 	command.level = level;
//		// 	command.usage = usage;
//		// 	command.policy = policy;
//		// 	state_.command_index.emplace_back(::std::addressof(command));
//		// 	return uint32_t(state_.command_index.size() - 1u);
//		// }
//
//
//
//		void set_policy(uint32_t command, command_pool_policy_kind policy) noexcept {
//			state_.command_index[command]->policy = policy;
//			state_.command_index[command]->policy_view = {};
//		}
//
//		template<typename Policy>
//		struct policy_holder : poly_list::node {
//			Policy value;
//
//			template<typename... Infos>
//			explicit policy_holder(Infos&&... infos)
//				: value{ use_base<command_pool_create_policy>{},
//					static_cast<Infos&&>(infos)... } {}
//		};
//
//		template<typename... Infos>
//		void set_policy_components(uint32_t command, Infos&&... infos) {
//			using policy_type = decltype(object{ use_base<command_pool_create_policy>{},
//				static_cast<Infos&&>(infos)... });
//			auto& holder = state_.policies.template emplace_back<policy_holder<policy_type>>(
//				static_cast<Infos&&>(infos)...);
//			state_.command_index[command]->policy_view =
//				command_pool_policy_view{ holder.value };
//		}
//
//		void set_usage(uint32_t command, VK_ VkCommandBufferUsageFlags usage) noexcept {
//			state_.command_index[command]->usage = usage;
//		}
//
//		template<typename Object>
//		void add_dependency(uint32_t, Object& object) {
//			object.init(); // TODO: Should not directly init.
//		}
//
//		template<typename... Infos>
//		void append_recipe(uint32_t command, Infos&&... infos) {
//			auto& unit = *state_.command_index[command];
//			emplace_recipe(unit.recipes, static_cast<Infos&&>(infos)...);
//		}
//
//		uint32_t add_submit(uint32_t command) {
//			auto& payload = state_.payloads.template emplace_back<submit_payload>();
//			payload.queue = state_.command_index[command]->queue_index;
//			payload.commands.emplace_back(command);
//			submits_.emplace_back(::std::addressof(payload));
//			sparse_.emplace_back(nullptr);
//#if VKTL_HAVE_WINDOW
//			presents_.emplace_back(nullptr);
//#endif
//			return uint32_t(submits_.size() - 1u);
//		}
//
//		void request_completion(uint32_t submit) noexcept {
//			assert(submit < submits_.size());
//			submits_[submit]->request_completion();
//		}
//
//		void add_command_to_submit(uint32_t submit, uint32_t command) {
//			auto& payload = *submits_[submit];
//			if (payload.queue != state_.command_index[command]->queue_index) {
//				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
//					"[TASK] A submit cannot contain commands from different queues." };
//			}
//			payload.commands.emplace_back(command);
//		}
//
//		template<typename Semaphore>
//		void add_wait(uint32_t submit, Semaphore& semaphore, uint64_t value,
//			queue_stage_flags stage) {
//			semaphore.init();
//			submits_[submit]->waits.emplace_back(semaphore_operand{
//				box<vptr::queue_semaphore>{ semaphore }, value, stage });
//		}
//
//		template<typename Semaphore>
//		void add_signal(uint32_t submit, Semaphore& semaphore, uint64_t value,
//			queue_stage_flags stage) {
//			semaphore.init();
//			submits_[submit]->signals.emplace_back(semaphore_operand{
//				box<vptr::queue_semaphore>{ semaphore }, value, stage });
//		}
//
//#if VKTL_HAVE_WINDOW
//		template<typename Swapchain>
//		void add_present(uint32_t submit, Swapchain& swapchain) {
//			swapchain.init();
//			auto& payload = present_for(submit);
//			auto object = box<vptr::presentable>{ swapchain };
//			execution_.validate_present(payload.queue, object.surface());
//			payload.swapchains.emplace_back(::std::move(object));
//		}
//
//		template<typename Semaphore>
//		void add_present_wait(uint32_t submit, Semaphore& semaphore) {
//			semaphore.init();
//			auto& payload = present_for(submit);
//			payload.waits.emplace_back(box<vptr::queue_semaphore>{ semaphore });
//		}
//#endif
//
//		void add_sparse_buffer_binds(uint32_t submit,
//			cspan<VK_ VkSparseBufferMemoryBindInfo> binds) {
//			auto& payload = sparse_for(submit);
//			payload.buffer_binds.insert(payload.buffer_binds.end(), binds.begin(), binds.end());
//		}
//
//		void record() {
//			acquire_command_pools();
//			record_commands();
//			prepare_operation_storage();
//		}
//
//	private:
//#if VKTL_HAVE_WINDOW
//		present_payload& present_for(uint32_t submit) {
//			if (!presents_[submit]) {
//				auto& payload = state_.payloads.template emplace_back<present_payload>();
//				payload.queue = submits_[submit]->queue;
//				presents_[submit] = ::std::addressof(payload);
//			}
//			return *presents_[submit];
//		}
//#endif
//
//		sparse_bind_payload& sparse_for(uint32_t submit) {
//			if (!sparse_[submit]) {
//				auto& payload = state_.payloads.template emplace_back<sparse_bind_payload>();
//				payload.queue = submits_[submit]->queue;
//				sparse_[submit] = ::std::addressof(payload);
//			}
//			return *sparse_[submit];
//		}
//
//		void acquire_command_pools() {
//			for (auto* command : state_.command_index) {
//				auto policy = make_command_pool_policy(command->policy);
//				auto flags = command->policy_view ? command->policy_view.flags : policy.flags;
//				// One pool per command keeps policy ownership and recording simple.
//				auto& pool = state_.command_pools.template emplace_back<default_command_pool>();
//				pool.worker = command->worker_index;
//				pool.family = command->queue_family;
//				pool.flags = flags;
//				pool.recycle = command->policy_view
//					? command->policy_view.recycle : command->policy;
//				pool.device = handle_of<vktl::device>(&execution_);
//				pool.allocator = execution_.allocator();
//				VK_ VkCommandPoolCreateInfo info{
//					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
//					.flags = flags,
//					.queueFamilyIndex = command->queue_family,
//				};
//				if (command->policy_view) command->policy_view.apply(info);
//				else policy.apply(info);
//				VK_ vkCreateCommandPool(handle_of<vktl::device>(&execution_), &info,
//					execution_.allocator(), &pool.handle)
//					| popup{ "[TASK] Failed to create a command pool." };
//				command->pool = ::std::addressof(pool);
//			}
//		}
//
//		void record_commands() {
//			vector<record_request> requests;
//			for (auto* command : state_.command_index) {
//				requests.emplace_back(record_request{ &execution_, command });
//			}
//
//			// record_job is intentionally small; requests own the typed job data.
//			vector<record_job> jobs;
//			jobs.reserve(requests.size());
//			for (auto& request : requests) {
//				jobs.emplace_back(record_job{ &record_request::invoke, &request });
//			}
//			record_job_group group;
//			for (uint32_t index = 0u; index < uint32_t(jobs.size()); ++index) {
//				jobs[index].group = &group;
//				execution_.enqueue(requests[index].command->worker_index,
//					jobs[index]);
//			}
//			group.wait();
//		}
//
//		void prepare_operation_storage() {
//			state_.operations.clear();
//			for (auto& node : state_.payloads) {
//				auto& payload = node.as<compiled_payload>();
//				payload.prepare();
//				if (payload.enabled()) state_.operations.emplace_back(payload.operation);
//			}
//
//			for (uint32_t operation = 0u;
//				operation < uint32_t(state_.operations.size()); ++operation) {
//				auto const& value = state_.operations[operation];
//				if (!value.user_completion) continue;
//				if (!value.fence_capable) {
//					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
//						"[TASK] Requested completion payload cannot accept a fence." };
//				}
//				state_.completion_operations.emplace_back(operation);
//			}
//			for (uint32_t operation = 0u;
//				operation < uint32_t(state_.operations.size()); ++operation) {
//				auto const& value = state_.operations[operation];
//				if (!value.fence_capable) continue;
//				bool later_on_queue = false;
//				for (uint32_t later = operation + 1u;
//					later < uint32_t(state_.operations.size()); ++later) {
//					if (state_.operations[later].fence_capable
//						&& state_.operations[later].queue_index == value.queue_index) {
//						later_on_queue = true;
//						break;
//					}
//				}
//				if (!later_on_queue && ::std::ranges::find(
//					state_.completion_operations, operation)
//					== state_.completion_operations.end()) {
//					state_.completion_operations.emplace_back(operation);
//				}
//			}
//			if (!state_.operations.empty() && state_.completion_operations.empty()) {
//				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
//					"[TASK] Submission has no completion-capable payload." };
//			}
//
//			state_.submission_slots.resize(2u);
//			for (auto& slot : state_.submission_slots) {
//				slot.completions.reserve(state_.completion_operations.size());
//				slot.completion_submitted.resize(state_.completion_operations.size());
//				for (uint32_t index = 0u;
//					index < uint32_t(state_.completion_operations.size()); ++index) {
//					slot.completions.emplace_back(execution_.create_completion_fence(true));
//				}
//			}
//		}
//
//		Execution& execution_;
//		// compiled_task& state_;
//		poly_list payloads_;
//	};

	// template<typename Task>
	// struct command_recorder {
	// 	
	// 	Task* parent_;
	// 
	// };


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


	template<typename Exec>
	struct builder {

		auto worker(uint32_t worker_index) {
			
		}

		Exec* parent;
	};

	template<typename Exec>
	struct worker_adopter {
		constexpr worker_adopter(Exec& parent, uint32_t index)
			: parent{ &parent }
			, index{ index }
		{}

		void push(auto...descs) {

		}

	private:
		Exec* parent;
		uint32_t index;
	};

	template<typename Exec>
	struct root {
		constexpr root(Exec& parent) : parent{&parent} {}

		auto worker(uint32_t index) { return worker_adopter<Exec>{ parent, index }; }

		// execute directly.
		void execute(auto...descs) {
			commands{ ::std::move(descs)... }.invoke();
		}

	private:
		Exec* parent;
	};
	template<typename Builder, typename Pass>
	struct pipe_root {
		constexpr pipe_root(Builder& builder, Pass& pass) {

		}
		
		constexpr void draw(/*VK_ VkDrawIndirectCommand*/) {

		}

	private:
		Builder* builder;
		Pass* pass;
	};
	template<typename Builder, typename Pass>
	struct pass_root {

		constexpr auto pipe(uint32_t index) {

		}

	private:
		Builder* builder;
		Pass* pass;
	};
	template<typename Builder>
	struct commmand_root {
		commmand_root(Builder& parent) {

		}
		auto begin(object_of<pass_> auto& pass) {
			
		}

	private:
		Builder* builder;
	};

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

		m(m&& other) requires(::std::move_constructible<N>)
			: N{ static_cast<N&&>(other) }
			, fn_{ ::std::move(other.fn_) }
			, states_{ ::std::move(other.states_) }
		{ assert(::std::this_thread::get_id() == parent_of<execution>(this)->thread_id()); }

		m& operator=(m&&) = delete;

		~m() { reset(); }

		void refresh() {
			default_builder builder{ N::as_this() };
			fn_(root{ builder });

		}

		void submit() {

		}

		void reset() noexcept { 
			
		}

	private:
		Fn fn_;
		poly_list states_;
	};

}
