#include <iostream>
#include <memory>
//
template<typename T>
class SharedPtr {
private:

    struct ControlBlockBase {
        size_t shared_count;
        size_t weak_count;
        T* ptr;
        
        ControlBlockBase(size_t sc, size_t wc, T* ptr) : shared_count(sc), weak_count(wc), ptr(ptr) {}

        virtual void dispose() = 0;
        virtual void destroy() = 0;
        virtual ~ControlBlockBase() {}
    };

    template<typename Alloc=std::allocator<T>, typename Deleter=std::default_delete<T>>
    struct ControlBlockDirect : public ControlBlockBase {
        Deleter del;
        Alloc alloc;

        ControlBlockDirect(T *ptr, size_t sc, size_t wc) : ControlBlockBase(sc, wc, ptr), del(Deleter()), alloc(Alloc()) {}
        ControlBlockDirect(T* ptr, size_t sc, size_t wc, const Deleter& del) : ControlBlockBase(sc, wc, ptr), del(del), alloc(Alloc()) {}
        ControlBlockDirect(T* ptr, size_t sc, size_t wc, const Deleter& del, const Alloc &alloc) : ControlBlockBase(sc, wc, ptr), del(del), alloc(alloc) {}
         
        virtual void dispose() override {
            using CBD = ControlBlockDirect<Alloc, Deleter>;
            using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBD>;
            AllocRebind allc(alloc);
            std::allocator_traits<AllocRebind>::deallocate(allc, this, 1); 
        }

        virtual void destroy() override {
            del(this->ptr);
        }

        virtual ~ControlBlockDirect() {}
    };

    template<typename Alloc=std::allocator<T>>
    struct ControlBlockMakeShared : public ControlBlockBase {
        T object;
        Alloc alloc;

        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc) : ControlBlockBase(sc, wc, ptr), object(T()), alloc(Alloc()) {}
        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc, const Alloc& alloc) : ControlBlockBase(sc, wc, ptr), object(T()), alloc(alloc) {} 
        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc, const T& object) : ControlBlockBase(sc, wc, ptr), object(object), alloc(Alloc()) {} 
        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc, const T& object, const Alloc &alloc) : ControlBlockBase(sc, wc, ptr), object(object), alloc(alloc) {}
        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc, T&& object) : ControlBlockBase(sc, wc, ptr), object(std::move(object)), alloc(Alloc()) {}
        ControlBlockMakeShared(T *ptr, size_t sc, size_t wc, T&& object, const Alloc &alloc) : ControlBlockBase(sc, wc, ptr), object(std::move(object)), alloc(alloc) {}
 
        
        virtual void dispose() override {
            // this->~ControlBlockMakeShared();
            using CBMS = ControlBlockMakeShared<Alloc>;
            using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBMS>;
            AllocRebind allc(alloc);
            std::allocator_traits<AllocRebind>::deallocate(allc, this, 1); 
        }

        virtual void destroy() override {
            std::allocator_traits<Alloc>::destroy(alloc, this->ptr);
        }

        virtual ~ControlBlockMakeShared() override {} 
    };

    template<typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(const Alloc &alloc, Args&&... args);
    
    template<typename U>
    friend class WeakPtr; 

    template<typename U>
    friend class SharedPtr;

    ControlBlockBase *cb; 

    SharedPtr(SharedPtr<T>::ControlBlockBase *cb) : cb(cb) {
        ++cb->shared_count;
    } 

public: 

    SharedPtr() : cb(nullptr) {}
    
    SharedPtr(T* ptr) : cb(new ControlBlockDirect{ptr, 1, 0}) {}
    
    template<typename Deleter>
    SharedPtr(T* ptr, Deleter del) : cb(new ControlBlockDirect{ptr, 1, 0, del}) {}

    template<typename Deleter, typename Alloc>
    SharedPtr(T* ptr, Deleter del, Alloc alloc) {
        using CBD = ControlBlockDirect<Alloc, Deleter>;
        using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBD>;
        AllocRebind allc(alloc);
        CBD* cbd = std::allocator_traits<AllocRebind>::allocate(allc, 1); 
        ::new (cbd) CBD(ptr, 1, 0, del, alloc);
        cb = cbd;
    }

    SharedPtr(const SharedPtr& other) : cb(other.cb) {
        if (cb != nullptr) ++cb->shared_count;
    }

    SharedPtr(SharedPtr&& other) : cb(other.cb) {
        // std::cerr << "move constructor\n";
        other.cb = nullptr;
    }


    SharedPtr& operator=(const SharedPtr& other) {
        if(cb != nullptr) { 
            --cb->shared_count; 
            if (cb->shared_count == 0) {
                cb->destroy();
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = other.cb;
        ++cb->shared_count;
        return *this;
    }

    SharedPtr& operator=(SharedPtr &&other) {
        if(cb != nullptr) { 
            --cb->shared_count; 
            if (cb->shared_count == 0) {
                cb->destroy();
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = other.cb; 
        other.cb = nullptr;
        return *this;
    }

    template<typename Y>
    SharedPtr(Y* ptr) : cb(new SharedPtr<T>::ControlBlockDirect{static_cast<T*>(ptr), 1, 0}) {}
    
    template<typename Y, typename Deleter>
    SharedPtr(Y* ptr, Deleter del) : cb(new SharedPtr<T>::ControlBlockDirect{static_cast<T*>(ptr), 1, 0, del}) {}

    template<typename Y, typename Deleter, typename Alloc>
    SharedPtr(Y* ptr, Deleter del, Alloc alloc) {
        using CBD = ControlBlockDirect<Alloc, Deleter>;
        using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBD>;
        AllocRebind allc(alloc);
        CBD* cbd = std::allocator_traits<AllocRebind>::allocate(allc, 1); 
        ::new (cbd) CBD(static_cast<T*>(ptr), 1, 0, del, alloc);
        cb = cbd;
    }

    template<typename Y>
    SharedPtr(const SharedPtr<Y> &other) : cb(reinterpret_cast<SharedPtr<T>::ControlBlockBase*>(other.cb)) {
        ++cb->shared_count;
    }

    template<typename Y>
    SharedPtr(SharedPtr<Y> &&other) : cb(other.cb) {
        other.cb = nullptr;
    }

    template<typename Y>
    SharedPtr& operator=(const SharedPtr<Y> &other) {
        if(cb != nullptr) { 
            --cb->shared_count; 
            if (cb->shared_count == 0) {
                cb->destroy();
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = reinterpret_cast<SharedPtr<T>::ControlBlockBase*>(other.cb);
        ++cb->shared_count;
        return *this;
    }

    template<typename Y>
    SharedPtr& operator=(SharedPtr<Y> &&other) {
        if(cb != nullptr) { 
            --cb->shared_count; 
            if (cb->shared_count == 0) {
                cb->destroy();
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = reinterpret_cast<SharedPtr<T>::ControlBlockBase*>(other.cb); 
        other.cb = nullptr;
        return *this;
    }

    ~SharedPtr() {
        if (cb != nullptr) {
            --cb->shared_count;
            if (cb->shared_count == 0) {
                cb->destroy();
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        } 
    }

    T& operator*() {
        return *cb->ptr;
    }

    T* operator->() {
        return cb->ptr;
    }

    const T& operator*() const {
        return *cb->ptr;
    }

    const T* operator->() const {
        return cb->ptr;
    }

    T* get() {
        return cb == nullptr ? nullptr : cb->ptr;
    }

    const T* get() const {
        return cb->ptr;
    }

    size_t use_count() const {
        return cb->shared_count;
    }


    void reset() {
        if (cb != nullptr) {
            --cb->shared_count;
            if (cb->shared_count == 0) {
                cb->destroy(); 
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = nullptr;
    }

    template<typename Y>
    void reset(Y* ptr) {
        if (cb != nullptr) { 
            --cb->shared_count;
            if (cb->shared_count == 0) {
                cb->destroy(); 
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = new ControlBlockDirect{ptr, 1, 0}; 
    } 

    template<typename Y, typename Deleter>
    void reset(Y* ptr, Deleter del) {
        if (cb != nullptr) {
            --cb->shared_count;
            if (cb->shared_count == 0) {
                cb->destroy(); 
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        cb = new ControlBlockBase(ptr, 1, 0, del);
    }

    template<typename Y, typename Deleter, typename Alloc>
    void reset(Y* ptr, Deleter del, Alloc alloc) {
        if (cb != nullptr) {
            --cb->shared_count;
            if (cb->shared_count == 0) {
                cb->destroy(); 
                if (cb->weak_count == 0) {
                    cb->dispose();
                }
            }
        }
        using CBD = ControlBlockDirect<Alloc, Deleter>;
        using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBD>; 
        AllocRebind allc(alloc);
        CBD* cbd = std::allocator_traits<AllocRebind>::allocate(allc, 1); 
        std::allocator_traits<AllocRebind>::construct(allc, cbd, ptr, 1, 0, del, alloc);
        cb = cbd;
    }

    void swap(SharedPtr& sp) {
        ControlBlockBase *tmp = cb;
        cb = sp.cb;
        sp.cb = tmp;
    }
};

template<typename T, typename Alloc=std::allocator<T>, typename... Args>
SharedPtr<T> allocateShared(const Alloc &alloc, Args&&... args) {
    using CBMS = typename SharedPtr<T>::template ControlBlockMakeShared<Alloc>;
    using AllocRebind = typename std::allocator_traits<Alloc>::template rebind_alloc<CBMS>;
    AllocRebind allc(alloc);
    CBMS *cbms = std::allocator_traits<AllocRebind>::allocate(allc, 1); 
    std::allocator_traits<AllocRebind>::construct(allc, cbms, nullptr, 0, 0, std::forward<Args>(args)..., alloc);
    cbms->ptr = &cbms->object;
    return SharedPtr<T>(static_cast<typename SharedPtr<T>::ControlBlockBase*>(cbms));
}

template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return allocateShared<T>(std::allocator<T>(), std::forward<Args>(args)...); 
}

template<typename T>
class WeakPtr {
private: 
    typename SharedPtr<T>::ControlBlockBase *cb;
    
    template<typename Y>
    friend class WeakPtr;

public:
    WeakPtr() : cb(nullptr) {}
    WeakPtr(const SharedPtr<T>& sp) : cb(sp.cb) {
        ++cb->weak_count;
    }
    WeakPtr& operator=(const SharedPtr<T>& sp) {
        if (cb != nullptr) {
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
        cb = sp.cb;
        ++cb->weak_count;
        return *this;
    }
    WeakPtr(const WeakPtr& other) : cb(other.cb) {
        ++cb->weak_count;
    }
    WeakPtr(WeakPtr&& other) : cb(other.cb) {
        other.cb = nullptr;
    }
    WeakPtr& operator=(const WeakPtr& other) {
        if (cb != nullptr) {
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
        cb = other.cb;
        ++cb->weak_count;
        return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) {
        if (cb != nullptr) {
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
        cb = other.cb;
        other.cb = nullptr;
        return *this;
    }

    template <typename Y>
    WeakPtr(const SharedPtr<Y>& sp) : cb(reinterpret_cast<typename SharedPtr<T>::ControlBlockBase*>(sp.cb)) {
        ++cb->weak_count;
    }

    template<typename Y>
    WeakPtr(const WeakPtr<Y>& wp): cb(reinterpret_cast<typename SharedPtr<T>::ControlBlockBase*>(wp.cb)) {
        ++cb->weak_count;
    }

    template<typename Y>
    WeakPtr(WeakPtr<Y>&& wp): cb(wp.cb) {
        wp.cb = nullptr;
    }

    template<typename Y>
    WeakPtr& operator=(const WeakPtr<Y>& other) {
        if (cb != nullptr) { 
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
        cb = reinterpret_cast<typename SharedPtr<T>::ControlBlockBase*>(other.cb);
        ++cb->weak_count;
        return *this;
    }

    template<typename Y>
    WeakPtr& operator=(WeakPtr<Y>&& other) {
        if (cb != nullptr) {
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
        cb = reinterpret_cast<typename SharedPtr<T>::ControlBlockBase*>(other.cb);
        other.cb = nullptr;
        return *this;
    }

    ~WeakPtr() {
        if (cb != nullptr) {
            --cb->weak_count;
            if (cb->weak_count == 0 && cb->shared_count == 0) {
                cb->dispose();
            }
        }
    }

    size_t use_count() const {
        return cb->shared_count;
    }

    bool expired() const {
        return this->use_count() == 0;
    }

    SharedPtr<T> lock() const {
        return SharedPtr<T>(cb); 
    }
};
