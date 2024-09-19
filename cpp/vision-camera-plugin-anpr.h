#ifndef VISIONCAMERAPLUGINANPR_H
#define VISIONCAMERAPLUGINANPR_H

#include <jsi/jsilib.h>
#include <jsi/jsi.h>

namespace visioncamerapluginanpr {
  void installPlugin(facebook::jsi::Runtime& jsiRuntime);
  void setAlprPaths(const char* configPath, const char* runtimePath);
}

#endif /* VISIONCAMERAPLUGINANPR_H */
