#pragma once

// not much say, only few containers.

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

		node& front() noexcept { assert(this->root_.next != &this->root_); return *this->root_.next; }
		node const& front() const noexcept { assert(this->root_.next != &this->root_); return *this->root_.next; }
		node& back() noexcept { assert(this->root_.prev != &this->root_); return *this->root_.prev; }
		node const& back() const noexcept { assert(this->root_.prev != &this->root_); return *this->root_.prev; }

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
			} else {
				root_.next->prev = &root_;
				root_.prev->next = &root_;
			}
			
			if (other.size_ == 0u) {
				other.reset_sentinel();
			} else {
				other.root_.next->prev = &other.root_;
				other.root_.prev->next = &other.root_;
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

	template<typename...Ts>
	struct vectors;

	template<typename...Ts>
	struct iterators {
		static constexpr auto is_const = ((::std::is_const_v<Ts>) || ...);
		using size_type = size_t;

		using iterator_category = ::std::random_access_iterator_tag;
		using iterator_concept = ::std::random_access_iterator_tag;
		using difference_type = ::std::ptrdiff_t;
		using value_type = ::std::tuple<Ts...>;
		using reference = ::std::tuple<Ts&...>;

		constexpr iterators() = default;
		constexpr explicit iterators(::std::tuple<Ts*...> pointers) noexcept
			: pointers_(pointers) 
		{}
		template<typename...Os>
			requires(::std::convertible_to<Os(*)[], Ts(*)[]>)
		constexpr iterators(iterators<Os...> const& other)
			: pointers_{ other.pointers }
		{}
		constexpr iterators(iterators const&) noexcept = default;
		constexpr iterators& operator=(iterators const&) noexcept = default;

		template<size_type I = 0u>
		constexpr decltype(auto) get() const noexcept {
			return *::std::get<I>(pointers_);
		}

		constexpr reference operator*() const noexcept {
			return dereference(::std::index_sequence_for<Ts...>{});
		}
		constexpr reference operator[](difference_type offset) const noexcept {
			return dereference_at(offset, ::std::index_sequence_for<Ts...>{});
		}
		constexpr iterators& operator++() noexcept {
			advance(1);
			return *this;
		}
		constexpr iterators operator++(int) noexcept {
			auto copy = *this;
			++*this;
			return copy;
		}

		constexpr iterators& operator--() noexcept {
			advance(-1);
			return *this;
		}
		constexpr iterators operator--(int) noexcept {
			auto copy = *this;
			--*this;
			return copy;
		}
		constexpr iterators& operator+=(difference_type offset) noexcept {
			advance(offset);
			return *this;
		}
		constexpr iterators& operator-=(difference_type offset) noexcept {
			advance(-offset);
			return *this;
		}
		constexpr iterators operator+(difference_type offset) const noexcept {
			auto copy = *this;
			return copy += offset;
		}
		constexpr iterators operator-(difference_type offset) const noexcept {
			auto copy = *this;
			return copy -= offset;
		}
		friend constexpr iterators operator+(difference_type offset, iterators self) noexcept {
			return self += offset;
		}
		constexpr difference_type operator-(const iterators& other) const noexcept {
			return ::std::get<0>(pointers_) -
				::std::get<0>(other.pointers_);
		}
		constexpr bool operator==(const iterators& other) const noexcept {
			return ::std::get<0>(pointers_) == ::std::get<0>(other.pointers_);
		}
		constexpr auto operator<=>(const iterators& other) const noexcept {
			return ::std::get<0>(pointers_) <=> ::std::get<0>(other.pointers_);
		}

	private:
		template<::std::size_t... Is>
		constexpr reference dereference(::std::index_sequence<Is...>) const noexcept {
			return { *::std::get<Is>(pointers_)... };
		}

		template<::std::size_t... Is>
		constexpr reference dereference_at(difference_type offset, ::std::index_sequence<Is...>) const noexcept {
			return { ::std::get<Is>(pointers_)[offset]... };
		}

		constexpr void advance(difference_type offset) noexcept {
			::std::apply([offset](auto*&... pointers) { ((pointers += offset), ...); }, pointers_);
		}

		::std::tuple<Ts*...> pointers_{};
	};

	template<typename...Ts>
	struct spans {
		static_assert(sizeof...(Ts) > 0, "multispan requires at least one type.");
		static_assert(((!::std::is_reference_v<Ts>) && ...), "Spans not allow reference.");
		static_assert(((::std::is_const_v<Ts>) && ...) || ((!::std::is_const_v<Ts>) && ...), "Must all const or non const.");
		static_assert(((::std::is_volatile_v<Ts>) && ...) || ((!::std::is_volatile_v<Ts>) && ...), "Must all volatile or non const.");
		
		template<typename...> friend struct spans;

		using types = ts<Ts...>;
		using size_type = ::std::size_t;
		using difference_type = ::std::ptrdiff_t;

		template<size_type I> 
		using element_t = tuple_at_t<I, types>;
		static constexpr size_type num_types = sizeof...(Ts);

		constexpr spans() noexcept = default;
		constexpr spans(const spans&) noexcept = default;
		constexpr spans& operator=(const spans&) noexcept = default;

		// ---- adopt raw pointers -------------------------------------------------
		constexpr spans(size_type count, Ts*...pointers) noexcept
			: pointers_{ pointers... }, size_(count) {
		}

		// ---- adopt N contiguous containers (vector, array, span, C array, ...) ---
		template<typename... Rs>
			requires(sizeof...(Rs) == num_types
			&& (!::std::same_as<::std::remove_cvref_t<Rs>, spans> && ...)
			&& (::std::ranges::contiguous_range<Rs> && ...)
			&& (::std::ranges::sized_range<Rs> && ...)
			&& (::std::convertible_to<::std::ranges::range_value_t<Rs>(*)[], Ts(*)[]> && ...))
		constexpr explicit spans(Rs&... ranges)
			: pointers_{ ::std::ranges::data(ranges)... }
			, size_(::std::min({ size_type(::std::ranges::size(ranges))... })) {
		}

		// ---- adopt a multivec ---------------------------------------------------
		template<typename... Us>
			requires(sizeof...(Us) == num_types && (::std::convertible_to<Us(*)[], Ts(*)[]> && ...))
		constexpr spans(vectors<Us...>& owner) noexcept { adopt(owner); }

		template<typename... Us>
			requires(sizeof...(Us) == num_types && (::std::convertible_to<const Us(*)[], Ts(*)[]> && ...))
		constexpr spans(const vectors<Us...>& owner) noexcept { adopt(owner); }

		// ---- qualification conversion: spans<T...> -> spans<const T...> ---------
		template<typename... Us>
			requires(sizeof...(Us) == num_types 
			&& !(::std::same_as<Us, Ts> && ...)
			&& (::std::convertible_to<Us(*)[], Ts(*)[]> && ...))
		constexpr spans(const spans<Us...>& other) noexcept : size_(other.size()) {
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				pointers_ = ::std::tuple<Ts*...>{ other.template data<Is>()... };
			}(::std::index_sequence_for<Ts...>{});
		}

		constexpr void swap(spans& other) noexcept {
			::std::swap(pointers_, other.pointers_);
			::std::swap(size_, other.size_);
		}

		VKTL_NODISCARD constexpr size_type size() const noexcept { return size_; }
		VKTL_NODISCARD constexpr bool empty() const noexcept { return size_ == 0; }
		VKTL_NODISCARD constexpr size_type size_bytes() const noexcept { return (size_ * ((sizeof(Ts)) + ...)); }

		template<size_type I>
		VKTL_NODISCARD constexpr element_t<I>* data() const noexcept { return ::std::get<I>(pointers_); }
		template<typename T>
		VKTL_NODISCARD constexpr auto data() const noexcept { return data<type_index<T>()>(); }

		template<size_type I>
		VKTL_NODISCARD constexpr element_t<I>& get(size_type index) const noexcept { assert(index < size_); return data<I>()[index]; }
		template<typename T>
		VKTL_NODISCARD constexpr auto& get(size_type index) const noexcept { return get<type_index<T>()>(index); }

		template<size_type I>
		VKTL_NODISCARD constexpr auto column() const noexcept { return ::std::span{ data<I>(), size_ }; }
		template<typename T>
		VKTL_NODISCARD constexpr auto column() const noexcept { return column<type_index<T>()>(); }
		template<size_type I>
		VKTL_NODISCARD constexpr auto get_span() const noexcept { return column<I>(); }

		VKTL_NODISCARD constexpr ::std::tuple<Ts&...> row(size_type index) const {
			assert(index < size_);
			return row_tuple(index, ::std::index_sequence_for<Ts...>{});
		}
		VKTL_NODISCARD constexpr ::std::tuple<Ts&...> operator[](size_type index) const { return row(index); }
		VKTL_NODISCARD constexpr ::std::tuple<Ts&...> front() const { assert(size_); return row(0u); }
		VKTL_NODISCARD constexpr ::std::tuple<Ts&...> back() const { assert(size_); return row(size_ - 1u); }

		// ---- sub-views ----------------------------------------------------------
		VKTL_NODISCARD constexpr spans subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const noexcept {
			assert(offset <= size_);
			const auto n = ::std::min(count, size_ - offset);
			return[&] <size_type... Is>(::std::index_sequence<Is...>) {
				return spans{ n, (data<Is>() + offset)... };
			}(::std::index_sequence_for<Ts...>{});
		}
		VKTL_NODISCARD constexpr spans first(size_type count) const noexcept { return subspan(0, count); }
		VKTL_NODISCARD constexpr spans last(size_type count) const noexcept { assert(count <= size_); return subspan(size_ - count, count); }
		VKTL_NODISCARD constexpr spans<const Ts...> as_const() const noexcept { return spans<const Ts...>{ *this }; }

		VKTL_NODISCARD constexpr bool operator==(const spans& other) const
			requires((::std::equality_comparable<::std::remove_const_t<Ts>> && ...)) {
			if (other.size_ != size_) { return false; }
			for (size_type i = 0; i < size_; ++i) {
				const bool equal = [&] <size_type... Is>(::std::index_sequence<Is...>) {
					return ((get<Is>(i) == other.template get<Is>(i)) && ...);
				}(::std::index_sequence_for<Ts...>{});
				if (!equal) { return false; }
			}
			return true;
		}

		// element constness lives in Ts, so a span view has a single iterator flavour
		using iterator = iterators<Ts...>;
		using const_iterator = iterators<Ts...>;

		VKTL_NODISCARD constexpr iterator begin() const noexcept { return { *this, 0 }; }
		VKTL_NODISCARD constexpr iterator end() const noexcept { return { *this, size_ }; }
		VKTL_NODISCARD constexpr const_iterator cbegin() const noexcept { return begin(); }
		VKTL_NODISCARD constexpr const_iterator cend() const noexcept { return end(); }

	private:
		template<typename T>
		static consteval size_type type_index() noexcept { return find_if_same_v<T, types>; }

		template<size_type... Is>
		constexpr auto row_tuple(size_type index, ::std::index_sequence<Is...>) const { return ::std::tie(get<Is>(index)...); }

		template<typename Owner>
		constexpr void adopt(Owner& owner) noexcept {
			[&]<size_type... Is>(::std::index_sequence<Is...>) {
				pointers_ = ::std::tuple<Ts*...>{ owner.template data<Is>()... };
			}(::std::index_sequence_for<Ts...>{});
			size_ = owner.size();
		}

	private:
		::std::tuple<Ts*...> pointers_{};
		size_type size_ = 0;
	};
	template<::std::ranges::contiguous_range... Rs>
	spans(Rs&...) -> spans<::std::ranges::range_value_t<Rs>...>;
	template<typename... Us>
	spans(vectors<Us...>&) -> spans<Us...>;
	template<typename... Us>
	spans(const vectors<Us...>&) -> spans<const Us...>;

	template<typename...Ts>
	struct vectors {
		static_assert(sizeof...(Ts) > 0, "Vectors requires at least one type.");
		static_assert(((!::std::is_reference_v<Ts>) && ...), "Vectors not allow reference.");
		static_assert(((!::std::is_const_v<Ts>) && ...) && ((!::std::is_volatile_v<Ts>) && ...), "Vectors not allow const or volatile.");

		using types = ts<Ts...>;
		using size_type = ::std::size_t;
		using byte = ::std::byte;
		using view_type = spans<Ts...>;
		using const_view_type = spans<Ts const...>;
		using iterator = iterators<Ts...>;
		using const_iterator = iterators<Ts const...>;

		constexpr vectors() = default;
		explicit vectors(size_type size) 
			requires((::std::default_initializable<Ts> && ...)) { resize(size); }
		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, const Args&> && ...))
		explicit vectors(size_type size, Args const&...values) { resize(size, values...); }

		~vectors() { clear(); this->deallocate_buffer(); }

		vectors(const vectors& other) { reserve(other.size_); copy_construct_from(other); }
		vectors& operator=(const vectors& other) { if (this != &other) { auto tmp = other; swap(tmp); } return *this; }
		vectors(vectors&& other) noexcept { swap(other); }
		vectors& operator=(vectors&& other) noexcept { if (this != &other) { clear(); deallocate_buffer(); swap(other); } return *this; }

		void swap(vectors& other) noexcept {
			::std::swap(buffer_, other.buffer_);
			::std::swap(size_, other.size_);
			::std::swap(capacity_, other.capacity_);
		}

		VKTL_NODISCARD size_type size() const noexcept { return size_; }
		VKTL_NODISCARD size_type capacity() const noexcept { return capacity_; }
		VKTL_NODISCARD bool empty() const noexcept { return size_ == 0; }

		// ---- the view is now the single source of truth for element access ------

		VKTL_NODISCARD view_type view() noexcept { return raw_view().first(size_); }
		VKTL_NODISCARD const_view_type view() const noexcept { return const_view_type{ raw_view().first(size_) }; }
		operator view_type() noexcept { return view(); }
		operator const_view_type() const noexcept { return view(); }

		template<size_type I> VKTL_NODISCARD auto* data() noexcept { return raw_view().template data<I>(); }
		template<size_type I> VKTL_NODISCARD const auto* data() const noexcept { return raw_view().template data<I>(); }

		template<size_type I> VKTL_NODISCARD auto& get(size_type i) noexcept { return view().template get<I>(i); }
		template<size_type I> VKTL_NODISCARD const auto& get(size_type i) const noexcept { return view().template get<I>(i); }
		template<typename T> VKTL_NODISCARD T& get(size_type i) noexcept { return view().template get<type_index<T>()>(i); }
		template<typename T> VKTL_NODISCARD const T& get(size_type i) const noexcept { return view().template get<type_index<T>()>(i); }

		template<size_type I> VKTL_NODISCARD auto column() noexcept { return view().template column<I>(); }
		template<size_type I> VKTL_NODISCARD auto column() const noexcept { return view().template column<I>(); }
		template<size_type I> VKTL_NODISCARD auto get_span() noexcept { return column<I>(); }
		template<size_type I> VKTL_NODISCARD auto get_span() const noexcept { return column<I>(); }

		VKTL_NODISCARD::std::tuple<Ts&...> row(size_type i) { return view().row(i); }
		VKTL_NODISCARD::std::tuple<Ts const&...> row(size_type i) const { return view().row(i); }
		VKTL_NODISCARD::std::tuple<Ts&...> front() { return view().front(); }
		VKTL_NODISCARD::std::tuple<Ts const&...> front() const { return view().front(); }
		VKTL_NODISCARD::std::tuple<Ts&...> back() { return view().back(); }
		VKTL_NODISCARD::std::tuple<Ts const&...> back() const { return view().back(); }

		VKTL_NODISCARD iterator begin() noexcept { return { view(), 0 }; }
		VKTL_NODISCARD iterator end() noexcept { return { view(), size_ }; }
		VKTL_NODISCARD const_iterator begin() const noexcept { return { view(), 0 }; }
		VKTL_NODISCARD const_iterator end() const noexcept { return { view(), size_ }; }
		VKTL_NODISCARD const_iterator cbegin() const noexcept { return begin(); }
		VKTL_NODISCARD const_iterator cend() const noexcept { return end(); }

		bool operator==(const vectors& other) const noexcept
			requires((::std::equality_comparable<Ts> && ...)) {
			return this == &other || view() == other.view();
		}

		void reserve(size_type new_cap) { if (new_cap > capacity_) reallocate(new_cap); }

		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, Args&&> && ...))
		void emplace_back(Args&&... args) { emplace(end(), static_cast<Args&&>(args)...); }
		void push_back(const Ts&... args) requires((::std::copy_constructible<Ts> && ...)) { emplace_back(args...); }
		void push_back(Ts&&... args) requires((::std::move_constructible<Ts> && ...)) { emplace_back(::std::move(args)...); }
		void pop_back() noexcept { assert(!empty()); destroy_row(--size_); }

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

		template<typename... Args>
			requires(sizeof...(Args) == sizeof...(Ts) && (::std::constructible_from<Ts, Args&&> && ...))
		iterator emplace(const_iterator where, Args&&... args) {
			const auto pos = checked_index(where, true);
			if (size_ == capacity_) { emplace_reallocating(pos, ::std::forward<Args>(args)...); }
			else { emplace_in_place(pos, ::std::forward<Args>(args)...); }
			return { view(), pos };
		}
		iterator insert(const_iterator where, const Ts&... args)
			requires((::std::copy_constructible<Ts> && ...)) { return emplace(where, args...); }
		iterator insert(const_iterator where, Ts&&... args)
			requires((::std::move_constructible<Ts> && ...)) { return emplace(where, ::std::move(args)...); }

		iterator erase(const_iterator where) {
			const auto pos = checked_index(where, false);
			move_rows_left(pos + 1, pos, size_ - pos - 1);
			destroy_row(--size_);
			return { view(), pos };
		}

	private:
		static constexpr size_type num_types = sizeof...(Ts);
		static constexpr size_type max_alignment = ::std::max({ alignof(Ts)... });
		static constexpr::std::index_sequence_for<Ts...> indices{};

		// one offsets computation, all N column pointers; covers uninitialised slots
		view_type raw_view() const noexcept {
			if (!buffer_) { return {}; }
			const auto offsets = get_offsets(capacity_);
			return[&] <size_type... Is>(::std::index_sequence<Is...>) {
				return view_type{ capacity_,
					reinterpret_cast<tuple_at_t<Is, types>*>(const_cast<byte*>(buffer_) + offsets[Is])... };
			}(indices);
		}

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
			return cap == 0 ? nullptr
				: static_cast<byte*>(::operator new(calc_total_bytes(cap), ::std::align_val_t(max_alignment)));
		}
		static void deallocate_raw(byte* buffer) noexcept { ::operator delete(buffer, ::std::align_val_t(max_alignment)); }
		template<typename T> static consteval size_type type_index() noexcept { return find_if_same_v<T, types>; }

		// ---- row lifetime, all expressed against a raw view ---------------------
		template<typename Tuple>
		void construct_row_from(size_type index, Tuple&& args) {
			const auto v = raw_view();
			size_type constructed = 0;
			try {
				[&] <size_type... Is>(::std::index_sequence<Is...>) {
					((::std::construct_at(v.template data<Is>() + index,
						::std::get<Is>(::std::forward<Tuple>(args))), ++constructed), ...);
				}(indices);
			}
			catch (...) { destroy_row_prefix(v, index, constructed); throw; }
		}
		template<typename... Args> void construct_row(size_type index, Args&&... args) {
			construct_row_from(index, ::std::forward_as_tuple(::std::forward<Args>(args)...));
		}
		void construct_default_row(size_type index) {
			const auto v = raw_view();
			size_type constructed = 0;
			try {
				[&] <size_type... Is>(::std::index_sequence<Is...>) {
					((::std::construct_at(v.template data<Is>() + index), ++constructed), ...);
				}(indices);
			}
			catch (...) { destroy_row_prefix(v, index, constructed); throw; }
		}
		static void destroy_row_prefix(const view_type& v, size_type index, size_type constructed) noexcept {
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				((Is < constructed ? ::std::destroy_at(v.template data<Is>() + index) : void()), ...);
			}(indices);
		}
		void destroy_row(size_type index) noexcept { destroy_row_prefix(raw_view(), index, num_types); }
		void shrink_to(size_type new_size) noexcept { while (size_ > new_size) { pop_back(); } }

		void copy_construct_from(const vectors& other) {
			for (auto r : other.view()) { ::std::apply([this](const Ts&... vs) { emplace_back(vs...); }, r); }
		}
		void move_construct_row_into(vectors& target, size_type index) {
			::std::apply([&](Ts&... vs) { target.emplace_back(::std::move(vs)...); }, view().row(index));
		}

		void reallocate(size_type new_cap) { relocate_into(new_cap, size_, [](vectors&) {}); }

		template<typename... Args>
		void emplace_reallocating(size_type pos, Args&&... args) {
			relocate_into(capacity_after_insert(), pos, [&](vectors& tmp) {
				tmp.construct_row(tmp.size_, ::std::forward<Args>(args)...);
				++tmp.size_;
			});
		}

		// shared body of reallocate / emplace_reallocating
		template<typename Middle>
		void relocate_into(size_type new_cap, size_type split, Middle&& middle) {
			vectors tmp;
			tmp.buffer_ = allocate_raw(new_cap);
			tmp.capacity_ = new_cap;
			try {
				for (size_type i = 0; i < split; ++i) { move_construct_row_into(tmp, i); }
				middle(tmp);
				for (size_type i = split; i < size_; ++i) { move_construct_row_into(tmp, i); }
			}
			catch (...) { tmp.clear(); tmp.deallocate_buffer(); throw; }
			clear();
			deallocate_buffer();
			swap(tmp);
		}

		template<typename... Args>
		void emplace_in_place(size_type pos, Args&&... args) {
			if (pos == size_) { construct_row(size_, ::std::forward<Args>(args)...); ++size_; return; }
			// index-based, so vectors<int,int> works (the old get<Ts>() form was ambiguous)
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				construct_row(size_, ::std::move(get<Is>(size_ - 1))...);
			}(indices);
			++size_;
			try {
				move_rows_right(pos, size_ - 2, 1);
				assign_row(pos, ::std::forward<Args>(args)...);
			}
			catch (...) { --size_; destroy_row(size_); throw; }
		}

		void move_rows_right(size_type first, size_type last, size_type distance) {
			for (size_type i = last + 1; i-- > first;) { assign_row_from(i + distance, i); }
		}
		void move_rows_left(size_type first, size_type dest, size_type count) {
			for (size_type i = 0; i < count; ++i) { assign_row_from(dest + i, first + i); }
		}
		void assign_row_from(size_type dest, size_type src) {
			const auto v = view();
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				((v.template get<Is>(dest) = ::std::move(v.template get<Is>(src))), ...);
			}(indices);
		}
		template<typename... Args> void assign_row(size_type index, Args&&... args) {
			const auto v = view();
			auto tuple = ::std::forward_as_tuple(::std::forward<Args>(args)...);
			[&] <size_type... Is>(::std::index_sequence<Is...>) {
				((v.template get<Is>(index) = ::std::get<Is>(::std::move(tuple))), ...);
			}(indices);
		}

		size_type capacity_after_insert() const noexcept {
			return size_ == capacity_ ? (capacity_ == 0 ? 1 : capacity_ * 2) : capacity_;
		}
		size_type checked_index(const_iterator where, bool allow_end) const noexcept {
			assert(where.index() < size_ || (allow_end && where.index() == size_));
			return where.index();
		}
		void deallocate_buffer() noexcept { if (buffer_) { deallocate_raw(buffer_); buffer_ = nullptr; capacity_ = 0; } }

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
