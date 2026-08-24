#pragma once

VKTL_EXPORT_ namespace vktl::detail {

}

VKTL_EXPORT_ namespace vktl::cmd {
	using detail::combine;

	template<typename Pass>
	struct bind_pass { Pass& pass; };
	template<typename...Buffers>
	struct bind_vertex_buffer : ::std::tuple<Buffers*...> { 
		using base = ::std::tuple<Buffers*...>;
		uint32_t first_binding = 0u;
		template<typename...Args>
		bind_vertex_buffer(Args&...args)
			: base{ &args... }
		{}
	};
	template<typename...Args>
	bind_vertex_buffer(Args&...) -> bind_vertex_buffer<Args...>;

	template<typename Buffer>
	struct bind_index_buffer { 
		Buffer& object; 
		VK_ VkDeviceSize offset = 0u;
		VK_ VkIndexType type = VK_ VK_INDEX_TYPE_UINT32;
	};

	struct bind_pipe { uint32_t index; };
	template<typename SyncObject>
	struct wait { SyncObject& object; };
	template<typename SyncObject>
	struct signal { SyncObject& object; };
}

VKTL_EXPORT_ namespace vktl::detail {

	template<object_of<pass_extensions::render_pass_> P, typename N>
	struct m<cmd::bind_pass<P>, N> : N {
		constexpr m(cmd::bind_pass<P> const& pass, auto&&...others)
			: N{ forward_(others)... }
			, pass_{ &pass.pass }
		{ }

		void begin(VK_ VkSubpassContents content = VK_ VK_SUBPASS_CONTENTS_INLINE) const {
			N::begin();

			VK_ vkCmdBeginRenderPass(N::cmdbuf(), &begin_pass, content);
		}

	protected:
		void end() const {
			N::end();
			VK_ vkCmdEndRenderPass(N::cmdbuf());
		}

		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }
		constexpr auto& pass() const noexcept { return *pass_; }

	protected:
		VK_ VkRenderPassBeginInfo begin_pass{
			.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		};

	private:
		P* pass_;
	};

#if defined(VK_KHR_dynamic_rendering)
	template<object_of<pass_extensions::rendering_> P, typename N>
	struct m<cmd::bind_pass<P>, N> : N {
		constexpr m(cmd::bind_pass<P> const& pass, auto&&...others)
			: N{ forward_(others)... }
			, pass_{ &pass.pass }
		{
		}

		void begin() const {
			N::begin();
			VK_ vkCmdBeginRenderingKHR(N::cmdbuf(), rendering_info_);
		}

	protected:
		void end() const {
			N::end();
			VK_ vkCmdEndRenderPass(N::cmdbuf());
		}

		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }
		constexpr auto& pass() const noexcept { return *pass_; }

		VK_ VkRenderingInfoKHR rendering_info_{ .sType = VK_ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
	private:
		P* pass_;
	};
#endif

	template<typename...Buffers, typename N>
	struct m<cmd::bind_vertex_buffer<Buffers...>, N> : N {
		using info_type = cmd::bind_vertex_buffer<Buffers...>;
		static_assert(sizeof...(Buffers) > 0u,
			"bind_vertex_buffer requires at least one buffer");

		constexpr m(info_type const& info, auto&&...others)
			: N{ forward_(others)... }, info_{ info } {}

		void begin() const {
			N::begin();
			bind(::std::index_sequence_for<Buffers...>{});
		}

	protected:
		void end() const { N::end(); }
		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	private:
		template<size_t...Indices>
		void bind(::std::index_sequence<Indices...>) const {
			VK_ VkBuffer handles[]{ ::std::get<Indices>(info_)->handle().value... };
			VK_ VkDeviceSize offsets[sizeof...(Buffers)]{};
			VK_ vkCmdBindVertexBuffers(N::cmdbuf(), info_.first_binding,
				uint32_t(sizeof...(Buffers)), handles, offsets);
		}

		info_type info_;
	};

	template<typename Buffer, typename N>
	struct m<cmd::bind_index_buffer<Buffer>, N> : N {
		constexpr m(cmd::bind_index_buffer<Buffer> info, auto&&...others)
			: N{ forward_(others)... }, info_{ info } {}

		void begin() const {
			N::begin();
			VK_ vkCmdBindIndexBuffer(N::cmdbuf(), info_.object.handle().value,
				info_.offset, info_.type);
		}

	protected:
		void end() const { N::end(); }
		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	private:
		cmd::bind_index_buffer<Buffer> info_;
	};

	template<typename N>
	struct m<cmd::bind_pipe, N> : N {
		constexpr m(cmd::bind_pipe info, auto&&...others)
			: N{ forward_(others)... }, index_{ info.index } {}

		void begin() const {
			N::begin();
			VK_ vkCmdBindPipeline(N::cmdbuf(), VK_ VK_PIPELINE_BIND_POINT_GRAPHICS,
				N::pass().pipe(uint16_t(index_)));
		}

	protected:
		void end() const { N::end(); }
		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	private:
		uint32_t index_;
	};

	template<typename SyncObject, typename N>
	struct m<cmd::wait<SyncObject>, N> : N {
		constexpr m(cmd::wait<SyncObject> info, auto&&...others)
			: N{ forward_(others)... }, object_{ &info.object } {}

		void begin() const {
			N::begin();
			auto event = object_->handle();
			VK_ vkCmdWaitEvents(N::cmdbuf(), 1u, &event,
				VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				0u, nullptr, 0u, nullptr, 0u, nullptr);
		}

	protected:
		void end() const { N::end(); }
		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	private:
		SyncObject* object_;
	};

	template<typename SyncObject, typename N>
	struct m<cmd::signal<SyncObject>, N> : N {
		constexpr m(cmd::signal<SyncObject> info, auto&&...others)
			: N{ forward_(others)... }, object_{ &info.object } {}

		void begin() const {
			N::begin();
			VK_ vkCmdSetEvent(N::cmdbuf(), object_->handle(),
				VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		}

	protected:
		void end() const { N::end(); }
		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	private:
		SyncObject* object_;
	};

	template<typename N>
	struct m<cmd::begin_command, N > : N {
		constexpr m(cmd::begin_command info, auto&&...others) 
			: N{forward_(others)...} {
			begin_cmd.flags = VK_ VkCommandBufferUsageFlagBits(
				((VK_ VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT << 1u) - 1u) & info.flags);
		}

		void begin() const {
			N::begin();
			VK_ vkBeginCommandBuffer(N::cmdbuf(), &begin_cmd);
		}

	protected:
		static constexpr auto cmbuf_index = []() constexpr {
			if constexpr (requires { N::cmbuf_index; }) {
				return N::cmbuf_index + 1u;
			}
			else {
				return 0u;
			}
		}();

	protected:
		void end() const {
			N::end();
			VK_ vkEndCommandBuffer(N::cmdbuf()) 
				| popup{ "[COMMANDS] Some command have some errors." };
		}

		constexpr auto& next() const noexcept { return *static_cast<N*>(this); }

	protected:
		VK_ VkCommandBufferBeginInfo begin_cmd {
			.sType = VK_ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};
	};

}

