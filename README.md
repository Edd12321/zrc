<img src="img/zrc1.png" width=100 height=auto />
<p>
<b>This is the new, better and REWRITTEN version of Zrc (master branch)! It has way less bugs, a better codebase, nicer syntax, etc.
</b>
</p>

Zrc is a small scripting language for Linux, BSD, etc. written in C++ (interpreter + builtins in a very small ~4000SLOC). It is a shell with syntax similar to Tcl/TK ([EIAS](https://wiki.tcl-lang.org/page/everything+is+a+string)). The code is quite small, but minimalism is not the main focus of Zrc (which is why it has way more "luxury features" compared to usual shells). Instead, the idea is to be a better alternative to `tclsh` that extends its syntax and adds features like job control, a custom line editor and more, while still almost keeping Tcl's "pure syntax".

**Note**: The shell is experimental and behvaiour may change often between releases. The codebase is also intentionally written in C++11 without any compiler extensions, for compatibility reasons.

## Features left to implement:

- [X] Path hashing/caching
- [X] Cdpath support
- [X] Home directory config file (`~/.zrc`)
- [X] Aliases
- [X] **Rich return values** (not just 0-255, but any string)
- [X] Complex I/O handling (control flow, functions, built-ins and commands can be seamlessly piped together)
- [X] Getopts
- [X] Login/Logout file sourcing
- [X] Procedures/functions
	- [X] Function arguments
		- [X] `${argv "..."}` or just `$0`, `$1`, ...
		- [X] `$argc`
    - [X] Introspection (via `help`)
- [X] Globbing (via `glob`)
	- [X] Tilde expansion (on GNU systems)
	- [X] Wildcards
- [X] Signal trapping (via `trap`)
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
        - [X] String concatenation
	- [X] Command substitution
		- [X] Output (`` `{...} ``)
        - [X] Process (`<{...}`)
		- [X] Return value (`[...]`)
- [X] Built-in commands (like `expr`, etc. but there are too many to list. View `dispatch.hpp`)
- [X] Tcl-style `unknown` (ex: `fn unknown { expr [arr argv vals] }`)
- [X] Stack trace (via `caller`)
- [X] Conditional logic/flow control with full C arithmetic operator set
	- [X] If/else
    - [X] Unless
	- [X] Do
	- [X] While
    - [X] Repeat
	- [X] Foreach
	- [X] For
	- [X] Switch
    - [X] Select
    - [X] Try/catch, throw
	- [X] Subshell (`@ {...}`)
	- [X] Lexial scoping (`let`)
	- [X] Until
	- [X] Eval
    - [X] Return
    - [X] Continue
    - [X] Break
    - [X] Fallthrough
- [X] Customisable `$prompt1` and `$prompt2`
- [X] FP paradigm (`list map|filter|reduce`, `apply`)
- [X] Full job control
	- [X] Job table support
	- [X] Job listing command
	- [X] Job manipulation
	- [X] Background and foreground processes with `&`
    - [X] Disowning jobs
- [ ] Usable regular expressions
    - [X] Basic built-in regex support (`regexp`)
    - [ ] More commandline options
    - [ ] `regcomp`
- [ ] Pleasant interactive shell
	- [X] History file
        - [X] Command fixing
    - [X] Login shell session
	- [X] Line editor
	- [X] Tab completion (need to re-implement)
	- [ ] Syntax highlighting
    - [X] Keybindings

As mentioned on [the Oil wiki's Alternative Shells list](https://github.com/oilshell/oil/wiki/Alternative-Shells).

## Inspirations:

* [Tcl](https://www.tcl.tk)
* [Powershell](https://learn.microsoft.com/en-us/powershell/)
* [Plan 9's rc](https://9fans.github.io/plan9port/man/man1/rc.html)
* [es](https://wryun.github.io/es-shell/)
* [(t)csh](https://en.wikipedia.org/wiki/C_shell)
