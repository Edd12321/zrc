enum Mode {
	GET,
	SET,
	UNSET
};

/** Scalars **/
typedef std::string Scalar;
typedef std::string Variable;

/** Associative array objects **/
class Array
{
public:
//private:
	std::unordered_map<std::string,
		std::string> hm;
//public:
	size_t size;

	std::string
	get(std::string key)
	{
		return (hm.find(key) != hm.end())
			? hm.at(key)
			: "";
	}

	void
	set(std::string key, std::string value)
	{
		if (hm.find(key) == hm.end())
			++size;
		hm[key] = value;
	}

	void
	destroy(std::string key)
	{
		if (hm.find(key) != hm.end()) {
			hm.erase(key);
			--size;
		}
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
			if (nam == $ENV)
				setenv(key.data(), a2.data(), 1);
			a_hm[nam].set(key, a2);
			break;
		case UNSET:
			if (nam == $ENV)
				unsetenv(key.data());
			a_hm[nam].destroy(key);
		}

	// Scalar
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
	return "";
}

#define   setvar(X, Y) variable_magic(X,  Y,   SET)
#define unsetvar(X)    variable_magic(X, "", UNSET)
#define   getvar(X)    variable_magic(X, "",   GET)
