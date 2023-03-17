/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 Sadie Powell <sadie@witchery.services>
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


namespace insp {

template <typename T, typename Tag>
class INSPIRCD_INTRUSIVE_LIST_NAME {
  public:
    class iterator {
        T* curr;

      public:
        typedef std::ptrdiff_t difference_type;
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef T** pointer;
        typedef T*& reference;
        typedef T* value_type;

        iterator(T* i = NULL)
            : curr(i) {
        }

        iterator& operator++() {
            curr = curr->intrusive_list_node<T, Tag>::ptr_next;
            return *this;
        }

        iterator operator++(int) {
            iterator ret(*this);
            operator++();
            return ret;
        }

        iterator& operator--() {
            curr = curr->intrusive_list_node<T, Tag>::ptr_prev;
            return *this;
        }

        iterator operator--(int) {
            iterator ret(*this);
            operator--();
            return ret;
        }

        bool operator==(const iterator& other) const {
            return (curr == other.curr);
        }
        bool operator!=(const iterator& other) const {
            return (curr != other.curr);
        }
        T* operator*() const {
            return curr;
        }
    };

    typedef iterator const_iterator;

    INSPIRCD_INTRUSIVE_LIST_NAME()
        : listhead(NULL)
#ifdef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
        , listtail(NULL)
#endif
        , listsize(0) {
    }

    bool empty() const {
        return (size() == 0);
    }

    size_t size() const {
        return listsize;
    }

    iterator begin() const {
        return iterator(listhead);
    }

    iterator end() const {
        return iterator();
    }

    void pop_front() {
        erase(listhead);
    }

    T* front() const {
        return listhead;
    }

    void push_front(T* x) {
        if (listsize++) {
            x->intrusive_list_node<T, Tag>::ptr_next = listhead;
            listhead->intrusive_list_node<T, Tag>::ptr_prev = x;
        }
#ifdef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
        else {
            listtail = x;
        }
#endif
        listhead = x;
    }

#ifdef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
    T* back() const {
        return listtail;
    }

    void push_back(T* x) {
        if (listsize++) {
            x->intrusive_list_node<T, Tag>::ptr_prev = listtail;
            listtail->intrusive_list_node<T, Tag>::ptr_next = x;
        } else {
            listhead = x;
        }
        listtail = x;
    }

    void pop_back() {
        erase(listtail);
    }
#endif

    void erase(const iterator& it) {
        erase(*it);
    }

    void erase(T* x) {
        if (listhead == x) {
            listhead = x->intrusive_list_node<T, Tag>::ptr_next;
        }
#ifdef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
        if (listtail == x) {
            listtail = x->intrusive_list_node<T, Tag>::ptr_prev;
        }
#endif
        x->intrusive_list_node<T, Tag>::unlink();
        listsize--;
    }

  private:
    T* listhead;
#ifdef INSPIRCD_INTRUSIVE_LIST_HAS_TAIL
    T* listtail;
#endif
    size_t listsize;
};

} // namespace insp
