#pragma once
// Custom Trie (prefix tree) for username autocomplete.
// insert / exact search: O(L). Prefix query: O(L + matches).
// Only [a-z0-9_.] are indexed; other characters are ignored.
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace ig::dsa {

class Trie {
 public:
  Trie();
  bool insert(const std::string& word);
  bool search(const std::string& word) const;
  bool remove(const std::string& word);
  std::vector<std::string> startsWith(const std::string& prefix,
                                      std::size_t limit = 10) const;
  std::size_t size() const noexcept { return wordCount_; }

 private:
  struct Node {
    std::array<int, 38> next{};
    bool terminal = false;
    Node() { next.fill(-1); }
  };

  std::vector<Node> nodes_;
  std::size_t wordCount_ = 0;

  static int indexOf(char c);
  void collect(int node, std::string& current,
               std::vector<std::string>& out, std::size_t limit) const;
};

}  // namespace ig::dsa
