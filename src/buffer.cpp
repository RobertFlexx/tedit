struct Snap {
  vector<string> lines;
  bool dirty = false;
  bool final_newline = true;
  size_t current_line = 0;
};
static const size_t UNDO_MAX = 200;
static const size_t UNDO_BYTES_MAX = 64U * 1024U * 1024U;
struct Stack {
  std::deque<Snap> st;
  size_t bytes = 0;

  static size_t snapshot_bytes(const vector<string> &lines) {
    size_t total = 0;
    for (const auto &line : lines) {
      if (line.size() > std::numeric_limits<size_t>::max() - total)
        return std::numeric_limits<size_t>::max();
      total += line.size();
    }
    return total;
  }

  void clear() {
    st.clear();
    bytes = 0;
  }
  void mark_all_dirty() {
    for (auto &snap : st)
      snap.dirty = true;
  }
  void push(const vector<string> &lines, bool dirty, bool final_newline,
            size_t current_line) {
    size_t added = snapshot_bytes(lines);
    while (!st.empty() &&
           (st.size() >= UNDO_MAX ||
            (added <= UNDO_BYTES_MAX && bytes > UNDO_BYTES_MAX - added))) {
      bytes -= snapshot_bytes(st.front().lines);
      st.pop_front();
    }
    if (added > UNDO_BYTES_MAX)
      clear();
    st.push_back(Snap{lines, dirty, final_newline, current_line});
    bytes = added > std::numeric_limits<size_t>::max() - bytes
                ? std::numeric_limits<size_t>::max()
                : bytes + added;
  }
  bool pop(Snap &s) {
    if (st.empty())
      return false;
    size_t removed = snapshot_bytes(st.back().lines);
    s = std::move(st.back());
    st.pop_back();
    bytes = removed > bytes ? 0 : bytes - removed;
    return true;
  }
};

struct Buffer {
  string path;
  vector<string> lines;
  bool dirty = false;
  bool number = true;
  bool backup = true;
  bool highlight = false;
  bool final_newline = true;
  size_t current_line = 0;
  Stack undo;
  Stack redo;
};

static size_t char_count(const Buffer &b) {
  size_t total = 0;
  for (size_t i = 0; i < b.lines.size(); ++i) {
    size_t amount = b.lines[i].size();
    if (i + 1 < b.lines.size() || b.final_newline)
      ++amount;
    if (amount > std::numeric_limits<size_t>::max() - total)
      return std::numeric_limits<size_t>::max();
    total += amount;
  }
  return total;
}
