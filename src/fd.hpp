/** Parses >(2), >(2=), >(2=1) syntax.
 * 
 * @param {vector<string>}&vec,{size_t}index
 * @return void
 */

extern inline bool
fd_parse(WordList& vec, size_t const& index)
{
	std::string fdstr;
	WordList fds;
	size_t i, len;
	int fd, fd1, fd2;

	for (i = 2, len = vec.wl[index].length(); i < len-1; ++i) {
		switch(vec.wl[index][i]) {
		case '=':
			if (!fdstr.empty()) {
				fds.add_token(fdstr);
				fdstr.clear();
			}
			fds.add_token("=");
			break;

		case  ' ': [[fallthrough]];
		case '\t': [[fallthrough]];
		case '\r': [[fallthrough]];
		case '\n':
			break;

		default:
			fdstr += vec.wl[index][i];
		}
	}
	if (!fdstr.empty())
		fds.add_token(fdstr);
	len = fds.size();

	//=========================
	// Case 1: Redirecting a fd
	//=========================
	if (fds.size() == 1 && is_number(fds.wl[0])) {
		fd = std::stoi(fds.wl[0]);
		baks.emplace_back(std::make_pair(dup(fd), fd));
		io_right(vec.wl[index+1], 0, fd);
		return 1;
	
	//==================================
	// Case 2: Closing a file descriptor
	//==================================
	} else if (fds.size() == 2
	       && (is_number(fds.wl[0]))
	       && (fds.wl[1] == "="))
	{
		fd = std::stoi(fds.wl[0]);
		baks.emplace_back(std::make_pair(dup(fd), fd));
		close(fd);

	//======================================
	// Case 3: Duplicating a file descriptor
	//======================================
	} else if (fds.size() == 3
	       && (is_number(fds.wl[0]))
	       && (fds.wl[1] == "=")
	       && (is_number(fds.wl[2])))
	{
		fd1 = std::stoi(fds.wl[0]);
		fd2 = std::stoi(fds.wl[2]);
		baks.emplace_back(std::make_pair(dup(fd1), fd1));
		dup2(dup(fd2), fd1);

	//=====================
	// Case 4: Syntax error
	//=====================
	} else {
		std::cerr << errmsg << "Invalid FD syntax!\n";
	}
	return 0;
}
