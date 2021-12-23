/*
Copyright 2022 Google LLC
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    https://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <algorithm>
#include <assert.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <iostream>
#include <list>
#include <math.h>
#include <stdlib.h>
#include <set>
#include <string>
#include <vector>

#define N Node::create
#define NT Node::create_test
#define WRITEOP(s, op, mathmode) { if (op == '+' && mathmode) s += "$+$"; else s += op; }
#define GET_COST(total, n) (total * total - n)
//#define GET_COST(total, n) (total - PI_RATIO)

using namespace std;
using namespace boost;
using boost::multiprecision::cpp_int;
using Mask = unsigned int;
using Ratio = rational<cpp_int>;
using Value = unsigned int;
using Values = list<Value>;

Ratio INT_SERIES[] = {Ratio( 1), Ratio( 2), Ratio( 3), Ratio( 4),
                      Ratio( 5), Ratio( 6), Ratio( 7), Ratio( 8),
                      Ratio( 9), Ratio(10), Ratio(11), Ratio(12)};
Ratio E12_SERIES[] = {Ratio(10,10), Ratio(12,10), Ratio(15,10), Ratio(18,10),
                      Ratio(22,10), Ratio(27,10), Ratio(33,10), Ratio(39,10),
                      Ratio(47,10), Ratio(56,10), Ratio(68,10), Ratio(82,10)};
Ratio ONE_SERIES[] = {Ratio(1), Ratio(1), Ratio(1), Ratio(1),
                      Ratio(1), Ratio(1), Ratio(1), Ratio(1),
                      Ratio(1), Ratio(1), Ratio(1), Ratio(1),
                      Ratio(1), Ratio(1), Ratio(1), Ratio(1)};
Ratio* SERIES = INT_SERIES;
Ratio PI_RATIO = Ratio(314159265358979,100000000000000);

struct Node {
  Values values; Values hidden; list<Node*> children; Ratio ratio;
  static Node& create() { return *(new Node()); }
  static Node& create(Value v) { Node* node = new Node(); node->values.push_back(v); return *node; }
  static Node& create(const Values& vs) { Node* node = new Node(); node->values = vs; return *node; }

  // The following two take care of the "off-by-one" issue in test networks
  // (where we prefer to use literal node values over indices)
  static Node& create_test(Value v) { Node* node = new Node(); node->values.push_back(v - 1); return *node; }
  static Node& create_test(const Values& vs) { Node* node = new Node(); for (auto v : vs) node->values.push_back(v - 1); return *node; }

  ~Node() { for (auto child : children) if (!child->ratio) delete child; }
  Node& operator[](Node& node) { children.push_back(&node); return *this; }
  Node* clone() {
    if (ratio) return this;
    Node* my_clone = &N(values);
    for (auto child : children) my_clone->children.push_back(child->clone());
    return my_clone;
  }
  void leafify() {
    if (children.empty()) {
      if (values.size() == 2) {
        while (!values.empty()) {
          children.push_back(&N(values.front()));
          values.pop_front();
        }
      }
    } else {
      for (auto child : children) child->leafify();
      if (!values.empty()) {
        children.push_back(&N(values));
        values.clear();
      }
    }
  }
  string to_string(bool mathmode = false, bool top = true, char op1 = '+', char op2 = '|') const {
    string s = "";
    if (!top && values.size() + children.size() > 1) s += "(";
    for (auto child = children.begin(); child != children.end(); ++child) {
      if (child != children.begin()) WRITEOP(s, op1, mathmode);
      bool subtop = top && values.size() == 0 && children.size() == 1;
      s += (*child)->ratio ? (*child)->to_string(mathmode, subtop)
                           : (*child)->to_string(mathmode, subtop, op2, op1);
    }
    if (!values.empty() && !children.empty()) WRITEOP(s, op1, mathmode);
    for (auto value = values.begin(); value != values.end(); ++value) {
      if (value != values.begin()) WRITEOP(s, op1, mathmode);
      auto v = rational_cast<long long>(SERIES[*value] * 10);
      s += std::to_string(v / 10);
      if (v % 10) s += "." + std::to_string(v % 10);
    }
    if (!top && values.size() + children.size() > 1) s += ")";
    return s;
  }
  string to_network(char op1 = '+', char op2 = '|') const {
    string s = "N(";
    if (values.size() > 1) s += "{";
    for (auto value = values.begin(); value != values.end(); ++value) {
      if (value != values.begin()) s += ",";
      s += std::to_string(*value);
    }
    if (values.size() > 1) s += "}";
    s += ")";
    for (auto child = children.begin(); child != children.end(); ++child) {
      s += "[";
      if ((*child)->ratio && op1 == '+') s += "N()[";
      s += (*child)->ratio ? (*child)->to_network() : (*child)->to_network(op2, op1);
      if ((*child)->ratio && op1 == '+') s += "]";
      s += "]";
    }
    return s;
  }

 private: Node() {}
};

struct NetworkEvaluator {
  Ratio evaluate_total(const Node* node, int bound = 0, char op1 = '+', char op2 = '|') {
    char op = (bound == 1) ? '+' : '|';
    Ratio result;
    if (!node->values.empty()) {
      Ratio subresult;
      char valueop = (node->values.size() > 2 || !node->children.empty()) ? op : op1;
      for (auto value : node->values) {
        subresult += (valueop == '+') ? SERIES[value] : 1 / SERIES[value];
      }
      result += (valueop == '+') ? subresult : 1 / subresult;
      if (op1 == '|') result = 1 / result;
    }
    for (auto child : node->children) {
      Ratio subresult = child->ratio ? child->ratio : evaluate_total(child, bound, op2, op1);
      result += (op1 == '+') ? subresult : 1 / subresult;
    }
    return (op1 == '+') ? result : 1 / result;
  }

  Ratio evaluate_cost(const Node* node, unsigned int n, int bound = 0) {
    Ratio total = evaluate_total(node, bound);
    Ratio cost = GET_COST(total, n);
    return (cost > 0) ? cost : -cost;
  }
} network_evaluator;

struct Bounder {
  Ratio bound(const Node* network, unsigned int n) {
    Ratio lower_bound = network_evaluator.evaluate_total(network, -1);
    Ratio upper_bound = network_evaluator.evaluate_total(network,  1);
    return max( GET_COST(lower_bound, n),
               -GET_COST(upper_bound, n));
  }
};

struct Expander {
  Expander(Node* n) : network(n) { stack.push_back(n); }
  
  Node* expandable() {
    while (!stack.empty()) {
      Node* node = stack.back(); stack.pop_back();
      for (auto child : node->children) stack.push_back(child);
      if (node->values.size() > 2) return node;
      if (node->values.size() > 1)
        if (!node->children.empty() || node == network) return node;
    }
    return NULL;
  }

 private: Node* network; list<Node*> stack;
};

struct SubsetCoder {
  void decode(Mask mask, const Values& values, Values& include, Values& exclude) {
    for (auto value : values) {
      if (include.empty()) { include.push_back(value); continue; }
      if (mask & 0x1) include.push_back(value);
      else exclude.push_back(value);
      mask >>= 1;
    }
  }
  
  Mask encode(const Values& values) {
    Mask mask = 0;
    for (auto value : values) mask |= 1 << value;
    return mask;
  }
} coder;

struct Tabulator {
  unsigned int m; vector<vector<pair<Ratio, Node*>>> lookup_table;
  Tabulator(unsigned int _m) : m(_m) { }
  ~Tabulator() { clear(); }

  void tabulate(unsigned int n) {
    clear();
    lookup_table.resize(1 << n);
    Node* network = &N();
    tabulate(n, network);
    delete network;
  }

  Node* binary_search(const Node* network, Node* expandable, const Values& values, unsigned int n) {
    Mask mask = coder.encode(values);
    vector<pair<Ratio, Node*>>& entry = lookup_table[mask];
    int lo = 0, hi = entry.size(), best_idx = -1;
    Ratio best_cost = -1;
    while (lo < hi) {
      int mid = (lo + hi) / 2;
      expandable->children.push_back(entry[mid].second);
      Ratio total = network_evaluator.evaluate_total(network);
      Ratio cost = GET_COST(total, n);
      if (cost < 0) cost = -cost;
      if (best_cost < 0 || best_cost > cost) {
        best_cost = cost; best_idx = mid;
      }
      if (total * total < n) lo = mid + 1; else hi = mid;
      expandable->children.pop_back();
    }
    return entry[best_idx].second;
  }

  pair<Node*,Node*> linear_search(const Node* network, Node* expandable_0,
      Node* expandable_1, const Values& values_0, const Values& values_1, unsigned int n) {
    Mask mask_0 = coder.encode(values_0), mask_1 = coder.encode(values_1);
    vector<pair<Ratio, Node*>>& entry_0 = lookup_table[mask_0],
                                entry_1 = lookup_table[mask_1];
    unsigned int lo = 0;
    int hi = entry_1.size() - 1, best_lo = -1, best_hi = -1;
    Ratio best_cost = -1;
    while (lo < entry_0.size() && hi >= 0) {
      expandable_0->children.push_back(entry_0[lo].second);
      expandable_1->children.push_back(entry_1[hi].second);
      Ratio total = network_evaluator.evaluate_total(network);
      Ratio cost = GET_COST(total, n);
      if (cost < 0) cost = -cost;
      if (best_cost < 0 || best_cost > cost) {
        best_cost = cost; best_lo = lo; best_hi = hi;
      }
      if (total * total < n) lo += 1; else hi -= 1;
      expandable_1->children.pop_back();
      expandable_0->children.pop_back();
    }
    return pair<Node*,Node*>(entry_0[best_lo].second, entry_1[best_hi].second);
  }

 private:

  void clear() {
    for (auto entries : lookup_table) for (auto entry : entries) delete entry.second;
    lookup_table.clear();
  }

  void tabulate(unsigned int n, Node* network, Mask mask = 0, Value i = 0) {
    if (i >= n) {
      if (mask) {
        vector<pair<Ratio, Node*>>& entry = lookup_table[mask];
        tabulate(entry, network);
        sort(entry.begin(), entry.end());
      }
      return;
    }
    tabulate(n, network, mask, i + 1);
    if (network->values.size() < m) {
      network->values.push_back(i);
      tabulate(n, network, mask | (1 << i), i + 1);
      network->values.pop_back();
    }
  }

  void tabulate(vector<pair<Ratio, Node*>>& entry, Node* network) {
    Expander expander(network);
    Node* expandable = expander.expandable();
    if (!expandable) {
      Node* clone = network->clone();
      clone->ratio = network_evaluator.evaluate_total(network);
      entry.push_back(pair<Ratio, Node*>(clone->ratio, clone));
      return;
    }
    Values values = expandable->values; expandable->values.clear();
    bool has_children = !expandable->children.empty();
    Node* child = &N();
    expandable->children.push_back(child);
    Mask max_mask = 1 << (values.size() - 1);
    for (Mask mask = 0; mask < max_mask; ++mask) {
      coder.decode(mask, values, child->values, expandable->values);
      if (has_children || !expandable->values.empty() || expandable == network)
        tabulate(entry, network);
      child->values.clear();
      expandable->values.clear();
    }
    expandable->children.pop_back();
    delete child;
    expandable->values = values;
  }
};

struct Solver {
  Solver(Bounder* b = NULL, Tabulator* t = NULL) :
      bounder(b), tabulator(t), best_network(NULL) {}
  ~Solver() { clear(); }

  Node* solve(unsigned int n) {
    clear();
    Node* network = &N();
    for (Value i = 0; i < n; ++i) network->values.push_back(i);
    if (tabulator) tabulator->tabulate(n);
    solve(n, network);
    delete network;
    return best_network;
  }

 private: Bounder* bounder; Tabulator* tabulator; Node* best_network;

  void clear() {
    if (best_network) delete best_network;
    best_network = NULL;
  }

  void solve(unsigned int n, Node* network) {
    if (bounder && best_network && bounder->bound(network, n) >= best_network->ratio)
      return;
    Expander expander(network);
    Node* expandable_0 = expander.expandable();
    if (!expandable_0) {
      Ratio cost = network_evaluator.evaluate_cost(network, n);
      if (!best_network || best_network->ratio > cost) {
        if (best_network) delete best_network;
        best_network = network->clone();
        best_network->ratio = cost;
      }
      return;
    }
    Node* expandable_1 = expander.expandable();
    Node* expandable_2 = expandable_1 ? expander.expandable() : NULL;
    Values values_0 = expandable_0->values; expandable_0->values.clear();
    if (tabulator && !expandable_1 && values_0.size() <= tabulator->m) {
      Node* node = tabulator->binary_search(network, expandable_0, values_0, n);
      expandable_0->children.push_back(node);
      solve(n, network);
      expandable_0->children.pop_back();
    } else if (tabulator && expandable_1 && !expandable_2 &&
               values_0.size() <= tabulator->m &&
               expandable_1->values.size() <= tabulator->m) {
      Values values_1 = expandable_1->values; expandable_1->values.clear();
      pair<Node*,Node*> nodes = tabulator->linear_search(
          network, expandable_0, expandable_1, values_0, values_1, n);
      expandable_0->children.push_back(nodes.first);
      expandable_1->children.push_back(nodes.second);
      solve(n, network);
      expandable_1->children.pop_back();
      expandable_0->children.pop_back();
      expandable_1->values = values_1;
    } else {
      bool has_children = !expandable_0->children.empty();
      Node* child = &N();
      expandable_0->children.push_back(child);
      Mask max_mask = 1 << (values_0.size() - 1);
      for (Mask mask = 0; mask < max_mask; ++mask) {
        coder.decode(mask, values_0, child->values, expandable_0->values);
        if (has_children || !expandable_0->values.empty() || expandable_0 == network)
          solve(n, network);
        child->values.clear();
        expandable_0->values.clear();
      }
      expandable_0->children.pop_back();
      delete child;
    }
    expandable_0->values = values_0;
  }
};

void print_summary(ostream& os, Node* network, unsigned int n, const string& prefix) {
  Ratio total = network_evaluator.evaluate_total(network);
  os << prefix << "Solution: " << network->to_string() << endl;
  os << prefix << " Network: " << network->to_network() << endl;
  os << setprecision(16);
  os << prefix << "  Target: " << sqrt(n) << endl;
  os << prefix << "   Total: " << rational_cast<double>(total) << " (" << total << ")" << endl;
  os << setprecision(4);
  os << prefix << "    Cost: " << abs(rational_cast<double>(total) - sqrt(n)) << endl;
}