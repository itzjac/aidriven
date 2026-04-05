// $(C) Copyright by TheByteCave Inc., 2026 All Rights Reserved. $
// $Creator: Dagon Meister $
// $File: C:\projects\aidriven\games\MSClaudia\loader.cpp $

#include "loader.h"
#include <cstdio>

bool LoadMazes(const char* filename, int* data, int totalElements) {
    FILE* file = fopen(filename, "rb");
    if (!file) return false;

    size_t read = fread(data, sizeof(int), totalElements, file);
    fclose(file);

    return ((int)read == totalElements);
}

bool SaveMazes(const char* filename, const int* data, int totalElements) {
    FILE* file = fopen(filename, "wb");
    if (!file) return false;

    size_t written = fwrite(data, sizeof(int), totalElements, file);
    fclose(file);

    return ((int)written == totalElements);
}
