#include <filesystem>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

static void	listify(std::vector<std::string> const &vec, size_t offset = 0)
{
	std::cout << "<ul>\n";

	for (const auto &v : vec) {
		std::cout << "<li>";
		std::cout << "<a href=\"" << v.substr(offset) << "\">";
		std::cout << v.substr(offset);
		std::cout << "</a>";
		std::cout << "</li>\n";
	}
	std::cout << "</ul>\n";
}

int	main()
{
	static std::string			currentDir = std::filesystem::current_path();
	std::vector<std::string>	files;
	std::vector<std::string>	directories;

	std::cout << "<!DOCTYPE html><html><head></head><body>\n";

	for (const auto &e : std::filesystem::directory_iterator(currentDir)) {

		std::string	name = e.path().string();

		if (e.is_regular_file()) {
			auto	pos = std::upper_bound(files.begin(), files.end(), name);
			files.insert(pos, name);
		} else if (e.is_directory()) {
			auto	pos = std::upper_bound(directories.begin(), directories.end(), name);
			directories.insert(pos, name);
		}
	}

	std::cout << "<h1>Directories</h1>\n";
	listify(directories, currentDir.length() + 1);

	std::cout << "\n<h1>Files</h1>\n";
	listify(files, currentDir.length() + 1);

	std::cout << "</body></html>\n";
	return 0;
}
