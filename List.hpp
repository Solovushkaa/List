#include <exception>
#include <type_traits>
#include <utility>
#include <memory>

namespace mls
{    
    //----------------------------------------------------------------------------------
    template<typename T>
    struct List_Node
    {
        T data_;
        List_Node* next_;
        List_Node* prev_;

        List_Node() = default;
        template<typename U = T>
        explicit List_Node(U&& el) : data_(el), next_(nullptr), prev_(nullptr){}
        List_Node(const List_Node& node) = delete;
        List_Node& operator=(const List_Node& node) = delete;
        ~List_Node() = default;
    };

    //----------------------------------------------------------------------------------
    template<typename T, bool IsConst>
    class List_Base_Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using pointer = std::conditional_t<IsConst, const List_Node<T>*, List_Node<T>*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using value_type = T;
        using difference_type = std::ptrdiff_t;

        pointer node_;

        List_Base_Iterator(pointer node) : node_(node) {}
        List_Base_Iterator(const List_Base_Iterator& it) = default;
        List_Base_Iterator& operator=(const List_Base_Iterator& it) = default;

        reference operator*() { return node_->data_; }
        pointer operator->() { return node_; }

        List_Base_Iterator& operator++() { 
            node_ = node_->next_; 
            return *this;
        }
        List_Base_Iterator& operator++(int) { 
            auto tmp = *this;
            node_ = node_->next_;
            return tmp; 
        }

        List_Base_Iterator& operator--() { 
            node_ = node_->prev_; 
            return *this; 
        }
        List_Base_Iterator& operator--(int) { 
            auto tmp = this;
            node_ = node_->prev_;
            return tmp; 
        }

        bool operator==(const List_Base_Iterator& it) { return node_ == it.node_; }
        bool operator!=(const List_Base_Iterator& it) { return node_ != it.node_; }
    };
    
    //----------------------------------------------------------------------------------
    template<typename T, typename Allocator = std::allocator<T>>
    class List
    {
    private:
        using Node_Alloc = typename Allocator::template rebind<List_Node<T>>::other;

        using iterator = List_Base_Iterator<T, false>;
        using const_iterator = List_Base_Iterator<T, true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
        List_Node<T> fake_node_;
        Node_Alloc node_alloc_;
        size_t sz_;

    public:
        List();

        List(const List& copy_list);
        List(List&& move_list);
        List& operator=(const List& copy_list);
        List& operator=(List&& move_list);

        size_t size() const { return sz_; }
        bool empty() const { return !sz_; }
        void clear();

        T& front() { return *begin(); }
        const T& front() const { return *begin(); }
        
        T& back() { return *(--end()); }
        const T& back() const { return *(--end()) ; }

        template<typename U = T>
        void push_front(U&& el);
        template<typename U = T>
        void push_back(U&& el);

        void pop_front();
        void pop_back();

        template<typename U = T>
        iterator insert(iterator& it, U&& el);
        void erase(iterator& it);

        void merge(List& other);
        void merge(List&& other);
        void swap(List& other);
        void sort(bool ascending = true);
        void reverse();

        iterator begin() { return {fake_node_.next_}; }
        iterator end() { return {&fake_node_}; }

        const_iterator begin() const { return {fake_node_.next_}; }
        const_iterator end() const { return {&fake_node_}; }

        const_iterator cbegin() const { return {fake_node_.next_}; }
        const_iterator cend() const { return {&fake_node_}; }

        reverse_iterator rbegin() { return {fake_node_.prev_}; }
        reverse_iterator rend() { return {&fake_node_}; }

        const_reverse_iterator rbegin() const { return {fake_node_.prev_}; }
        const_reverse_iterator rend() const { return {&fake_node_}; }
        
        const_reverse_iterator crbegin() const { return {fake_node_.prev_}; }
        const_reverse_iterator crend() const { return {&fake_node_}; }

    private:
        void insert_node(List_Node<T>* old_node, List_Node<T>* new_node);
        
        template<typename U = T>
        List_Node<T>* obj_construct(U&& el);
        void obj_destruct(List_Node<T>* del_node);  

    public:
        ~List();
    };

    template<typename T, typename Allocator>
    List<T, Allocator>::List() : fake_node_(0), node_alloc_(), sz_(0) {
        fake_node_.next_ = &fake_node_;
        fake_node_.prev_ = &fake_node_;
    }

    template <typename T, typename Allocator>
    List<T, Allocator>::List(const List &copy_list) : List()
    {
        *this = copy_list;
    }

    template <typename T, typename Allocator>
    List<T, Allocator>::List(List &&move_list) : List()
    {
        *this = std::forward<List>(move_list);
    }

    template <typename T, typename Allocator>
    List<T, Allocator>& List<T, Allocator>::operator=(const List& copy_list)
    {
        clear();
        for (auto it = copy_list.begin(); it != copy_list.end(); ++it)
        {
            push_back(*it);
        }
        return *this;
    }

    template <typename T, typename Allocator>
    List<T, Allocator> &List<T, Allocator>::operator=(List&& move_list)
    {
        clear();
        fake_node_.next_ = move_list.fake_node_.next_;
        fake_node_.prev_ = move_list.fake_node_.prev_;
        fake_node_.next_->prev_ = &fake_node_;
        fake_node_.prev_->next_ = &fake_node_;

        move_list.fake_node_.next_ = &move_list.fake_node_;
        move_list.fake_node_.prev_ = &move_list.fake_node_;
        sz_ = move_list.sz_;
        move_list.sz_ = 0;
        return *this;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::clear()
    {
        List_Node<T>* del_node = fake_node_.next_;
        while (del_node != &fake_node_)
        {
            fake_node_.next_ = fake_node_.next_->next_;
            obj_destruct(del_node);
            del_node = fake_node_.next_;
        }
        fake_node_.prev_ = &fake_node_;
        sz_ = 0;
    }

    template <typename T, typename Allocator>
    template <typename U>
    void List<T, Allocator>::push_front(U&& el)
    {
        List_Node<T>* new_node = nullptr;
        try {
            new_node = obj_construct(std::forward<U>(el));
            insert_node(fake_node_.next_, new_node);
            ++sz_;
        } catch(...) {
            obj_destruct(new_node);
            throw std::bad_alloc();
        } 
    }

    template <typename T, typename Allocator>
    template <typename U>
    void List<T, Allocator>::push_back(U&& el)
    {
        List_Node<T>* new_node = nullptr;
        try {
            new_node = obj_construct(std::forward<U>(el));
            insert_node(&fake_node_, new_node);
            ++sz_;
        } catch(...) {
            obj_destruct(new_node);
            throw std::bad_alloc();
        } 
    }

    template <typename T, typename Allocator>
    template <typename U>
    typename List<T, Allocator>::iterator List<T, Allocator>::insert(iterator &it, U &&el)
    {
        List_Node<T>* new_node = nullptr;
        try {
            new_node = obj_construct(el);
            insert_node(it.node_, new_node);
            ++sz_;
        } catch(...) {
            obj_destruct(new_node);
            throw std::bad_alloc();
        }
        auto new_it = iterator(new_node);
        return new_it;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::erase(iterator &it)
    {
        List_Node<T>* del_node = it.node_;
        it->prev_->next_ = del_node->next_;
        del_node->next_->prev_ = del_node->prev_;
        obj_destruct(del_node);
        --sz_;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::merge(List& other)
    {
        if(this == &other) { return; }
        
        List_Node<T>* new_node = nullptr;
        for(auto it = other.begin(); it != other.end(); ++it)
        {
            try {
                new_node = obj_construct(*it);
                insert_node(end().node_, new_node);
            } catch(...) {
                obj_destruct(new_node);
                throw std::bad_alloc();
            }
        }
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::merge(List&& other)
    {
        if(this == &other) { return; }

        fake_node_.prev_->next_ = other.fake_node_.next_;
        other.fake_node_.next_->prev_ = fake_node_.prev_;
        fake_node_.prev_ = other.fake_node_.prev_;
        fake_node_.prev_->next_ = &fake_node_;

        other.fake_node_.next_ = &other.fake_node_;
        other.fake_node_.prev_ = &other.fake_node_;

        sz_ += other.sz_;
        other.sz_ = 0;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::swap(List& other)
    {
        if(this == &other) { return; }
        std::swap(fake_node_.next_, other.fake_node_.next_);
        std::swap(fake_node_.prev_, other.fake_node_.prev_);
        std::swap(sz_, other.sz_);
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::sort(bool ascending)
    {
        if(sz_ < 2) { return; }

        auto lamb = ascending ? [](T& a, T& b){ return a<b; } : [](T& a, T& b){ return a>b; };
        auto lamb_t = ascending ? [](T& a, T& b){ return a<=b; } : [](T& a, T& b){ return a>=b; };

        bool flag = true;
        for (auto it_i = ++begin(); it_i != end();)
        {
            auto it_j = it_i;
            --it_j;
            for (; it_j != end(); --it_j)
            {
                auto it_next = it_j;
                --it_next;
                if(lamb(*it_i,*it_j) && (it_next == end() || lamb_t(*it_next, *it_i))){
                    auto it_s = it_i;
                    ++it_s;

                    it_i->next_->prev_ = it_i->prev_;
                    it_i->prev_->next_ = it_i->next_;

                    insert_node(it_j.node_, it_i.node_);
                    it_i = it_s;

                    flag = false;
                    break;
                }
            }
            if(flag) { ++it_i; }
            flag = true;
        }
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::reverse()
    {
        for (auto it = begin(); it != end(); --it)
        {
            std::swap(it->next_, it->prev_);
        }
        std::swap(end()->next_, end()->prev_);
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::pop_front()
    {
        List_Node<T>* del_node = fake_node_.next_;
        fake_node_.next_ = del_node->next_;
        del_node->next_->prev_ = &fake_node_;
        obj_destruct(del_node);
        --sz_;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::pop_back()
    {
        List_Node<T>* del_node = fake_node_.prev_;
        fake_node_.prev_ = del_node->prev_;
        del_node->prev_->next_ = &fake_node_;
        obj_destruct(del_node);
        --sz_;
    }

    template <typename T, typename Allocator>
    template <typename U>
    List_Node<T>* List<T, Allocator>::obj_construct(U&& el)
    {
        List_Node<T>* new_node = node_alloc_.allocate(1);
        node_alloc_.construct(new_node, std::forward<U>(el));
        return new_node;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::insert_node(List_Node<T>* old_node, List_Node<T>* new_node)
    {
        new_node->next_ = old_node;
        new_node->prev_ = old_node->prev_;
        old_node->prev_->next_ = new_node;
        old_node->prev_ = new_node;
    }

    template <typename T, typename Allocator>
    void List<T, Allocator>::obj_destruct(List_Node<T>* del_node)
    {
        node_alloc_.destroy(del_node);
        node_alloc_.deallocate(del_node, 1);
    }

    template <typename T, typename Allocator>
    List<T, Allocator>::~List()
    {
        clear();
    }
    //----------------------------------------------------------------------------------
}