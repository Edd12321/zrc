<img src="zrc.png" width=100 height=auto />

Zrc is a small scripting language for Linux,BSD,etc. written in C++. It is a shell with syntax similar to Tcl/TK ([EIAS](https://wiki.tcl-lang.org/page/everything+is+a+string)).

It was created because the old shell languages that I used weren't "programmable" like Tcl or Lisp, didn't have builtin expression evaluator, and were very basic. It is hard to write simple scripts in these usual shells because of the compilcated grammar rules. Zrc makes it easy to write even small programs, being a full programming language.

Zrc is almost half of 1mb in size because its implementation is to the point and simple. The language doesn't even need a Backus -- Naur form (BNF). You can compile with -Os to make it even smaller, at the expense of performance !

## Features left to implement:

- [ ] Standard library soon?
- [X] Home directory config file (`~/.zrc`)
- [X] Aliases
- [X] **Rich return values** (not just 0-255, but any string)
- [X] Procedures/functions
	- [X] Function arguments
		- [X] `$argv(...)`
		- [X] `$argc`
- [X] Globbing
	- [X] Tilde expansion
	- [X] Wildcards
- [X] Pipelines
- [X] Full redirection
	- [X] Basic redirection (`>>`, `>`, `<`)
    - [X] Here(documents/strings) (`<<< STR`, `<< EOF[...]EOF`)
	- [X] ~~Sometimes, fd syntax is broken (`>(2=1)`, `>(2=)`, `>(2)`)~~
- [X] Non-I/O shell operators
	- [X] `&&`
	- [X] `||`
	- [X] `!`
- [X] Word splitting via `{*}`
- [X] Quoting
	- [X] Brace quoting (`{}`)
	- [X] Regular quoting (`` ` ``, `'`, `"`)
- [ ] Escape sequences
	- [X] Basic support (every other esc. code)
	- [ ] Full support (`\uhhh`, `\xhh`)
- [X] Word substitution
	- [X] Variable expansion
		- [X] Scalars, arrays/hashes (`$`)
		- [X] Separate envvar namespace (`$E:`)
	- [X] Command substitution
		- [X] Output (`` `{...} ``)
		- [X] Return value (`[...]`)
- [X] Built-in commands (like `expr`, `jobs`, etc. but there are too many to list. View `dispatch.hpp`)
- [X] Conditional logic/flow control with full C arithmetic operator set
	- [X] If/elsif/else
	- [X] Do
	- [X] While
	- [X] Foreach
	- [X] For
	- [X] Switch
	- [X] Subshell (`@ {...}`)
	- [X] Lexial scoping (`let`)
	- [X] Until
	- [X] Eval
- [X] Full job control
	- [X] Job table support
	- [X] Job listing command
	- [X] Job manipulation
	- [X] Don't separate jobs by pipe
	- [X] Background and foreground processes with `&`
- [ ] Pleasant interactive shell
	- [X] History file
	- [X] Line editor
	- [X] Tab completion
	- [ ] Syntax highlighting
## Inspirations:

* [Tcl](https://www.tcl.tk)
* [Powershell](https://learn.microsoft.com/en-us/powershell/)
* [Plan 9's rc](https://9fans.github.io/plan9port/man/man1/rc.html)
* [(t)csh](https://en.wikipedia.org/wiki/C_shell)
