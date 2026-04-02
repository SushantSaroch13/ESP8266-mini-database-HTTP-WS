#pragma once
#include <LittleFS.h>

String readFile(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) return "";
  String content = file.readString();
  file.close();
  return content;
}

void writeFile(const char* path, String content) {
  File file = LittleFS.open(path, "w");
  file.print(content);
  file.close();
}
