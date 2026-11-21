#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

static const string EPS = "eps";
static const string END = "$";

using Symbols = vector<string>;

struct Grammar {
    vector<string> order;                    // nonterminals in definition order
    map<string, vector<Symbols>> prods;      // empty Symbols == epsilon production
    set<string> terminals;
    string start;
    bool isNT(const string& s) const { return prods.count(s) > 0; }
};

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static Symbols split(const string& s) {
    istringstream is(s);
    Symbols v;
    string w;
    while (is >> w) v.push_back(w);
    return v;
}

static string show(const string& lhs, const Symbols& rhs) {
    string r = lhs + " ->";
    if (rhs.empty()) return r + " " + EPS;
    for (auto& s : rhs) r += " " + s;
    return r;
}

Grammar readGrammar(istream& in) {
    Grammar g;
    string line;
    while (getline(in, line)) {
        size_t h = line.find('#');
        if (h != string::npos) line.erase(h);
        size_t arrow = line.find("->");
        if (arrow == string::npos) continue;
        string lhs = trim(line.substr(0, arrow));
        if (lhs.empty()) continue;
        if (!g.prods.count(lhs)) {
            g.order.push_back(lhs);
            g.prods[lhs];
        }
        if (g.start.empty()) g.start = lhs;

        string rest = line.substr(arrow + 2);
        stringstream alts(rest);
        string alt;
        while (getline(alts, alt, '|')) {
            Symbols rhs = split(alt);
            if (rhs.size() == 1 && rhs[0] == EPS) rhs.clear();
            g.prods[lhs].push_back(rhs);
        }
    }
    for (auto& [lhs, alts] : g.prods)
        for (auto& rhs : alts)
            for (auto& s : rhs)
                if (!g.isNT(s)) g.terminals.insert(s);
    return g;
}

using SetMap = map<string, set<string>>;

// FIRST of a symbol string. Contains EPS iff the whole string is nullable.
set<string> firstOfSeq(const Grammar& g, const SetMap& first, const Symbols& seq, size_t from = 0) {
    set<string> res;
    bool nullable = true;
    for (size_t i = from; i < seq.size() && nullable; ++i) {
        const string& s = seq[i];
        nullable = false;
        if (!g.isNT(s)) {
            res.insert(s);
        } else {
            const auto& f = first.at(s);
            for (auto& t : f)
                if (t != EPS) res.insert(t);
            nullable = f.count(EPS) > 0;
        }
    }
    if (nullable) res.insert(EPS);
    return res;
}

SetMap computeFirst(const Grammar& g) {
    SetMap first;
    for (auto& nt : g.order) first[nt];
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& nt : g.order)
            for (auto& rhs : g.prods.at(nt))
                for (auto& t : firstOfSeq(g, first, rhs))
                    changed |= first[nt].insert(t).second;
    }
    return first;
}

SetMap computeFollow(const Grammar& g, const SetMap& first) {
    SetMap follow;
    for (auto& nt : g.order) follow[nt];
    follow[g.start].insert(END);
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& A : g.order)
            for (auto& rhs : g.prods.at(A))
                for (size_t i = 0; i < rhs.size(); ++i) {
                    const string& B = rhs[i];
                    if (!g.isNT(B)) continue;
                    set<string> f = firstOfSeq(g, first, rhs, i + 1);
                    bool nullable = f.erase(EPS) > 0;
                    for (auto& t : f) changed |= follow[B].insert(t).second;
                    if (nullable)
                        for (auto& t : follow[A]) changed |= follow[B].insert(t).second;
                }
    }
    return follow;
}

// table[A][terminal] = index of production in g.prods[A]
using Table = map<string, map<string, int>>;

bool buildTable(const Grammar& g, const SetMap& first, const SetMap& follow, Table& table) {
    bool ok = true;
    auto put = [&](const string& A, const string& t, int idx) {
        auto it = table[A].find(t);
        if (it != table[A].end() && it->second != idx) {
            cerr << "LL(1) conflict at M[" << A << ", " << t << "]:\n  "
                 << show(A, g.prods.at(A)[it->second]) << "\n  "
                 << show(A, g.prods.at(A)[idx]) << "\n";
            ok = false;
            return;
        }
        table[A][t] = idx;
    };
    for (auto& A : g.order) {
        const auto& alts = g.prods.at(A);
        for (int i = 0; i < (int)alts.size(); ++i) {
            set<string> f = firstOfSeq(g, first, alts[i]);
            for (auto& t : f)
                if (t != EPS) put(A, t, i);
            if (f.count(EPS))
                for (auto& t : follow.at(A)) put(A, t, i);
        }
    }
    return ok;
}

struct Node {
    string sym;
    vector<unique_ptr<Node>> kids;
    explicit Node(string s) : sym(move(s)) {}
};

static void printTree(const Node* n, const string& prefix, bool last, bool root = true) {
    if (root) cout << n->sym << "\n";
    else cout << prefix << (last ? "`-- " : "|-- ") << n->sym << "\n";
    string np = root ? "" : prefix + (last ? "    " : "|   ");
    for (size_t i = 0; i < n->kids.size(); ++i)
        printTree(n->kids[i].get(), np, i + 1 == n->kids.size(), false);
}

unique_ptr<Node> parse(const Grammar& g, const Table& table, Symbols input, bool trace = true) {
    input.push_back(END);
    auto root = make_unique<Node>(g.start);
    vector<Node*> stack;                       // top is back()
    auto endNode = make_unique<Node>(END);
    stack.push_back(endNode.get());
    stack.push_back(root.get());

    size_t pos = 0;
    int step = 0;
    if (trace) cout << left << setw(5) << "Step" << setw(34) << "Stack (top on right)"
                    << setw(28) << "Remaining input" << "Action\n";

    auto stackStr = [&] {
        string s;
        for (auto* n : stack) s += n->sym + " ";
        return s;
    };
    auto inputStr = [&] {
        string s;
        for (size_t i = pos; i < input.size(); ++i) s += input[i] + " ";
        return s;
    };
    auto log = [&](const string& action) {
        if (trace)
            cout << left << setw(5) << ++step << setw(34) << stackStr()
                 << setw(28) << inputStr() << action << "\n";
    };

    while (!stack.empty()) {
        Node* top = stack.back();
        const string& a = input[pos];

        if (top->sym == END) {
            if (a == END) { log("accept"); return root; }
            log("error");
            cerr << "Syntax error: unexpected '" << a << "', expected end of input\n";
            return nullptr;
        }
        if (!g.isNT(top->sym)) {                   // terminal on stack
            if (top->sym == a) {
                log("match " + a);
                stack.pop_back();
                ++pos;
            } else {
                log("error");
                cerr << "Syntax error: expected '" << top->sym << "' but found '" << a << "'\n";
                return nullptr;
            }
            continue;
        }
        auto row = table.find(top->sym);           // nonterminal on stack
        auto cell = row == table.end() ? map<string, int>::const_iterator() : row->second.find(a);
        if (row == table.end() || cell == row->second.end()) {
            log("error");
            cerr << "Syntax error: unexpected '" << a << "' while expanding " << top->sym
                 << "; expected one of:";
            if (row != table.end()) for (auto& [t, _] : row->second) cerr << " " << t;
            cerr << "\n";
            return nullptr;
        }
        const Symbols& rhs = g.prods.at(top->sym)[cell->second];
        log(show(top->sym, rhs));
        stack.pop_back();
        if (rhs.empty()) top->kids.push_back(make_unique<Node>("eps"));
        for (auto& s : rhs) top->kids.push_back(make_unique<Node>(s));
        for (auto it = top->kids.rbegin(); it != top->kids.rend(); ++it)
            if ((*it)->sym != "eps" || !rhs.empty()) stack.push_back(it->get());
    }
    return nullptr;
}

static void printSets(const Grammar& g, const string& name, const SetMap& m) {
    cout << name << " sets:\n";
    for (auto& nt : g.order) {
        cout << "  " << name << "(" << nt << ") = { ";
        bool firstItem = true;
        for (auto& t : m.at(nt)) { cout << (firstItem ? "" : ", ") << t; firstItem = false; }
        cout << " }\n";
    }
    cout << "\n";
}

static void printTable(const Grammar& g, const Table& table) {
    vector<string> cols(g.terminals.begin(), g.terminals.end());
    cols.push_back(END);
    cout << "Parse table:\n" << left << setw(8) << "";
    for (auto& c : cols) cout << setw(14) << c;
    cout << "\n";
    for (auto& A : g.order) {
        cout << setw(8) << A;
        for (auto& c : cols) {
            string cell;
            auto r = table.find(A);
            if (r != table.end()) {
                auto it = r->second.find(c);
                if (it != r->second.end()) {
                    const Symbols& rhs = g.prods.at(A)[it->second];
                    cell = rhs.empty() ? EPS : "";
                    for (auto& s : rhs) cell += (cell.empty() ? "" : " ") + s;
                }
            }
            cout << setw(14) << cell;
        }
        cout << "\n";
    }
    cout << "\n";
}

static const char* DEFAULT_GRAMMAR =
    "E  -> T E'\n"
    "E' -> + T E' | eps\n"
    "T  -> F T'\n"
    "T' -> * F T' | eps\n"
    "F  -> ( E ) | id\n";

int main(int argc, char** argv) {
    string grammarFile, inputStr = "id + id * id";
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "-g" && i + 1 < argc) grammarFile = argv[++i];
        else inputStr = a;
    }

    Grammar g;
    if (grammarFile.empty()) {
        istringstream is(DEFAULT_GRAMMAR);
        g = readGrammar(is);
    } else {
        ifstream f(grammarFile);
        if (!f) { cerr << "Cannot open " << grammarFile << "\n"; return 1; }
        g = readGrammar(f);
    }
    if (g.start.empty()) { cerr << "Empty grammar\n"; return 1; }

    cout << "Grammar:\n";
    for (auto& nt : g.order)
        for (auto& rhs : g.prods[nt]) cout << "  " << show(nt, rhs) << "\n";
    cout << "\n";

    SetMap first = computeFirst(g);
    SetMap follow = computeFollow(g, first);
    printSets(g, "FIRST", first);
    printSets(g, "FOLLOW", follow);

    Table table;
    if (!buildTable(g, first, follow, table)) {
        cerr << "\nGrammar is NOT LL(1).\n";
        return 1;
    }
    cout << "Grammar is LL(1).\n\n";
    printTable(g, table);

    cout << "Parsing: " << inputStr << "\n\n";
    auto tree = parse(g, table, split(inputStr));
    if (!tree) return 2;
    cout << "\nParse tree:\n";
    printTree(tree.get(), "", true);
    return 0;
}
