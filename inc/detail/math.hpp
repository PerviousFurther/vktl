#pragma once

VKTL_EXPORT_ namespace vktl::math {
  template <size_t... ids> using extents = ::std::index_sequence<ids...>;

  template <typename T = float> struct vec1 {
    static constexpr size_t row = 1u;
    static constexpr size_t col = 0u;
    using extent = extents<row>;
    using type = T[row];
    union {
      T r;
      T x;
      T u;
      type value;
    };

    constexpr T &operator[](size_t index) noexcept { return value[index]; }
    constexpr T const &operator[](size_t index) const noexcept {
      return value[index];
    }
    constexpr operator T const &() const noexcept { return r; }
    constexpr operator T &() noexcept { return r; }
    constexpr operator type const &() const noexcept { return value; }
    constexpr operator type &() noexcept { return value; }
  };
  template <typename T = float> struct vec2 {
    static constexpr size_t row = 2u;
    static constexpr size_t col = 0u;
    using extent = extents<row>;
    using type = T[row];
    union {
      struct {
        T r, g;
      };
      struct {
        T x, y;
      };
      struct {
        T u, v;
      };
      type value;
    };
    constexpr T &operator[](size_t index) noexcept { return value[index]; }
    constexpr T const &operator[](size_t index) const noexcept {
      return value[index];
    }
    constexpr operator type const &() const noexcept { return value; }
    constexpr operator type &() noexcept { return value; }
  };
  template <typename T = float> struct vec3 {
    static constexpr size_t row = 3u;
    static constexpr size_t col = 0u;
    using extent = extents<row>;
    using type = T[row];
    union {
      struct {
        T r, g, b;
      };
      struct {
        T x, y, z;
      };
      struct {
        T u, v, w;
      };
      type value;
    };
    constexpr T &operator[](size_t index) noexcept { return value[index]; }
    constexpr T const &operator[](size_t index) const noexcept {
      return value[index];
    }
    constexpr operator type const &() const noexcept { return value; }
    constexpr operator type &() noexcept { return value; }
  };
  template <typename T = float> struct vec4 {
    static constexpr size_t row = 4u;
    static constexpr size_t col = 0u;
    using extent = extents<row>;
    using type = T[row];
    union {
      struct {
        T r, g, b, a;
      };
      struct {
        T x, y, z, w;
      };
      type value;
    };
    constexpr T &operator[](size_t index) noexcept { return value[index]; }
    constexpr T const &operator[](size_t index) const noexcept {
      return value[index];
    }
    constexpr operator type const &() const noexcept { return value; }
    constexpr operator type &() noexcept { return value; }
  };

  using vec1f = vec1<float>;
  using float1 = vec1f;
  using f32x1 = vec1f;
  using vec1f32 = vec1f;

  using vec1d = vec1<double>;
  using double1 = vec1d;
  using f64x1 = vec1d;
  using vec1f64 = vec1d;

  using vec1u = vec1<uint32_t>;
  using vec1u32 = vec1u;
  using u32x1 = vec1u;
  using vec1u16 = vec1<uint16_t>;
  using u16x1 = vec1u16;
  using vec1u8 = vec1<uint8_t>;
  using u8x1 = vec1u8;

  using vec1i = vec1<::std::int32_t>;
  using vec1i32 = vec1i;
  using i32x1 = vec1i;
  using vec1i16 = vec1<::std::int16_t>;
  using vec1i8 = vec1<::std::int8_t>;
  using vec1b = vec1<bool>;

  using vec2f = vec2<float>;
  using float2 = vec2f;
  using f32x2 = vec2f;
  using vec2f32 = vec2f;

  using vec2d = vec2<double>;
  using double2 = vec2d;
  using f64x2 = vec2d;
  using vec2f64 = vec2d;

  using vec2u = vec2<uint32_t>;
  using vec2u32 = vec2u;
  using u32x2 = vec2u;
  using vec2u16 = vec2<uint16_t>;
  using u16x2 = vec2u16;
  using vec2u8 = vec2<uint8_t>;
  using u8x2 = vec2u8;

  using vec2i = vec2<::std::int32_t>;
  using vec2i32 = vec2i;
  using i32x2 = vec2i;
  using vec2i16 = vec2<::std::int16_t>;
  using vec2i8 = vec2<::std::int8_t>;
  using vec2b = vec2<bool>;

  using vec3f = vec3<float>;
  using float3 = vec3f;
  using f32x3 = vec3f;
  using vec3f32 = vec3f;

  using vec3d = vec3<double>;
  using double3 = vec3d;
  using f64x3 = vec3d;
  using vec3f64 = vec3d;

  using vec3u = vec3<uint32_t>;
  using vec3u32 = vec3u;
  using u32x3 = vec3u;
  using vec3u16 = vec3<uint16_t>;
  using u16x3 = vec3u16;
  using vec3u8 = vec3<uint8_t>;
  using u8x3 = vec3u8;

  using vec3i = vec3<::std::int32_t>;
  using vec3i32 = vec3i;
  using i32x3 = vec3i;
  using vec3i64 = vec3<::std::int64_t>;
  using i64x3 = vec3i;
  using vec3i16 = vec3<::std::int16_t>;
  using vec3i8 = vec3<::std::int8_t>;
  using vec3b = vec3<bool>;

  using vec4f = vec4<float>;
  using float4 = vec4f;
  using f32x4 = vec4f;
  using vec4f32 = vec4f;

  using vec4d = vec4<double>;
  using double4 = vec4d;
  using f64x4 = vec4d;
  using vec4f64 = vec4d;

  using vec4u = vec4<uint32_t>;
  using vec4u32 = vec4u;
  using u32x4 = vec4u;
  using vec4u16 = vec4<uint16_t>;
  using u16x4 = vec4u16;
  using vec4u8 = vec4<uint8_t>;
  using u8x4 = vec4u8;

  using vec4i = vec4<::std::int32_t>;
  using vec4i32 = vec4i;
  using i32x4 = vec4i;
  using vec4i16 = vec4<::std::int16_t>;
  using vec4i8 = vec4<::std::int8_t>;
  using vec4b = vec4<bool>;

  template <typename T = float> struct mat1x1 {
    static constexpr size_t row = 1u;
    static constexpr size_t col = 1u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      T m11;
      vec1<T> v1;
      vec1<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat1x2 {
    static constexpr size_t row = 1u;
    static constexpr size_t col = 2u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m12;
      };
      struct {
        vec1<T> v1, v2;
      };
      vec1<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat1x3 {
    static constexpr size_t row = 1u;
    static constexpr size_t col = 3u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m12, m13;
      };
      struct {
        vec1<T> v1, v2, v3;
      };
      vec1<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat1x4 {
    static constexpr size_t row = 1u;
    static constexpr size_t col = 4u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m12, m13, m14;
      };
      struct {
        vec1<T> v1, v2, v3, v4;
      };
      vec1<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat2x1 {
    static constexpr size_t row = 2u;
    static constexpr size_t col = 1u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21;
      };
      vec2<T> v1;
      vec2<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat2x2 {
    static constexpr size_t row = 2u;
    static constexpr size_t col = 2u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m12, m22;
      };
      struct {
        vec2<T> v1, v2;
      };
      vec2<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat2x3 {
    static constexpr size_t row = 2u;
    static constexpr size_t col = 3u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m12, m22, m13, m23;
      };
      struct {
        vec2<T> v1, v2, v3;
      };
      vec2<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat2x4 {
    static constexpr size_t row = 2u;
    static constexpr size_t col = 4u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m12, m22, m13, m23, m14, m24;
      };
      struct {
        vec2<T> v1, v2, v3, v4;
      };
      vec2<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat3x1 {
    static constexpr size_t row = 3u;
    static constexpr size_t col = 1u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31;
      };
      vec3<T> v1;
      vec3<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat3x2 {
    static constexpr size_t row = 3u;
    static constexpr size_t col = 2u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m12, m22, m32;
      };
      struct {
        vec3<T> v1, v2;
      };
      vec3<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat3x3 {
    static constexpr size_t row = 3u;
    static constexpr size_t col = 3u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m12, m22, m32, m13, m23, m33;
      };
      struct {
        vec3<T> v1, v2, v3;
      };
      vec3<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat3x4 {
    static constexpr size_t row = 3u;
    static constexpr size_t col = 4u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m12, m22, m32, m13, m23, m33, m14, m24, m34;
      };
      struct {
        vec3<T> v1, v2, v3, v4;
      };
      vec3<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat4x1 {
    static constexpr size_t row = 4u;
    static constexpr size_t col = 1u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m41;
      };
      vec4<T> v1;
      vec4<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat4x2 {
    static constexpr size_t row = 4u;
    static constexpr size_t col = 2u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m41, m12, m22, m32, m42;
      };
      struct {
        vec4<T> v1, v2;
      };
      vec4<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T = float> struct mat4x3 {
    static constexpr size_t row = 4u;
    static constexpr size_t col = 3u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m41, m12, m22, m32, m42, m13, m23, m33, m43;
      };
      struct {
        vec4<T> v1, v2, v3;
      };
      vec4<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  // using mat1x1f = mat1x1<float>;
  // using mat1x1f32 = mat1x1f;
  // using float1x1 = mat1x1f;

  template <typename T = float> struct mat4x4 {
    static constexpr size_t row = 4u;
    static constexpr size_t col = 4u;

    using extent = extents<row, col>;

    using vec = T[col];
    using vec_T = T[row];
    using type = T[row][col];
    using transpose = T[col][row];

    union {
      struct {
        T m11, m21, m31, m41, m12, m22, m32, m42, m13, m23, m33, m43, m14, m24,
            m34, m44;
      };
      struct {
        vec4<T> v1, v2, v3, v4;
      };
      vec4<T> v[col];
      type value;
    };

    constexpr auto &operator()(size_t x, size_t y) const noexcept {
      return value[x][y];
    }
    constexpr auto &operator()(size_t x, size_t y) noexcept {
      return value[x][y];
    }
  };

  template <typename T> struct view {
    template <typename M>
    constexpr view(M &mat) noexcept
      requires(requires { typename ::std::remove_cvref_t<M>::extents; })
        : value{mat.value}, stride_x{::std::remove_cvref_t<M>::row},
          stride_y{::std::remove_cvref_t<M>::col} {}

    template <typename M>
    constexpr view(M &mat, uint32_t offset_x) noexcept
      requires(requires { typename ::std::remove_cvref_t<M>::extents; })
        : view(mat), offset_x{offset_x} {}

    template <typename M>
    constexpr view(M &mat, uint32_t offset_x, uint32_t offset_y) noexcept
      requires(requires { typename ::std::remove_cvref_t<M>::extents; })
        : view(mat), offset_x{offset_x}, offset_y{offset_y} {}

  private:
    T *value = nullptr;
    uint32_t offset_x = 0;
    uint32_t offset_y = 0;
    uint32_t stride_x = 0;
    uint32_t stride_y = 0;
  };

  template <typename T> using vec_view = view<T>;
  template <typename T> using mat_view = view<T>;
}
