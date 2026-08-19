#pragma once

// --- Agents specification -------------------------------------------------
// `refresh()` transactionally describes command units and queue payloads,
// expands frame variants per command unit, records stale variants, sizes the
// execution pools, and publishes only a complete revision-validated plan. A
// failed refresh releases its own command-pool key and leaves the active plan
// untouched.
//
// Command validity is derived from a real FNV-1a recipe revision, stable frame
// scope identities, and per-frame revisions. There is no authoritative dirty
// enum, and no revision may be produced by a counter: a counter would make
// every refresh look changed and disable variant reuse entirely.
//
// `submit()` only selects the current variants and drives each plan payload
// through `vptr::queue_payload`; it contains no payload type switch and no
// allocation. Adding a payload kind means adding a `trait<Payload>` plus a
// declaration in the plan arena, never an edit to `submit()`.
//
// A task has a stable lazy ID, submits at most once per epoch, and detaches
// through execution without freeing live GPU storage immediately.
// Synchronization uses KHR-suffixed APIs when the device provides them, and all
// presentation paths are excluded when `VKTL_HAVE_WINDOW` is false.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

	// -----------------------------------------------------------------------
	// Recipe fingerprints
	// -----------------------------------------------------------------------

	inline constexpr uint64_t recipe_hash_basis = 1469598103934665603ull;
	inline constexpr uint64_t recipe_hash_prime = 1099511628211ull;

	// Real FNV-1a. This is load bearing: the fingerprint is what lets a refresh
	// recognize that a command unit has not changed and reuse its recorded
	// command buffers instead of re-recording every variant.
	inline constexpr uint64_t recipe_hash_bytes(uint64_t hash, void const* data, size_t size) noexcept {
		auto const* bytes = static_cast<unsigned char const*>(data);
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

	// Stable within a process, allocated once per type. Deliberately not a static
	// object address, which is not a valid runtime type key.
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

	// -----------------------------------------------------------------------
	// Recipe payloads
	// -----------------------------------------------------------------------

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
	struct callable_command_recipe {
		Fn function;

		static void invoke(void const* payload, VK_ VkCommandBuffer command,
			frame_selection const& selection) {
			auto& self = *static_cast<callable_command_recipe const*>(payload);
			if constexpr (::std::invocable<Fn const&, VK_ VkCommandBuffer, frame_selection const&>) {
				self.function(command, selection);
			}
			else if constexpr (::std::invocable<Fn const&, VK_ VkCommandBuffer>) {
				self.function(command);
			}
			else {
				static_assert(always_false<Fn>,
					"A command recipe must accept VkCommandBuffer, optionally with frame_selection.");
			}
		}
	};

	struct draw_command_recipe {
		uint32_t vertex_count = 0u;
		uint32_t instance_count = 1u;
		uint32_t first_vertex = 0u;
		uint32_t first_instance = 0u;

		static void invoke(void const* payload, VK_ VkCommandBuffer command, frame_selection const&) {
			auto const& self = *static_cast<draw_command_recipe const*>(payload);
			VK_ vkCmdDraw(command, self.vertex_count, self.instance_count,
				self.first_vertex, self.first_instance);
		}
	};

	struct dispatch_command_recipe {
		uint32_t x = 1u, y = 1u, z = 1u;

		static void invoke(void const* payload, VK_ VkCommandBuffer command, frame_selection const&) {
			auto const& self = *static_cast<dispatch_command_recipe const*>(payload);
			VK_ vkCmdDispatch(command, self.x, self.y, self.z);
		}
	};

	template<typename Pass>
	struct bind_pipeline_command_recipe {
		Pass* pass = nullptr;
		uint16_t index = 0u;
		VK_ VkPipelineBindPoint bind_point = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;

		static void invoke(void const* payload, VK_ VkCommandBuffer command, frame_selection const&) {
			auto const& self = *static_cast<bind_pipeline_command_recipe const*>(payload);
			auto pipeline = self.pass->pipe(self.index);
			assert(pipeline != VK_NULL_HANDLE);
			VK_ vkCmdBindPipeline(command, self.bind_point, pipeline);
		}
	};

	template<typename BindSet>
	struct bind_descriptor_command_recipe {
		BindSet* bind_set = nullptr;
		VK_ VkPipelineBindPoint bind_point = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
		VK_ VkPipelineLayout layout = VK_NULL_HANDLE;
		uint32_t set = 0u;

		static void invoke(void const* payload, VK_ VkCommandBuffer command,
			frame_selection const& selection) {
			auto const& self = *static_cast<bind_descriptor_command_recipe const*>(payload);
			auto frame = selected_frame(*self.bind_set, selection);
			auto descriptor = self.bind_set->descriptor_set(frame, self.set);
			assert(descriptor != VK_NULL_HANDLE && self.layout != VK_NULL_HANDLE);
			VK_ vkCmdBindDescriptorSets(command, self.bind_point, self.layout,
				self.set, 1u, &descriptor, 0u, nullptr);
		}
	};

	template<typename Buffer>
	struct bind_vertex_buffer_command_recipe {
		Buffer* buffer = nullptr;
		uint32_t binding = 0u;
		VK_ VkDeviceSize offset = 0u;

		static void invoke(void const* payload, VK_ VkCommandBuffer command,
			frame_selection const& selection) {
			auto const& self = *static_cast<bind_vertex_buffer_command_recipe const*>(payload);
			auto frame = selected_frame(*self.buffer, selection);
			auto locked_handle = self.buffer->handle(frame);
			auto handle = locked_handle.value;
			VK_ vkCmdBindVertexBuffers(command, self.binding, 1u, &handle, &self.offset);
		}
	};

	template<typename Buffer>
	struct bind_index_buffer_command_recipe {
		Buffer* buffer = nullptr;
		VK_ VkDeviceSize offset = 0u;
		VK_ VkIndexType type = VK_ VK_INDEX_TYPE_UINT32;

		static void invoke(void const* payload, VK_ VkCommandBuffer command,
			frame_selection const& selection) {
			auto const& self = *static_cast<bind_index_buffer_command_recipe const*>(payload);
			auto frame = selected_frame(*self.buffer, selection);
			auto locked_handle = self.buffer->handle(frame);
			VK_ vkCmdBindIndexBuffer(command, locked_handle.value, self.offset, self.type);
		}
	};

	// -----------------------------------------------------------------------
	// Context declarations
	// -----------------------------------------------------------------------

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

	// A frame-scope box must be a non-owning view. `box`'s single-pointer
	// constructor adopts ownership whenever the pointee does not expose
	// reference counting, which would delete the caller's object, so binding
	// always goes through the `object<T>&` overload instead.
	template<typename Object>
	box<vptr::frame_related> make_frame_scope_box(Object& object) {
		return box<vptr::frame_related>{ object };
	}

	// -----------------------------------------------------------------------
	// Refresh builder
	// -----------------------------------------------------------------------

	template<typename Execution>
	struct refresh_builder {
		static constexpr uint32_t max_record_attempts = 8u;

		struct record_request {
			Execution* execution = nullptr;
			command_unit* command = nullptr;
			command_variant* variant = nullptr;
			uint64_t plan_key = 0u;

			static void invoke(void* data) {
				auto& self = *static_cast<record_request*>(data);
				// Captured revisions are validated before recording and again
				// before publishing, so a scope that changed mid-flight simply
				// discards this attempt instead of publishing a stale buffer.
				if (!self.revisions_match()) return;

				auto handle = self.execution->allocate_command_buffer(
					self.command->worker_index, self.command->queue_family,
					self.plan_key, self.command->level);
				VK_ VkCommandBufferBeginInfo begin{
					.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				};
				VK_ vkBeginCommandBuffer(handle, &begin)
					| popup{ "[TASK] Failed to begin command buffer recording." };
				frame_selection selection{ self.command->frame_scopes, self.variant->frames };
				for (auto const& operation : self.command->recipe) {
					operation.record(operation.payload, handle, selection);
				}
				VK_ vkEndCommandBuffer(handle)
					| popup{ "[TASK] Failed to end command buffer recording." };

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

	public:
		refresh_builder(Execution& execution, ::std::shared_ptr<task_plan> old_plan)
			: execution_{ execution }
			, old_plan_{ ::std::move(old_plan) }
			, plan_{ ::std::make_shared<task_plan>() } {
			plan_->generation = old_plan_ ? old_plan_->generation + 1u : 1u;
			plan_->key = execution_.acquire_plan_key();
		}

		// --- command declaration --------------------------------------------

		uint32_t add_command(uint32_t worker, queue_duty::type duty,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			if (worker >= execution_.thread_count()) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] Worker index is out of range." };
			}
			auto queue = execution_.resolve_queue(duty);
			plan_->commands.emplace_back(command_unit{
				.worker_index = worker,
				.queue_index = queue,
				.queue_family = execution_.queue_family(queue),
				.level = level,
				.recipe_revision = recipe_hash_basis,
			});
			return uint32_t(plan_->commands.size() - 1u);
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
				auto& scopes = plan_->commands[command].frame_scopes;
				auto id = object.frame_scope_identity();
				if (::std::ranges::none_of(scopes, [&](auto const& scope) {
					return scope.frame_scope_identity() == id;
				})) {
					scopes.emplace_back(make_frame_scope_box(object));
				}
			}
		}

		template<typename Payload, typename...Args>
		void append_recipe(uint32_t command, uint64_t fingerprint, Args&&...args) {
			auto& payload = plan_->arena.template emplace<Payload>(static_cast<Args&&>(args)...);
			auto& unit = plan_->commands[command];
			unit.recipe.emplace_back(recipe_operation{
				.payload = ::std::addressof(payload),
				.record = &Payload::invoke,
				.fingerprint = fingerprint,
			});
			unit.recipe_revision = recipe_hash_value(unit.recipe_revision, fingerprint);
		}

		// --- submission declaration -------------------------------------------

		uint32_t add_submit(uint32_t command) {
			assert(command < plan_->commands.size());
			auto& declaration = plan_->arena.template emplace<submit_payload>();
			declaration.queue = plan_->commands[command].queue_index;
			declaration.commands.emplace_back(command);
			submits_.emplace_back(::std::addressof(declaration));
			// The per-submit side tables stay the same length as `submits_`.
			sparse_.emplace_back(nullptr);
#if VKTL_HAVE_WINDOW
			presents_.emplace_back(nullptr);
#endif
			return uint32_t(submits_.size() - 1u);
		}

		void add_command_to_submit(uint32_t submit, uint32_t command) {
			auto& declaration = *submits_[submit];
			if (plan_->commands[command].queue_index != declaration.queue) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] One submit unit cannot contain command buffers from different queues." };
			}
			declaration.commands.emplace_back(command);
		}

		template<typename Semaphore>
		void add_wait(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->waits.emplace_back(make_semaphore_operand(semaphore, value, stage));
		}

		template<typename Semaphore>
		void add_signal(uint32_t submit, Semaphore& semaphore, uint64_t value,
			queue_stage_flags stage) {
			semaphore.init();
			submits_[submit]->signals.emplace_back(make_semaphore_operand(semaphore, value, stage));
		}

#if VKTL_HAVE_WINDOW
		// Presentation is not a command-context operation and never shares a
		// payload with a submission: `vkQueuePresentKHR` is its own queue
		// operation with its own binary wait semaphores.
		template<typename Swapchain>
		void add_present(uint32_t submit, Swapchain& swapchain) {
			swapchain.init();
			auto& declaration = present_for(submit);
			auto object = box<vptr::presentable>{ swapchain };
			execution_.validate_present(declaration.queue, object.surface());
			declaration.swapchains.emplace_back(::std::move(object));
		}

		template<typename Semaphore>
		void add_present_wait(uint32_t submit, Semaphore& semaphore) {
			semaphore.init();
			present_for(submit).waits.emplace_back(box<vptr::queue_semaphore>{ semaphore });
		}
#endif

		void add_sparse_buffer_binds(uint32_t submit,
			cspan<VK_ VkSparseBufferMemoryBindInfo> binds) {
			auto& declaration = sparse_for(submit);
			declaration.buffer_binds.insert(declaration.buffer_binds.end(),
				binds.begin(), binds.end());
		}

		// --- commit -----------------------------------------------------------

		void finish(uint64_t task_id) {
			try {
				prepare_variants();
				record_missing_variants();
				collect_payloads();
				compute_requirements();
				execution_.publish(task_id, plan_);
			}
			catch (...) {
				// Nothing was published, so the command pools this staging plan
				// claimed can be recycled immediately.
				execution_.release_plan_key(::std::exchange(plan_->key, 0u));
				throw;
			}
		}

		::std::shared_ptr<task_plan> const& plan() const noexcept { return plan_; }
		uint64_t generation() const noexcept { return plan_->generation; }

	private:
		template<typename Semaphore>
		static semaphore_operand make_semaphore_operand(Semaphore& semaphore,
			uint64_t value, queue_stage_flags stage) {
			return semaphore_operand{
				.object = box<vptr::queue_semaphore>{ semaphore },
				.value = value,
				.stage = stage,
			};
		}

#if VKTL_HAVE_WINDOW
		present_payload& present_for(uint32_t submit) {
			if (!presents_[submit]) {
				auto& declaration = plan_->arena.template emplace<present_payload>();
				declaration.queue = submits_[submit]->queue;
				presents_[submit] = ::std::addressof(declaration);
			}
			return *presents_[submit];
		}
#endif

		sparse_bind_payload& sparse_for(uint32_t submit) {
			if (!sparse_[submit]) {
				auto& declaration = plan_->arena.template emplace<sparse_bind_payload>();
				declaration.queue = submits_[submit]->queue;
				sparse_[submit] = ::std::addressof(declaration);
			}
			return *sparse_[submit];
		}

		void prepare_variants() {
			for (uint32_t index = 0u; index < uint32_t(plan_->commands.size()); ++index) {
				auto& command = plan_->commands[index];
				command.recipe_revision = recipe_hash_value(command.recipe_revision, command.worker_index);
				command.recipe_revision = recipe_hash_value(command.recipe_revision, command.queue_index);
				command.recipe_revision = recipe_hash_value(command.recipe_revision, command.level);

				auto const variant_count = command_variant_count(command.frame_scopes);
				command.strides.resize(command.frame_scopes.size());
				uint32_t stride = 1u;
				for (uint32_t scope = 0u; scope < uint32_t(command.frame_scopes.size()); ++scope) {
					command.strides[scope] = stride;
					stride *= command.frame_scopes[scope].frame_count();
				}
				assert(stride == variant_count);
				command.variants.resize(variant_count);

				command_unit const* old = nullptr;
				if (old_plan_ && index < old_plan_->commands.size()) {
					auto const& candidate = old_plan_->commands[index];
					if (same_recipe_and_scopes(command, candidate)) old = ::std::addressof(candidate);
				}

				for (uint32_t variant_index = 0u; variant_index < variant_count; ++variant_index) {
					auto& variant = command.variants[variant_index];
					variant.frames.resize(command.frame_scopes.size());
					variant.revisions.resize(command.frame_scopes.size());
					for (uint32_t scope = 0u; scope < uint32_t(command.frame_scopes.size()); ++scope) {
						auto count = command.frame_scopes[scope].frame_count();
						auto frame = (variant_index / command.strides[scope]) % count;
						variant.frames[scope] = frame;
						variant.revisions[scope] = command.frame_scopes[scope].frame_revision(frame);
					}
					if (old && variant_index < old->variants.size()) {
						auto const& previous = old->variants[variant_index];
						if (previous.handle && previous.frames == variant.frames
							&& previous.revisions == variant.revisions) {
							variant.handle = previous.handle;
							variant.generation = previous.generation;
						}
					}
				}
			}

			// Reused command buffers still live in the previous plan's pools, so
			// the new plan takes over that ownership key outright. The old plan
			// gives the key up in the same step: if both held it, retiring the old
			// plan would reset pools the new one is still submitting from.
			if (old_plan_ && old_plan_->key != 0u && reuses_old_storage()) {
				auto unused = ::std::exchange(plan_->key, ::std::exchange(old_plan_->key, 0u));
				execution_.release_plan_key(unused);
			}
		}

		bool reuses_old_storage() const noexcept {
			for (auto const& command : plan_->commands) {
				for (auto const& variant : command.variants) {
					if (variant.handle) return true;
				}
			}
			return false;
		}

		static bool same_recipe_and_scopes(command_unit const& left, command_unit const& right) noexcept {
			if (left.worker_index != right.worker_index || left.queue_index != right.queue_index
				|| left.level != right.level || left.recipe_revision != right.recipe_revision
				|| left.frame_scopes.size() != right.frame_scopes.size()) return false;
			for (uint32_t scope = 0u; scope < uint32_t(left.frame_scopes.size()); ++scope) {
				if (left.frame_scopes[scope].frame_scope_identity()
					!= right.frame_scopes[scope].frame_scope_identity()) return false;
			}
			return true;
		}

		void record_missing_variants() {
			for (uint32_t attempt = 0u; attempt < max_record_attempts; ++attempt) {
				uint32_t missing = 0u;
				for (auto const& command : plan_->commands) {
					for (auto const& variant : command.variants) if (!variant.handle) ++missing;
				}
				if (missing == 0u) return;

				vector<record_request> requests;
				requests.reserve(missing);
				record_job_group group;

				for (auto& command : plan_->commands) {
					for (auto& variant : command.variants) {
						if (variant.handle) continue;
						for (uint32_t scope = 0u; scope < uint32_t(command.frame_scopes.size()); ++scope) {
							variant.revisions[scope] =
								command.frame_scopes[scope].frame_revision(variant.frames[scope]);
						}
						requests.emplace_back(record_request{
							::std::addressof(execution_), ::std::addressof(command),
							::std::addressof(variant), plan_->key });
					}
				}
				for (auto& request : requests) {
					execution_.enqueue(request.command->worker_index,
						record_job{ &record_request::invoke, ::std::addressof(request), &group });
				}
				group.wait();

				for (auto& command : plan_->commands) {
					for (auto& variant : command.variants) {
						if (variant.handle) variant.generation = plan_->generation;
					}
				}
			}

			throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
				"[TASK] Frame dependencies kept changing; recording could not converge." };
		}

		// Turns the described submissions into ordered, heterogeneous payload
		// references. The order of this vector is the order of queue operations.
		void collect_payloads() {
			for (uint32_t index = 0u; index < uint32_t(submits_.size()); ++index) {
				auto const* submit = submits_[index];
				if (!submit->empty()) plan_->payloads.emplace_back(payload_ref{ *submit });
				if (sparse_[index] && !sparse_[index]->empty()) {
					plan_->payloads.emplace_back(payload_ref{ *sparse_[index] });
				}
#if VKTL_HAVE_WINDOW
				if (presents_[index] && !presents_[index]->empty()) {
					plan_->payloads.emplace_back(payload_ref{ *presents_[index] });
				}
#endif
			}
		}

		// Capacity is asked of the payloads themselves; nothing here knows what a
		// submit, a present, or a sparse bind is.
		void compute_requirements() {
			for (auto const& payload : plan_->payloads) {
				auto requirement = payload.count();
				if (requirement.required_duty != queue_duty::none
					&& (execution_.queue_duties(requirement.queue_index) & requirement.required_duty)
						!= requirement.required_duty) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[TASK] A queue payload was routed to a queue without the required duty." };
				}
				auto found = ::std::ranges::find_if(plan_->requirements,
					[&](payload_requirement const& value) {
						return value.type == requirement.type
							&& value.queue_index == requirement.queue_index;
					});
				if (found == plan_->requirements.end()) {
					plan_->requirements.emplace_back(requirement);
				}
				else {
					found->capacity += requirement.capacity;
				}
			}
		}

	private:
		Execution& execution_;
		::std::shared_ptr<task_plan> old_plan_;
		::std::shared_ptr<task_plan> plan_;

		vector<submit_payload*> submits_;
		vector<sparse_bind_payload*> sparse_;
#if VKTL_HAVE_WINDOW
		vector<present_payload*> presents_;
#endif
	};

	// -----------------------------------------------------------------------
	// Contexts
	// -----------------------------------------------------------------------

	template<typename Builder>
	struct command_context {
	public:
		command_context(Builder& builder, uint32_t command) noexcept
			: builder_{ &builder }, command_{ command } {}

		uint32_t index() const noexcept { return command_; }

		template<typename Object>
		command_context& depends(Object& object) {
			builder_->add_dependency(command_, object);
			return *this;
		}

		// A free-form recipe cannot be compared structurally, so its identity is
		// the callable's type plus an explicit revision. Bump `revision` whenever
		// the captured state changes the recorded commands, otherwise the buffer
		// is legitimately reused.
		template<typename Fn>
		command_context& record(Fn&& function, uint64_t revision = 0u) {
			using payload = callable_command_recipe<::std::decay_t<Fn>>;
			auto fingerprint = recipe_hash_type<payload>(recipe_hash_basis);
			fingerprint = recipe_hash_value(fingerprint, revision);
			builder_->template append_recipe<payload>(command_, fingerprint,
				payload{ static_cast<Fn&&>(function) });
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
	public:
		worker_context(Builder& builder, uint32_t worker) noexcept
			: builder_{ &builder }, worker_{ worker } {}

		command_context<Builder> commands(queue_duty::type duty,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return { *builder_, builder_->add_command(worker_, duty, level) };
		}

		command_context<Builder> commands(vktl::queue_extensions::graphics_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::graphics, level);
		}

		command_context<Builder> commands(vktl::queue_extensions::compute_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::compute, level);
		}

		command_context<Builder> commands(vktl::queue_extensions::transfer_,
			VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
			return commands(queue_duty::transfer, level);
		}

	private:
		Builder* builder_;
		uint32_t worker_;
	};

	template<typename Builder, typename Pass>
	struct pass_context {
	public:
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
	public:
		pipe_context(Builder& builder, uint32_t command, Pass& pass, uint16_t index)
			: builder_{ &builder }, command_{ command }, pass_{ &pass }, index_{ index } {
			bind_point_ = object_of<Pass, compute_>
				? VK_ VK_PIPELINE_BIND_POINT_COMPUTE : VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
			auto hash = recipe_hash_pointer(recipe_hash_basis, pass_);
			hash = recipe_hash_value(hash, index_);
			builder_->template append_recipe<bind_pipeline_command_recipe<Pass>>(
				command_, hash, bind_pipeline_command_recipe<Pass>{ pass_, index_, bind_point_ });
		}

		template<typename BindSet>
		pipe_context& bind(BindSet& bind_set, uint32_t set = 0u) {
			builder_->add_dependency(command_, bind_set);
			auto hash = recipe_hash_pointer(recipe_hash_basis, ::std::addressof(bind_set));
			hash = recipe_hash_value(hash, set);
			builder_->template append_recipe<bind_descriptor_command_recipe<BindSet>>(
				command_, hash, bind_descriptor_command_recipe<BindSet>{
					::std::addressof(bind_set), bind_point_, pass_->pipeline_layout(), set });
			return *this;
		}

		template<typename Buffer>
		pipe_context& bind_vertex_buffer(uint32_t binding, Buffer& buffer,
			VK_ VkDeviceSize offset = 0u) {
			builder_->add_dependency(command_, buffer);
			auto hash = recipe_hash_pointer(recipe_hash_basis, ::std::addressof(buffer));
			hash = recipe_hash_value(hash, binding);
			hash = recipe_hash_value(hash, offset);
			builder_->template append_recipe<bind_vertex_buffer_command_recipe<Buffer>>(
				command_, hash, bind_vertex_buffer_command_recipe<Buffer>{
					::std::addressof(buffer), binding, offset });
			return *this;
		}

		template<typename Buffer>
		pipe_context& bind_index_buffer(Buffer& buffer, VK_ VkDeviceSize offset = 0u,
			VK_ VkIndexType type = VK_ VK_INDEX_TYPE_UINT32) {
			builder_->add_dependency(command_, buffer);
			auto hash = recipe_hash_pointer(recipe_hash_basis, ::std::addressof(buffer));
			hash = recipe_hash_value(hash, offset);
			hash = recipe_hash_value(hash, type);
			builder_->template append_recipe<bind_index_buffer_command_recipe<Buffer>>(
				command_, hash, bind_index_buffer_command_recipe<Buffer>{
					::std::addressof(buffer), offset, type });
			return *this;
		}

		pipe_context& draw(uint32_t vertices, uint32_t instances = 1u,
			uint32_t first_vertex = 0u, uint32_t first_instance = 0u) {
			draw_command_recipe payload{ vertices, instances, first_vertex, first_instance };
			auto hash = recipe_hash_value(recipe_hash_basis, payload);
			builder_->template append_recipe<draw_command_recipe>(command_, hash, payload);
			return *this;
		}

		pipe_context& dispatch(uint32_t x, uint32_t y = 1u, uint32_t z = 1u) {
			dispatch_command_recipe payload{ x, y, z };
			auto hash = recipe_hash_value(recipe_hash_basis, payload);
			builder_->template append_recipe<dispatch_command_recipe>(command_, hash, payload);
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
	public:
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

		// `vkQueuePresentKHR` only accepts binary semaphores, so the presentation
		// wait is declared separately from the submission waits.
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
	public:
		explicit refresh_context(Builder& builder) noexcept : builder_{ &builder } {}

		worker_context<Builder> worker(uint32_t index) { return { *builder_, index }; }

		submit_context<Builder> submit(command_context<Builder> const& command) {
			return { *builder_, builder_->add_submit(command.index()) };
		}

	private:
		Builder* builder_;
	};

	// -----------------------------------------------------------------------
	// task component
	// -----------------------------------------------------------------------

	template<typename Fn, typename N>
	struct m<task<Fn>, N> : N {
		using base = N;

		template<similiar_to<task<Fn>> F>
		constexpr m(F&& task_info, auto&&...infos)
			: base{ forward_(infos)... }
			, fn_{ forward_(task_info).func } {
		}

		~m() { reset(); }

		void refresh() {
			auto _ = locker_of(this);
			auto* execution = parent_of<vktl::execution>(this);
			execution->init();
			if (task_id_ == 0u) task_id_ = execution->attach_task();

			refresh_builder builder{ *execution, execution->active_plan(task_id_) };
			fn_(refresh_context{ builder });
			builder.finish(task_id_);
		}

		void submit(bool active = true) {
			auto _ = locker_of(this);
			auto* execution = parent_of<vktl::execution>(this);
			if (task_id_ == 0u) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
					"[TASK] A task must refresh before it can submit." };
			}

			auto plan = execution->active_plan(task_id_);
			if (!active || !plan) {
				// An intentionally inactive task still marks the epoch so
				// execution can tell omission from intent.
				execution->mark_submitted(task_id_, false, nullptr);
				return;
			}

			validate_current_revisions(*plan);

			// From here on nothing allocates: every write lands in storage that
			// refresh already sized.
			auto fill = execution->begin_fill();
			execution->mark_submitted(task_id_, true, plan.get());

			payload_context context{ plan.get(), execution->make_payload_writer() };
			for (auto const& payload : plan->payloads) {
				payload.materialize(context);
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			if (task_id_ != 0u) {
				parent_of<vktl::execution>(this)->detach(::std::exchange(task_id_, 0u));
			}
		}

	private:
		static void validate_current_revisions(task_plan const& plan) {
			for (auto const& command : plan.commands) {
				auto const& variant = current_variant(command);
				if (!variant.handle) {
					throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
						"[TASK] A command variant was never recorded; refresh is required before submit." };
				}
				for (uint32_t scope = 0u; scope < uint32_t(command.frame_scopes.size()); ++scope) {
					if (command.frame_scopes[scope].frame_revision(variant.frames[scope])
						!= variant.revisions[scope]) {
						throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED),
							"[TASK] A command dependency changed; refresh is required before submit." };
					}
				}
			}
		}

	private:
		Fn fn_;
		uint64_t task_id_ = 0u;
	};

}
