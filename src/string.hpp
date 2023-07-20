/** Userland interface for strings and scalars.
 * 
 * @param {int}argc,{char**}argv
 * @return std::string
 */
#define USAGE {string_usage();return "1";}
sub string_usage()
{
	std::cerr << "usage: string [index   <str> <i>       ]\n"
              << "              [length  <str>           ]\n"
	          << "              [cat     <str1> <str2>...]\n"
	          << "              [cmp     <str1> <str2>   ]\n"
	          << "              [replace <str> <i1> <c2> ]\n"
	          << "              [range   <str> <i1> <i2> ]\n";
}

extern inline std::string
string(int argc, char *argv[])
{
	int i;

	if (argc < 2)
		USAGE

	// % string index <str> <i>
	if (!strcmp(argv[1], "index")) {
		if (argc != 4)
			USAGE
		return std::string(
			1,
			argv[2][atoi(argv[3])]
		);
	}
	
	// % string length <str>
	if (!strcmp(argv[1], "length"))
		return std::to_string(strlen(argv[2]));
	
	// % string cat <str1> <str2>...
	if (!strcmp(argv[1], "cat")) {
		if (argc < 4)
			USAGE
		std::string temp;
		for (i = 2; i < argc; ++i)
			temp += argv[i];
		return temp;
	}
	
	// % string cmp <str1> <str2>
	if (!strcmp(argv[1], "cmp")) {
		if (argc != 4)
			USAGE
		return std::to_string(strcmp(argv[2], argv[3]));
	}
	
	// % string replace <i1> <c2>
	if (!strcmp(argv[1], "replace")) {
		if (argc != 5)
			USAGE
		std::string temp = argv[2];
		temp[std::stoi(argv[3])] = argv[4][0];
		return temp;
	}

	// % string range <str> <i1> <i2>
	if (!strcmp(argv[1], "range")) {
		if (argc != 5)
			USAGE
		std::string temp = argv[2];

		int i1 = atoi(argv[3]);
		int i2 = atoi(argv[4]);
		if (i1 < 0)
			i1 = 0;
		if (i2 > temp.length())
			i2 = temp.length();
		if (i2 <= i1)
			return "";

		return temp.substr(i1, i2);
	}

	return "";
}
