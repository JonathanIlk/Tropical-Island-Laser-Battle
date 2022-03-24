// SPDX-License-Identifier: MIT
in vec3 aModel_col0;
in vec3 aModel_col1;
in vec3 aModel_col2;
in vec3 aModel_col3;
in uint aPickID;

mat4x3 uModel = mat4x3(aModel_col0, aModel_col1, aModel_col2, aModel_col3);

flat out uint vPickID;

void init_wo() {
    vPickID = aPickID;
}
