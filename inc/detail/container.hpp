#pragma once

// Interface style: compact containers provide stable-address heterogeneous
// storage and structure-of-arrays helpers used by Vulkan create-info builders.
// Implementation: poly_list nodes carry an explicit deleter and intrusive
// links; typed APIs preserve ownership without virtual functions.

VKTL_EXPORT_ namespace vktl::detail {

	// currently standard c++ is not support *provenance* object.
	// thus object emplace inside this list should inherit from poly_list::node.
	class poly_list {
	public:
		struct node {
			friend poly_list;

			node() = default;

			node(const node&) = delete;
			node& operator=(const node&) = delete;
			node(node&&) noexcept = delete;
			node& operator=(node&&) noexcept = delete;

			template<::std::derived_from<node> T >
			auto& as() const noexcept { return *static_cast<const T*>(this); }
			template<::std::derived_from<node> T>
			auto& as() noexcept { return *static_cast<T*>(this); }

			template<::std::derived_from<node> T>
			operator T& () noexcept { return as<T>(); }
			template<::std::derived_from<node> T>
			operator T const& () const noexcept { return as<T>(); }

		private:
			void(*deleter)(node const*) noexcept = nullptr;
			node* prev = nullptr;
			node* next = nullptr;
		};

		using size_type = ::std::size_t;
		using difference_type = ::std::ptrdiff_t;

	private:
		template<bool is_const>
		struct basic_iterator {
			friend poly_list;

			using value_type = ::std::conditional_t<is_const, node const, node>;
			using difference_type = ::std::ptrdiff_t;
			using reference = value_type&;
			using pointer = value_type*;
			using iterator_category = ::std::bidirectional_iterator_tag;
			using iterator_concept = ::std::bidirectional_iterator_tag;

			basic_iterator() noexcept = default;
			basic_iterator(pointer p) noexcept : ptr_(p) {}

			basic_iterator(const basic_iterator&) = default;
			basic_iterator& operator=(const basic_iterator&) = default;

			template<bool other_const>
				requires(is_const && !other_const)
			basic_iterator(const basic_iterator<other_const>& other) noexcept : ptr_(other.ptr_) {}

			reference operator*() const noexcept { return *ptr_; }
			pointer operator->() const noexcept { return ptr_; }

			basic_iterator& operator++() noexcept { ptr_ = ptr_->next; return *this; }
			basic_iterator operator++(int) noexcept { auto tmp = *this; ptr_ = ptr_->next; return tmp; }
			basic_iterator& operator--() noexcept { ptr_ = ptr_->prev; return *this; }
			basic_iterator operator--(int) noexcept { auto tmp = *this; ptr_ = ptr_->prev; return tmp; }

			friend bool operator==(const basic_iterator& a, const basic_iterator& b) noexcept {
				return a.ptr_ == b.ptr_;
			}

		private:
			pointer ptr_ = nullptr;
		};

	public:
		using iterator = basic_iterator<false>;
		using const_iterator = basic_iterator<true>;

		poly_list() noexcept { reset_sentinel(); }
		~poly_list() { clear(); }

		poly_list(const poly_list&) = delete;
		poly_list& operator=(const poly_list&) = delete;

		poly_list(poly_list&& other) noexcept {
			reset_sentinel();
			swap(other);
		}

		poly_list& operator=(poly_list&& other) noexcept {
			if (this != &other) {
				clear();
				swap(other);
			}
			return *this;
		}

		template<::std::derived_from<node> T, typename... Args>
		T& emplace(const_iterator where, Args&&... args) requires(::std::constructible_from<T, Args&&...>) {
			auto* obj = new T(static_cast<Args&&>(args)...);
			auto* node_ptr = static_cast<node*>(obj);
			insert_before(const_cast<node*>(where.ptr_), node_ptr);
			node_ptr->deleter = [](node const* ptr) noexcept { delete static_cast<T const*>(ptr); };
			return *obj;
		}

		template<::std::derived_from<node> T, typename... Args>
		T& emplace_back(Args&&... args){
			return emplace<T>(&root_, static_cast<Args&&>(args)...);
		}

		template<::std::derived_from<node> T, typename... Args>
		T& emplace_front(Args&&... args) {
			return emplace<T>(root_.next, static_cast<Args&&>(args)...);
		}

		iterator begin() noexcept { return iterator(root_.next); }
		iterator end() noexcept { return iterator(&root_); }
		const_iterator begin() const noexcept { return const_iterator(root_.next); }
		const_iterator end() const noexcept { return const_iterator(&root_); }
		const_iterator cbegin() const noexcept { return begin(); }
		const_iterator cend() const noexcept { return end(); }

		iterator erase(const_iterator pos) noexcept {
			auto* n = pos.ptr_;
			node* next_node = n->next;

			n->prev->next = n->next;
			n->next->prev = n->prev;
			--size_;

			assert(n->deleter); // might be sential, it is not allowed.
			n->deleter(n);

			return iterator(next_node);
		}

		template<::std::derived_from<node> T>
		void erase(T& value) noexcept {
			auto* node_ptr = static_cast<node*>(&value);
			erase(const_iterator(node_ptr));
		}

		void pop_back() noexcept { if (!empty()) erase(const_iterator(root_.prev)); }
		void pop_front() noexcept { if (!empty()) erase(const_iterator(root_.next)); }

		void clear() noexcept {
			while (!empty()) pop_front();
		}

		VKTL_NODISCARD size_type size() const noexcept { return size_; }
		VKTL_NODISCARD bool empty() const noexcept { return size_ == 0; }

		void swap(poly_list& other) noexcept {
			if (this == &other) return;

			::std::swap(root_.next, other.root_.next);
			::std::swap(root_.prev, other.root_.prev);
			::std::swap(size_, other.size_);

			if (size_ == 0u) {
				reset_sentinel();
			}
			if (other.size_ == 0u) {
				other.reset_sentinel();
			}
		}

	private:
		void reset_sentinel() noexcept {
			root_.next = &root_;
			root_.prev = &root_;
		}

		void insert_before(node* pos, node* n) noexcept {
			n->next = pos;
			n->prev = pos->prev;
			pos->prev->next = n;
			pos->prev = n;
			++size_;
		}

	private:
		node root_;
		size_type size_ = 0u;
	};

	// if defined VK_DEFINE_HANDLE to customize vulkan's handle, 
	// you need to specialize this to enable customize vulkan handle.


	template<typename...Ts>
	struct vectors {
		static_assert(sizeof...(Ts) > 0, "multivec requires at least one type.");
		using types = ts<Ts...>;
		using size_type = size_t;
		using byte = ::std::byte;

		constexpr vectors() = default;
		explicit vectors(size_type size) requires((::std::default_initializable<Ts> && ...)) { resize(size); }
		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, const Args&> && ...))
		explicit vectors(size_type size, const Args&... values) { resize(size, values...); }

		~vectors() { clear(); deallocate_buffer(); }
		vectors(const vectors& other) { reserve(other.size_); copy_construct_from(other); }
		vectors& operator=(const vectors& other) { if (this != &other) { auto tmp = other; swap(tmp); } return *this; }
		vectors(vectors&& other) noexcept { swap(other); }
		vectors& operator=(vectors&& other) noexcept { if (this != &other) { clear(); deallocate_buffer(); swap(other); } return *this; }

		void swap(vectors& other) noexcept { ::std::swap(buffer_, other.buffer_); ::std::swap(size_, other.size_); ::std::swap(capacity_, other.capacity_); }
		VKTL_NODISCARD size_type size() const noexcept { return size_; }
		VKTL_NODISCARD size_type capacity() const noexcept { return capacity_; }
		VKTL_NODISCARD bool empty() const noexcept { return size_ == 0; }

		void reserve(size_type new_cap) {
			if (new_cap <= capacity_) return;
			reallocate(new_cap);
		}

		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, Args&&> && ...))
		void emplace_back(Args&&... args) { emplace(end(), static_cast<Args&&>(args)...); }
		void push_back(const Ts&... args)
			requires((::std::copy_constructible<Ts> && ...)) { emplace_back(args...); }
		void push_back(Ts&&... args)
			requires((::std::move_constructible<Ts> && ...)) { emplace_back(::std::move(args)...); }

		void pop_back() noexcept((::std::is_nothrow_destructible_v<Ts> && ...)) { assert(!empty()); destroy_row(--size_); }

		void resize(size_type new_size) requires((::std::default_initializable<Ts> && ...)) {
			if (new_size < size_) { shrink_to(new_size); return; }
			reserve(new_size);
			while (size_ < new_size) { construct_default_row(size_++); }
		}
		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, const Args&> && ...))
		void resize(size_type new_size, const Args&... values) {
			if (new_size < size_) { shrink_to(new_size); return; }
			reserve(new_size);
			while (size_ < new_size) { emplace_back(values...); }
		}

		void clear() noexcept { shrink_to(0); }

		template<size_type I>
		tuple_at_t<I, types>& get(size_type index) noexcept { assert(index < size_); return data<I>()[index]; }
		template<size_type I>
		const tuple_at_t<I, types>& get(size_type index) const noexcept { assert(index < size_); return data<I>()[index]; }

		template<typename T>
		T& get(size_type index) noexcept { return get<type_index<T>()>(index); }

		template<typename T>
		const T& get(size_type index) const noexcept { return get<type_index<T>()>(index); }

		template<size_type I>
		auto column() noexcept { return ::std::span{ data<I>(), size_ }; }
		template<size_type I>
		auto column() const noexcept { return ::std::span{ data<I>(), size_ }; }

		template<size_type I>
		auto get_span() noexcept { return column<I>(); }
		template<size_type I>
		auto get_span() const noexcept { return column<I>(); }

		::std::tuple<Ts&...> row(size_type index) { assert(index < size_); return row_tuple(index, ::std::index_sequence_for<Ts...>{}); }
		::std::tuple<Ts const&...> row(size_type index) const { assert(index < size_); return row_tuple(index, ::std::index_sequence_for<Ts...>{}); }

		bool operator==(vectors const& other) const noexcept requires(((::std::equality_comparable<Ts>) &&...)) {
			if (&other == this) {
				return true;
			}
			if (other.size() != this->size()) {
				return false;
			}
			for (auto i = 0u; i < size_; i++) {
				if (!::std::apply(
					[](auto const& left, auto const& right) {
						return left == right;
					}, ::std::forward_as_tuple(other.row(i), this->row(i)))) {
					return false;
				}
			}
			return true;
		}

	private:
		template<bool is_const>
		class row_iterator {
			template<bool> friend class row_iterator;
			using owner_type = ::std::conditional_t<is_const, const vectors, vectors>;
		public:
			using iterator_category = ::std::random_access_iterator_tag;
			using difference_type = ::std::ptrdiff_t;
			using value_type = ::std::tuple<::std::conditional_t<is_const, Ts const&, Ts&>...>;

			constexpr row_iterator() = default;
			constexpr row_iterator(owner_type* owner, size_type index) : owner_(owner), index_(index) {}
			constexpr row_iterator(const row_iterator<false>& other) requires(is_const) : owner_(other.owner_), index_(other.index_) {}
			constexpr row_iterator(const row_iterator&) = default;

			template<::std::size_t index = 0u>
			constexpr tuple_at_t<index, value_type> get() const { return::std::get<index>(owner_->row(index_)); }

			constexpr value_type operator*() const { return owner_->row(index_); }
			constexpr value_type operator[](difference_type offset) const { return owner_->row(index_ + offset); }

			constexpr row_iterator& operator++() { ++index_; return *this; }
			constexpr row_iterator operator++(int) { auto copy = *this; ++*this; return copy; }
			constexpr row_iterator& operator--() { --index_; return *this; }
			constexpr row_iterator operator--(int) { auto copy = *this; --*this; return copy; }
			constexpr row_iterator& operator+=(difference_type offset) { index_ += offset; return *this; }
			constexpr row_iterator& operator-=(difference_type offset) { index_ -= offset; return *this; }
			constexpr row_iterator operator+(difference_type offset) const { auto copy = *this; return copy += offset; }
			constexpr row_iterator operator-(difference_type offset) const { auto copy = *this; return copy -= offset; }

			template<bool C> constexpr difference_type operator-(const row_iterator<C>& other) const {
				return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
			}
			constexpr bool operator!=(row_iterator const& other) const { return !operator==(other); }
			constexpr bool operator==(row_iterator const& other) const { return owner_ == other.owner_ && index_ == other.index_; }
			template<bool C> constexpr bool operator==(row_iterator<C> const& other) const { return owner_ == other.owner_ && index_ == other.index_; }

			// template<bool C> constexpr auto operator<=>(const row_iterator<C>& other) const { return index_ <=> other.index_; }

			VKTL_NODISCARD size_type index() const noexcept { return index_; }
			VKTL_NODISCARD owner_type* owner() const noexcept { return owner_; }

		private:
			owner_type* owner_ = nullptr;
			size_type index_ = 0;
		};

	public:
		using iterator = row_iterator<false>;
		using const_iterator = row_iterator<true>;

		VKTL_NODISCARD iterator begin() noexcept { return { this, 0 }; }
		VKTL_NODISCARD iterator end() noexcept { return { this, size_ }; }
		VKTL_NODISCARD const_iterator begin() const noexcept { return { this, 0 }; }
		VKTL_NODISCARD const_iterator end() const noexcept { return { this, size_ }; }
		VKTL_NODISCARD const_iterator cbegin() const noexcept { return begin(); }
		VKTL_NODISCARD const_iterator cend() const noexcept { return end(); }

		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, Args&&> && ...))
		iterator emplace(const_iterator where, Args&&... args) {
			const auto pos = checked_index(where, true);
			if (size_ == capacity_) {
				emplace_reallocating(pos, ::std::forward<Args>(args)...);
			}
			else {
				emplace_in_place(pos, ::std::forward<Args>(args)...);
			}
			return { this, pos };
		}
		iterator insert(const_iterator where, const Ts&... args)
			requires((::std::copy_constructible<Ts> && ...)) { return emplace(where, args...); }
		iterator insert(const_iterator where, Ts&&... args)
			requires((::std::move_constructible<Ts> && ...)) { return emplace(where, ::std::move(args)...); }

		iterator erase(const_iterator where) {
			const auto pos = checked_index(where, false);
			move_rows_left(pos + 1, pos, size_ - pos - 1);
			destroy_row(--size_);
			return { this, pos };
		}

		template<size_type I>
		tuple_at_t<I, types>* data() noexcept { return reinterpret_cast<tuple_at_t<I, types>*>(buffer_ + get_offsets(capacity_)[I]); }
		template<size_type I>
		const tuple_at_t<I, types>* data() const noexcept { return reinterpret_cast<const tuple_at_t<I, types>*>(buffer_ + get_offsets(capacity_)[I]); }

	private:
		static constexpr size_type num_types = sizeof...(Ts);
		static constexpr size_type max_alignment = ::std::max({ alignof(Ts)... });

		static auto get_offsets(size_type cap) noexcept {
			::std::array<size_type, num_types> offsets{};
			size_type offset = 0, idx = 0;
			((offset = align_up(offset, alignof(Ts)), offsets[idx++] = offset, offset += cap * sizeof(Ts)), ...);
			return offsets;
		}
		static size_type calc_total_bytes(size_type cap) noexcept {
			size_type offset = 0;
			((offset = align_up(offset, alignof(Ts)), offset += cap * sizeof(Ts)), ...);
			return offset;
		}
		static byte* allocate_raw(size_type cap) {
			return cap == 0 ? nullptr : static_cast<byte*>(::operator new(calc_total_bytes(cap), ::std::align_val_t(max_alignment)));
		}
		static void deallocate_raw(byte* buffer) noexcept { ::operator delete(buffer, ::std::align_val_t(max_alignment)); }

		template<typename T>
		static consteval size_type type_index() noexcept { return find_if_same_v<T, types>; }

		template<size_type... Is> auto row_tuple(size_type index, ::std::index_sequence<Is...>) { return ::std::tie(get<Is>(index)...); }
		template<size_type... Is> auto row_tuple(size_type index, ::std::index_sequence<Is...>) const { return ::std::tie(get<Is>(index)...); }

		template<typename... Args> void construct_row(size_type index, Args&&...args) {
			construct_row_impl(index, ::std::index_sequence_for<Ts...>{}, ::std::forward_as_tuple(::std::forward<Args>(args)...));
		}
		template<size_type... Is, typename Tuple> void construct_row_impl(size_type index, ::std::index_sequence<Is...>, Tuple&& args) {
			size_type constructed = 0;
			try { ((::std::construct_at(data<Is>() + index, ::std::get<Is>(::std::forward<Tuple>(args))), ++constructed), ...); }
			catch (...) { destroy_constructed_row_prefix(index, constructed); throw; }
		}

		void construct_default_row(size_type index) {
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				size_type constructed = 0;
				try { ((::std::construct_at(data<Is>() + index), ++constructed), ...); }
				catch (...) { destroy_constructed_row_prefix(index, constructed); throw; }
			}(::std::index_sequence_for<Ts...>{});
		}

		void destroy_constructed_row_prefix(size_type index, size_type constructed) noexcept {
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				((Is < constructed ? (::std::destroy_at(data<Is>() + index)) : void()), ...);
			}(::std::index_sequence_for<Ts...>{});
		}
		void destroy_row(size_type index) noexcept {
			[&] <size_type... Is>(::std::index_sequence<Is...>) { (::std::destroy_at(data<Is>() + index), ...); }(::std::index_sequence_for<Ts...>{});
		}
		void shrink_to(size_type new_size) noexcept { while (size_ > new_size) { pop_back(); } }

		void copy_construct_from(const vectors& other) {
			for (size_type i = 0; i < other.size_; ++i) {
				::std::apply([this](const Ts&... values) { emplace_back(values...); }, other.row(i));
			}
		}
		void move_construct_row_into(vectors& target, size_type index) {
			::std::apply([&](Ts&... values) { target.emplace_back(::std::move(values)...); }, row(index));
		}

		void reallocate(size_type new_cap) {
			vectors tmp;
			tmp.buffer_ = allocate_raw(new_cap);
			tmp.capacity_ = new_cap;
			try { for (size_type i = 0; i < size_; ++i) move_construct_row_into(tmp, i); }
			catch (...) { tmp.clear(); tmp.deallocate_buffer(); throw; }
			clear();
			deallocate_buffer();
			swap(tmp);
		}

		template<typename... Args>
		void emplace_reallocating(size_type pos, Args&&... args) {
			vectors tmp;
			tmp.buffer_ = allocate_raw(capacity_after_insert());
			tmp.capacity_ = capacity_after_insert();
			try {
				for (size_type i = 0; i < pos; ++i) move_construct_row_into(tmp, i);
				tmp.construct_row(tmp.size_, ::std::forward<Args>(args)...);
				++tmp.size_;
				for (size_type i = pos; i < size_; ++i) move_construct_row_into(tmp, i);
			}
			catch (...) { tmp.clear(); tmp.deallocate_buffer(); throw; }
			clear();
			deallocate_buffer();
			swap(tmp);
		}

		template<typename... Args>
		void emplace_in_place(size_type pos, Args&&... args) {
			if (pos == size_) {
				construct_row(size_, ::std::forward<Args>(args)...);
				++size_;
				return;
			}
			construct_row(size_, ::std::move(get<Ts>(size_ - 1))...);
			++size_;
			try {
				move_rows_right(pos, size_ - 2, 1);
				assign_row(pos, ::std::forward<Args>(args)...);
			}
			catch (...) {
				--size_;
				destroy_row(size_);
				throw;
			}
		}

		void move_rows_right(size_type first, size_type last, size_type distance) {
			for (size_type i = last + 1; i-- > first;) assign_row_from(i + distance, i);
		}
		void move_rows_left(size_type first, size_type dest, size_type count) {
			for (size_type i = 0; i < count; ++i) assign_row_from(dest + i, first + i);
		}
		void assign_row_from(size_type dest, size_type src) {
			[&] <size_type... Is>(::std::index_sequence<Is...>) { ((get<Is>(dest) = ::std::move(get<Is>(src))), ...); }(::std::index_sequence_for<Ts...>{});
		}
		template<typename... Args>
		void assign_row(size_type index, Args&&... args) {
			assign_row_impl(index, ::std::index_sequence_for<Ts...>{}, ::std::forward_as_tuple(::std::forward<Args>(args)...));
		}
		template<size_type... Is, typename Tuple>
		void assign_row_impl(size_type index, ::std::index_sequence<Is...>, Tuple&& args) {
			((get<Is>(index) = ::std::get<Is>(::std::forward<Tuple>(args))), ...);
		}

		size_type capacity_after_insert() const noexcept { return size_ == capacity_ ? (capacity_ == 0 ? 1 : capacity_ * 2) : capacity_; }
		size_type checked_index(const_iterator where, bool allow_end) const noexcept {
			assert(where.owner() == this);
			assert(where.index() < size_ || (allow_end && where.index() == size_));
			return where.index();
		}

		void deallocate_buffer() noexcept { if (buffer_) { deallocate_raw(buffer_); buffer_ = nullptr; capacity_ = 0; } }

	private:
		byte* buffer_ = nullptr;
		size_type size_ = 0;
		size_type capacity_ = 0;
	};

	template<typename T>
	struct access_list {
		using range_type = typename T::range_type;

		access_list() = default;

		void insert(T access) {
			auto& vec = this->accesses_;
			auto itb = vec.end() - 1u;
			while (itb != vec.begin() - 1u && itb->stages != access.stages) { itb--; }
			if (itb == vec.begin() - 1u) {
				vec.emplace_back(::std::move(access));
				return;
			}
			auto ite = itb - 1u;
			while (ite != vec.begin() - 1u && ite->stages == access.stages) { ite--; }

			list<T> stack;
			stack.emplace_front(::std::move(access));
			for (; itb != ite; ) {
				auto c = ::std::move(stack.front());
				stack.erase(stack.begin());
				if (c.stages == itb->stages && itb->dependencies == c.dependencies) {
					auto& it_range = static_cast<range_type const&>(*itb);
					auto& c_range = static_cast<range_type const&>(c);
					if (*itb == c && subres.adjacent_intersect(it_range, c_range)) {
						static_cast<range_type&>(c) = subres.merge(it_range, c_range);
						itb = vec.erase(itb);
					}
					else if (subres.intersected(it_range, c_range)) {
						auto intersected = subres.get_intersect(it_range, c_range);

						auto& a = *itb;
						static_cast<range_type&>(a) = intersected;
						a |= c;

						bool can_break = !stack.size();
						for (auto non_intersected : subres.get_not_intersected(it_range, c_range)) {
							auto slice = non_intersected.is_first ? *itb : c;
							static_cast<range_type&>(slice) = static_cast<range_type&&>(non_intersected);
							if (!non_intersected.is_first && !subres.empty(slice)) {
								vec.insert(itb, ::std::move(slice));
								can_break = false;
							}
							else {
								itb = vec.insert(itb, slice);
							}
						}
						if (can_break) {
							break;
						}
					}
					itb--;
				}
			}
			itb++; // if at ite, then itb is not stage, inc to make insert operation correct.
			for (auto&& v : ::std::move(stack)) {
				itb = vec.insert(itb, ::std::move(v));
			}
		}

	private:
		vector<T> accesses_;
	};
}
