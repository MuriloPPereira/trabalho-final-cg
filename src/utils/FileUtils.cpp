#include "utils/FileUtils.h"

#include <fstream>

std::string ResolveExistingPath(const char *filename) {
  const std::string requested(filename);
  const std::string candidates[] = {requested,
                                    std::string("../../") + requested,
                                    std::string("../") + requested};

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    std::ifstream file(candidates[i].c_str(), std::ios::binary);
    if (file.good())
      return candidates[i];
  }

  return requested;
}

bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &bytes) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file.good())
    return false;

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  file.seekg(0, std::ios::beg);
  if (size <= 0)
    return false;

  bytes.resize((size_t)size);
  file.read((char *)bytes.data(), size);
  return file.good();
}
