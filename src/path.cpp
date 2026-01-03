#include <sys/stat.h>
#include <string>
#include <unordered_map>
#include <dirent.h>
#include <strings.h>
#include "path.hpp"
#include "vars.hpp"

std::unordered_map<std::string, std::string> hctable;

std::string basename(std::string const& str) {
	auto fnd = str.rfind('/');
	if (fnd == std::string::npos)
		return str;
	return str.substr(fnd + 1);
}

/** Walk through $PATH and return its contents
 *
 * @param none
 * @return std::vector<std::string>
 */
std::unordered_map<std::string, std::string> pathwalk() {
	std::stringstream iss;
	std::string tmp;
	std::unordered_map<std::string, std::string> ret_val;
#if WINDOWS
	std::vector<std::string> pathext;
	std::set<std::string> replaceable;
	iss << vars::PATHEXT;
	while (getline(iss, tmp, ';'))
		pathext.push_back(tmp);
	iss.str(std::string());
	iss.clear();
#endif
	iss << vars::PATH;
	auto is_ok_file = [&](std::string const& name, std::string const& short_name) {
		struct stat sb;	
		return (stat(name.c_str(), &sb) == 0)
		&&     (S_ISREG(sb.st_mode))
		&&     (sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		&&     (
#if WINDOWS
		           replaceable.find(short_name) != replaceable.end() ||
#endif
		           ret_val.find(short_name) == ret_val.end()
		       );
	};
	while (getline(iss, tmp, ':')) {
		struct dirent *entry;
		DIR *d = opendir(tmp.c_str());
		if (d) {
			SCOPE_EXIT { closedir(d); };
			while ((entry = readdir(d))) {
				std::string short_name = entry->d_name;
				std::string full_name = tmp + "/" + short_name;
				if (is_ok_file(full_name, short_name)) {
					ret_val[short_name] = full_name;
#if WINDOWS
					bool has_ext = false;
					for (auto const& ext : pathext) {
						if (short_name.length() > ext.length()) {
							size_t dif = short_name.length() - ext.length();
							std::string suff = short_name.substr(dif);
							if (!strcasecmp(suff.c_str(), ext.c_str())) {
								has_ext = true;
								std::string file_no_ext = short_name.substr(0, dif);
								replaceable.insert(file_no_ext);
								ret_val[file_no_ext] = full_name;
							}
						}
					}
					if (!has_ext)
						replaceable.erase(short_name);
#endif			
				}
			}
		}
	}
	return ret_val;
}
