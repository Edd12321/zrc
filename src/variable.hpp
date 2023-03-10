enum Mode {
	GET,
	SET,
	UNSET
};

/** Scalars **/
typedef std::string Scalar;

/** Associative array objects **/
class Array
{
private:
	std::unordered_map<std::string,
		std::string> hm;
public:
	size_t size;

	std::string
	get(std::string key)
	{
		return hm[key];
	}

	void
	set(std::string key, std::string value)
	{
		hm[key] = value;
		++size;
	}

	void
	destroy(std::string key)
	{
		hm.erase(key);
		--size;
	}
};

std::unordered_map<std::string, Scalar> s_hm;
std::unordered_map<std::string, Array>  a_hm;

/** Gets a variable's value
 * 
 * @param {std::string}a1,{std::string}a2
 * @return std::string
 */
std::string
variable_magic(std::string a1, std::string a2, Mode mode)
{
	auto it = a1.find('(');

	// Array
	if (it != std::string::npos && a1.back() == ')') {
		std::string nam = a1;
		std::string key = a1;
		nam.erase(nam.begin()+it, nam.end());
		key.erase(key.begin(), key.begin()+it+1);
		key.pop_back();

		switch(mode) {
		case GET:
			return a_hm[nam].get(key);
		case SET:
			a_hm[nam].set(key, a2);
			break;
		case UNSET:
			a_hm[nam].destroy(key);
		}


	// Scalar
	} else {
		if (a1[0] == 'E' && a1[1] == ':') {
			a1.erase(0, 2);
			switch(mode) {
			case GET:
				if (getenv(a1.data()))
					return getenv(a1.data());
				break;
			case SET:
				setenv(a1.data(), a2.data(), 1);
				break;
			case UNSET:
				unsetenv(a1.data());
			}
		} else {
			switch(mode) {
			case GET:
				return s_hm[a1];
			case SET:
				s_hm[a1] = a2;
				break;
			case UNSET:
				s_hm.erase(a1);
			}
		}
	}
	return "";
}

#define   setvar(X, Y) variable_magic(X,  Y,   SET)
#define unsetvar(X)    variable_magic(X, "", UNSET)
#define   getvar(X)    variable_magic(X, "",   GET)
