#include <catch2/catch_test_macros.hpp>
#include <utils/storage/single_linked_list.hpp>

#include <vector>

struct Node {
    int value;
    Node *next = nullptr;
};

using TestList = SingleLinkedList<&Node::next>;

template <typename T>
class fancy_ptr {
public:
    fancy_ptr() = default;
    fancy_ptr(std::nullptr_t)
        : ptr { nullptr } {}
    fancy_ptr(T *ptr)
        : ptr { ptr } {}

    T &operator*() const {
        return *ptr;
    }
    T *operator->() const {
        return ptr;
    }

    explicit operator bool() const {
        return ptr != nullptr;
    }

    bool operator==(const fancy_ptr &other) const = default;
    bool operator==(const T *other) const {
        return ptr == other;
    }

    fancy_ptr &operator=(std::nullptr_t) {
        ptr = nullptr;
        return *this;
    }

    template <typename M>
    M &operator->*(M T::*member) const {
        return ptr->*member;
    }

private:
    T *ptr = nullptr;
};

struct FancyNode {
    int value;
    fancy_ptr<FancyNode> next;
};

using FancyTestList = SingleLinkedList<&FancyNode::next>;

TEST_CASE("SingleLinkedList basics", "[single_linked_list]") {

    SECTION("empty on construction") {
        TestList list;
        CHECK(list.empty());
    }

    SECTION("push_front makes list non-empty") {
        TestList list;
        Node node { .value = 42 };

        list.push_front(node);

        CHECK(!list.empty());
        CHECK(list.front().value == 42);
    }

    SECTION("front returns first element") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        CHECK(list.front().value == 2);
    }

    SECTION("back returns last element") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };
        Node node3 { .value = 3 };

        list.push_front(node1);
        list.push_front(node2);
        list.push_front(node3);

        CHECK(list.back().value == 1);
    }

    SECTION("pop_front removes front element") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        list.pop_front();

        CHECK(list.front().value == 1);
    }

    SECTION("pop_front until empty") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        list.pop_front();
        CHECK(!list.empty());

        list.pop_front();
        CHECK(list.empty());
    }

    SECTION("remove specific element from middle") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };
        Node node3 { .value = 3 };

        list.push_front(node1);
        list.push_front(node2);
        list.push_front(node3);

        CHECK(list.remove(node2));

        std::vector<int> values;
        for (auto &n : list) {
            values.push_back(n.value);
        }
        CHECK(values == std::vector<int> { 3, 1 });
    }

    SECTION("remove front element") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        CHECK(list.remove(node2));
        CHECK(list.front().value == 1);
    }

    SECTION("remove back element") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        CHECK(list.remove(node1));
        CHECK(list.front().value == 2);
        CHECK(list.back().value == 2);
    }

    SECTION("remove non-existent element returns false") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);

        CHECK(!list.remove(node2));
    }

    SECTION("remove from empty list returns false") {
        TestList list;
        Node node { .value = 1 };

        CHECK(!list.remove(node));
    }
}

TEST_CASE("SingleLinkedList iterator", "[single_linked_list]") {

    SECTION("empty list has begin == end") {
        TestList list;
        CHECK(list.begin() == list.end());
    }

    SECTION("range-based for loop") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };
        Node node3 { .value = 3 };

        list.push_front(node1);
        list.push_front(node2);
        list.push_front(node3);

        std::vector<int> values;
        for (auto &n : list) {
            values.push_back(n.value);
        }

        CHECK(values == std::vector<int> { 3, 2, 1 });
    }

    SECTION("iterator dereference") {
        TestList list;
        Node node { .value = 42 };

        list.push_front(node);

        auto it = list.begin();
        CHECK((*it).value == 42);
    }

    SECTION("iterator increment") {
        TestList list;
        Node node1 { .value = 1 };
        Node node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        auto it = list.begin();
        CHECK((*it).value == 2);

        ++it;
        CHECK((*it).value == 1);

        ++it;
        CHECK(it == list.end());
    }

    SECTION("iterator equality") {
        TestList list;
        Node node { .value = 1 };

        list.push_front(node);

        auto it1 = list.begin();
        auto it2 = list.begin();

        CHECK(it1 == it2);

        ++it1;
        CHECK(it1 != it2);
        CHECK(it1 == list.end());
    }
}

TEST_CASE("SingleLinkedList with fancy pointers", "[single_linked_list]") {

    SECTION("empty on construction") {
        FancyTestList list;
        CHECK(list.empty());
    }

    SECTION("push_front and front") {
        FancyTestList list;
        FancyNode node { .value = 42 };

        list.push_front(node);

        CHECK(!list.empty());
        CHECK(list.front().value == 42);
    }

    SECTION("multiple push_front maintains order") {
        FancyTestList list;
        FancyNode node1 { .value = 1 };
        FancyNode node2 { .value = 2 };
        FancyNode node3 { .value = 3 };

        list.push_front(node1);
        list.push_front(node2);
        list.push_front(node3);

        CHECK(list.front().value == 3);
        CHECK(list.back().value == 1);
    }

    SECTION("pop_front") {
        FancyTestList list;
        FancyNode node1 { .value = 1 };
        FancyNode node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        list.pop_front();
        CHECK(list.front().value == 1);

        list.pop_front();
        CHECK(list.empty());
    }

    SECTION("remove element") {
        FancyTestList list;
        FancyNode node1 { .value = 1 };
        FancyNode node2 { .value = 2 };
        FancyNode node3 { .value = 3 };

        list.push_front(node1);
        list.push_front(node2);
        list.push_front(node3);

        CHECK(list.remove(node2));

        std::vector<int> values;
        for (auto &n : list) {
            values.push_back(n.value);
        }
        CHECK(values == std::vector<int> { 3, 1 });
    }

    SECTION("iterator") {
        FancyTestList list;
        FancyNode node1 { .value = 1 };
        FancyNode node2 { .value = 2 };

        list.push_front(node1);
        list.push_front(node2);

        std::vector<int> values;
        for (auto &n : list) {
            values.push_back(n.value);
        }

        CHECK(values == std::vector<int> { 2, 1 });
    }
}

TEST_CASE("SingleLinkedList node in multiple lists", "[single_linked_list]") {

    struct MultiNode {
        int value;
        MultiNode *next_a = nullptr;
        MultiNode *next_b = nullptr;
    };

    using ListA = SingleLinkedList<&MultiNode::next_a>;
    using ListB = SingleLinkedList<&MultiNode::next_b>;

    SECTION("same node can belong to two different lists") {
        ListA list_a;
        ListB list_b;

        MultiNode node1 { .value = 1 };
        MultiNode node2 { .value = 2 };
        MultiNode node3 { .value = 3 };

        // Add all nodes to list_a
        list_a.push_front(node1);
        list_a.push_front(node2);
        list_a.push_front(node3);

        // Add only node1 and node3 to list_b (in different order)
        list_b.push_front(node3);
        list_b.push_front(node1);

        // Verify list_a contains all three
        std::vector<int> values_a;
        for (auto &n : list_a) {
            values_a.push_back(n.value);
        }
        CHECK(values_a == std::vector<int> { 3, 2, 1 });

        // Verify list_b contains only node1 and node3
        std::vector<int> values_b;
        for (auto &n : list_b) {
            values_b.push_back(n.value);
        }
        CHECK(values_b == std::vector<int> { 1, 3 });

        // Removing from one list doesn't affect the other
        list_a.remove(node1);

        values_a.clear();
        for (auto &n : list_a) {
            values_a.push_back(n.value);
        }
        CHECK(values_a == std::vector<int> { 3, 2 });

        // node1 is still in list_b
        values_b.clear();
        for (auto &n : list_b) {
            values_b.push_back(n.value);
        }
        CHECK(values_b == std::vector<int> { 1, 3 });
    }
}
