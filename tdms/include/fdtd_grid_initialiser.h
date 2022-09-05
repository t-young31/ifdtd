#pragma once
#include <vector>
#include "mat_io.h"


class fdtdGridInitialiser {

private:
    const mxArray *pointer;
    const char *mat_filename;
    std::vector<mwSize> dimensions;

    mwSize value_of_attribute(const std::string& key);

public:
    fdtdGridInitialiser(const mxArray *fdtd_pointer, const char* mat_filename);

    void add_tensor(const std::string &name);
};
