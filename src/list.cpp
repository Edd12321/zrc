/** Return a list from args.
 *
 * @param {int}argc,{char**}argv
 * @return zrc_obj
 */
zrc_obj list(int argc, char *argv[])
{
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
		for (int j = 0, c; (c = argv[i][j]); ++j) {
			switch (c) {
				case  '[': /* FALLTHROUGH */
				case  '$': /* FALLTHROUGH */
				case  '`': /* FALLTHROUGH */
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

/* Ditto */
zrc_obj list(std::vector<token>& vec)
{
	char **argv = new char*[vec.size()];
	for (size_t i = 0; i < vec.size(); ++i)
		argv[i] = &std::string(vec[i])[0];
	auto ret = list(vec.size(), argv);
	delete [] argv;
	return ret;
}

/* Ditto */
zrc_obj list(std::string& str)
{
	char *argv[] = { &str[0], nullptr };
	return list(1, argv);
}

/* Ditto */
zrc_obj list(std::string const& str)
{
	return list(const_cast<std::string&>(str));
}
