# LL(1) Parser in C++

A small table-driven LL(1) parser. Give it a grammar and some input tokens, and it will:

1. Compute the FIRST and FOLLOW sets
2. Build the parse table and check the grammar is LL(1)
3. Parse the input with a stack and print each step
4. Print the parse tree

## Build

```
g++ -std=c++17 -O2 -o ll1 ll1.cpp
```

## Run

```
./ll1                          # built-in expression grammar, default input
./ll1 "id + id * id"           # custom input tokens
./ll1 -g grammar.txt "a b c"   # custom grammar file
```

Input tokens are separated by spaces.

## Grammar format

One rule per line. Symbols are separated by spaces.

```
# comments start with #
E  -> T E'
E' -> + T E' | eps
T  -> F T'
T' -> * F T' | eps
F  -> ( E ) | id
```

- `|` separates alternatives
- `eps` means the empty string
- Anything on a left-hand side is a nonterminal; everything else is a terminal
- The first rule's left-hand side is the start symbol

## Example output

```
Parsing: id + id * id

Step Stack (top on right)     Remaining input        Action
1    $ E                      id + id * id $         E -> T E'
2    $ E' T                   id + id * id $         T -> F T'
...
17   $                        $                      accept
```

On a syntax error, it shows where parsing failed and which tokens were expected:

```
Syntax error: unexpected '*' while expanding T; expected one of: ( id
```

If the grammar is not LL(1), it prints the conflicting rules and exits:

```
LL(1) conflict at M[S, c]:
  S -> A a
  S -> A b
```

## Limitations

- Input is split on whitespace (no built-in lexer)
- It does not remove left recursion or left-factor the grammar; those show up as conflicts

## Exit codes

| Code | Meaning                        |
|------|--------------------------------|
| 0    | Input parsed successfully      |
| 1    | Bad grammar file or not LL(1)  |
| 2    | Syntax error in the input      |
