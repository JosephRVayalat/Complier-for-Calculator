# Simple Calculator Language Compiler (C)

## Overview

This project implements a **simple calculator language compiler in C** to demonstrate the fundamental phases of a compiler. The program accepts arithmetic expressions and variable assignments, processes them through **lexical analysis, parsing, postfix (RPN) generation, and evaluation**, and prints the final result.

The compiler supports basic arithmetic operations, variables, and parentheses. It also maintains a **symbol table** so variables defined in one expression can be reused in later expressions during the same session.

Example expressions:

```
a = 5 + 1
b = a + 4
(3 + 2) * 4
```

---

## Requirements

To build and run the program you need:

* A **C compiler** (recommended: GCC)
* A terminal or command line environment

Supported platforms:

* Linux
* macOS
* Windows (MinGW / WSL / GCC)

---

## How to Compile

Navigate to the project directory and compile the program using:

```bash
gcc calculator_compiler.c -o calculator
```

---

## How to Run

Execute the compiled program:

```bash
./calculator
```

You will see an input prompt:

```
>> 
```

Enter expressions to evaluate:

```
>> a=5+1
>> b=a+4
>> exit
```

Typing `exit` terminates the program.

---

## Notes

* The program evaluates **one expression per line**.
* Variables remain stored in memory during the session.
* Supports operators: `+  -  *  /`
* Supports parentheses for precedence.
* Displays intermediate stages such as **token stream, parsing steps, postfix code, and result**.
* Includes basic runtime checks such as **division by zero and undefined variables**.
