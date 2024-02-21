#define STR_KLUDGE(X) {                           \
    if (line[i] == X) {                           \
        tmp += X;                                 \
        for (++i; ; ++i) {                        \
            CHK_LINE;                             \
            if (line[i] == X) break;              \
            tmp += line[i];                       \
            if (i < (len-1) && line[i] == '\\') { \
              if (line[i+1] == X)                 \
                tmp += line[++i];                 \
              else if (line[i+1] == '\\')         \
                tmp += '\\', ++i;                 \
            }                                     \
        }                                         \
        tmp += X;                                 \
    }                                             \
}

#define CHK_LINE {                                \
    CHK_ESC;                                      \
    while (i >= len) {                            \
        if (!zrc_read_line(in, line, ">")) {      \
            std::cerr << errmsg << "Unmatched bracket\n";\
            return wl;                            \
        }                                         \
        i = 0, len = line.length(), tmp += '\n';  \
    }	                                            \
}

class WordList
{
private:
	size_t len = 0;
	enum WordType {
		BARE,
		SUBSTITUTED
	} curr {};
public:
	std::vector<std::string> wl;
	std::vector<WordType> nb;

	inline size_t is_bare(size_t index)
		{ return nb[index] == WordType::BARE; }
	inline void make_not_bare()
		{ curr = WordType::SUBSTITUTED; }
	inline std::string back()
		{ return wl.back(); }
	inline size_t size()
		{ return len; }

	void
	add_token(std::string& tok)
	{
		if (!tok.empty()) {
			wl.emplace_back(tok);
			nb.emplace_back(curr);
			curr = WordType::BARE;
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
	bool subs;

	tmp.reserve(DEFAULT_TOKSIZ);
	for (i = len2 = 0; i < len; ++i) {
		switch(line[i]) {
		// -----Brace quoting-----
		case '[':
		case '{':
		case '(':
			cmpnd = subs = 0;
			q = line[i];
			if (q == '[') p = ']';
			if (q == '{') p = '}';
			if (q == '(') p = ')';
			
			if (!tmp.empty() && strchr("$`<", tmp.back()) && q == '{') {
				char b = tmp.back();
				tmp.pop_back();
				tmp += b;
				subs = 1;
			} else if (q == '{') {
				wl.add_token(tmp);
			}

			for ever {
				CHK_LINE;
				tmp += line[i];
				if (line[i] == '\\') {
					tmp += line[i+1];
					i += 2;
					continue;
				}
				if (q == '(') {
					if (line[i] == '$' || line[i] == '[')
						wl.make_not_bare();
					else if (i < len-1)
						if ((line[i] == '<' || line[i] == '`') && line[i+1] == '{')
							wl.make_not_bare();
				}
				if (line[i] == q) ++cmpnd;
				if (line[i] == p) --cmpnd;
				if (!cmpnd) break;
				++i;
			}
			if (q != '(')
				wl.make_not_bare();
			if (q == '{' && !subs)
				wl.add_token(tmp);
			break;

		// -----Regular quoting-----
		case '\'':
		case  '"':
			q = line[i];
			STR_KLUDGE(q);
			wl.make_not_bare();
			break;
		
		// -----Vars-----
		case '$':
			wl.make_not_bare();
			tmp += line[i];
			break;

		// -----Comments-----
		case '#':
			for (++i; i < len && line[i] != '\n'; ++i)
				;
			/* FALLTHROUGH */

		// -----Whitespace-----
		case  ' ':
		case '\t':
		case '\n':
		case '\r':
		case '\v':
		case '\f':
			wl.add_token(tmp);
			break;

		// -----Command separators-----
		case ';':
			wl.add_token(tmp);
			wl.add_token(";");
			break;

		// -----Normal characters-----
		default:
			tmp += line[i];
			if (line[i] == '\\')
				tmp += line[++i];
		}
	}
	wl.add_token(tmp);
	return wl;
}
