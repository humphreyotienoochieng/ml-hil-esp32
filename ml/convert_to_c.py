import numpy as np

with open('../ml/model_quant.tflite', 'rb') as f:
    tflite_bytes = f.read()

c_array = ', '.join([f'0x{b:02x}' for b in tflite_bytes])

c_code = f"""
#ifndef MODEL_H
#define MODEL_H

const unsigned char model_tflite[] = {{
  {c_array}
}};
const int model_tflite_len = {len(tflite_bytes)};

#endif
"""

with open('C:/Users/Administrator/Desktop/VSCode/Buck_Converter/ml-hil-esp32/src/model.h', 'w') as f:
    f.write(c_code)

print("model.h generated successfully")
print(f"Array size: {len(tflite_bytes)} bytes")