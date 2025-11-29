/** Return a list from args.
 *
 * @param {int}argc,{char**}argv
 * @return zrc_obj
 */
zrc_obj list(int argc, const char *argv[]) {
	zrc_obj ret;

	for (int i = 0; i < argc; ++i) {
		bool contains_space = false;
		for (int j = 0, c; (c = argv[i][j]); ++j)
			if (isspace(c)) {
				contains_space = true;
				break;
			}
		if (contains_space)
			ret += '\'';
		if (!argv[i][0])
			ret += "\'\'";
		for (int j = 0, c; (c = argv[i][j]); ++j) {
			switch (c) {
				case  '<': /* FALLTHROUGH */
				case  '`':
					if (argv[i][j+1] == '{')
						ret += '\\';
					ret += c;
					break;

				case  '"': /* FALLTHROUGH */
				case  '[': /* FALLTHROUGH */
				case  '$': /* FALLTHROUGH */
				case '\'': /* FALLTHROUGH */ 
				case '\\':
					ret += '\\';

				default:
					ret += c;
			}
		}
		if (contains_space)
			ret += '\'';
		if (i < argc-1)
			ret += ' ';
	}
	return ret;
}

inline zrc_obj list(int argc, char *argv[]) {
	return list(argc, const_cast<const char**>(argv));
}

/* Ditto */
inline zrc_obj list(std::vector<token>& vec) {
	zrc_obj ret;
	for (size_t i = 0; i < vec.size(); ++i) {
		ret += list((std::string)vec[i]);
		if (i < vec.size() - 1)
			ret += ' ';
	}
	return ret;
}

/* Ditto */
inline zrc_obj list(std::string const& str) {
	const char *argv[] = { str.c_str(), nullptr };
	return list(1, argv);
}
