#include "dsa/trie.hpp"

#include <algorithm>
#include <cctype>

namespace ig::dsa {

Trie::Trie() { nodes_.emplace_back(); }

int Trie::indexOf(char c) {
  const unsigned char uc = static_cast<unsigned char>(c);
  if (uc >= 'a' && uc <= 'z') return uc - 'a';
  if (uc >= '0' && uc <= '9') return 26 + (uc - '0');
  if (c == '_') return 36;
  if (c == '.') return 37;
  return -1;
}

bool Trie::insert(const std::string& word) {
  int node = 0;
  for (char raw : word) {
    const char c = static_cast<char>(std::tolower(raw));
    int idx = indexOf(c);
    if (idx < 0) continue;  // skip unsupported characters
    if (nodes_[node].next[idx] == -1) {
      nodes_[node].next[idx] = static_cast<int>(nodes_.size());
      nodes_.emplace_back();
    }
    node = nodes_[node].next[idx];
  }
  if (nodes_[node].terminal) return false;  // duplicate word
  nodes_[node].terminal = true;
  ++wordCount_;
  return true;
}

bool Trie::search(const std::string& word) const {
  int node = 0;
  for (char raw : word) {
    int idx = indexOf(static_cast<char>(std::tolower(raw)));
    if (idx < 0) continue;
    int next = nodes_[node].next[idx];
    if (next == -1) return false;
    node = next;
  }
  return nodes_[node].terminal;
}

bool Trie::remove(const std::string& word) {
  int node = 0;
  for (char raw : word) {
    int idx = indexOf(static_cast<char>(std::tolower(raw)));
    if (idx < 0) continue;
    int next = nodes_[node].next[idx];
    if (next == -1) return false;
    node = next;
  }
  if (!nodes_[node].terminal) return false;
  nodes_[node].terminal = false;
  --wordCount_;
  return true;
}

void Trie::collect(int node, std::string& current,
                   std::vector<std::string>& out, std::size_t limit) const {
  if (out.size() >= limit) return;
  if (nodes_[node].terminal) out.push_back(current);
  for (std::size_t i = 0; i < nodes_[node].next.size(); ++i) {
    int next = nodes_[node].next[i];
    if (next == -1) continue;
    char c;
    if (i < 26) c = static_cast<char>('a' + i);
    else if (i < 36) c = static_cast<char>('0' + (i - 26));
    else if (i == 36) c = '_';
    else c = '.';
    current.push_back(c);
    collect(next, current, out, limit);
    current.pop_back();
    if (out.size() >= limit) return;
  }
}

std::vector<std::string> Trie::startsWith(const std::string& prefix,
                                          std::size_t limit) const {
  int node = 0;
  for (char raw : prefix) {
    int idx = indexOf(static_cast<char>(std::tolower(raw)));
    if (idx < 0) return {};
    int next = nodes_[node].next[idx];
    if (next == -1) return {};
    node = next;
  }
  std::vector<std::string> out;
  std::string current = prefix;
  collect(node, current, out, limit);
  return out;
}

}  // namespace ig::dsa
