// Payload for constexpr optionals.
template <typename _Tp,
          bool /*_HasTrivialDestructor*/ =
          is_trivially_destructible<_Tp>::value>
struct _Optional_payload {
    constexpr _Optional_payload()
        : _M_empty(), _M_engaged(false) {}

    template<typename... _Args>
    constexpr _Optional_payload(in_place_t, _Args&&... __args)
        : _M_payload(std::forward<_Args>(__args)...),
          _M_engaged(true)
    {}

    template<typename _Up, typename... _Args>
    constexpr _Optional_payload(std::initializer_list<_Up> __il,
                                _Args&&... __args)
        : _M_payload(__il, std::forward<_Args>(__args)...),
          _M_engaged(true) {}

    template <class _Up> struct __ctor_tag {};

    constexpr _Optional_payload(__ctor_tag<bool>,
                                const _Tp& __other)
        : _M_payload(__other),
          _M_engaged(true)
    {}

    constexpr _Optional_payload(__ctor_tag<void>)
        : _M_empty(), _M_engaged(false)
    {}

    constexpr _Optional_payload(__ctor_tag<bool>, _Tp&& __other)
        : _M_payload(std::move(__other)),
          _M_engaged(true)
    {}

    constexpr _Optional_payload(bool __engaged,
                                const _Optional_payload& __other)
        : _Optional_payload(__engaged ?
                            _Optional_payload(__ctor_tag<bool> {},
    __other._M_payload) :
    _Optional_payload(__ctor_tag<void> {}))
    {}

    constexpr _Optional_payload(bool __engaged,
                                _Optional_payload&& __other)
        : _Optional_payload(__engaged
                            ? _Optional_payload(__ctor_tag<bool> {},
    std::move(__other._M_payload))
    : _Optional_payload(__ctor_tag<void> {}))
    {}

    using _Stored_type = remove_const_t<_Tp>;
    struct _Empty_byte { };
    union {
        _Empty_byte _M_empty;
        _Stored_type _M_payload;
    };
    bool _M_engaged;
};

// Payload for optionals with non-trivial destructor.
template <typename _Tp>
struct _Optional_payload<_Tp, false> {
    constexpr _Optional_payload()
        : _M_empty() {}

    template <typename... _Args>
    constexpr _Optional_payload(in_place_t, _Args&&... __args)
        : _M_payload(std::forward<_Args>(__args)...),
          _M_engaged(true) {}

    template<typename _Up, typename... _Args>
    constexpr _Optional_payload(std::initializer_list<_Up> __il,
                                _Args&&... __args)
        : _M_payload(__il, std::forward<_Args>(__args)...),
          _M_engaged(true) {}
    constexpr
    _Optional_payload(bool __engaged, const _Optional_payload& __other)
        : _Optional_payload(__other)
    {}

    constexpr
    _Optional_payload(bool __engaged, _Optional_payload&& __other)
        : _Optional_payload(std::move(__other))
    {}

    constexpr _Optional_payload(const _Optional_payload& __other)
    {
        if (__other._M_engaged) {
            this->_M_construct(__other._M_payload);
        }
    }

    constexpr _Optional_payload(_Optional_payload&& __other)
    {
        if (__other._M_engaged) {
            this->_M_construct(std::move(__other._M_payload));
        }
    }

    _Optional_payload&
    operator=(const _Optional_payload& __other)
    {
        if (this->_M_engaged && __other._M_engaged) {
            this->_M_get() = __other._M_get();
        } else {
            if (__other._M_engaged) {
                this->_M_construct(__other._M_get());
            } else {
                this->_M_reset();
            }
        }
        return *this;
    }

    _Optional_payload&
    operator=(_Optional_payload&& __other)
    noexcept(__and_<is_nothrow_move_constructible<_Tp>,
             is_nothrow_move_assignable<_Tp>>())
    {
        if (this->_M_engaged && __other._M_engaged) {
            this->_M_get() = std::move(__other._M_get());
        } else {
            if (__other._M_engaged) {
                this->_M_construct(std::move(__other._M_get()));
            } else {
                this->_M_reset();
            }
        }
        return *this;
    }

    using _Stored_type = remove_const_t<_Tp>;
    struct _Empty_byte { };
    union {
        _Empty_byte _M_empty;
        _Stored_type _M_payload;
    };
    bool _M_engaged = false;

    ~_Optional_payload()
    {
        if (_M_engaged) {
            _M_payload.~_Stored_type();
        }
    }

    template<typename... _Args>
    void
    _M_construct(_Args&&... __args)
    noexcept(is_nothrow_constructible<_Stored_type, _Args...>())
    {
        ::new ((void *) std::__addressof(this->_M_payload))
        _Stored_type(std::forward<_Args>(__args)...);
        this->_M_engaged = true;
    }

    // The _M_get operations have _M_engaged as a precondition.
    constexpr _Tp&
    _M_get() noexcept
    {
        return this->_M_payload;
    }

    constexpr const _Tp&
    _M_get() const noexcept
    {
        return this->_M_payload;
    }

    // _M_reset is a 'safe' operation with no precondition.
    void
    _M_reset() noexcept
    {
        if (this->_M_engaged) {
            this->_M_engaged = false;
            this->_M_payload.~_Stored_type();
        }
    }
};

template<typename _Tp, typename _Dp>
class _Optional_base_impl
{
protected:
    using _Stored_type = remove_const_t<_Tp>;

    // The _M_construct operation has !_M_engaged as a precondition
    // while _M_destruct has _M_engaged as a precondition.
    template<typename... _Args>
    void
    _M_construct(_Args&&... __args)
    noexcept(is_nothrow_constructible<_Stored_type, _Args...>())
    {
        ::new
        (std::__addressof(static_cast<_Dp*>(this)->_M_payload._M_payload))
        _Stored_type(std::forward<_Args>(__args)...);
        static_cast<_Dp*>(this)->_M_payload._M_engaged = true;
    }

    void
    _M_destruct() noexcept
    {
        static_cast<_Dp*>(this)->_M_payload._M_engaged = false;
        static_cast<_Dp*>(this)->_M_payload._M_payload.~_Stored_type();
    }

    // _M_reset is a 'safe' operation with no precondition.
    void
    _M_reset() noexcept
    {
        if (static_cast<_Dp*>(this)->_M_payload._M_engaged) {
            static_cast<_Dp*>(this)->_M_destruct();
        }
    }
};

/**
  * @brief Class template that takes care of copy/move constructors
  of optional
  *
  * Such a separate base class template is necessary in order to
  * conditionally make copy/move constructors trivial.
  * @see optional, _Enable_special_members
  */
template<typename _Tp,
         bool = is_trivially_copy_constructible_v<_Tp>,
         bool = is_trivially_move_constructible_v<_Tp>>
class _Optional_base
// protected inheritance because optional needs to reach that base too
    : protected _Optional_base_impl<_Tp, _Optional_base<_Tp>>
{
    friend class _Optional_base_impl<_Tp, _Optional_base<_Tp>>;
public:

    // Constructors for disengaged optionals.
    constexpr _Optional_base() = default;

    // Constructors for engaged optionals.
    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t, _Args&&... __args)
        : _M_payload(in_place,
                     std::forward<_Args>(__args)...) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t,
                                      initializer_list<_Up> __il,
                                      _Args&&... __args)
        : _M_payload(in_place,
                     __il, std::forward<_Args>(__args)...)
    { }

    // Copy and move constructors.
    constexpr _Optional_base(const _Optional_base& __other)
        : _M_payload(__other._M_payload._M_engaged,
                     __other._M_payload)
    { }

    constexpr _Optional_base(_Optional_base&& __other)
    noexcept(is_nothrow_move_constructible<_Tp>())
        : _M_payload(__other._M_payload._M_engaged,
                     std::move(__other._M_payload))
    { }

    // Assignment operators.
    _Optional_base& operator=(const _Optional_base&) = default;
    _Optional_base& operator=(_Optional_base&&) = default;

protected:

    constexpr bool _M_is_engaged() const noexcept
    {
        return this->_M_payload._M_engaged;
    }

    // The _M_get operations have _M_engaged as a precondition.
    constexpr _Tp&
    _M_get() noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

    constexpr const _Tp&
    _M_get() const noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

private:
    _Optional_payload<_Tp> _M_payload;
};

template<typename _Tp>
class _Optional_base<_Tp, false, true>
    : protected _Optional_base_impl<_Tp, _Optional_base<_Tp>>
{
    friend class _Optional_base_impl<_Tp, _Optional_base<_Tp>>;
public:

    // Constructors for disengaged optionals.
    constexpr _Optional_base() = default;

    // Constructors for engaged optionals.
    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t, _Args&&... __args)
        : _M_payload(in_place,
                     std::forward<_Args>(__args)...) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t,
                                      initializer_list<_Up> __il,
                                      _Args&&... __args)
        : _M_payload(in_place,
                     __il, std::forward<_Args>(__args)...)
    { }

    // Copy and move constructors.
    constexpr _Optional_base(const _Optional_base& __other)
        : _M_payload(__other._M_payload._M_engaged,
                     __other._M_payload)
    { }

    constexpr _Optional_base(_Optional_base&& __other) = default;

    // Assignment operators.
    _Optional_base& operator=(const _Optional_base&) = default;
    _Optional_base& operator=(_Optional_base&&) = default;

protected:

    constexpr bool _M_is_engaged() const noexcept
    {
        return this->_M_payload._M_engaged;
    }

    // The _M_get operations have _M_engaged as a precondition.
    constexpr _Tp&
    _M_get() noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

    constexpr const _Tp&
    _M_get() const noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

private:
    _Optional_payload<_Tp> _M_payload;
};

template<typename _Tp>
class _Optional_base<_Tp, true, false>
    : protected _Optional_base_impl<_Tp, _Optional_base<_Tp>>
{
    friend class _Optional_base_impl<_Tp, _Optional_base<_Tp>>;
public:

    // Constructors for disengaged optionals.
    constexpr _Optional_base() = default;

    // Constructors for engaged optionals.
    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t, _Args&&... __args)
        : _M_payload(in_place,
                     std::forward<_Args>(__args)...) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t,
                                      initializer_list<_Up> __il,
                                      _Args&&... __args)
        : _M_payload(in_place,
                     __il, std::forward<_Args>(__args)...)
    { }

    // Copy and move constructors.
    constexpr _Optional_base(const _Optional_base& __other) = default;

    constexpr _Optional_base(_Optional_base&& __other)
    noexcept(is_nothrow_move_constructible<_Tp>())
        : _M_payload(__other._M_payload._M_engaged,
                     std::move(__other._M_payload))
    { }

    // Assignment operators.
    _Optional_base& operator=(const _Optional_base&) = default;
    _Optional_base& operator=(_Optional_base&&) = default;

protected:

    constexpr bool _M_is_engaged() const noexcept
    {
        return this->_M_payload._M_engaged;
    }

    // The _M_get operations have _M_engaged as a precondition.
    constexpr _Tp&
    _M_get() noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

    constexpr const _Tp&
    _M_get() const noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

private:
    _Optional_payload<_Tp> _M_payload;
};

template<typename _Tp>
class _Optional_base<_Tp, true, true>
    : protected _Optional_base_impl<_Tp, _Optional_base<_Tp>>
{
    friend class _Optional_base_impl<_Tp, _Optional_base<_Tp>>;
public:

    // Constructors for disengaged optionals.
    constexpr _Optional_base() = default;

    // Constructors for engaged optionals.
    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t, _Args&&... __args)
        : _M_payload(in_place,
                     std::forward<_Args>(__args)...) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    constexpr explicit _Optional_base(in_place_t,
                                      initializer_list<_Up> __il,
                                      _Args&&... __args)
        : _M_payload(in_place,
                     __il, std::forward<_Args>(__args)...)
    { }

    // Copy and move constructors.
    constexpr _Optional_base(const _Optional_base& __other) = default;
    constexpr _Optional_base(_Optional_base&& __other) = default;

    // Assignment operators.
    _Optional_base& operator=(const _Optional_base&) = default;
    _Optional_base& operator=(_Optional_base&&) = default;

protected:

    constexpr bool _M_is_engaged() const noexcept
    {
        return this->_M_payload._M_engaged;
    }

    // The _M_get operations have _M_engaged as a precondition.
    constexpr _Tp&
    _M_get() noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

    constexpr const _Tp&
    _M_get() const noexcept
    {
        __glibcxx_assert(this->_M_is_engaged());
        return this->_M_payload._M_payload;
    }

private:
    _Optional_payload<_Tp> _M_payload;
};

template<typename _Tp>
class optional;

template<typename _Tp, typename _Up>
using __converts_from_optional =
    __or_<is_constructible<_Tp, const optional<_Up>&>,
    is_constructible<_Tp, optional<_Up>&>,
    is_constructible<_Tp, const optional<_Up>&&>,
    is_constructible<_Tp, optional<_Up>&&>,
    is_convertible<const optional<_Up>&, _Tp>,
    is_convertible<optional<_Up>&, _Tp>,
    is_convertible<const optional<_Up>&&, _Tp>,
    is_convertible<optional<_Up>&&, _Tp>>;

template<typename _Tp, typename _Up>
using __assigns_from_optional =
    __or_<is_assignable<_Tp&, const optional<_Up>&>,
    is_assignable<_Tp&, optional<_Up>&>,
    is_assignable<_Tp&, const optional<_Up>&&>,
    is_assignable<_Tp&, optional<_Up>&&>>;

/**
  * @brief Class template for optional values.
  */
template<typename _Tp>
class optional
    : private _Optional_base<_Tp>
{
    static_assert(!is_same_v<remove_cv_t<_Tp>, nullopt_t>);
    static_assert(!is_same_v<remove_cv_t<_Tp>, in_place_t>);
    static_assert(!is_reference_v<_Tp>);

private:
    using _Base = _Optional_base<_Tp>;

public:
    using value_type = _Tp;

    constexpr optional() = default;

    constexpr optional(nullopt_t) noexcept { }

    constexpr optional(optional const&)
    requires is_copy_constructible_v<_Tp> = default;
    constexpr optional(optional&&)
    requires is_move_constructible_v<_Tp> = default;
    constexpr optional& operator=(optional const&)
    requires is_copy_constructible_v<_Tp> && is_copy_assignable_v<_Tp> = default;
    constexpr optional& operator=(optional&&)
    requires is_move_constructible_v<_Tp> && is_move_assignable_v<_Tp> = default;

    // Converting constructors for engaged optionals.
    template <typename _Up = _Tp,
              enable_if_t<__and_<
                              __not_<is_same<optional<_Tp>, decay_t<_Up>>>,
                                      __not_<is_same<in_place_t, decay_t<_Up>>>,
                                              is_constructible<_Tp, _Up&&>,
                                              is_convertible<_Up&&, _Tp>
                                              >::value, bool> = true>
                                     constexpr optional(_Up&& __t)
                                         : _Base(std::in_place, std::forward<_Up>(__t)) { }

    template <typename _Up = _Tp,
              enable_if_t<__and_<
                              __not_<is_same<optional<_Tp>, decay_t<_Up>>>,
                                      __not_<is_same<in_place_t, decay_t<_Up>>>,
                                              is_constructible<_Tp, _Up&&>,
                                              __not_<is_convertible<_Up&&, _Tp>>
                                              >::value, bool> = false>
                                     explicit constexpr optional(_Up&& __t)
                                         : _Base(std::in_place, std::forward<_Up>(__t)) { }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, const _Up&>,
                              is_convertible<const _Up&, _Tp>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = true>
    constexpr optional(const optional<_Up>& __t)
    {
        if (__t) {
            emplace(*__t);
        }
    }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, const _Up&>,
                              __not_<is_convertible<const _Up&, _Tp>>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = false>
    explicit constexpr optional(const optional<_Up>& __t)
    {
        if (__t) {
            emplace(*__t);
        }
    }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, _Up&&>,
                              is_convertible<_Up&&, _Tp>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = true>
    constexpr optional(optional<_Up>&& __t)
    {
        if (__t) {
            emplace(std::move(*__t));
        }
    }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, _Up&&>,
                              __not_<is_convertible<_Up&&, _Tp>>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = false>
    explicit constexpr optional(optional<_Up>&& __t)
    {
        if (__t) {
            emplace(std::move(*__t));
        }
    }

    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    explicit constexpr optional(in_place_t, _Args&&... __args)
        : _Base(std::in_place, std::forward<_Args>(__args)...) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    explicit constexpr optional(in_place_t,
                                initializer_list<_Up> __il,
                                _Args&&... __args)
        : _Base(std::in_place, __il, std::forward<_Args>(__args)...) { }

    // Assignment operators.
    optional&
    operator=(nullopt_t) noexcept
    {
        this->_M_reset();
        return *this;
    }

    template<typename _Up = _Tp>
    enable_if_t<__and_<
                    __not_<is_same<optional<_Tp>, decay_t<_Up>>>,
                                   is_constructible<_Tp, _Up>,
                                   __not_<__and_<is_scalar<_Tp>,
                                           is_same<_Tp, decay_t<_Up>>>>,
                                           is_assignable<_Tp&, _Up>>::value,
                                          optional&>
                                   operator=(_Up&& __u)
    {
        if (this->_M_is_engaged()) {
            this->_M_get() = std::forward<_Up>(__u);
        } else {
            this->_M_construct(std::forward<_Up>(__u));
        }

        return *this;
    }

    template<typename _Up>
    enable_if_t<__and_<
                    __not_<is_same<_Tp, _Up>>,
                    is_constructible<_Tp, const _Up&>,
                    is_assignable<_Tp&, _Up>,
                    __not_<__converts_from_optional<_Tp, _Up>>,
                    __not_<__assigns_from_optional<_Tp, _Up>>
                    >::value,
                optional&>
    operator=(const optional<_Up>& __u)
    {
        if (__u) {
            if (this->_M_is_engaged()) {
                this->_M_get() = *__u;
            } else {
                this->_M_construct(*__u);
            }
        } else {
            this->_M_reset();
        }
        return *this;
    }

    template<typename _Up>
    enable_if_t<__and_<
                    __not_<is_same<_Tp, _Up>>,
                    is_constructible<_Tp, _Up>,
                    is_assignable<_Tp&, _Up>,
                    __not_<__converts_from_optional<_Tp, _Up>>,
                    __not_<__assigns_from_optional<_Tp, _Up>>
                    >::value,
                optional&>
    operator=(optional<_Up>&& __u)
    {
        if (__u) {
            if (this->_M_is_engaged()) {
                this->_M_get() = std::move(*__u);
            } else {
                this->_M_construct(std::move(*__u));
            }
        } else {
            this->_M_reset();
        }

        return *this;
    }

    template<typename... _Args>
    enable_if_t<is_constructible<_Tp, _Args&&...>::value, _Tp&>
    emplace(_Args&&... __args)
    {
        this->_M_reset();
        this->_M_construct(std::forward<_Args>(__args)...);
        return this->_M_get();
    }

    template<typename _Up, typename... _Args>
    enable_if_t<is_constructible<_Tp, initializer_list<_Up>&,
                                 _Args&&...>::value, _Tp&>
    emplace(initializer_list<_Up> __il, _Args&&... __args)
    {
        this->_M_reset();
        this->_M_construct(__il, std::forward<_Args>(__args)...);
        return this->_M_get();
    }

    // Destructor is implicit, implemented in _Optional_base.

    // Swap.
    void
    swap(optional& __other)
    noexcept(is_nothrow_move_constructible<_Tp>()
             && is_nothrow_swappable_v<_Tp>)
    {
        using std::swap;

        if (this->_M_is_engaged() && __other._M_is_engaged()) {
            swap(this->_M_get(), __other._M_get());
        } else if (this->_M_is_engaged()) {
            __other._M_construct(std::move(this->_M_get()));
            this->_M_destruct();
        } else if (__other._M_is_engaged()) {
            this->_M_construct(std::move(__other._M_get()));
            __other._M_destruct();
        }
    }

    // Observers.
    constexpr const _Tp*
    operator->() const
    {
        return std::__addressof(this->_M_get());
    }

    _Tp*
    operator->()
    {
        return std::__addressof(this->_M_get());
    }

    constexpr const _Tp&
    operator*() const&
    {
        return this->_M_get();
    }

    constexpr _Tp&
    operator*()&
    { return this->_M_get(); }

    constexpr _Tp&&
    operator*()&&
    { return std::move(this->_M_get()); }

    constexpr const _Tp&&
    operator*() const&&
    {
        return std::move(this->_M_get());
    }

    constexpr explicit operator bool() const noexcept
    {
        return this->_M_is_engaged();
    }

    constexpr bool has_value() const noexcept
    {
        return this->_M_is_engaged();
    }

    constexpr const _Tp&
    value() const&
    {
        return this->_M_is_engaged()
               ?  this->_M_get()
               : (__throw_bad_optional_access(),
                  this->_M_get());
    }

    constexpr _Tp&
    value()& {
        return this->_M_is_engaged()
        ?  this->_M_get()
        : (__throw_bad_optional_access(),
        this->_M_get());
    }

    constexpr _Tp&&
    value()&& {
        return this->_M_is_engaged()
        ?  std::move(this->_M_get())
        : (__throw_bad_optional_access(),
        std::move(this->_M_get()));
    }

    constexpr const _Tp&&
    value() const&&
    {
        return this->_M_is_engaged()
               ?  std::move(this->_M_get())
               : (__throw_bad_optional_access(),
                  std::move(this->_M_get()));
    }

    template<typename _Up>
    constexpr _Tp
    value_or(_Up&& __u) const&
    {
        static_assert(is_copy_constructible_v<_Tp>);
        static_assert(is_convertible_v<_Up&&, _Tp>);

        return this->_M_is_engaged()
               ? this->_M_get()
               : static_cast<_Tp>(std::forward<_Up>(__u));
    }

    template<typename _Up>
    _Tp
    value_or(_Up&& __u) && {
        static_assert(is_move_constructible_v<_Tp>);
        static_assert(is_convertible_v<_Up&&, _Tp>);

        return this->_M_is_engaged()
        ? std::move(this->_M_get())
        : static_cast<_Tp>(std::forward<_Up>(__u));
    }
    void reset() noexcept
    {
        this->_M_reset();
    }
};

template<typename _Tp>
using __optional_relop_t =
enable_if_t<is_convertible<_Tp, bool>::value, bool>;

// Comparisons between optional values.
template<typename _Tp, typename _Up>
constexpr auto
operator==(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() == declval<_Up>())> {
    return static_cast<bool>(__lhs) == static_cast<bool>(__rhs)
    && (!__lhs || *__lhs == *__rhs);
}

template<typename _Tp, typename _Up>
constexpr auto
operator!=(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() != declval<_Up>())> {
    return static_cast<bool>(__lhs) != static_cast<bool>(__rhs)
    || (static_cast<bool>(__lhs) && *__lhs != *__rhs);
}

template<typename _Tp, typename _Up>
constexpr auto
operator<(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() < declval<_Up>())> {
    return static_cast<bool>(__rhs) && (!__lhs || *__lhs < *__rhs);
}

template<typename _Tp, typename _Up>
constexpr auto
operator>(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() > declval<_Up>())> {
    return static_cast<bool>(__lhs) && (!__rhs || *__lhs > *__rhs);
}

template<typename _Tp, typename _Up>
constexpr auto
operator<=(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() <= declval<_Up>())> {
    return !__lhs || (static_cast<bool>(__rhs) && *__lhs <= *__rhs);
}

template<typename _Tp, typename _Up>
constexpr auto
operator>=(const optional<_Tp>& __lhs, const optional<_Up>& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() >= declval<_Up>())> {
    return !__rhs || (static_cast<bool>(__lhs) && *__lhs >= *__rhs);
}

// Comparisons with nullopt.
template<typename _Tp>
constexpr bool
operator==(const optional<_Tp>& __lhs, nullopt_t) noexcept
{
    return !__lhs;
}

template<typename _Tp>
constexpr bool
operator==(nullopt_t, const optional<_Tp>& __rhs) noexcept
{
    return !__rhs;
}

template<typename _Tp>
constexpr bool
operator!=(const optional<_Tp>& __lhs, nullopt_t) noexcept
{
    return static_cast<bool>(__lhs);
}

template<typename _Tp>
constexpr bool
operator!=(nullopt_t, const optional<_Tp>& __rhs) noexcept
{
    return static_cast<bool>(__rhs);
}

template<typename _Tp>
constexpr bool
operator<(const optional<_Tp>& /* __lhs */, nullopt_t) noexcept
{
    return false;
}

template<typename _Tp>
constexpr bool
operator<(nullopt_t, const optional<_Tp>& __rhs) noexcept
{
    return static_cast<bool>(__rhs);
}

template<typename _Tp>
constexpr bool
operator>(const optional<_Tp>& __lhs, nullopt_t) noexcept
{
    return static_cast<bool>(__lhs);
}

template<typename _Tp>
constexpr bool
operator>(nullopt_t, const optional<_Tp>& /* __rhs */) noexcept
{
    return false;
}

template<typename _Tp>
constexpr bool
operator<=(const optional<_Tp>& __lhs, nullopt_t) noexcept
{
    return !__lhs;
}

template<typename _Tp>
constexpr bool
operator<=(nullopt_t, const optional<_Tp>& /* __rhs */) noexcept
{
    return true;
}

template<typename _Tp>
constexpr bool
operator>=(const optional<_Tp>& /* __lhs */, nullopt_t) noexcept
{
    return true;
}

template<typename _Tp>
constexpr bool
operator>=(nullopt_t, const optional<_Tp>& __rhs) noexcept
{
    return !__rhs;
}

// Comparisons with value type.
template<typename _Tp, typename _Up>
constexpr auto
operator==(const optional<_Tp>& __lhs, const _Up& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() == declval<_Up>())>
{ return __lhs && *__lhs == __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator==(const _Up& __lhs, const optional<_Tp>& __rhs)
-> __optional_relop_t<decltype(declval<_Up>() == declval<_Tp>())>
{ return __rhs && __lhs == *__rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator!=(const optional<_Tp>& __lhs, const _Up& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() != declval<_Up>())>
{ return !__lhs || *__lhs != __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator!=(const _Up& __lhs, const optional<_Tp>& __rhs)
-> __optional_relop_t<decltype(declval<_Up>() != declval<_Tp>())>
{ return !__rhs || __lhs != *__rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator<(const optional<_Tp>& __lhs, const _Up& __rhs)
         -> __optional_relop_t<decltype(declval<_Tp>() < declval<_Up>())>
{ return !__lhs || *__lhs < __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator<(const _Up& __lhs, const optional<_Tp>& __rhs)
         -> __optional_relop_t<decltype(declval<_Up>() < declval<_Tp>())>
{ return __rhs && __lhs < *__rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator>(const optional<_Tp>& __lhs, const _Up& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() > declval<_Up>())>
{ return __lhs && *__lhs > __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator>(const _Up& __lhs, const optional<_Tp>& __rhs)
-> __optional_relop_t<decltype(declval<_Up>() > declval<_Tp>())>
{ return !__rhs || __lhs > *__rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator<=(const optional<_Tp>& __lhs, const _Up& __rhs)
         -> __optional_relop_t<decltype(declval<_Tp>() <= declval<_Up>())>
{ return !__lhs || *__lhs <= __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator<=(const _Up& __lhs, const optional<_Tp>& __rhs)
         -> __optional_relop_t<decltype(declval<_Up>() <= declval<_Tp>())>
{ return __rhs && __lhs <= *__rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator>=(const optional<_Tp>& __lhs, const _Up& __rhs)
-> __optional_relop_t<decltype(declval<_Tp>() >= declval<_Up>())>
{ return __lhs && *__lhs >= __rhs; }

template<typename _Tp, typename _Up>
constexpr auto
operator>=(const _Up& __lhs, const optional<_Tp>& __rhs)
-> __optional_relop_t<decltype(declval<_Up>() >= declval<_Tp>())>
{ return !__rhs || __lhs >= *__rhs; }

// Swap and creation functions.

// _GLIBCXX_RESOLVE_LIB_DEFECTS
// 2748. swappable traits for optionals
template<typename _Tp>
inline enable_if_t<is_move_constructible_v<_Tp> && is_swappable_v<_Tp>>
swap(optional<_Tp>& __lhs, optional<_Tp>& __rhs)
noexcept(noexcept(__lhs.swap(__rhs)))
{
    __lhs.swap(__rhs);
}

template<typename _Tp>
enable_if_t<!(is_move_constructible_v<_Tp> && is_swappable_v<_Tp>)>
swap(optional<_Tp>&, optional<_Tp>&) = delete;

template<typename _Tp>
constexpr optional<decay_t<_Tp>>
make_optional(_Tp&& __t)
{
    return optional<decay_t<_Tp>> { std::forward<_Tp>(__t) };
}

template<typename _Tp, typename ..._Args>
constexpr optional<_Tp>
make_optional(_Args&&... __args)
{
    return optional<_Tp> { in_place, std::forward<_Args>(__args)... };
}

template<typename _Tp, typename _Up, typename ..._Args>
constexpr optional<_Tp>
make_optional(initializer_list<_Up> __il, _Args&&... __args)
{
    return optional<_Tp> { in_place, __il, std::forward<_Args>(__args)... };
}

// Hash.

template<typename _Tp, typename _Up = remove_const_t<_Tp>,
         bool = __poison_hash<_Up>::__enable_hash_call>
struct __optional_hash_call_base {
    size_t
    operator()(const optional<_Tp>& __t) const
    noexcept(noexcept(hash<_Up> {}(*__t)))
    {
        // We pick an arbitrary hash for disengaged optionals which hopefully
        // usual values of _Tp won't typically hash to.
        constexpr size_t __magic_disengaged_hash = static_cast<size_t>(-3333);
        return __t ? hash<_Up> {}(*__t) : __magic_disengaged_hash;
    }
};

template<typename _Tp, typename _Up>
struct __optional_hash_call_base<_Tp, _Up, false> {};

template<typename _Tp>
struct hash<optional<_Tp>>
: private __poison_hash<remove_const_t<_Tp>>,
public __optional_hash_call_base<_Tp> {
    using result_type [[__deprecated__]] = size_t;
    using argument_type [[__deprecated__]] = optional<_Tp>;
};

template<typename _Tp>
struct __is_fast_hash<hash<optional<_Tp>>> : __is_fast_hash<hash<_Tp>> {
};

/// @}

#if __cpp_deduction_guides >= 201606
template <typename _Tp> optional(_Tp) -> optional<_Tp>;
#endif