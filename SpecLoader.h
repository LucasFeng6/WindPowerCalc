#ifndef SPECLOADER_H
#define SPECLOADER_H

#include "Spec.h"

class SpecLoader {
public:
    static ProjectSpecSet loadDefault();  // 从内嵌 JSON 载入
};

#endif // SPECLOADER_H
