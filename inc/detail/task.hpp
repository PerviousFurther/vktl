#pragma once

// --- Agents specification -------------------------------------------------
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {

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
    default_command_pool *pool = nullptr;
    uint32_t worker_index = 0u;
    uint32_t queue_index = uint32_t(invalid);
    uint32_t queue_family = uint32_t(invalid); // for convenient.
    VK_ VkCommandBufferLevel level = VK_ VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VK_ VkCommandBufferUsageFlags usage = VK_ VkCommandBufferUsageFlags(0u);
    vector<VK_ VkCommandBuffer> handles;
  };

  struct compiled_task;
  struct compiled_payload : poly_list::node {};

  namespace tasks {
    bool intersects(span<VK_ VkSemaphore> a, span<VK_ VkSemaphore> b) {
      for (auto aa : a) {
        for (auto bb : b) {
          if (aa == bb) {
            return true;
          }
        }
      }
      return false;
    }

    bool submit_can_merge(
      span<VK_ VkSemaphore> fronter_wait, span<VK_ VkSemaphore> fronter_emit,
      span<VK_ VkSemaphore> current_emit, span<VK_ VkSemaphore> current_wait) {
      // Keep opposite operations separated when their semaphore ordering was
      // declared across the submission boundary being considered for merging.
      return !intersects(fronter_wait, current_emit)
        && !intersects(fronter_emit, current_wait);
    }
  }

  struct submit_payload : compiled_payload {
    using submit_type = VK_ VkSubmitInfo;

    vector<VK_ VkCommandBuffer> command_handles;
    vector<VK_ VkSemaphore> emit_semaphores;
    vectors<VK_ VkSemaphore, VK_ VkPipelineStageFlags> wait_semaphores;
    
    void fill(vector<VK_ VkSubmitInfo> &infos) {
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
    vectors<VK_ VkCommandBufferSubmitInfoKHR,
      vector<VK_ VkCommandBuffer>> command_infos;
    vectors<VK_ VkSemaphoreSubmitInfoKHR, 
      vector<VK_ VkSemaphore>> emit_semaphore_infos;
    vectors<VK_ VkSemaphoreSubmitInfoKHR,
      vector<VK_ VkSemaphore>> wait_semaphore_infos;

    void fill(vector<VK_ VkSubmitInfo2KHR> &infos) {
      infos.emplace_back(VK_ VkSubmitInfo2KHR{
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

    void prepare_submit_storage(compiled_task const &) {
      for (uint32_t index = 0u; index < uint32_t(swapchains.size()); ++index) {
        handles[index] = swapchains[index].handle();
        image_indices[index] = swapchains[index].frame_index();
      }
      for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
        wait_handles[index] = waits[index].handle();
      }
    }

    static void invoke(VK_ VkQueue queue_handle, void const *storage,
                       VK_ VkFence) {
      auto &self = *const_cast<present_payload *>(
          static_cast<present_payload const *>(storage));
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
        for (auto &&[result, swapchain] :
             spans{self.results, self.swapchains}) {
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
    sparse_bind_payload() { /*bind_payload<sparse_bind_payload>();*/ }

    uint32_t queue = uint32_t(invalid);
    vector<VK_ VkSparseImageMemoryBindInfo> image_binds;
    vector<VK_ VkSparseImageOpaqueMemoryBindInfo> image_opaque_binds;
    vector<VK_ VkSparseBufferMemoryBindInfo> buffer_binds;
    vector<box<vptr::queue_semaphore>> waits;
    vector<box<vptr::queue_semaphore>> signals;
    vector<VK_ VkSemaphore> wait_handles;
    vector<VK_ VkSemaphore> signal_handles;

    void prepare_storage() {
      wait_handles.resize(waits.size());
      signal_handles.resize(signals.size());
    }

    void prepare_submit_storage(compiled_task const &) {
      for (uint32_t index = 0u; index < uint32_t(waits.size()); ++index) {
        wait_handles[index] = waits[index].handle();
      }
      for (uint32_t index = 0u; index < uint32_t(signals.size()); ++index) {
        signal_handles[index] = signals[index].handle();
      }
    }

    static void invoke(VK_ VkQueue queue_handle, void const *storage,
                       VK_ VkFence completion) {
      auto &self = *static_cast<sparse_bind_payload const *>(storage);
      VK_ VkBindSparseInfo info{
          .sType = VK_ VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
          .waitSemaphoreCount = uint32_t(self.wait_handles.size()),
          .pWaitSemaphores = self.wait_handles.data(),
          .bufferBindCount = uint32_t(self.buffer_binds.size()),
          .pBufferBinds = self.buffer_binds.data(),
          .signalSemaphoreCount = uint32_t(self.signal_handles.size()),
          .pSignalSemaphores = self.signal_handles.data(),
      };
      VK_ vkQueueBindSparse(queue_handle, 1u, &info, completion) |
          popup{"[EXECUTION] Sparse queue binding failed."};
    }
  };



  template <typename Fn, typename N> struct m<task<Fn>, N> : N {
    using base = N;
    static_assert(!object_of<N, extensions::allocate_from>,
                  "task object use execution's allocator, standalone allocator "
                  "is not allowed.");

    template <similiar_to<task<Fn>> Info>
    constexpr m(Info &&info, auto &&...others)
        : N{forward_(others)...}, fn_{info.func} {}

    m(m const &) = delete;
    m &operator=(m const &) = delete;

    m(m &&other)
      requires(::std::move_constructible<N>)
        : N{static_cast<N &&>(other)}, fn_{::std::move(other.fn_)},
          states_{::std::move(other.states_)} {
      VKTL_ASSERT(::std::this_thread::get_id() ==
                  parent_of<execution>(this)->thread_id());
    }

    m &operator=(m &&) = delete;

    ~m() { reset(); }

    void refresh() {

    }

    void submit() {}

    void reset() noexcept {}

  private:
    Fn fn_;
    poly_list states_;
  };
}
