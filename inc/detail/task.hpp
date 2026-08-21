#pragma once

// --- Agents specification -------------------------------------------------
// A task owns every compiled generation, payload, command buffer, command pool,
// submission slot, and completion frontier. Refresh publishes a staging
// generation only after recording and revision validation succeed. Frame
// variants expand independently per command unit. prepare_submit() updates only
// preallocated task-local storage; task::submit() invokes queue operations via
// execution's per-queue service. No execution epoch or task registry is used.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	enum class generation_state : uint8_t { staging, ready, retiring, failed };
	enum class submission_state : uint8_t {
		complete, preparing, submitting, in_flight, partial_submission
	};
	enum class busy_policy : uint8_t { wait, skip };
	struct submit_policy {
		busy_policy busy = busy_policy::wait;
		bool allow_refresh = true;
	};

	struct task_command_pool : poly_list::node {
		uint32_t worker = 0u;
		uint32_t family = uint32_t(invalid);
		VK_ VkCommandPoolCreateFlags flags = 0u;
		command_pool_policy_kind recycle = command_pool_policy_kind::generation;
		uint64_t fingerprint = 0u;
		VK_ VkCommandPool handle = VK_NULL_HANDLE;
		vector<VK_ VkCommandBuffer> buffers;
		VK_ VkDevice device = VK_NULL_HANDLE;
		VK_ VkAllocationCallbacks const* allocator = nullptr;
	};

	inline void reclaim_task_command_pool(void* data) {
		auto& pool = *static_cast<task_command_pool*>(data);
		if (!pool.handle) return;
		if (pool.recycle == command_pool_policy_kind::free_command_buffer
			&& !pool.buffers.empty()) {
			VK_ vkFreeCommandBuffers(pool.device, pool.handle,
				uint32_t(pool.buffers.size()), pool.buffers.data());
		}
		else if (pool.recycle == command_pool_policy_kind::reset_command_buffer) {
			for (auto command : pool.buffers) {
				VK_ vkResetCommandBuffer(command, 0u)
					| popup{ "[TASK] Failed to reset a command buffer." };
			}
		}
		else {
			VK_ vkResetCommandPool(pool.device, pool.handle, 0u)
				| popup{ "[TASK] Failed to reset a command pool." };
		}
		VK_ vkDestroyCommandPool(pool.device,
			::std::exchange(pool.handle, VK_NULL_HANDLE), pool.allocator);
	}

	struct optional_frame_scope {
		bool present = false;
		frame_scope_id identity = 0u;
	};

	template<typename Object>
	constexpr optional_frame_scope frame_scope_of(Object const& object) noexcept {
		if constexpr (requires { object.frame_scope_identity(); }) {
			return { true, frame_scope_id(object.frame_scope_identity()) };
		}
		else return {};
	}

	template<typename Source, typename Destination>
	constexpr void validate_frame_dependency(Source const& source,
		Destination const& destination) noexcept {
		auto lhs = frame_scope_of(source);
		auto rhs = frame_scope_of(destination);
		assert((!lhs.present || !rhs.present || lhs.identity == rhs.identity)
			&& "dependent frame resources must belong to the same frame scope");
	}

	enum class dependency_implementation : uint8_t { pipeline_barrier, semaphore };

	struct dependency_edge {
		uint32_t source_command = uint32_t(invalid);
		uint32_t destination_command = uint32_t(invalid);
		uint32_t source_queue = uint32_t(invalid);
		uint32_t destination_queue = uint32_t(invalid);
		optional_frame_scope frame_scope;
		dependency_implementation implementation =
			dependency_implementation::pipeline_barrier;
	};

	inline void select_implementation(dependency_edge& edge) noexcept {
		edge.implementation = edge.source_queue == edge.destination_queue
			? dependency_implementation::pipeline_barrier
			: dependency_implementation::semaphore;
	}

	inline uint32_t command_variant_count(
		span<box<vptr::frame_related> const> scopes) {
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

	struct command_variant {
		vector<uint32_t> frames;
		vector<uint64_t> revisions;
		VK_ VkCommandBuffer handle = VK_NULL_HANDLE;
	};

	struct command_unit : poly_list::node {
		uint32_t worker_index = 0u;
		uint32_t queue_index = uint32_t(invalid);
		uint32_t queue_family = uint32_t(invalid);
		VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u);
		command_pool_policy_kind policy = command_pool_policy_kind::generation;
		command_pool_policy_view policy_view;
		task_command_pool* pool = nullptr;
		poly_list recipes;
		vector<box<vptr::frame_related>> frame_scopes;
		vector<uint32_t> strides;
		vector<command_variant> variants;
		uint64_t fingerprint = recipe_hash_basis;
	};

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

	struct compiled_task;

	struct compiled_payload : poly_list::node {
		using prepare_fn = void(*)(compiled_payload&);
		using prepare_submit_fn = void(*)(compiled_payload&, compiled_task const&);

		void prepare() { prepare_(*this); }
		void prepare_submit(compiled_task const& task) { prepare_submit_(*this, task); }
		bool enabled() const noexcept { return enabled_; }
		void request_completion() noexcept {
			completion_requested_ = true;
			operation.user_completion = true;
		}

	protected:
		template<typename T>
		void bind_payload() noexcept {
			prepare_ = [](compiled_payload& base) { static_cast<T&>(base).prepare_storage(); };
			prepare_submit_ = [](compiled_payload& base, compiled_task const& task) {
				static_cast<T&>(base).prepare_submit_storage(task);
			};
			operation.invoke = &T::invoke;
			operation.storage = static_cast<T const*>(this);
			operation.fence_capable = T::fence_capable;
		}

		prepare_fn prepare_ = nullptr;
		prepare_submit_fn prepare_submit_ = nullptr;
		bool enabled_ = false;
		bool completion_requested_ = false;

	public:
		queue_operation operation;
		uint64_t fingerprint = recipe_hash_basis;
	};

	struct submit_semaphore_value {
		VK_ VkSemaphore handle = VK_NULL_HANDLE;
		uint64_t value = 0u;
		queue_stage_flags stage = queue_all_commands_stage;
	};

	struct semaphore_operand {
		box<vptr::queue_semaphore> object;
		uint64_t value = 0u;
		queue_stage_flags stage = queue_all_commands_stage;
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
			operation.queue_index = queue;
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
			operation.queue_index = queue;
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
			if (result != VK_ VK_SUCCESS && result != VK_ VK_SUBOPTIMAL_KHR
				&& result != VK_ VK_ERROR_OUT_OF_DATE_KHR) {
				result | popup{ "[EXECUTION] Queue presentation failed." };
			}
		}
	};
#endif

	struct sparse_bind_payload : compiled_payload {
		static constexpr bool fence_capable = true;
		sparse_bind_payload() { bind_payload<sparse_bind_payload>(); }

		uint32_t queue = uint32_t(invalid);
		vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
		vector<box<vptr::queue_semaphore>> waits;
		vector<box<vptr::queue_semaphore>> signals;
		vector<VK_ VkSemaphore> wait_handles;
		vector<VK_ VkSemaphore> signal_handles;

		void prepare_storage() {
			enabled_ = !buffer_binds.empty();
			operation.queue_index = queue;
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

	struct submission_slot {
		submission_state state = submission_state::complete;
		vector<VK_ VkFence> completions;
		vector<uint8_t> completion_submitted;
		uint32_t successful_operations = 0u;
	};

	struct task_submission {
		uint64_t generation = 0u;
		bool accepted = false;
		void* owner = nullptr;
		compiled_task* state = nullptr;
		submission_slot* slot = nullptr;
		bool (*ready_)(void*, compiled_task&, submission_slot&) = nullptr;
		void (*wait_)(void*, compiled_task&, submission_slot&) = nullptr;
		explicit operator bool() const noexcept { return accepted; }
		bool ready() const {
			return !accepted || ready_(owner, *state, *slot);
		}
		void wait() const {
			if (accepted) wait_(owner, *state, *slot);
		}
	};

	struct compiled_task : poly_list::node {
		uint64_t generation = 0u;
		generation_state state = generation_state::staging;
		poly_list commands;
		poly_list policies;
		poly_list payloads;
		poly_list command_pools;
		vector<command_unit*> command_index;
		vector<queue_operation> operations;
		vector<uint32_t> completion_operations;
		vector<submission_slot> submission_slots;
		uint32_t in_flight_submissions = 0u;
		record_job_group reclaim_jobs;
		bool reclaim_dispatched = false;
		uint64_t fingerprint = recipe_hash_basis;
		::std::exception_ptr terminal_error;

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
			auto handle = current_variant(*task.command_index[command_index]).handle;
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

	template<object_of<frame_scope> Object>
	box<vptr::frame_related> make_frame_scope_box(Object& object) {
		return box<vptr::frame_related>{ object };
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
		static constexpr uint32_t max_record_attempts = 8u;

		struct record_request {
			Execution* execution = nullptr;
			command_unit* command = nullptr;
			command_variant* variant = nullptr;

			static void invoke(void* data) {
				auto& self = *static_cast<record_request*>(data);
				if (!self.revisions_match()) return;
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
				frame_selection frames{ self.command->frame_scopes, self.variant->frames };
				command_record_context context{ handle, &frames };
				for (auto const& node : self.command->recipes) {
					node.template as<compiled_recipe>().record(context);
				}
				VK_ vkEndCommandBuffer(handle)
					| popup{ "[TASK] Failed to end command recording." };
				if (self.revisions_match()) self.variant->handle = handle;
			}

			bool revisions_match() const noexcept {
				for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
					if (command->frame_scopes[scope].frame_revision(variant->frames[scope])
						!= variant->revisions[scope]) return false;
				}
				return true;
			}
		};

		refresh_builder(Execution& execution, compiled_task const* active,
			compiled_task& staging, uint32_t& recording_jobs)
			: execution_{ execution }, active_{ active }, staging_{ staging },
				recording_jobs_{ recording_jobs } {
			staging_.generation = active_ ? active_->generation + 1u : 1u;
		}

		uint32_t add_command(uint32_t worker, queue_duty::type duty,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u),
			command_pool_policy_kind policy = command_pool_policy_kind::generation) {
			if (worker >= execution_.thread_count()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] Worker index is out of range." };
			}
			auto queue = execution_.resolve_queue(duty);
			auto& command = staging_.commands.template emplace_back<command_unit>();
			command.worker_index = worker;
			command.queue_index = queue;
			command.queue_family = execution_.queue_family(queue);
			command.level = level;
			command.usage = usage;
			command.policy = policy;
			staging_.command_index.emplace_back(::std::addressof(command));
			return uint32_t(staging_.command_index.size() - 1u);
		}

		void set_policy(uint32_t command, command_pool_policy_kind policy) noexcept {
			staging_.command_index[command]->policy = policy;
			staging_.command_index[command]->policy_view = {};
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
			auto& holder = staging_.policies.template emplace_back<policy_holder<policy_type>>(
				static_cast<Infos&&>(infos)...);
			staging_.command_index[command]->policy_view =
				command_pool_policy_view{ holder.value };
		}

		void set_usage(uint32_t command, VK_ VkCommandBufferUsageFlags usage) noexcept {
			staging_.command_index[command]->usage = usage;
		}

		template<typename Object>
		void add_dependency(uint32_t command, Object& object) {
			object.init();
			if constexpr (requires(Object const& value, uint32_t frame) {
				{ value.frame_scope_identity() } -> ::std::convertible_to<frame_scope_id>;
				{ value.frame_count() } -> ::std::convertible_to<uint32_t>;
				{ value.frame_index() } -> ::std::convertible_to<uint32_t>;
				{ value.frame_revision(frame) } -> ::std::convertible_to<uint64_t>;
			}) {
				auto& scopes = staging_.command_index[command]->frame_scopes;
				auto id = object.frame_scope_identity();
				if (::std::ranges::none_of(scopes, [&](auto const& scope) {
					return scope.frame_scope_identity() == id;
				})) scopes.emplace_back(make_frame_scope_box(object));
			}
		}

		template<typename... Infos>
		void append_recipe(uint32_t command, Infos&&... infos) {
			auto& unit = *staging_.command_index[command];
			auto& recipe = emplace_recipe(unit.recipes, static_cast<Infos&&>(infos)...);
			unit.fingerprint = recipe_hash_value(unit.fingerprint, recipe.fingerprint());
		}

		uint32_t add_submit(uint32_t command) {
			auto& payload = staging_.payloads.template emplace_back<submit_payload>();
			payload.queue = staging_.command_index[command]->queue_index;
			payload.commands.emplace_back(command);
			payload.fingerprint = recipe_hash_value(payload.fingerprint, payload.queue);
			payload.fingerprint = recipe_hash_value(payload.fingerprint, command);
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
			if (payload.queue != staging_.command_index[command]->queue_index) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] A submit cannot contain commands from different queues." };
			}
			payload.commands.emplace_back(command);
			payload.fingerprint = recipe_hash_value(payload.fingerprint, command);
		}

		template<typename Semaphore>
		void add_wait(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->waits.emplace_back(semaphore_operand{
				box<vptr::queue_semaphore>{ semaphore }, value, stage });
			auto& fingerprint = submits_[submit]->fingerprint;
			fingerprint = recipe_hash_pointer(fingerprint, ::std::addressof(semaphore));
			fingerprint = recipe_hash_value(fingerprint, value);
			fingerprint = recipe_hash_value(fingerprint, stage);
		}

		template<typename Semaphore>
		void add_signal(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->signals.emplace_back(semaphore_operand{
				box<vptr::queue_semaphore>{ semaphore }, value, stage });
			auto& fingerprint = submits_[submit]->fingerprint;
			fingerprint = recipe_hash_pointer(fingerprint, ::std::addressof(semaphore));
			fingerprint = recipe_hash_value(fingerprint, value);
			fingerprint = recipe_hash_value(fingerprint, stage);
		}

#if VKTL_HAVE_WINDOW
		template<typename Swapchain>
		void add_present(uint32_t submit, Swapchain& swapchain) {
			swapchain.init();
			auto& payload = present_for(submit);
			auto object = box<vptr::presentable>{ swapchain };
			execution_.validate_present(payload.queue, object.surface());
			payload.swapchains.emplace_back(::std::move(object));
			payload.fingerprint = recipe_hash_pointer(payload.fingerprint,
				::std::addressof(swapchain));
		}

		template<typename Semaphore>
		void add_present_wait(uint32_t submit, Semaphore& semaphore) {
			semaphore.init();
			auto& payload = present_for(submit);
			payload.waits.emplace_back(box<vptr::queue_semaphore>{ semaphore });
			payload.fingerprint = recipe_hash_pointer(payload.fingerprint,
				::std::addressof(semaphore));
		}
#endif

		void add_sparse_buffer_binds(uint32_t submit,
			cspan<VK_ VkSparseBufferMemoryBindInfo> binds) {
			auto& payload = sparse_for(submit);
			payload.buffer_binds.insert(payload.buffer_binds.end(), binds.begin(), binds.end());
			for (auto const& bind : binds) {
				payload.fingerprint = recipe_hash_value(payload.fingerprint, bind);
			}
		}

		bool finish() {
			finish_description();
			expand_frame_variants();
			finish_fingerprint();
			if (active_ && active_->state == generation_state::ready
				&& !active_->terminal_error && equivalent_to(*active_)) {
				return false;
			}
			acquire_command_pools();
			record_commands();
			validate_recorded_revisions();
			prepare_operation_storage();
			staging_.state = generation_state::ready;
			return true;
		}

	private:
#if VKTL_HAVE_WINDOW
		present_payload& present_for(uint32_t submit) {
			if (!presents_[submit]) {
				auto& payload = staging_.payloads.template emplace_back<present_payload>();
				payload.queue = submits_[submit]->queue;
				payload.fingerprint = recipe_hash_value(payload.fingerprint, payload.queue);
				presents_[submit] = ::std::addressof(payload);
			}
			return *presents_[submit];
		}
#endif

		sparse_bind_payload& sparse_for(uint32_t submit) {
			if (!sparse_[submit]) {
				auto& payload = staging_.payloads.template emplace_back<sparse_bind_payload>();
				payload.queue = submits_[submit]->queue;
				payload.fingerprint = recipe_hash_value(payload.fingerprint, payload.queue);
				sparse_[submit] = ::std::addressof(payload);
			}
			return *sparse_[submit];
		}

		void finish_description() {
			for (auto* command : staging_.command_index) {
				command->fingerprint = recipe_hash_value(command->fingerprint, command->worker_index);
				command->fingerprint = recipe_hash_value(command->fingerprint, command->queue_index);
				command->fingerprint = recipe_hash_value(command->fingerprint, command->queue_family);
				command->fingerprint = recipe_hash_value(command->fingerprint, command->level);
				command->fingerprint = recipe_hash_value(command->fingerprint, command->usage);
				if (command->policy_view) {
					command->fingerprint = recipe_hash_value(command->fingerprint,
						command->policy_view.fingerprint);
				}
				else {
					command->fingerprint = recipe_hash_value(command->fingerprint, command->policy);
				}
			}
		}

		void acquire_command_pools() {
			for (auto* command : staging_.command_index) {
				auto policy = make_command_pool_policy(command->policy);
				auto flags = command->policy_view ? command->policy_view.flags : policy.flags;
				auto fingerprint = command->policy_view
					? command->policy_view.fingerprint : policy.fingerprint;
				for (auto& node : staging_.command_pools) {
					auto& existing = node.as<task_command_pool>();
					if (existing.worker == command->worker_index
						&& existing.family == command->queue_family
						&& existing.flags == flags
						&& existing.fingerprint == fingerprint) {
						command->pool = ::std::addressof(existing);
						break;
					}
				}
				if (command->pool) continue;
				auto& pool = staging_.command_pools.template emplace_back<task_command_pool>();
				pool.worker = command->worker_index;
				pool.family = command->queue_family;
				pool.flags = flags;
				pool.recycle = command->policy_view
					? command->policy_view.recycle : command->policy;
				pool.fingerprint = fingerprint;
				pool.device = handle_of<vktl::device>(&execution_);
				pool.allocator = execution_.allocation_callbacks();
				VK_ VkCommandPoolCreateInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.flags = flags,
					.queueFamilyIndex = command->queue_family,
				};
				if (command->policy_view) command->policy_view.apply(info);
				else policy.apply(info);
				VK_ vkCreateCommandPool(handle_of<vktl::device>(&execution_), &info,
					execution_.allocation_callbacks(), &pool.handle)
					| popup{ "[TASK] Failed to create a command pool." };
				command->pool = ::std::addressof(pool);
			}
		}

		void expand_frame_variants() {
			for (auto* command : staging_.command_index) {
				auto count = command_variant_count(command->frame_scopes);
				command->strides.resize(command->frame_scopes.size());
				uint32_t stride = 1u;
				for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
					command->strides[scope] = stride;
					stride *= command->frame_scopes[scope].frame_count();
				}
				command->variants.resize(count);
				for (uint32_t variant_index = 0u; variant_index < count; ++variant_index) {
					auto& variant = command->variants[variant_index];
					variant.frames.resize(command->frame_scopes.size());
					variant.revisions.resize(command->frame_scopes.size());
					for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
						auto frame = (variant_index / command->strides[scope])
							% command->frame_scopes[scope].frame_count();
						variant.frames[scope] = frame;
						variant.revisions[scope] = command->frame_scopes[scope].frame_revision(frame);
						command->fingerprint = recipe_hash_value(command->fingerprint,
							command->frame_scopes[scope].frame_scope_identity());
						command->fingerprint = recipe_hash_value(command->fingerprint,
							variant.revisions[scope]);
					}
				}
			}
		}

		void finish_fingerprint() {
			for (auto* command : staging_.command_index) {
				staging_.fingerprint = recipe_hash_value(staging_.fingerprint,
					command->fingerprint);
			}
			for (auto& node : staging_.payloads) {
				auto& payload = node.as<compiled_payload>();
				staging_.fingerprint = recipe_hash_value(staging_.fingerprint,
					payload.fingerprint);
			}
		}

		bool equivalent_to(compiled_task const& other) const noexcept {
			return staging_.fingerprint == other.fingerprint;
		}

		void record_commands() {
			for (uint32_t attempt = 0u; attempt < max_record_attempts; ++attempt) {
				uint32_t missing = 0u;
				for (auto* command : staging_.command_index) {
					for (auto const& variant : command->variants) if (!variant.handle) ++missing;
				}
				if (missing == 0u) return;

				vector<record_request> requests;
				requests.reserve(missing);
				for (auto* command : staging_.command_index) {
					for (auto& variant : command->variants) {
						if (variant.handle) continue;
						for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
							variant.revisions[scope] = command->frame_scopes[scope]
								.frame_revision(variant.frames[scope]);
						}
						requests.emplace_back(record_request{
							&execution_, command, ::std::addressof(variant) });
					}
				}

				record_job_group group;
				recording_jobs_ = uint32_t(requests.size());
				for (auto& request : requests) {
					execution_.enqueue(request.command->worker_index,
						record_job{ &record_request::invoke, &request, &group });
				}
				try {
					group.wait();
					recording_jobs_ = 0u;
				}
				catch (...) {
					recording_jobs_ = 0u;
					throw;
				}
			}

			throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
				"[TASK] Frame dependencies changed continuously during recording." };
		}

		void validate_recorded_revisions() const {
			for (auto* command : staging_.command_index) {
				for (auto const& variant : command->variants) {
					if (!variant.handle) {
						throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
							"[TASK] A command variant was not recorded." };
					}
					for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
						if (command->frame_scopes[scope].frame_revision(variant.frames[scope])
							!= variant.revisions[scope]) {
							throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
								"[TASK] Frame revisions changed after command recording." };
						}
					}
				}
			}
		}

		void prepare_operation_storage() {
			staging_.operations.clear();
			for (auto& node : staging_.payloads) {
				auto& payload = node.as<compiled_payload>();
				payload.prepare();
				if (payload.enabled()) staging_.operations.emplace_back(payload.operation);
			}

			for (uint32_t operation = 0u;
				operation < uint32_t(staging_.operations.size()); ++operation) {
				auto const& value = staging_.operations[operation];
				if (!value.user_completion) continue;
				if (!value.fence_capable) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[TASK] Requested completion payload cannot accept a fence." };
				}
				staging_.completion_operations.emplace_back(operation);
			}
			for (uint32_t operation = 0u;
				operation < uint32_t(staging_.operations.size()); ++operation) {
				auto const& value = staging_.operations[operation];
				if (!value.fence_capable) continue;
				bool later_on_queue = false;
				for (uint32_t later = operation + 1u;
					later < uint32_t(staging_.operations.size()); ++later) {
					if (staging_.operations[later].fence_capable
						&& staging_.operations[later].queue_index == value.queue_index) {
						later_on_queue = true;
						break;
					}
				}
				if (!later_on_queue && ::std::ranges::find(
					staging_.completion_operations, operation)
					== staging_.completion_operations.end()) {
					staging_.completion_operations.emplace_back(operation);
				}
			}
			if (!staging_.operations.empty() && staging_.completion_operations.empty()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] Submission has no completion-capable payload." };
			}

			staging_.submission_slots.resize(2u);
			for (auto& slot : staging_.submission_slots) {
				slot.completions.reserve(staging_.completion_operations.size());
				slot.completion_submitted.resize(staging_.completion_operations.size());
				for (uint32_t index = 0u;
					index < uint32_t(staging_.completion_operations.size()); ++index) {
					slot.completions.emplace_back(execution_.create_completion_fence(true));
				}
			}
		}

		Execution& execution_;
		compiled_task const* active_ = nullptr;
		compiled_task& staging_;
		uint32_t& recording_jobs_;
		vector<submit_payload*> submits_;
		vector<sparse_bind_payload*> sparse_;
#if VKTL_HAVE_WINDOW
		vector<present_payload*> presents_;
#endif
	};

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
			validate_frame_dependency(source, destination);
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
		command_context& record(Fn&& function, uint64_t revision = 0u) {
			using recipe = callable_recipe<::std::decay_t<Fn>>;
			builder_->append_recipe(command_, recipe{
				static_cast<Fn&&>(function), revision });
			return *this;
		}

		template<typename Pass, typename... Scopes>
		auto begin(Pass& pass, Scopes&... scopes) {
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

		submit_context& completion() noexcept {
			builder_->request_completion(submit_);
			return *this;
		}

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

	template<typename Fn, typename N>
	struct m<task<Fn>, N> : N {
		using base = N;

		template<similiar_to<task<Fn>> Info>
		constexpr m(Info&& info, auto&&... others)
			: N{ forward_(others)... }, fn_{ forward_(info).func } {}

		m(m const&) = delete;
		m& operator=(m const&) = delete;

		m(m&& other) noexcept(
			::std::is_nothrow_move_constructible_v<N>
			&& ::std::is_nothrow_move_constructible_v<Fn>)
			: N{ static_cast<N&&>(other) }
			, fn_{ ::std::move(other.fn_) }
			, compiled_{ ::std::move(other.compiled_) }
			, active_{ ::std::exchange(other.active_, nullptr) } {
			assert(!other.refreshing_ && other.recording_jobs_ == 0u);
		}

		m& operator=(m&&) = delete;

		~m() { release_all(); }

		void refresh() {
			auto* execution = parent_of<vktl::execution>(this);
			execution->init();
			auto _ = locker_of(this);
			assert(!refreshing_ && recording_jobs_ == 0u);
			refreshing_ = true;
			auto flag = defer{ [&] { refreshing_ = false; } };

			auto& staging = compiled_.template emplace_back<compiled_task>();
			try {
				refresh_builder builder{ *execution, active_, staging, recording_jobs_ };
				fn_(refresh_context{ builder });
				if (!builder.finish()) {
					compiled_.erase(staging);
					return;
				}
				// All potentially throwing preparation is complete.
				if (active_) active_->state = generation_state::retiring;
				active_ = ::std::addressof(staging);
				collect_completed_states();
			}
			catch (...) {
				destroy_generation(staging);
				compiled_.erase(staging);
				throw;
			}
		}

		task_submission submit(submit_policy policy = {}) {
			auto* execution = parent_of<vktl::execution>(this);
			execution->init();
			bool needs_refresh = false;
			{
				auto _ = locker_of(this);
				collect_completed_states();
				needs_refresh = !active_ || active_->terminal_error
					|| !current_revisions_match(*active_);
			}
			if (needs_refresh) {
				if (!policy.allow_refresh) return {};
				refresh();
			}
			auto _ = locker_of(this);
			auto& generation = *active_;
			if (generation.terminal_error) ::std::rethrow_exception(generation.terminal_error);
			if (!current_revisions_match(generation)) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] Dependencies changed while preparing submission." };
			}

			if (generation.in_flight_submissions != 0u
				&& !all_commands_allow_simultaneous_use(generation)) {
				if (policy.busy == busy_policy::skip) return {};
				wait_oldest(generation);
			}
			auto* slot = claim_slot(generation);
			if (!slot) {
				if (policy.busy == busy_policy::skip) return {};
				wait_oldest(generation);
				slot = claim_slot(generation);
				assert(slot);
			}

			generation.prepare_submit();
			slot->state = submission_state::preparing;
			slot->successful_operations = 0u;
			::std::ranges::fill(slot->completion_submitted, uint8_t(0u));
			for (auto fence : slot->completions) execution->reset_completion_fence(fence);
			slot->state = submission_state::submitting;
			try {
				for (uint32_t index = 0u;
					index < uint32_t(generation.operations.size()); ++index) {
					VK_ VkFence completion = VK_NULL_HANDLE;
					for (uint32_t sink = 0u;
						sink < uint32_t(generation.completion_operations.size()); ++sink) {
						if (generation.completion_operations[sink] == index) {
							completion = slot->completions[sink];
						}
					}
					execution->invoke(generation.operations[index], completion);
					for (uint32_t sink = 0u;
						sink < uint32_t(generation.completion_operations.size()); ++sink) {
						if (generation.completion_operations[sink] == index)
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
					establish_recovery_completion(generation, *slot);
					generation.terminal_error = failure;
					generation.state = generation_state::failed;
					++generation.in_flight_submissions;
				}
				::std::rethrow_exception(failure);
			}
			if (generation.operations.empty()) slot->state = submission_state::complete;
			else {
				slot->state = submission_state::in_flight;
				++generation.in_flight_submissions;
			}
			return {
				generation.generation, true, this, ::std::addressof(generation), slot,
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
		static bool current_revisions_match(compiled_task const& task) noexcept {
			for (auto* command : task.command_index) {
				auto const& variant = current_variant(*command);
				if (!variant.handle) return false;
				for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
					if (command->frame_scopes[scope].frame_revision(variant.frames[scope])
						!= variant.revisions[scope]) return false;
				}
			}
			return true;
		}

		void collect_completed_states() {
			auto* execution = parent_of<vktl::execution>(this);
			for (auto& node : compiled_) {
				auto& generation = node.template as<compiled_task>();
				for (auto& slot : generation.submission_slots) {
					submission_ready(generation, slot);
				}
			}
			while (!compiled_.empty()) {
				auto& state = compiled_.begin()->template as<compiled_task>();
				if (::std::addressof(state) == active_) break;
				if (state.in_flight_submissions != 0u) break;
				if (!state.reclaim_dispatched) {
					state.reclaim_dispatched = true;
					for (auto& node : state.command_pools) {
						auto& pool = node.template as<task_command_pool>();
						execution->enqueue(pool.worker, record_job{
							&reclaim_task_command_pool, ::std::addressof(pool),
							::std::addressof(state.reclaim_jobs) });
					}
				}
				if (!state.reclaim_jobs.ready()) break;
				state.reclaim_jobs.wait();
				destroy_generation(state);
				compiled_.pop_front();
			}
		}

		bool submission_ready(compiled_task& generation, submission_slot& slot) {
			if (slot.state == submission_state::complete) return true;
			if (slot.state != submission_state::in_flight
				&& slot.state != submission_state::partial_submission) return false;
			auto* execution = parent_of<vktl::execution>(this);
			for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
				if (slot.completion_submitted[sink]
					&& !execution->completion_ready(slot.completions[sink])) return false;
			}
			slot.state = submission_state::complete;
			assert(generation.in_flight_submissions != 0u);
			--generation.in_flight_submissions;
			return true;
		}

		void wait_submission(compiled_task& generation, submission_slot& slot) {
			if (slot.state == submission_state::complete) return;
			auto* execution = parent_of<vktl::execution>(this);
			for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
				if (slot.completion_submitted[sink])
					execution->wait_completion(slot.completions[sink]);
			}
			if (slot.state == submission_state::in_flight
				|| slot.state == submission_state::partial_submission) {
				slot.state = submission_state::complete;
				assert(generation.in_flight_submissions != 0u);
				--generation.in_flight_submissions;
			}
		}

		static bool all_commands_allow_simultaneous_use(compiled_task const& task) noexcept {
			return ::std::ranges::all_of(task.command_index, [](command_unit const* command) {
				return bool(command->usage & VK_ VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
			});
		}

		submission_slot* claim_slot(compiled_task& generation) noexcept {
			for (auto& slot : generation.submission_slots) {
				if (slot.state == submission_state::complete) return ::std::addressof(slot);
			}
			return nullptr;
		}

		void wait_oldest(compiled_task& generation) {
			auto* execution = parent_of<vktl::execution>(this);
			for (auto& slot : generation.submission_slots) {
				if (slot.state != submission_state::in_flight
					&& slot.state != submission_state::partial_submission) continue;
				for (uint32_t sink = 0u; sink < uint32_t(slot.completions.size()); ++sink) {
					if (slot.completion_submitted[sink])
						execution->wait_completion(slot.completions[sink]);
				}
				slot.state = submission_state::complete;
				assert(generation.in_flight_submissions != 0u);
				--generation.in_flight_submissions;
				return;
			}
		}

		void establish_recovery_completion(compiled_task const& generation,
			submission_slot& slot) {
			auto* execution = parent_of<vktl::execution>(this);
			for (uint32_t sink = 0u;
				sink < uint32_t(generation.completion_operations.size()); ++sink) {
				if (slot.completion_submitted[sink]) continue;
				auto operation = generation.completion_operations[sink];
				execution->close_queue(generation.operations[operation].queue_index,
					slot.completions[sink]);
				slot.completion_submitted[sink] = 1u;
			}
		}

		void destroy_generation(compiled_task& generation) noexcept {
			auto* execution = parent_of<vktl::execution>(this);
			for (auto& slot : generation.submission_slots) {
				for (auto fence : slot.completions)
					execution->destroy_completion_fence(fence);
				slot.completions.clear();
			}
			for (auto& node : generation.command_pools) {
				auto& pool = node.template as<task_command_pool>();
				if (pool.handle) reclaim_task_command_pool(::std::addressof(pool));
			}
		}

		void release_all() noexcept {
			auto* execution = parent_of<vktl::execution>(this);
			assert(!refreshing_ && recording_jobs_ == 0u);
			for (auto& node : compiled_) {
				auto& generation = node.template as<compiled_task>();
				while (generation.in_flight_submissions != 0u) wait_oldest(generation);
				if (generation.reclaim_dispatched) {
					try { generation.reclaim_jobs.wait(); }
					catch (...) { ::std::terminate(); }
				}
				destroy_generation(generation);
			}
			compiled_.clear();
			active_ = nullptr;
		}

		Fn fn_;
		poly_list compiled_;
		compiled_task* active_ = nullptr;
		bool refreshing_ = false;
		uint32_t recording_jobs_ = 0u;
	};

}
