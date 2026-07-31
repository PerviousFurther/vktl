#pragma once

// BEGIN TASK

VKTL_EXPORT_ namespace vktl::detail::event {
    using namespace vktl::emit;
    
    struct exit {};

    struct express_resource_state {
        list<default_buffer_access> buffer_states;
        list<default_image_access> image_states;
    };

    template<typename T>
    struct submit_data : T {
        ::std::uint16_t task_id;
        ::std::uint16_t queue_id;
    };

    template<typename T>
    struct append_task : T {
        // task side should initialize these values.
        ::std::span<VK_ VkSemaphore> wait_semaphores;
        ::std::span<VK_ VkSemaphore> emit_semaphores;
        ::std::uint16_t thread_id = 0u;

        // execution side will express.
        ::std::uint16_t task_id = invalid;
    };

    template<typename T>
    struct remove_task : T {
        ::std::uint16_t task_id;
    };


    template<typename P>
    struct fill_payloads {
        ::std::vector<P>& payloads;
    };

    struct fill_barrier {};


    template<typename T, ::std::uint16_t serial_code>
    struct serialed {
        static constexpr auto serial = serial_code;
    };

    template<typename T = event::record>
    struct begin_data : T {
        constexpr void set_current_index(::std::uint16_t index) { current_index = index; }

        constexpr auto graphics() const noexcept { return current(); }
        constexpr auto transfer() const noexcept { return current(); }
        constexpr auto compute() const noexcept { return current(); }
        constexpr auto current() const noexcept { return cmdbufs[current_index]; }

        ::std::uint16_t current_index = 0u;
        ::std::span<VK_ VkCommandBuffer> cmdbufs;
    };
    // might for defragmentation in future.
    template<typename T = event::record>
    struct with_temp : T {
        constexpr void set_temporary_index(::std::uint16_t index) { temp_index = index; }

        constexpr auto temp_graphics() const noexcept { return temp_current(); }
        constexpr auto temp_transfer() const noexcept { return temp_current(); }
        constexpr auto temp_compute() const noexcept { return temp_current(); }
        constexpr auto temp_current() const noexcept { return temp_cmdbufs[temp_index]; }

        ::std::uint16_t temp_index = 0u;
        ::std::span<VK_ VkCommandBuffer> temp_cmdbufs;
    };
}

VKTL_EXPORT_ namespace vktl::detail {
    inline constexpr auto EXECUTION_SCOPE = DEVICE_SCOPE + 0x1u;

    template<typename T, template<typename>typename EventType>
    concept event_of = is_extend_from<T, EventType>::value;

    template<typename T>
    struct cmdbufs_info : T {
        constexpr cmdbufs_info(auto&&...infos) : T{ forward_(infos)... } {}

        constexpr auto receive(VK_ VkCommandBufferAllocateInfo info) {
            auto itc = cmdbufs.begin();
            while (itc != cmdbufs.end() && (itc->level != info.level || vk_next.same(info.pNext, itc->pNext))) { itc++; }
            if (itc == cmdbufs.end() || itc->pNext != info.pNext || itc->level != info.level) {
                itc = cmdbufs.insert(itc, ::std::move(info));
            }
            else {
                itc->commandBufferCount += info.commandBufferCount;
                vk_next.modify(itc->pNext, info.pNext);
            }
            return::std::distance(cmdbufs.begin(), itc);
        }

        ::std::vector<VK_ VkCommandBufferAllocateInfo> cmdbufs;
    };

    struct task_payload {
        using submit_type = VK_ VkSubmitInfo;

        void place(submit_type& info) {
            info.sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO;

            info.waitSemaphoreCount = ::std::uint32_t(wait_semaphores.size());
            info.pWaitSemaphores = wait_semaphores.data();
            info.pWaitDstStageMask = wait_stages.data();

            info.commandBufferCount = ::std::uint32_t(cmdbufs.size());
            info.pCommandBuffers = cmdbufs.data();

            info.signalSemaphoreCount = ::std::uint32_t(emit_semaphores.size());
            info.pSignalSemaphores = emit_semaphores.data();
        }

        static VK_ VkResult submit(VK_ VkQueue handle, ::std::vector<submit_type> const& submits, VK_ VkFence fence = VK_NULL_HANDLE) {
            return VK_ vkQueueSubmit(handle, ::std::uint32_t(submits.size()), submits.data(), fence);
        }

        ::std::vector<VK_ VkSemaphore> emit_semaphores;
        ::std::vector<VK_ VkSemaphore> wait_semaphores;
        ::std::vector<VK_ VkCommandBuffer> cmdbufs;
        ::std::vector<VK_ VkPipelineStageFlags> emit_stages;
        ::std::vector<VK_ VkPipelineStageFlags> wait_stages;
    };

    struct sparse_bind_payload {
        using submit_type = VK_ VkBindSparseInfo;

        void place(submit_type& info) {
            info.sType = VK_ VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;

            info.waitSemaphoreCount = ::std::uint32_t(wait_semaphores.size());
            info.pWaitSemaphores = wait_semaphores.data();

            info.bufferBindCount = ::std::uint32_t(buffer_binds.size());
            info.pBufferBinds = buffer_binds.data();

            info.imageOpaqueBindCount = ::std::uint32_t(image_opaue_binds.size());
            info.pImageOpaqueBinds = image_opaue_binds.data();

            info.imageBindCount = ::std::uint32_t(image_binds.size());
            info.pImageBinds = image_binds.data();

            info.signalSemaphoreCount = ::std::uint32_t(emit_semaphores.size());
            info.pSignalSemaphores = emit_semaphores.data();
        }

        static VK_ VkResult submit(VK_ VkQueue handle, ::std::vector<submit_type> const& submits, VK_ VkFence fence = VK_NULL_HANDLE) {
            return VK_ vkQueueBindSparse(handle, ::std::uint32_t(submits.size()), submits.data(), fence);
        }

        ::std::vector<VK_ VkSemaphore> emit_semaphores;
        ::std::vector<VK_ VkSemaphore> wait_semaphores;
        ::std::vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
        ::std::vector<VK_ VkSparseImageOpaqueMemoryBindInfo> image_opaue_binds;
        ::std::vector<VK_ VkSparseImageMemoryBindInfo> image_binds;
    };

    struct present_payload {
        using submit_type = VK_ VkPresentInfoKHR;

        void place(submit_type& info) {
            info.sType = VK_ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            info.waitSemaphoreCount = ::std::uint32_t(wait_semaphores.size());
            info.pWaitSemaphores = wait_semaphores.data();

            info.swapchainCount = ::std::uint32_t(swapchains.size());
            info.pSwapchains = swapchains.data();
            info.pImageIndices = indices.data();

            if (results.size()) {
                assert(results.size() == swapchains.size()); // required by vulkan.
                info.pResults = results.data();
            }
        }

        static VK_ VkResult submit(VK_ VkQueue handle, submit_type const& present_info) {
            return VK_ vkQueuePresentKHR(handle, &present_info);
        }

        ::std::vector<VK_ VkSemaphore> wait_semaphores;
        ::std::vector<VK_ VkSwapchainKHR> swapchains;
        ::std::vector<::std::uint32_t> indices;
        ::std::vector<VK_ VkResult> results;
    };

    template<>
    struct meta_of<event::execute> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0xf000u);
        static constexpr auto name = fixed_string{ "event::execute" };
    };

    using namespace execution_extensions;
    struct unplaced_queue_info : VK_ VkDeviceQueueInfo2 {
        queue_duty::type duty;
    };
    struct queue_handle : move_only<VK_ VkQueue> {
        queue_duty::type duty;
        ::std::uint16_t family;
        ::std::mutex mtx;
    };

    template<>
    struct meta_of<execution> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x0u);
        static constexpr auto name = fixed_string{ "execution" };

        using order = order::at_middle;
        using extend = void;

        template<typename N>
        struct info : N {
            constexpr info(auto const& info)
                : N{ info }
                , frame_count{ get_by<N>(info).frame_count }
                , thread_count{ get_by<N>(info).thread_count }
            {
            }

            constexpr void receive(unplaced_queue_info info) {
                auto itf = queue_families.begin();
                while (itf != queue_families.end() && *itf < info.queueFamilyIndex) { itf++; }
                if (itf == queue_families.end() || *itf != info.queueFamilyIndex) {
                    queue_families.insert(itf, info.queueFamilyIndex);
                }

                auto it = queues.begin();
                while (it != queues.end() && (it->duty < info.duty
                    || (it->duty == info.duty && it->queueFamilyIndex < info.queueFamilyIndex)
                    || (it->duty == info.duty
                        && it->queueFamilyIndex == info.queueFamilyIndex
                        && it->queueIndex < info.queueIndex))) {
                    it++;
                }
                if (it == queues.end()
                    || it->duty != info.duty
                    || it->queueFamilyIndex != info.queueFamilyIndex
                    || it->queueIndex != info.queueIndex) {
                    queues.insert(it, ::std::move(info));
                }
                else {
                    it->flags = info.flags;
                }

                duty |= info.duty;
            }

            constexpr void receive(VK_ VkFenceCreateInfo info) { receive(fences, ::std::move(info)); }
            constexpr void receive(VK_ VkEventCreateInfo info) { receive(events, ::std::move(info)); }
            constexpr void receive(VK_ VkSemaphoreCreateInfo info) { receive(semaphores, ::std::move(info)); }

            ::std::uint16_t thread_count;
            ::std::uint16_t frame_count;
            queue_duty::type duty;

            ::std::vector<unplaced_queue_info> queues;
            ::std::vector<::std::uint32_t> queue_families;

            ::std::vector<VK_ VkFenceCreateInfo> fences;
            ::std::vector<VK_ VkEventCreateInfo> events;
            ::std::vector<VK_ VkSemaphoreCreateInfo> semaphores;

        private:
            static constexpr void receive(auto& vec, auto create) {
                vec.emplace_back(create);
            }
        };

        static constexpr auto status_pending = 0u;
        static constexpr auto status_running = 1u;
        static constexpr auto status_stopped = 2u;
        static constexpr auto status_error = 3u;

        struct task_life {
            ::std::uint16_t thread_id;
            ::std::uint16_t task_id;

            void* ptr;
            void(*set_image_state)(void*, event::express_resource_state&);
            void(*notify_exit)(void*);
        };

        struct task_run {
            ::std::uint16_t task_id;
            void* ptr;
            void(*emit_run)(void*);
        };

        template<typename N>
        struct make : N {
            template<infomation_of<execution> T>
            constexpr make(T&& info, auto&&...infos)
                : N{ forward_(infos)... }
                , duty{ info.duty }
                , families{ forward_like<T>(info.queue_families) }
                , thread_count_{ info.thread_count }
                , queue_count_(::std::uint16_t(info.queues.size())) {
                auto dv = N::template parent<device>();
                auto hdv = dv->device_handle();
                try {
                    fetch_queues(hdv, this->queues_ = new queue_handle[queue_count_], info.queues);
                    for (auto i{ 0u }; i < frame_count_; i++) {
                        create_(hdv, fences_, info.fences, &VK_ vkCreateFence, N::allocator())
                            | popup{ "[EXECUTION] Create fence failure." };
                        create_(hdv, events_, info.events, &VK_ vkCreateEvent, N::allocator())
                            | popup{ "[EXECUTION] Create event failure." };
                        create_(hdv, semaphores_, info.semaphores, &VK_ vkCreateSemaphore, N::allocator())
                            | popup{ "[EXECUTION] Create semaphore failure." };
                    }
                    this->runs_ = new thread_run[thread_count_];

                }
                catch (...) {
                    clear();
                    throw;
                }
            }

            make(make&&) = delete; // execution is not allow to moved.
            make& operator=(make&&) = delete; // execution is not allow to be moved.

            ~make() {
                stop();
                clear();
            }

            auto& send(event::set_event set) {
                auto hdv = N::template parent<device>()->device_handle();
                if (set.index != invalid) {
                    VK_ vkSetEvent(hdv, events_[set.index])
                        | popup{ "[EXECUTION] Set event failure." };
                }
                else for (auto e : events_) {
                    VK_ vkSetEvent(hdv, e)
                        | popup{ "[EXECUTION] Set event failure." };
                }
                return *this;
            }
            auto& send(event::reset_event set) {
                auto hdv = N::template parent<device>()->device_handle();
                if (set.index != invalid) {
                    VK_ vkResetEvent(hdv, events_[set.index])
                        | popup{ "[EXECUTION] Reset event failure." };
                }
                else for (auto e : events_) {
                    VK_ vkResetEvent(hdv, e)
                        | popup{ "[EXECUTION] Reset event failure." };
                }
                return *this;
            }

            void send(event::execute e) {
                N::send(e);
                auto runs = ::std::span{ runs_, thread_count_ };
                for (auto& run : runs) {
                    ::std::unique_lock _(run.mtx);
                    if (run.error) {
                        ::std::rethrow_exception(::std::exchange(run.error, nullptr));
                    }
                }

                event::express_resource_state state;
                for (auto& task : task_fns_) {
                    
                }

                for (auto& run : runs) {
                    run.cv.notify_one();
                }
            }

            template<typename Fn>
            void send(task info, Fn task) {

            }

            template<typename T>
            void receive(event_of<event::append_task> auto& e, T& task) {
                N::receive(e, task);
                assert(e.thread_id < thread_count_); // try create task in not existed thread.

                // TODO: maybe other things' injuction.
                runs_[e.thread_id].receive(e, task);

                auto& fn = task_fns_.emplace_back();
                fn.task_id = e.task_id = task_id_++;
                fn.thread_id = e.thread_id;
                fn.set_image_state = [](void* ptr, event::express_resource_state& state) {
                    static_cast<T*>(ptr)->send(state);
                };
                fn.notify_exit = [](void* ptr) {
                    static_cast<T*>(ptr)->send(event::exit{});
                };
            }

            void receive(event_of<event::remove_task> auto& e, auto& task) {
                auto it = task_fns_.begin();
                while (it != task_fns_.end() && it->task_id != e.task_id) { it++; }
                if (it != task_fns_.end() && it->task_id == e.task_id) {
                    runs_[it->thread_id]->receive(e, task);
                    task_fns_.erase(it);
                }
            }

            constexpr auto& queue(::std::uint16_t index) const noexcept {
                return queues_[index];
            }
            constexpr auto thread_count() const noexcept {
                return thread_count_;
            }

            const queue_duty::type duty;
            // noticed that the `families` is not same with queue's size.
            // you cannot query queue's family by this member.
            // by function queue(index).
            const::std::vector<::std::uint32_t> families;

        protected:
            ::std::uint16_t thread_count_;
            ::std::uint16_t queue_count_;
            ::std::uint16_t remaining_count_ = 0u;
            ::std::uint16_t task_id_ = 0u;

            ::std::shared_mutex mtx_;
            queue_handle* queues_;

            ::std::vector<VK_ VkFence> fences_;
            ::std::vector<VK_ VkEvent> events_;
            ::std::vector<VK_ VkSemaphore> semaphores_;

            ::std::vector<transformed<task_life, T>> task_fns_;

        private:
            struct payload {
                using payload_tag = void;
                using sb_payload = transformed<sparse_bind_payload, T>;
                using t_payload = transformed<task_payload, T>;
                using p_payload = transformed<present_payload, T>;
                using sb_submit = typename sb_payload::submit_type;
                using t_submit = typename t_submit::submit_type;
                using p_submit = typename p_submit::submit_type;

                template<::std::size_t index = 0u>
                auto submit() requires(index < 3u) {
                    ::std::unique_lock _(queue->mtx);
                    VK_ VkResult reuslt;
                    if constexpr (index == 0u) {
                        reuslt = submit_each(queue, sparse_bind_payloads, binds);
                    }
                    else if constexpr (index == 0u) {
                        reuslt = submit_each(queue, task_payloads, tasks);
                    }
                    else {
                        reuslt = submit_one(queue, present_payload, present);
                    }
                    return reuslt;
                }

                ::std::vector<sb_payload> sparse_bind_payloads;
                ::std::vector<t_payload>  task_payloads;
                p_payload present_payload;

                ::std::vector<sb_submit> binds;
                ::std::vector<t_submit>  tasks;
                p_submit present;

                queue_handle* queue;
                VK_ VkFence fence;

            protected:
                template<typename T>
                static auto submit_one(VK_ VkQueue queue, T const& payload, auto& submit, auto fence) {
                    payload.place(submit);
                    return T::submit(queue, submit, fence);
                }

                template<typename T>
                static auto submit_each(VK_ VkQueue queue, ::std::vector<T> const& payloads, auto& submits, auto fence) {
                    auto itp = payloads.begin();auto its = submits.begin();
                    for (; itp != payloads.end(); itp++, its++) {
                        itp->place(*its);
                    }
                    return T::submit(queue, submits, fence);
                }
            };

        public:
            using payload_type = transformed<payload, T>;

        private:
            struct thread_run {
                using thread_run_tag = void;
                using func = void(::std::uint32_t, void*, thread_run*);

                thread_run() { main = ::std::this_thread::get_id(); }

                template<::std::size_t index>
                static void do_submit(auto& payload) {
                    if constexpr (requires { payload.submit<index>(); }) {
                        payload.submit<index>();
                        do_submit<index + 1u>();
                    }
                }

                void operator()() noexcept {
                    while (true) try {
                        {
                            ::std::unique_lock lock(mtx);
                            cv.wait(lock);
                            if (this->error != nullptr) {
                                ::std::abort(); // error must be handled.
                            }
                            status = status_running;
                        }
                        {
                            if (should_stop()) {
                                break;
                            }
                        }

                        for (auto& fn : runs) {
                            fn.emit_run(fn.ptr);
                        }

                        for (auto& payload : payloads) {
                            payload.template submit<0u>();
                        }

                        {
                            if (should_stop()) {
                                break;
                            }
                            else {
                                ::std::unique_lock _(mtx);
                                status = status_pending;
                            }
                        }
                    }
                    catch (...) {
                        auto result = mtx.try_lock();
                        error = ::std::current_exception();
                        status = status_error;
                        if (result) {
                            mtx.unlock();
                        }
                    }
                    auto result = mtx.try_lock();
                    if (error) {
                        ::std::rethrow_exception(error);
                    }
                    status = status_stopped;
                    if (result) {
                        mtx.unlock();
                    }
                    cv.notify_all();
                }

                void receive(event_of<event::append_task> auto& e, auto& task) {
                    assert(::std::this_thread::get_id() == main); // append operation only allow call from main thread.
                    event::express_payloads e{ payloads };
                    task.receive(e, *this);
                }

                void receive(event_of<event::remove_task> auto& e, auto& task) {
                    assert(::std::this_thread::get_id() == main); // remove operation only allow call from main thread.
                    event::express_payloads e{ payloads };
                    task.receive(e, *this);
                }

                void unwrap() {

                }

                bool should_stop() {
                    ::std::shared_lock _(parent->mtx_);
                    return parent_->frame_index != invalid;
                }

                // payloads per queue.
                make* parent = nullptr;

                ::std::uint16_t status = status_pending;
                ::std::uint16_t reserved = invalid;
                typename::std::thread::id main;

                ::std::vector<payload_type> payloads;
                ::std::vector<transformed<task_run, T>> runs;

                ::std::mutex mtx;
                ::std::condition_variable cv;
                ::std::exception_ptr error;
                ::std::thread hthread;
            };

            void stop() {
                {
                    ::std::unique_lock _(mtx_);
                    this->frame_index_ = invalid;
                }
                auto runs = ::std::span{ runs_, thread_count_ };
                for (auto& run : runs) {
                    auto status = status_pending;
                    {
                        ::std::unique_lock _(run.mtx);
                        status = run.status;
                    }

                    switch (status) {
                    case status_pending:
                        run.cv.notify_one();
                        break;
                    case status_running: {
                        ::std::unique_lock lock(run.mtx);
                        while (run.status != status_stopped) {
                            run.cv.wait(lock);
                        }
                    } break;
                    }
                }
                for (auto& run : runs) {
                    
                    run.cv.notify_one();
                }
            }

            void clear() {
                auto hdv = this->template parent<device>()->device_handle();
                destroy_(hdv, fences_, &VK_ vkDestroyFence, N::allocator());
                destroy_(hdv, events_, &VK_ vkDestroyEvent, N::allocator());
                destroy_(hdv, semaphores_, &VK_ vkDestroySemaphore, N::allocator());
                delete[] queues_;
                delete[] runs_;
            }

            static void destroy_(VkDevice hdv, auto& objects, auto fn, auto alloc) {
                for (const auto& obj : objects) {
                    fn(hdv, obj, alloc);
                }
                objects.clear();
            }

            static auto create_(VkDevice hdv, auto& result, auto const& infos, auto fn, auto alloc) {
                result.resize(infos.size());
                auto pdata = result.data();
                for (const auto& info : infos) {
                    auto result = fn(hdv, &info, alloc, &*pdata++);
                    if (result != VK_ VK_SUCCESS) {
                        return result;
                    }
                }
                return VK_ VK_SUCCESS;
            }

            static auto fetch_queues(VkDevice hdv, auto& result, auto const& queues) {
                auto p = result;
                for (auto const& queue : queues) {
                    auto& handle = *p++;
                    if (queue.pNext) {
                        VK_ vkGetDeviceQueue2(hdv, &queue, &handle.value);
                    }
                    else {
                        VK_ vkGetDeviceQueue(hdv, queue.queueFamilyIndex, queue.queueIndex, &handle.value);
                    }
                    handle.family = queue.queueFamilyIndex;
                    handle.duty = queue.duty;
                }
            }

            auto init_runs(auto runs) {
                for (auto& run : ::std::span{ runs, thread_count() }) {
                    run.parent = this;
                    run.hthread = ::std::thread(run);
                }
            }

        private:
            thread_run* runs_;
        };
    };

    template<typename T>
    struct basic_sync_object_info : control_connectable<T> {
        using base = control_connectable<T>;
        constexpr basic_sync_object_info(auto&& infos)
            : base{ forward_(infos) }
        {
        }

        constexpr void connect(infomation_of<execution> auto& dv) {
            if (this->connectable()) {
                dv.receive(T::create());
                this->set_connectable(false);
            }
        }
    };

    using namespace fence_extensions;
    template<>
    struct meta_of<fence> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x100u);
        static constexpr auto name = fixed_string{ "fence" };

        using order = order::at_middle;
        using extend = void;

        using sync_object = void;

        template<typename T>
        struct detail : T {
            constexpr detail(auto&& infos)
                : T{ forward_(infos) }
                , fence{
                    .sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    // we dont seperate first run with other run, thus need to set it.
                    .flags = VK_ VK_FENCE_CREATE_SIGNALED_BIT,
                }
            {
            }

            constexpr auto& create() noexcept { return fence; }
            constexpr auto& create() const noexcept { return fence; }

            VK_ VkFenceCreateInfo fence;
        };

        template<typename T>
        using info = basic_sync_object_info<detail<T>>;

        template<typename T>
        using make = skipped_make<T>;
    };

    using namespace semaphore_extensions;
    template<>
    struct meta_of<semaphore> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x200u);
        static constexpr auto name = fixed_string{ "semaphore" };

        using order = order::at_middle;
        using extend = void;

        using sync_object = void;

        template<typename T>
        struct detail : T {
            constexpr detail(auto&& infos)
                : T{ forward_(infos) }
                , semaphore{
                    .sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                } {
            }

            constexpr auto& create() noexcept { return semaphore; }
            constexpr auto& create() const noexcept { return semaphore; }

            VK_ VkSemaphoreCreateInfo semaphore;
        };
        template<typename T>
        using info = basic_sync_object_info<detail<T>>;

        template<typename T>
        using make = skipped_make<T>;
    };

    using namespace event_extensions;
    template<>
    struct meta_of<event> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x300u);
        static constexpr auto name = fixed_string{ "event" };

        using order = order::at_middle;
        using extend = void;

        using sync_object = void;

        template<typename T>
        struct detail : T {
            constexpr detail(auto&& infos)
                : T{ forward_(infos) }
                , event{
                    .sType = VK_ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
                }
            {
            }

            constexpr auto& create() noexcept { return event; }
            constexpr auto& create() const noexcept { return event; }

            VK_ VkEventCreateInfo event;
        };

        template<typename T>
        using info = basic_sync_object_info<detail<T>>;

        template<typename T>
        using make = skipped_make<T>;
    };

    template<typename T>
        requires(contains<T, event> || contains<T, fence> || contains<T, semaphore>)
    struct meta_of<repeat>::info<T> : T {
        constexpr info(auto&& infos)
            : T{ forward_(infos) }
            , repeat_{ get_by<T>(infos) } {
        }

        constexpr void connect(infomation_of<execution> auto& dv) {
            if (T::connectable()) {
                for (auto c = repeat_.count; c--; ) {
                    dv.receive(T::create());
                }
                T::set_connectable(false);
            }
        }

    private:
        repeat repeat_;
    };

    using namespace queue_extensions;
    template<> struct meta_of<queue> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1000);
        static constexpr auto name = fixed_string{ "queue" };

        using order = order::at_middle;
        using extend = void;

        template<typename T>
        struct info : control_connectable<T> {
            using base = control_connectable<T>;
            info(queue const& info, auto&& infos) : base{ forward_(infos) }
                , queue_info{} {
                queue_info.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
                queue_info.queueFamilyIndex = info.family_index;
                queue_info.queueIndex = info.index;
            }
            info(auto const& infos) : info{ get_by<queue>(infos), infos } {}

            void connect(infomation_of<execution> auto& exec) {
                if (this->connectable()) {
                    exec.receive(queue_info); this->set_connectable(false);
                }
            }

            unplaced_queue_info queue_info;
        };

        template<typename T>
        using make = skipped_make<T>;
    };

    template<typename T, typename Next, template<typename>typename EventType>
    concept event_in_task = event_of<T, EventType> && T::serial == Next::serial;

    template<typename T, ::std::size_t index>
    struct basic_task : T {
        static constexpr auto serial = [] () constexpr {
            if constexpr (have_parent_v<T, split>) {
                return object_parent_t<T, split>::serial + 1u;
            }
            else if constexpr (have_parent_v<T, task>) {
                return object_parent_t<T, task>::serial + 1u;
            }
            else {
                return::std::uint16_t(0u);
            }
        }();
        
        constexpr basic_task(auto exec, auto&& info, auto&& infos)
            : T{ forward_(infos)... }
            , queue_(exec->queue(info.queue_index)) {

        }

        constexpr basic_task(auto&& info, auto&&...infos)
            : basic_task(T::template parent<execution>(), forward_(info), forward_(infos)...)
        {}

    protected:
        queue_handle& queue_;
    };

    template<::std::size_t index>
    using task_parent_type = tuple_at_t<typename T::parent_by_ref, index>;

    template<typename T, ::std::size_t index>
        requires(index < T::num_parents)
    struct basic_task : basic_task<T, index + 1u> {
        constexpr basic_task(auto&&...infos)
            : T{ forward_(infos)... }
        {
        }

        void receive(event_of<event::fill_payloads> auto& e, auto& invoker) {
            auto& payloads = e.payloads;

        }

        void receive(event_of<> auto& e, auto& invoker) {

        }

    protected:

        
    };


    using namespace task_extensions;
    template<> struct meta_of<task> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x2000);
        static constexpr auto name = fixed_string{ "task" };

        using order = order::at_middle;
        using extend = void;

        struct cmdpool : handle_wrapper<VK_ VkCommandPool> {
            VK_ VkCommandPoolCreateFlags flags; 
        };

        template<typename T>
        struct info : cmdbufs_info<basic_time_point<T>> {
            using time_point = basic_time_point<T>;
            using base = cmdbufs_info<time_point>;

            static constexpr auto default_main_cmdbufs = VK_ VkCommandBufferAllocateInfo{
                .sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1u,
            };

            constexpr info(task info, VK_ VkCommandBufferAllocateInfo const& main_info, auto&& infos)
                : base{ forward_(infos) }
                , thread_index(info.thread_index)
                , queue_index(info.queue_index)
                , fence_index(info.fence_index)
                , pool{
                    .sType = VK_ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                } {
                this->cmdbufs.emplace_back(main_info);
            }
            constexpr info(auto&& infos, VK_ VkCommandBufferAllocateInfo const& main_info = default_main_cmdbufs)
                : info(get_by<T>(infos), main_info, forward_(infos)) {
            }

            constexpr void set_connectable() {
                this->cmdbufs.clear();
            }

            ::std::uint16_t thread_index;
            ::std::uint16_t queue_index;
            ::std::uint16_t fence_index = invalid;
            
            VK_ VkCommandPoolCreateInfo pool;
        };

        template<typename N>
        struct make : N {
            using base = N;

            template<infomation_of<task> F>
            constexpr make(F&& info, auto&&...others)
                : base{ forward_(info), forward_(others)... }
                , fence_index_(info.fence_index)
                , queue_index_(info.queue_index)
                , submit_index_(info.submit_index)
                , cmdbuf_size_(::std::uint16_t(info.cmdbufs.size())) {
                auto dv = base::template parent<device>();
                auto exec = base::template parent<execution>();
                auto hdv = dv->device_handle();

                auto const& queue = exec->queue(queue_index_);
                try {
                    auto wait_smps = forward_like<F>(info.wait_semaphores);
                    wait_smps_.reserve(wait_smps.size());
                    for (auto idx : wait_smps) {
                        wait_smps_.emplace_back(exec->semaphore(idx));
                    }
                    auto emit_smps = forward_like<F>(info.emit_semaphores);
                    emit_smps_.reserve(wait_smps.size());
                    for (auto idx : wait_smps) {
                        emit_smps_.emplace_back(exec->semaphore(idx));
                    }

                    pools_.resize(frame_count());
                    cmdbufs_.resize(frame_count() * cmdbuf_size_);

                    auto pool_info = info.pool;
                    pool_info.queueFamilyIndex = queue_index_ = queue.index;
                    auto cmdbufs_info = forward_like<F>(info.cmdbufs);
                    for (auto& pool : pools_) {
                        VK_ vkCreateCommandPool(hdv, &pool_info, base::allocator(), &pool)
                            | popup{ "[TASK] Create command pool resource failure." };
                        for (auto& c : cmdbufs_info) {
                            c.commandPool = pool;
                            ::std::vector<VK_ VkCommandBuffer> cmdbufs(cmdbuf_size_);
                            VK_ vkAllocateCommandBuffers(hdv, &c, cmdbufs.data())
                                | popup{ "[TASK] Allocate command buffer failure." };
                            cmdbufs_.insert(cmdbufs_.end(), cmdbufs.begin(), cmdbufs.end());
                        }
                    }

                    event::append_task<empty_info> append;
                    append.thread_id = info.thread_index;
                    append.wait_semaphores = wait_smps_;
                    append.emit_semaphores = emit_smps_;
                    exec->receive(append, *this);

                    task_id_ = append.task_id;
                }
                catch (...) {
                    clear();
                    throw;
                }
            }

            ~make() { clear(); }

            void receive(event_of<event::fill_payloads> auto& payloads, auto& invoker) {

            }

            template<event_of<end_frame> E>
            void receive(E& event, auto& invoker) {
                auto hdv = base::template parent<device>()->device_handle();
                auto exec = base::template parent<execution>();

                if (fence_index_ != invalid) {
                    VK_ VkFence f[]{ exec->fence(frame_index(), fence_index_) };
                    VK_ vkWaitForFences(hdv, 1u, f);
                }
            }

            template<event_of<begin_data> E>
            void receive(E& event, auto& invoker) {
                auto hdv = base::template parent<device>()->device_handle();

                if constexpr (requires { base::next_frame(); }) {
                    base::next_frame();

                    end_frame<serialed<empty_info, serial>> enf{};
                    base::as_self().receive(enf, *this);
                }

                auto& pool = pools_[frame_index()];
                VK_ vkResetCommandPool(hdv, pool, VK_ VkCommandPoolResetFlags(0u))
                    | popup{ "[TASK] Reset command pool failure." };

                auto size_frame = cmdbufs_.size() / frame_count();
                event.cmdbufs = ::std::span{ cmdbufs_.begin() + size_frame * frame_index(), size_frame };
            }

            void receive(event_of<submit_data> auto& event, auto& invoker) {
                base::receive(event, invoker);

                auto dv = base::template parent<device>()->device_handle();
                auto exec = base::template parent<execution>();

                // event.
                if (event.submits.size() <= submit_index_) {
                    const auto new_size = submit_index_ + 1u;
                    event.wait_semaphores.resize(new_size);
                    event.emit_semaphores.resize(new_size);
                    event.wait_stages.resize(new_size);
                    event.submits.resize(new_size, VK_ VkSubmitInfo{
                        .sType = VK_ VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        });
                    event.cmdbufs.resize(new_size);
                }

                auto& wait_smps = event.wait_semaphores[submit_index_];
                auto& wait_stages = event.wait_stages[submit_index_];
                auto& cmdbufs = event.cmdbufs[submit_index_];

                wait_smps.reserve(this->wait_semaphores.size());
                wait_stages.reserve(this->wait_semaphores.size());
                for (const auto& info : this->wait_semaphores) {
                    wait_smps.push_back(exec->semaphore(info.index));
                    wait_stages.push_back(info.stages);
                }

                auto& emit_smps = event.emit_semaphores[submit_index_];
                emit_smps.reserve(this->emit_semaphores.size());
                for (const auto& info : this->emit_semaphores) {
                    emit_smps.push_back(exec->semaphore(info.index));
                }

                cmdbufs.insert(cmdbufs.end(),
                    cmdbufs_.begin() + cmdbuf_size_ * base::frame_index(),
                    cmdbufs_.begin() + cmdbuf_size_ * (base::frame_index() + 1u));

                auto& submit = event.submits[submit_index_];
                submit.waitSemaphoreCount = ::std::uint32_t(wait_smps.size());
                submit.pWaitSemaphores = wait_smps.data();
                submit.pWaitDstStageMask = wait_stages.data();

                submit.commandBufferCount = ::std::uint32_t(cmdbufs.size());
                submit.pCommandBuffers = cmdbufs.data();

                submit.signalSemaphoreCount = ::std::uint32_t(emit_smps.size());
                submit.pSignalSemaphores = emit_smps.data();
            }

            void send(event::record e) {
                base::send(e);

                begin_data<serialed<event::record, serial>> begin_info{ {{e}} };
                base::as_self().receive(begin_info, *this);

                for (auto const& cmdbuf
                    : ::std::span(cmdbufs_).subspan(base::frame_index()* cmdbuf_size_, cmdbuf_size_)) {
                    VK_ vkEndCommandBuffer(cmdbuf)
                        | popup{ "[TASK] End command buffer failure." };
                }
            }

            void send(event::submit e, void* ptr = nullptr) {
                N::send(e);
                auto exec = base::template parent<execution>();
                auto const& queue = exec->queue(queue_index_);

                submit_data<serialed<event::submit, serial>> event{};
                base::receive(event, *this);

                VK_ vkQueueSubmit(queue, ::std::uint32_t(event.submits.size()), event.submits.data(),
                    VK_ VkFence(fence_index_ == invalid ? nullptr : exec->fence(frame_index(), fence_index_)))
                    | popup{ "[TASK] submit queue failure." };
            }

            constexpr auto frame_count() const noexcept {
                return base::frame_count();
            }
            
            constexpr auto& wait_semaphores() const noexcept {
                return wait_smps_;
            }
            constexpr auto& emit_semaphores() const noexcept {
                return emit_smps_;
            }

            constexpr auto command_buffer_count() const noexcept {
                return cmdbuf_size_;
            }
            constexpr auto command_buffers(::std::uint16_t frame_index) const noexcept {
                return::std::span{ cmdbufs_ }.subspan(frame_index * cmdbuf_size_, cmdbuf_size_);
            }
            constexpr auto command_buffers() const noexcept {
                return command_buffers(frame_index());
            }

        protected:
            void clear() noexcept {
                auto hdv = base::template parent<device>()->device_handle();
                for (auto& pool : pools_) {
                    VK_ vkDestroyCommandPool(hdv, pool, base::allocator());
                }
                cmdbufs_.clear();
                wait_smps_.clear();
                emit_smps_.clear();
            }

        protected:
            ::std::uint16_t task_id_;
            ::std::uint16_t fence_index_;
            ::std::uint16_t queue_index_;
            ::std::uint16_t submit_index_;
            ::std::uint16_t cmdbuf_size_;

            ::std::vector<cmdpool> pools_;
            ::std::vector<VK_ VkCommandBuffer> cmdbufs_;
            ::std::vector<VK_ VkSemaphore> wait_smps_, emit_smps_;

        private:
            using base::wait_semaphores;
            using base::emit_semaphores;
            using base::append_emit;
            using base::append_wait;
        };
    };


    template<> struct meta_of<allow_temporary_command_buffers> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x2001);
        static constexpr auto name = fixed_string{ "allow_temporary_command_buffers" };

        using extend = task;
        using order = order::at_middle;

        template<typename N>
        struct info : N {
            constexpr info(auto&& info)
                : N{ forward_(info) } {
                N::pool.flags |= VK_ VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            }
        };

        template<typename N>
        struct make : N {
            constexpr make(auto&&...infos)
                : N{ forward_(infos)... }
                , temp_cmdbufs_(N::frame_count())
            {
            }

            void receive(event_in_task<end_frame, N> auto& event, auto& invoker) {
                N::receive(event, invoker);

                auto fi = N::frame_index();
                auto dv = N::template parent<device>();
                auto pool = N::pools_[fi];
                auto& cmdbufs = N::temporary_cmdbufs_[fi];
                VK_ vkFreeCommandBuffers(dv->device_handle(), pool, ::std::uint32_t(cmdbufs.size()), cmdbufs.data());
                cmdbufs.clear();
            }

            template<event_in_task<cmdbufs_info, N> T>
            void receive(T&& event, auto& invoker) {
                auto hdv = N::template parent<device>()->device_handle();
                auto fi = N::frame_index();
                auto fc = N::frame_count();
                auto& cmdbufs = temp_cmdbufs_[fi];
                for (auto const& alloc : ::std::move(event.cmdbufs)) {
                    auto offset = cmdbufs.size();
                    cmdbufs.resize(offset + alloc.commandBufferCount);
                    VK_ vkAllocateCommandBuffers(hdv, &alloc, cmdbufs.data() + offset);
                }
            }

            void send(event::record begin, auto&& invoker) {
                N::send(begin, invoker);

                auto fi = N::frame_index();

                cmdbufs_info<serialed<event::record, N::serial>> collect{ { {begin} }, };
                N::as_self().receive(collect, N::as_local());

                begin_data<with_temp<serialed<event::record, N::serial>>>
                    event{ {{{begin}}, 0u, temp_cmdbufs_[fi] }, 0u, N::command_buffers() };
                N::as_self().receive(event, invoker);
            }

        protected:
            ::std::vector<::std::vector<VK_ VkCommandBuffer>> temp_cmdbufs_;
        };
    };


    template<> struct meta_of<split> {
        static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x2500);
        static constexpr auto name = fixed_string{ "secondary" };

        using order = order::at_middle;
        using extend = void;

        // enum SPLIT_TYPE : ::std::uint16_t {
        //     unknown,
        //     subtask,
        //     secondary,
        // };

        template<typename T>
        struct info : T {};

        template<typename T>
        struct make : T {
            constexpr make(auto&& info, auto&&...others)
                : T{ forward_(others)... } {
            }

            void receive(event_of<begin_data> auto&, auto& invoker) {

            }

            ::std::uint16_t index{ 0u };
        };
    };
}

