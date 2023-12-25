#ifndef DEQUE_H
#define DEQUE_H

#include <memory>
#include <stdexcept>
#include <utility>

template<typename Container>
class node_t;

template<typename Container>
class iterator_t;

template<typename Value>
class deque_t
{
public:

  using node = node_t<deque_t<Value>>;

  using type = Value;

  using iterator = iterator_t<deque_t<Value>>;

  deque_t()
    : rend_(node::make()->iterator_)
    , end_(node::make()->iterator_)
  {
    rend_.get_node()->next_ = end_.get_node();
    end_.get_node()->prev_ = rend_.get_node();
  }

  deque_t(deque_t&) = delete;

  deque_t(deque_t&& other) = delete;

  ~deque_t()
  {
    remove_all();
  }

  iterator add(Value&& value)
  {
    auto tmp = node::make(std::move(value));
    auto end_node = end_.get_node();
    tmp->next_ = end_node;
    if (rend_ + 1) {
      end_node->prev_->next_ = tmp;
      tmp->prev_ = end_node->prev_;
      end_node->prev_ = tmp;
    } else {
      rend_.get_node()->next_ = tmp;
      tmp->prev_ = rend_.get_node();
      end_node->prev_ = tmp;
    }
    return end_ - 1;
  }

  void remove_all()
  {
    while (auto it = rend_ + 1) {
      it.remove_element();
    }
  }

  iterator begin() { return rend_ + 1; }

  iterator end() { return end_; }

private:
  const iterator rend_;
  const iterator end_;
};

template<typename Container>
class iterator_t : public std::shared_ptr<typename Container::node>
{
private:
  using node = typename Container::node;

public:
  using type = typename Container::type;

  iterator_t()
  {
  }

  iterator_t(node * element)
    : std::shared_ptr<node>(element)
  {
  }

  iterator_t& operator++()
  {
    *this = base::get()->next_->iterator_;
    return *this;
  }

  iterator_t operator++(int)
  {
    return std::exchange(*this, base::get()->next_->iterator_);
  }

  iterator_t& operator--()
  {
    *this = base::get()->prev_->iterator_;
    return *this;
  }

  iterator_t operator--(int)
  {
    return std::exchange(*this, base::get()->prev_->iterator_);
  }

  iterator_t operator-(int back) const
  {
    node* current = base::get();
    for (int i = 0; i < back; i++) {
      check(current);
      current = current->prev_;
    }
    return current->iterator_;
  }

  iterator_t operator+(int forward) const
  {
    node* current = base::get();
    check(current);
    for (int i = 0; i < forward; i++) {
      current = current->next_;
      check(current);
    }
    return current->iterator_;
  }

  operator bool() const
  {
    return base::operator bool() && base::get()->operator bool();
  }

  bool is_end() const
  {
    return this->operator bool();
  }

  type& operator *() const
  {
    check(get());
    return *get();
  }

  type* operator ->() const
  {
    return get();
  }

  type* get() const
  {
    check(base::get());
    return base::get()->get();
  }

  void remove_element()
  {
    check(base::get());
    base::get()->remove();
  }

private:
  friend Container;
  using base = std::shared_ptr<node>;

  static void check(void* ptr)
  {
    if (!ptr) throw std::out_of_range("error: bad iterator");
  }

  node* get_node() const { return base::get(); }
};

template<typename Container>
class node_t : public std::unique_ptr<typename Container::type>
{
public:

  using iterator = typename Container::iterator;

  using type = typename Container::type;

  static node_t<Container>* make() {
    return new node_t();
  }

  static node_t<Container>* make(type&& value) {
    return new node_t(std::move(value));
  }

  void remove()
  {
    next_->prev_ = prev_;
    prev_->next_ = next_;
    delete release();
  }

  using std::unique_ptr<type>::reset;
  using std::unique_ptr<type>::release;

  iterator iterator_;
  node_t<Container>* prev_;
  node_t<Container>* next_;

private:
  node_t()
    : iterator_(this)
  {
  }

  node_t(type&& value)
    : std::unique_ptr<type>(std::make_unique<type>(std::move(value)))
    , iterator_(this)
  {
  }

  node_t(node_t&) = delete;

  node_t(node_t&&) = delete;

};


#endif // DEQUE_H
