#ifndef UTILS_FILEUTILS_H
#define UTILS_FILEUTILS_H

#include <string>
#include <vector>

std::string ResolveExistingPath(const char *filename);
bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &bytes);

#endif
