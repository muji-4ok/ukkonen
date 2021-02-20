#include <iostream>
#include <vector>
#include <fstream>
#include <optional>
#include <map>
#include <memory>

struct Node;

struct Transition {
  Transition() = default;
  explicit Transition(std::unique_ptr<Node> &&node,
                      size_t start,
                      std::optional<size_t> end)
      : node(std::move(node)), start(start), end(end) {}

  size_t Length() const { return end.value() - start; }

  std::unique_ptr<Node> node = nullptr;
  // [start, end)
  size_t start = 0;
  std::optional<size_t> end = std::nullopt;
};

struct Node {
  std::map<char, Transition> to;
  Node *suffix_link = nullptr;
  size_t id = 0;
};

struct ActivePoint {
  Node *node = nullptr;
  // c == \0 <=> length = 0 and that we don't have an edge to go to
  char c = '\0';
  size_t length = 0;
};

class SuffixTree {
 public:
  explicit SuffixTree(const std::string &s) : s(s), root(std::make_unique<Node>()) {
    ap.node = root.get();
    Initialize();
  }

 private:
  void Initialize();

  void AddSuffix(char c, size_t index);
  void FinishString(Node &from);
  bool NeedToSplit(char c) const;
  void FixLengthOverflow(size_t index);

  void ExportToDot(size_t read_size);
  void ExportToDot(std::ostream &out, size_t read_size);
  void ExportNodeToDot(Node &from,
                       std::ostream &out,
                       size_t read_size,
                       size_t &node_id);
  void ExportSuffLinks(Node &from, std::ostream &out);

  const std::string &s;
  std::unique_ptr<Node> root;
  ActivePoint ap{};
  size_t remainder = 0;
  size_t node_count = 0;

  size_t step_id = 0;
};

void SuffixTree::Initialize() {
  ++node_count;

  for (size_t i = 0; i < s.size(); ++i)
    AddSuffix(s[i], i);

  FinishString(*root);
  ExportToDot(s.size());
}

bool SuffixTree::NeedToSplit(char c) const {
  if (ap.c)
    return s[ap.node->to.at(ap.c).start + ap.length] != c;
  else
    return ap.node->to.find(c) == ap.node->to.end();
}

void SuffixTree::FixLengthOverflow(size_t index) {
  while (ap.c && ap.node->to[ap.c].end.has_value()
      && ap.length > ap.node->to[ap.c].end.value() - ap.node->to[ap.c].start) {
    ActivePoint new_ap{};
    size_t node_len = ap.node->to[ap.c].Length();
    char next_c = s[index - ap.length + node_len];
    new_ap.node = ap.node->to[ap.c].node.get();
    new_ap.length = ap.length - node_len;
    new_ap.c = next_c;
    ap = new_ap;
  }

  if (ap.c) {
    Transition &to_c = ap.node->to[ap.c];

    if (to_c.end.has_value() && ap.length == to_c.end.value() - to_c.start) {
      ap.node = to_c.node.get();
      ap.length = 0;
      ap.c = '\0';
    }
  }
}

void SuffixTree::AddSuffix(char c, size_t index) {
  ++remainder;
  Node *prev_created = nullptr;

  while (remainder > 0) {
    if (NeedToSplit(c)) {
      if (ap.c) {
        Transition &to_c = ap.node->to[ap.c];

        size_t split_end = to_c.start + ap.length;

        auto other_node = std::make_unique<Node>();
        to_c.node.swap(other_node);
        to_c.node->to.emplace(
            std::make_pair(s[split_end], Transition(std::move(other_node), split_end, to_c.end))
        );
        to_c.end = split_end;
        to_c.node->to.emplace(
            std::make_pair(c, Transition(std::make_unique<Node>(), index, std::nullopt))
        );

        node_count += 2;

        if (prev_created)
          prev_created->suffix_link = to_c.node.get();

        prev_created = to_c.node.get();
      } else {
        ap.node->to.emplace(
            std::make_pair(c, Transition(std::make_unique<Node>(), index, std::nullopt))
        );
        ++node_count;

        if (prev_created && ap.node != root.get())
          prev_created->suffix_link = ap.node;

        prev_created = nullptr;
      }

      --remainder;

      if (ap.node == root.get()) {
        if (ap.length > 0)
          --ap.length;

        ap.c = ap.length ? s[index - remainder + 1] : '\0';
      } else {
        ap.node = ap.node->suffix_link ? ap.node->suffix_link : root.get();
      }

      FixLengthOverflow(index);
    } else {
      if (!ap.c)
        ap.c = c;

      ++ap.length;

      if (prev_created && ap.node != root.get())
        prev_created->suffix_link = ap.node;

      prev_created = nullptr;

      FixLengthOverflow(index);
      ExportToDot(index + 1);
      break;
    }

    ExportToDot(index + 1);
  }
}

void SuffixTree::FinishString(Node &from) {
  for (auto &it : from.to) {
    if (!it.second.end.has_value())
      it.second.end = s.size();

    FinishString(*it.second.node);
  }
}

void SuffixTree::ExportToDot(size_t read_size) {
  std::ofstream file("step_" + std::to_string(step_id++) + ".dot");
  ExportToDot(file, read_size);
}

void SuffixTree::ExportToDot(std::ostream &out, size_t read_size) {
  out << "digraph G {\n";
  out << "rankdir = LR;\n";
  out << "nodesep = 0.5;\n";
  size_t node_id = 0;
  ExportNodeToDot(*root, out, read_size, node_id);
  ExportSuffLinks(*root, out);
  out << "}\n";
}

void SuffixTree::ExportNodeToDot(Node &from,
                                 std::ostream &out,
                                 size_t read_size,
                                 size_t &node_id) {
  from.id = node_id;
  ++node_id;

  if (&from == ap.node)
    out << from.id << R"( [color="red", style="filled"])" << "\n";

  for (const auto &it : from.to) {
    std::string label;
    size_t end_index = it.second.end.value_or(read_size) - it.second.start;

    if (&from == ap.node && it.first == ap.c)
      label = s.substr(it.second.start, ap.length) + "|"
          + s.substr(it.second.start + ap.length, end_index - ap.length);
    else
      label = s.substr(it.second.start, end_index);

    out << from.id << " -> " << node_id << " [label=\"" << label << "\"";
    out << R"(, color="blue"])";

    out << '\n';

    ExportNodeToDot(*it.second.node, out, read_size, node_id);
  }
}

void SuffixTree::ExportSuffLinks(Node &from, std::ostream &out) {
  if (from.suffix_link)
    out << from.id << " -> " << from.suffix_link->id << R"( [style="dotted"])" << "\n";

  for (const auto &it : from.to)
    ExportSuffLinks(*it.second.node, out);
}

int main() {
  // s must end with "$", e.g. "abacaba$"
  std::string s;
  std::cin >> s;

  SuffixTree suffix_tree(s);
}
