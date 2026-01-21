#ifndef PCH_HPP
#define PCH_HPP
#ifndef __OpenBSD__
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif

// System headers
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

// ARPA header
#include <arpa/inet.h>

// POSIX C headers
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <netdb.h>

// C++ standard headers
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <istream>
#include <limits>
#include <ostream>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#endif
