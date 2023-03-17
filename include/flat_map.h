/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include <vector>

namespace insp {

namespace detail {

template <typename T, typename Comp>
class map_pair_compare : public Comp {
    typedef T value_type;
    typedef typename value_type::first_type key_type;

  public:
    bool operator()(const value_type& x, const value_type& y) const {
        return Comp::operator()(x.first, y.first);
    }

    bool operator()(const value_type& x, const key_type& y) const {
        return Comp::operator()(x.first, y);
    }

    bool operator()(const key_type& x, const value_type& y) const {
        return Comp::operator()(x, y.first);
    }
};

template <typename Val, typename Comp>
class map_value_compare {
  public:
    // Constructor should be private

    bool operator()(const Val& x, const Val& y) const {
        Comp c;
        return c(x.first, y.first);
    }
};

template <typename T, typename Comp, typename Key = T, typename ElementComp = Comp>
class flat_map_base {
  protected:
    typedef std::vector<T> storage_type;
    storage_type vect;

  public:
    typedef typename storage_type::iterator iterator;
    typedef typename storage_type::const_iterator const_iterator;
    typedef typename storage_type::reverse_iterator reverse_iterator;
    typedef typename storage_type::const_reverse_iterator const_reverse_iterator;

    typedef typename storage_type::size_type size_type;
    typedef typename storage_type::difference_type difference_type;
    typedef Key key_type;
    typedef T value_type;

    typedef Comp key_compare;
    typedef ElementComp value_compare;

    flat_map_base() { }

    flat_map_base(const flat_map_base& other)
        : vect(other.vect) {
    }

#if __cplusplus >= 201103L
    flat_map_base& operator=(const flat_map_base& other) = default;
#endif

    size_type size() const {
        return vect.size();
    }
    bool empty() const {
        return vect.empty();
    }
    size_type capacity() const {
        return vect.capacity();
    }
    size_type max_size() const {
        return vect.max_size();
    }

    void clear() {
        vect.clear();
    }
    void reserve(size_type n) {
        vect.reserve(n);
    }

    iterator begin() {
        return vect.begin();
    }
    iterator end() {
        return vect.end();
    }
    reverse_iterator rbegin() {
        return vect.rbegin();
    }
    reverse_iterator rend() {
        return vect.rend();
    }

    const_iterator begin() const {
        return vect.begin();
    }
    const_iterator end() const {
        return vect.end();
    }
    const_reverse_iterator rbegin() const {
        return vect.rbegin();
    }
    const_reverse_iterator rend() const {
        return vect.rend();
    }

    key_compare key_comp() const {
        return Comp();
    }

    iterator erase(iterator it) {
        return vect.erase(it);
    }
    iterator erase(iterator first, iterator last) {
        return vect.erase(first, last);
    }
    size_type erase(const key_type& x) {
        size_type n = vect.size();
        std::pair<iterator, iterator> itpair = equal_range(x);
        vect.erase(itpair.first, itpair.second);
        return n - vect.size();
    }

    iterator find(const key_type& x) {
        value_compare c;
        iterator it = std::lower_bound(vect.begin(), vect.end(), x, c);
        if ((it != vect.end()) && (!c(x, *it))) {
            return it;
        }
        return vect.end();
    }

    const_iterator find(const key_type& x) const {
        // Same as above but this time we return a const_iterator
        value_compare c;
        const_iterator it = std::lower_bound(vect.begin(), vect.end(), x, c);
        if ((it != vect.end()) && (!c(x, *it))) {
            return it;
        }
        return vect.end();
    }

    std::pair<iterator, iterator> equal_range(const key_type& x) {
        return std::equal_range(vect.begin(), vect.end(), x, value_compare());
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& x) const {
        return std::equal_range(vect.begin(), vect.end(), x, value_compare());
    }

    iterator lower_bound(const key_type& x) {
        return std::lower_bound(vect.begin(), vect.end(), x, value_compare());
    }

    const_iterator lower_bound(const key_type& x) const {
        return std::lower_bound(vect.begin(), vect.end(), x, value_compare());
    }

    iterator upper_bound(const key_type& x) {
        return std::upper_bound(vect.begin(), vect.end(), x, value_compare());
    }

    const_iterator upper_bound(const key_type& x) const {
        return std::upper_bound(vect.begin(), vect.end(), x, value_compare());
    }

    size_type count(const key_type& x) const {
        std::pair<const_iterator, const_iterator> itpair = equal_range(x);
        return std::distance(itpair.first, itpair.second);
    }

  protected:
    std::pair<iterator, bool> insert_single(const value_type& x) {
        bool inserted = false;

        value_compare c;
        iterator it = std::lower_bound(vect.begin(), vect.end(), x, c);
        if ((it == vect.end()) || (c(x, *it))) {
            inserted = true;
            it = vect.insert(it, x);
        }
        return std::make_pair(it, inserted);
    }

    iterator insert_multi(const value_type& x) {
        iterator it = std::lower_bound(vect.begin(), vect.end(), x, value_compare());
        return vect.insert(it, x);
    }
};

} // namespace detail

template <typename T, typename Comp = std::less<T>, typename ElementComp = Comp>
class flat_set : public detail::flat_map_base<T, Comp, T, ElementComp> {
    typedef detail::flat_map_base<T, Comp, T, ElementComp> base_t;

  public:
    typedef typename base_t::iterator iterator;
    typedef typename base_t::value_type value_type;

    flat_set() { }

    template <typename InputIterator>
    flat_set(InputIterator first, InputIterator last) {
        this->insert(first, last);
    }

    flat_set(const flat_set& other)
        : base_t(other) {
    }

#if __cplusplus >= 201103L
    flat_set& operator=(const flat_set& other) = default;
#endif

    std::pair<iterator, bool> insert(const value_type& x) {
        return this->insert_single(x);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            this->insert_single(*first);
        }
    }

    void swap(flat_set& other) {
        base_t::vect.swap(other.vect);
    }
};

template <typename T, typename Comp = std::less<T>, typename ElementComp = Comp>
class flat_multiset : public detail::flat_map_base<T, Comp, T, ElementComp> {
    typedef detail::flat_map_base<T, Comp, T, ElementComp> base_t;

  public:
    typedef typename base_t::iterator iterator;
    typedef typename base_t::value_type value_type;

    flat_multiset() { }

    template <typename InputIterator>
    flat_multiset(InputIterator first, InputIterator last) {
        this->insert(first, last);
    }

    flat_multiset(const flat_multiset& other)
        : base_t(other) {
    }

#if __cplusplus >= 201103L
    flat_multiset& operator=(const flat_multiset& other) = default;
#endif

    iterator insert(const value_type& x) {
        return this->insert_multi(x);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            insert_multi(*first);
        }
    }

    void swap(flat_multiset& other) {
        base_t::vect.swap(other.vect);
    }
};

template <typename T, typename U, typename Comp = std::less<T>, typename ElementComp = Comp >
class flat_map : public
    detail::flat_map_base<std::pair<T, U>, Comp, T, detail::map_pair_compare<std::pair<T, U>, ElementComp> > {
    typedef detail::flat_map_base<std::pair<T, U>, Comp, T, detail::map_pair_compare<std::pair<T, U>, ElementComp> >
    base_t;

  public:
    typedef typename base_t::iterator iterator;
    typedef typename base_t::key_type key_type;
    typedef typename base_t::value_type value_type;
    typedef U mapped_type;
    typedef typename base_t::value_compare value_compare;

    flat_map() { }

    template <typename InputIterator>
    flat_map(InputIterator first, InputIterator last) {
        insert(first, last);
    }

    flat_map(const flat_map& other)
        : base_t(other) {
    }

#if __cplusplus >= 201103L
    flat_map& operator=(const flat_map& other) = default;
#endif

    std::pair<iterator, bool> insert(const value_type& x) {
        return this->insert_single(x);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            this->insert_single(*first);
        }
    }

    void swap(flat_map& other) {
        base_t::vect.swap(other.vect);
    }

    mapped_type& operator[](const key_type& x) {
        return insert(std::make_pair(x, mapped_type())).first->second;
    }

    value_compare value_comp() const {
        return value_compare();
    }
};

template <typename T, typename U, typename Comp = std::less<T>, typename ElementComp = Comp >
class flat_multimap : public
    detail::flat_map_base<std::pair<T, U>, Comp, T, detail::map_pair_compare<std::pair<T, U>, ElementComp> > {
    typedef detail::flat_map_base<std::pair<T, U>, Comp, T, detail::map_pair_compare<std::pair<T, U>, ElementComp> >
    base_t;

  public:
    typedef typename base_t::iterator iterator;
    typedef typename base_t::value_type value_type;
    typedef U mapped_type;
    typedef typename base_t::value_compare value_compare;

    flat_multimap() { }

    template <typename InputIterator>
    flat_multimap(InputIterator first, InputIterator last) {
        this->insert(first, last);
    }

    flat_multimap(const flat_multimap& other)
        : base_t(other) {
    }

#if __cplusplus >= 201103L
    flat_multimap& operator=(const flat_multimap& other) = default;
#endif

    iterator insert(const value_type& x) {
        return this->insert_multi(x);
    }

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            this->insert_multi(*first);
        }
    }

    void swap(flat_multimap& other) {
        base_t::vect.swap(other.vect);
    }

    value_compare value_comp() const {
        return value_compare();
    }
};

} // namespace insp
