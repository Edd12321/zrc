<img src="img/zrc.svg" width=100 height=auto />

Zrc is a small scripting language for Linux, BSD, etc. written in C++ (core interpreter in a very small ~3000SLOC). It is a shell with syntax similar to Tcl/TK ([EIAS](https://wiki.tcl-lang.org/page/everything+is+a+string)). The code is quite small, but minimalism is not the main focus of Zrc (which is why it has way more "luxury features" compared to usual shells). Instead, the idea is to be a better alternative to `tclsh` that extends its syntax and adds features like job control, a custom line editor and more, while still almost keeping Tcl's "pure syntax".

It was created because the old shell languages that I used weren't "programmable" like Tcl or Lisp, didn't have a builtin expression evaluator, and were very basic. It is hard to write simple scripts in these usual shells because of the compilcated grammar rules. Zrc makes it easy to write even small programs, being a full programming language.

Zrc is just ~600KB in size because its implementation is to the point and simple. The language doesn't even need a Backus -- Naur form (BNF). You can compile with -Os to make it even smaller, at the expense of performance!

## Features left to implement:

- [ ] Full standard library
    - [ ] Finish corebuf
    - [ ] Finish zrclib
        - [X] List implementation
        - [X] Basic utility library
        - [X] Math.h equivalent
        - [ ] Formatted I/O
        - [X] <s>Classes/objects,</s> structs
        - [ ] More?
- [X] Path hashing/caching
- [X] Cdpath support
- [X] Home directory config file (`~/.zrc`)
- [X] Aliases
- [X] **Rich return values** (not just 0-255, but any string)
- [X] Complex I/O handling (control flow, functions, built-ins and commands can be seamlessly piped together)
- [X] Procedures/functions
	- [X] Function arguments
		- [X] `$argv(...)`
		- [X] `$argc`
- [X] Globbing
	- [X] Tilde expansion
	- [X] Wildcards
- [X] Signal trapping (via `fn`, `nf`)
- [X] Directory stack
- [X] Pipelines
- [X] Full redirection
	- [X] Basic redirection (`^`, `^?`, `^^`, `>>`, `>`, `>?`, `<`)
    - [X] Here(documents/strings) (`<<< STR`, `<< EOF[...]EOF`)
    - [X] File descriptors (`x>`, `x>>`, `x> &-`, `x> &y` or `x>? y`)
- [X] Non-I/O shell operators
	- [X] `&&`
	- [X] `||`
	- [X] `!`
- [X] Word splitting via `{*}`
- [X] Quoting
	- [X] Brace quoting (`{}`)
	- [X] Regular quoting (`'`, `"`)
- [ ] Escape sequences
	- [X] Basic support (every other esc. code)
    - [X] `\e`, `\cx`
	- [ ] Full support (`\uhhh`, `\xhh`)
- [X] Word substitution
	- [X] Variable expansion
		- [X] Scalars, arrays/hashes (`$`, `${...}`)
		- [X] Separate envvar array (`$env(...)`)
	- [X] Command substitution
		- [X] Output (`` `{...} ``)
        - [X] Process (`<{...}`)
		- [X] Return value (`[...]`)
- [X] Built-in commands (like `expr`, `jobs`, etc. but there are too many to list. View `dispatch.hpp`)
- [X] Conditional logic/flow control with full C arithmetic operator set
	- [X] If/else
    - [X] Unless
	- [X] Do
	- [X] While
	- [X] Foreach
	- [X] For
	- [X] Switch
	- [X] Subshell (`@ {...}`)
	- [X] Lexial scoping (`let`)
	- [X] Until
	- [X] Eval
    - [X] Return
    - [X] Continue
    - [X] Break
    - [X] Fallthrough
- [X] Full job control
	- [X] Job table support
	- [X] Job listing command
	- [X] Job manipulation
	- [X] Don't separate jobs by pipe
	- [X] Background and foreground processes with `&`
- [ ] Usable regular expressions
    - [X] Basic built-in regex support (`regexp`)
    - [ ] More commandline options
    - [ ] `regcomp`
- [ ] Pleasant interactive shell
	- [X] History file
	- [X] Line editor
	- [X] Tab completion
	- [ ] Syntax highlighting
    - [X] Keybindings

As mentioned on [the Oil wiki's Alternative Shells list](https://github.com/oilshell/oil/wiki/Alternative-Shells).

## Inspirations:

* [Tcl](https://www.tcl.tk)
* [Powershell](https://learn.microsoft.com/en-us/powershell/)
* [Plan 9's rc](https://9fans.github.io/plan9port/man/man1/rc.html)
* [(t)csh](https://en.wikipedia.org/wiki/C_shell)
