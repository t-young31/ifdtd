#include "string"
#include "utils.h"
#include "matlabio.h"
#include "tensor_init.h"

using namespace std;


void init_split_field(const mxArray *ptr, SplitField &E_s, SplitField &H_s){

  const char elements[][15] = {"Exy", "Exz", "Eyx", "Eyz", "Ezx", "Ezy",
                               "Hxy", "Hxz", "Hyx", "Hyz", "Hzx", "Hzy", "materials"};

  auto num_fields = mxGetNumberOfFields(ptr);
  if (num_fields != 13) {
    throw runtime_error("fdtdgrid should have 13 members, it only has " + to_string(num_fields));
  }

  for (int i = 0; i < num_fields; i++) {
    auto element = mxGetField((mxArray *) ptr, 0, elements[i]);
    string element_name = elements[i];

    double *array_ptr_dbl;
    unsigned char *array_ptr_uint8;

    if (mxIsDouble(element)) {
      array_ptr_dbl = mxGetPr(element);
    } else if (mxIsUint8(element)) {
      array_ptr_uint8 = (unsigned char *) mxGetPr(element);
    } else {
      throw runtime_error("Incorrect data type in fdtdgrid. " + element_name);
    }

    auto ndims = mxGetNumberOfDimensions(element);
    if (ndims != 2 && ndims != 3){
      throw runtime_error("field matrix %s should be 2- or 3-dimensional " + element_name);
    }

    auto raw_dims = mxGetDimensions(element);
    int dims[3] = {0, 0, 0};

    for (int j = 0; j < ndims; j++){
      dims[j] = raw_dims[j];
    }
    
    if (are_equal(elements[i], "Exy")) {
      E_s.xy = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Exz")) {
      E_s.xz = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Eyx")) {
      E_s.yx = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Eyz")) {
      E_s.yz = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Ezx")) {
      E_s.zx = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Ezy")) {
      E_s.zy = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hxy")) {
      H_s.xy = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hxz")) {
      H_s.xz = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hyx")) {
      H_s.yx = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hyz")) {
      H_s.yz = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hzx")) {
      H_s.zx = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "Hzy")) {
      H_s.zy = castMatlab3DArray(array_ptr_dbl, dims[0], dims[1], dims[2]);
    } else if (are_equal(elements[i], "materials")) {

      materials = castMatlab3DArrayUint8(array_ptr_uint8, dims[0], dims[1], dims[2]);
      //save this for later when freeing memory
      material_nlayers = dims[2];
      I_tot = dims[0] - 1;//The _tot variables do NOT include the additional cell at the
      J_tot = dims[1] - 1;//edge of the grid which is only partially used
      K_tot = dims[2] - 1;
    } else {
      throw runtime_error("element fdtdgrid.%s not handled " + element_name);
    }
  }
}
