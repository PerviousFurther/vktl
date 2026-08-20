#pragma once

// --- Agents specification -------------------------------------------------
// A task owns every compiled generation. `compiled_` is FIFO ordered and
// `active_` changes only after description, queue resolution, pool acquisition,
// recording, revision validation, and task-local storage preparation succeed.
// Refresh rollback erases only its staging node and immediately abandons that
// node's command-pool group; it never mutates the prior active generation.
// Frame variants expand independently per command unit. Selection is an epoch
// marker only; materialization updates preallocated task-local payload columns
// and never calls a Vulkan queue API or allocates.
// Registration occurs from `finalize()`, relocation rebinds the registry view,
// and destruction requires all submitted generations to be complete.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

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
		command_pool_handle_id pool = 0u;
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
		using materialize_fn = void(*)(compiled_payload&, compiled_task const&);

		void prepare() { prepare_(*this); }
		void materialize(compiled_task const& task) { materialize_(*this, task); }
		bool enabled() const noexcept { return enabled_; }

	protected:
		template<typename T>
		void bind_payload() noexcept {
			prepare_ = [](compiled_payload& base) { static_cast<T&>(base).prepare_storage(); };
			materialize_ = [](compiled_payload& base, compiled_task const& task) {
				static_cast<T&>(base).materialize_storage(task);
			};
			operation.invoke = &T::invoke;
			operation.storage = static_cast<T const*>(this);
		}

		prepare_fn prepare_ = nullptr;
		materialize_fn materialize_ = nullptr;
		bool enabled_ = false;

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

	struct submit_payload : compiled_payload {
		submit_payload() { bind_payload<submit_payload>(); }

		uint32_t queue = uint32_t(invalid);
		vector<uint32_t> commands;
		vector<semaphore_operand> waits;
		vector<semaphore_operand> signals;
		queue_dispatch_config dispatch;

		vector<VK_ VkCommandBuffer> command_handles;
		vector<submit_semaphore_value> semaphore_values;
#if defined(VK_KHR_synchronization2)
		vector<VK_ VkCommandBufferSubmitInfoKHR> command_infos;
		vector<VK_ VkSemaphoreSubmitInfoKHR> semaphore_infos;
#endif
		vector<VK_ VkSemaphore> legacy_semaphores;
		vector<VK_ VkPipelineStageFlags> legacy_stages;
		vector<uint64_t> legacy_values;

		void prepare_storage() {
			enabled_ = !commands.empty() || !waits.empty() || !signals.empty();
			operation.queue_index = queue;
			command_handles.resize(commands.size());
			semaphore_values.resize(waits.size() + signals.size());
#if defined(VK_KHR_synchronization2)
			command_infos.resize(commands.size());
			semaphore_infos.resize(waits.size() + signals.size());
#endif
			legacy_semaphores.resize(waits.size() + signals.size());
			legacy_stages.resize(waits.size());
			legacy_values.resize(waits.size() + signals.size());
		}

		void materialize_storage(compiled_task const& task);

		static void invoke(VK_ VkQueue queue_handle, void const* storage) {
			auto& self = *static_cast<submit_payload const*>(storage);
#if defined(VK_KHR_synchronization2)
			if (self.dispatch.submit2) {
				VK_ VkSubmitInfo2KHR info{
					.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
					.waitSemaphoreInfoCount = uint32_t(self.waits.size()),
					.pWaitSemaphoreInfos = self.semaphore_infos.data(),
					.commandBufferInfoCount = uint32_t(self.command_infos.size()),
					.pCommandBufferInfos = self.command_infos.data(),
					.signalSemaphoreInfoCount = uint32_t(self.signals.size()),
					.pSignalSemaphoreInfos = self.semaphore_infos.data() + self.waits.size(),
				};
				self.dispatch.submit2(queue_handle, 1u, &info, VK_NULL_HANDLE)
					| popup{ "[EXECUTION] Queue submission failed." };
				return;
			}
#endif
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
			if (self.dispatch.timeline_semaphore) info.pNext = &timeline;
#endif
			VK_ vkQueueSubmit(queue_handle, 1u, &info, VK_NULL_HANDLE)
				| popup{ "[EXECUTION] Queue submission failed." };
		}
	};

#if VKTL_HAVE_WINDOW
	struct present_payload : compiled_payload {
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

		void materialize_storage(compiled_task const&) {
			for (uint32_t index = 0u; index < uint32_t(swapchains.size()); ++index) {
				handles[index] = swapchains[index].handle();
				image_indices[index] = swapchains[index].frame_index();
			}
			for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
				wait_handles[index] = waits[index].handle();
			}
		}

		static void invoke(VK_ VkQueue queue_handle, void const* storage) {
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

		void materialize_storage(compiled_task const&) {
			for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
				wait_handles[index] = waits[index].handle();
			}
			for (uint32_t index = 0u; index < uint32_t(signals.size()); ++index) {
				signal_handles[index] = signals[index].handle();
			}
		}

		static void invoke(VK_ VkQueue queue_handle, void const* storage) {
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
			VK_ vkQueueBindSparse(queue_handle, 1u, &info, VK_NULL_HANDLE)
				| popup{ "[EXECUTION] Sparse queue binding failed." };
		}
	};

	struct compiled_task : poly_list::node {
		uint64_t generation = 0u;
		uint64_t last_submit_epoch = uint64_t(invalid);
		command_pool_group_id pool_group = 0u;
		poly_list commands;
		poly_list policies;
		poly_list payloads;
		vector<command_unit*> command_index;
		vector<queue_operation> operations;
		uint32_t operation_count = 0u;
		uint64_t fingerprint = recipe_hash_basis;

		void materialize() {
			operation_count = 0u;
			for (auto& node : payloads) {
				auto& payload = node.as<compiled_payload>();
				if (!payload.enabled()) continue;
				payload.materialize(*this);
				assert(operation_count < operations.size());
				operations[operation_count++] = payload.operation;
			}
		}
	};

	inline void submit_payload::materialize_storage(compiled_task const& task) {
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

#if defined(VK_KHR_synchronization2)
		for (uint32_t index = 0u; index < uint32_t(command_handles.size()); ++index) {
			command_infos[index] = VK_ VkCommandBufferSubmitInfoKHR{
				.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
				.commandBuffer = command_handles[index],
			};
		}
		for (uint32_t index = 0u; index < uint32_t(semaphore_values.size()); ++index) {
			auto const& operand = semaphore_values[index];
			semaphore_infos[index] = VK_ VkSemaphoreSubmitInfoKHR{
				.sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
				.semaphore = operand.handle,
				.value = operand.value,
				.stageMask = operand.stage,
			};
		}
#endif
		for (uint32_t index = 0u; index < uint32_t(semaphore_values.size()); ++index) {
			legacy_semaphores[index] = semaphore_values[index].handle;
			legacy_values[index] = semaphore_values[index].value;
			if (index < waits.size()) legacy_stages[index] = legacy_stage(semaphore_values[index].stage);
		}
	}

	template<typename Object>
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
				auto handle = self.execution->allocate_command_buffer(
					self.command->worker_index, self.command->pool, self.command->level);
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
			payload.dispatch = execution_.dispatch();
			payload.fingerprint = recipe_hash_value(payload.fingerprint, payload.queue);
			payload.fingerprint = recipe_hash_value(payload.fingerprint, command);
			submits_.emplace_back(::std::addressof(payload));
			sparse_.emplace_back(nullptr);
#if VKTL_HAVE_WINDOW
			presents_.emplace_back(nullptr);
#endif
			return uint32_t(submits_.size() - 1u);
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
			staging_.pool_group = execution_.acquire_pool_group();
			acquire_command_pools();
			expand_frame_variants();
			finish_fingerprint();
			if (active_ && equivalent_to(*active_)) {
				execution_.abandon_pool_group(staging_.pool_group);
				staging_.pool_group = 0u;
				return false;
			}
			record_commands();
			validate_recorded_revisions();
			prepare_operation_storage();
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
				if (command->policy_view) {
					command->pool = execution_.acquire_command_pool(staging_.pool_group,
						command->worker_index, command->queue_family, command->policy_view);
				}
				else {
					auto policy = make_command_pool_policy(command->policy);
					command->pool = execution_.acquire_command_pool(staging_.pool_group,
						command->worker_index, command->queue_family, policy);
				}
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
			uint32_t count = 0u;
			for (auto& node : staging_.payloads) {
				auto& payload = node.as<compiled_payload>();
				payload.prepare();
				if (payload.enabled()) ++count;
			}
			staging_.operations.resize(count);
			staging_.operation_count = 0u;
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
			, active_{ ::std::exchange(other.active_, nullptr) }
			, registration_id_{ ::std::exchange(other.registration_id_, 0u) }
			, selected_epoch_{ ::std::exchange(other.selected_epoch_, uint64_t(invalid)) } {
			assert(!other.refreshing_ && other.recording_jobs_ == 0u);
		}

		m& operator=(m&&) = delete;

		~m() { detach_and_release(); }

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
				active_ = ::std::addressof(staging);
				collect_completed_states();
			}
			catch (...) {
				execution->abandon_pool_group(staging.pool_group);
				compiled_.erase(staging);
				throw;
			}
		}

		void submit(bool selected = true) {
			auto _ = locker_of(this);
			if (!active_) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] refresh() is required before submit()." };
			}
			auto epoch = parent_of<vktl::execution>(this)->current_epoch();
			selected_epoch_ = selected ? epoch : uint64_t(invalid);
		}

		bool selected(uint64_t epoch) const noexcept {
			return selected_epoch_ == epoch;
		}

		void materialize(uint64_t epoch) {
			assert(selected(epoch) && active_);
			validate_current_revisions(*active_);
			active_->materialize();
		}

		cspan<queue_operation> operations() const noexcept {
			if (!active_) return {};
			return { active_->operations.data(), active_->operation_count };
		}

		void submitted(uint64_t epoch) noexcept {
			assert(selected(epoch) && active_);
			active_->last_submit_epoch = epoch;
		}

		void complete(uint64_t) noexcept { collect_completed_states(); }

		void reset() noexcept { detach_and_release(); }

	protected:
		void finalize() {
			if constexpr (requires { N::finalize(); }) N::finalize();
			registration_id_ = parent_of<vktl::execution>(this)->attach_task(*this);
		}

		void relocate() noexcept {
			N::relocate();
			if (registration_id_) {
				parent_of<vktl::execution>(this)->rebind_task(registration_id_, *this);
			}
		}

	private:
		static void validate_current_revisions(compiled_task const& task) {
			for (auto* command : task.command_index) {
				auto const& variant = current_variant(*command);
				if (!variant.handle) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[TASK] Command recording is missing; refresh is required." };
				}
				for (uint32_t scope = 0u; scope < uint32_t(command->frame_scopes.size()); ++scope) {
					if (command->frame_scopes[scope].frame_revision(variant.frames[scope])
						!= variant.revisions[scope]) {
						throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
							"[TASK] A command dependency changed; refresh is required." };
					}
				}
			}
		}

		void collect_completed_states() noexcept {
			if (!registration_id_) return;
			auto* execution = parent_of<vktl::execution>(this);
			while (!compiled_.empty()) {
				
				auto& state = compiled_.begin()->template as<compiled_task>();
				if (::std::addressof(state) == active_) break;
				if (!execution->epoch_complete(state.last_submit_epoch)) break;
				execution->release_pool_group(state.pool_group);
				compiled_.pop_front();
			}
		}

		void detach_and_release() noexcept {
			if (!registration_id_) return;
			auto* exec = parent_of<vktl::execution>(this);
			assert(!refreshing_ && recording_jobs_ == 0u);
			assert(selected_epoch_ != exec->current_epoch());
			assert(::std::ranges::all_of(compiled_, [](compiled_task const& state) { return exec->epoch_complete(state.last_submit_epoch); }));
			exec->detach_task(::std::exchange(registration_id_, 0u));
			for (auto const& node : compiled_) {
				exec->release_pool_group(node.template as<compiled_task>().pool_group);
			}
			compiled_.clear();
			active_ = nullptr;
		}

		Fn fn_;
		poly_list compiled_;
		compiled_task* active_ = nullptr;
		uint64_t registration_id_ = 0u;
		uint64_t selected_epoch_ = uint64_t(invalid);
		bool refreshing_ = false;
		uint32_t recording_jobs_ = 0u;
	};

}
