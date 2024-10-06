<img src="img/zrc.svg" width=100 height=auto />
<p>
<b>This is the new, better and REWRITTEN version of Zrc (master branch)! It has way less bugs, a better codebase, nicer syntax, etc. It also no longer has job control, regex, and a bunch of other stuff. I plan to add it again.
</b>
</p>

Zrc is a small scripting language for Linux, BSD, etc. written in C++ (core interpreter in a very small ~2000SLOC). It is a shell with syntax similar to Tcl/TK ([EIAS](https://wiki.tcl-lang.org/page/everything+is+a+string)). The code is quite small, but minimalism is not the main focus of Zrc (which is why it has way more "luxury features" compared to usual shells). Instead, the idea is to be a better alternative to `tclsh` that extends its syntax and adds features like job control, a custom line editor and more, while still almost keeping Tcl's "pure syntax".

It was created because the old shell languages that I used weren't "programmable" like Tcl or Lisp, didn't have a builtin expression evaluator, and were very basic. It is hard to write simple scripts in these usual shells because of the compilcated grammar rules. Zrc makes it easy to write even small programs, being a full programming language.

Zrc is just ~600KB in size because its implementation is to the point and simple. The language doesn't even need a Backus -- Naur form (BNF). You can compile with -Os to make it even smaller, at the expense of performance!

To use the classic version, issue the following command:
```
git checkout legacy
```
## Features left to implement:

- [X] Path hashing/caching
- [X] Cdpath support
- [X] Home directory config file (`~/.zrc`)
- [X] Aliases
- [X] **Rich return values** (not just 0-255, but any string)
- [X] Complex I/O handling (control flow, functions, built-ins and commands can be seamlessly piped together)
- [X] Procedures/functions
	- [X] Function arguments
		- [X] `${argv "..."}`
		- [X] `$argc`
- [X] Globbing (via `glob`)
	- [X] Tilde expansion (on GNU systems)
	- [X] Wildcards
- [X] Signal trapping (via `fn`)
- [X] Directory stack
- [X] Pipelines
- [X] Full redirection
	- [X] Basic redirection (`^`, `^?`, `^^`, `>>`, `>`, `>?`, `<`)
    - [X] File descriptors (`> x`, `>> x`, `>&- x`, `>& x y` or `>? x`)
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
		- [X] Environment scalars, exporting
	- [X] Command substitution
		- [X] Output (`` `{...} ``)
        - [X] Process (`<{...}`)
		- [X] Return value (`[...]`)
- [X] Built-in commands (like `expr`, etc. but there are too many to list. View `dispatch.hpp`)
- [X] Conditional logic/flow control with full C arithmetic operator set
	- [X] If/else
    - [X] Unless
	- [X] Do
	- [X] While
	- [X] Foreach
	- [X] For
	- [X] Switch (currently does nothing)
	- [X] Subshell (`@ {...}`)
	- [X] Lexial scoping (`let`)
	- [X] Until
	- [X] Eval
    - [X] Return
    - [X] Continue
    - [X] Break
    - [X] Fallthrough
- [ ] Full job control (need to re-implement)
	- [ ] Job table support
	- [ ] Job listing command
	- [ ] Job manipulation
	- [ ] Don't separate jobs by pipe
	- [ ] Background and foreground processes with `&`
- [ ] Usable regular expressions
    - [ ] Basic built-in regex support (`regexp`)
    - [ ] More commandline options
    - [ ] `regcomp`
- [ ] Pleasant interactive shell
	- [X] History file
	- [X] Line editor
	- [ ] Tab completion (need to re-implement)
	- [ ] Syntax highlighting
    - [X] Keybindings

As mentioned on [the Oil wiki's Alternative Shells list](https://github.com/oilshell/oil/wiki/Alternative-Shells).

## Inspirations:

* [Tcl](https://www.tcl.tk)
* [Powershell](https://learn.microsoft.com/en-us/powershell/)
* [Plan 9's rc](https://9fans.github.io/plan9port/man/man1/rc.html)
* [(t)csh](https://en.wikipedia.org/wiki/C_shell)
