#define STR_KLUDGE(X) {                           \
    if (line[i] == X) {                           \
        tmp += X;                                 \
        for (++i; ; ++i) {                        \
            CHK_LINE;                             \
            if (line[i] == X) break;              \
            tmp += line[i];                       \
            if (i < (len-1) && line[i] == '\\')   \
              if (line[i+1] == X)                 \
                tmp += line[++i];                 \
              else if (line[i+1] == '\\')         \
                tmp += '\\', ++i;                 \
        }                                         \
        tmp += X;                                 \
    }                                             \
}

#define CHK_LINE {                                \
    CHK_ESC;                                      \
    while (i >= len) {                            \
        zrc_read_line(in, line, ">")              \
            or die("Unmatched bracket\n");        \
        i = 0, len = line.length(), tmp += '\n';  \
    }	                                          \
}

class WordList
{
private:
	std::unordered_map<size_t, bool> not_bareword;
	size_t len = 0;
public:
	std::vector<std::string> wl;

	inline size_t      is_bare      (size_t index) { return !not_bareword[index]; }
	inline void        make_not_bare(            ) { not_bareword[len] = true;    }
	inline std::string back         (            ) { return wl.back();            }
	inline size_t      size         (            ) { return len;                  }

	void
	add_token(std::string& tok)
	{
		if (!tok.empty()) {
			wl.emplace_back(tok);
			if (tok == ";") bg_or_fg.push_back(FG);
			if (tok == "&") bg_or_fg.push_back(BG);
			tok.clear();
			tok.reserve(DEFAULT_TOKSIZ);
			++len;
		}
	}

	void
	add_token(std::string_view tok)
	{
		std::string tmp = {tok.begin(), tok.end()};
		add_token(tmp);
	}
};

extern WordList
tokenize(std::string line, std::istream& in)
{
	std::string ltmp, tmp;
	WordList wl;
	char q, p;
	long cmpnd;
	size_t i, len2, len = line.length();

	tmp.reserve(DEFAULT_TOKSIZ);
	for (i = len2 = 0; i < len; ++i) {
		switch(line[i]) {
		// Brace quoting
		case '[':
		case '{':
		case '(':
			cmpnd = 0;
			q = line[i];
			if (q == '[') p = ']';
			if (q == '{') p = '}';
			if (q == '(') p = ')';
			
			if (!tmp.empty() && tmp.back() == '`' && q == '{') {
				tmp.pop_back();
				wl.add_token(tmp);
				tmp += "`{";
			} else if (q == '{') {
				wl.add_token(tmp);
			}

			for ever {
				CHK_LINE;
				if (line[i] == '\\'
				&& (line[i+1] == q || line[i+1] == p)) {
					tmp += "\\";
					tmp += line[i+1];
					i += 2;
					continue;
				}
				if (line[i] == q) ++cmpnd;
				if (line[i] == p) --cmpnd;
				tmp += line[i];
				if (!cmpnd) break;
				++i;
			}
			
			wl.make_not_bare();
			if (q == '{')
				wl.add_token(tmp);
			break;

		// Regular quoting
		case '\'':
		case  '"':
			q = line[i];
			STR_KLUDGE(q);
			wl.make_not_bare();
			break;

		case '#':
			for (++i; i < len && line[i] != '\n'; ++i)
				;
			[[fallthrough]];
		case  ' ':
		case '\t':
		case '\n':
		case '\r':
		case '\v':
		case '\f':
			wl.add_token(tmp);
			break;

		case ';':
			wl.add_token(tmp);
			wl.add_token(";");
			break;

		default:
			if (strchr("$\\", line[i]))
				wl.make_not_bare();
			tmp += line[i];
			if (line[i] == '\\')
				tmp += line[++i];
		}
	}
	wl.add_token(tmp);
	return wl;
}
