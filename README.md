# Interpreted Language
This project is an implementation of a custom interpred programming laguage written in C++.

# Prerequisites to build
1. C++ compiler
2. [xmake](https://xmake.io/)
# Building
In the root folder: `xmake build`, add `-y` flag after `build` to automatically install dependencies.
# Running
`xmake run app` to run REPL mode.

`xmake run app filepath` to run a file. Filepath must be absolute because otherwise it would be relative to location of executable, not to working directory.

# Syntax
Syntax is prone to change, so look at examples to see what works.

# Dependencies
- [lexy](https://lexy.foonathan.net/) for parsing source code
- [fmt](https://fmt.dev) for printing and formatting
