#include <stdio.h>

#include "tensorflow/lite/c/c_api.h"

int main()
{
    printf(TfLiteVersion());
    return 0;
}