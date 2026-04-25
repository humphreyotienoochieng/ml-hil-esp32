#include <Arduino.h>
#include "model.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// TFLite includes
#include "TensorFlowLite_ESP32.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// TFLite globals
const int kTensorArenaSize = 90 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Creating Semaphor
SemaphoreHandle_t xMutex;

float Kp = 0.01;
float Ki = 0.1;

float integral = 0;
float dt = 50e-6;

float vref = 12.0;
float vout = 0;
float duty = 0;

volatile int health_state = 0;
volatile float ml_confidence = 0.0f;

void PI_Task(void *pvParameters);
void ML_Task(void *pvParameters);

void setup()
{
    Serial.begin(115200);
    xMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(PI_Task, "PI", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(ML_Task, "ML", 32768, NULL, 1, NULL, 0);
}

void loop()
{
}

void PI_Task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    const TickType_t xControlPeriod = pdMS_TO_TICKS(1);

    while (true)
    {
        float local_vout = 0.0f;
        float local_duty = 0.0f;

        if (Serial.available())
        {
            local_vout = Serial.parseFloat();

            float error = vref - local_vout;

            integral += error * dt;

            local_duty = Kp * error + Ki * integral;

            // Saturation
            if (local_duty > 1.0f)
            {
                local_duty = 1.0f;
                integral -= error * dt; // anti-windup
            }

            if (local_duty < 0.0f)
            {
                local_duty = 0.0f;
                integral -= error * dt;
            }

            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE)
            {
                vout = local_vout;
                duty = local_duty;
                xSemaphoreGive(xMutex);
            }

            Serial.println(local_duty, 4);
        }

        vTaskDelayUntil(&xLastWakeTime, xControlPeriod);
    }
}

void ML_Task(void *pvParameters)
{
    /* ---------- 1. Load TFLite Model ---------- */
    const tflite::Model *model = tflite::GetModel(model_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        Serial.println("Model schema mismatch");
        vTaskDelete(NULL);
        return;
    }

    /* ---------- 2. Register Ops ---------- */
    static tflite::MicroMutableOpResolver<7> resolver;

    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();

    /* ---------- 3. Create Interpreter ---------- */
    static tflite::MicroInterpreter interpreter(
        model,
        resolver,
        tensor_arena,
        kTensorArenaSize,
        nullptr);

    /* ---------- 4. Allocate Tensors ---------- */
    TfLiteStatus allocate_status = interpreter.AllocateTensors();

    if (allocate_status != kTfLiteOk)
    {
        Serial.println("AllocateTensors failed");
        vTaskDelete(NULL);
        return;
    }

    Serial.println("TFLite Ready");

    TfLiteTensor *input = interpreter.input(0);
    TfLiteTensor *output = interpreter.output(0);

    /* ---------- 5. Confirm Quantized Model ---------- */
    Serial.print("Input type: ");
    Serial.println(input->type);

    Serial.print("Output type: ");
    Serial.println(output->type);

    Serial.print("Input bytes: ");
    Serial.println(input->bytes);

    Serial.print("Output bytes: ");
    Serial.println(output->bytes);

    Serial.print("Input dims size: ");
    Serial.println(input->dims->size);

    for (int i = 0; i < input->dims->size; i++)
    {
        Serial.println(input->dims->data[i]);
    }

    Serial.print("Output dims size: ");
    Serial.println(output->dims->size);

    for (int i = 0; i < output->dims->size; i++)
    {
        Serial.println(output->dims->data[i]);
    }

    /* ---------- 6. Rolling Sample Buffer ---------- */
    const int BUFFER_SIZE = 101;
    static float sample_buffer[BUFFER_SIZE];

    if (input->bytes < BUFFER_SIZE)
    {
        Serial.println("Input tensor too small");
        vTaskDelete(NULL);
        return;
    }

    if (output->bytes < 3)
    {
        Serial.println("Output tensor too small");
        vTaskDelete(NULL);
        return;
    }

    if (input->type != kTfLiteInt8)
    {
        Serial.println("Input is not int8");
        vTaskDelete(NULL);
        return;
    }

    if (output->type != kTfLiteInt8)
    {
        Serial.println("Output is not int8");
        vTaskDelete(NULL);
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        sample_buffer[i] = 0.0f;
    }

    int buf_index = 0;

    /* ---------- 7. Main ML Loop ---------- */
    while (true)
    {
        float local_vout = 0.0f;

        /* Read latest Vout safely */
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            local_vout = vout;
            xSemaphoreGive(xMutex);
        }

        /* Store sample */
        sample_buffer[buf_index] = local_vout;
        buf_index++;

        /* Buffer full -> inference */
        if (buf_index >= BUFFER_SIZE)
        {
            /* ---------- 8. Quantize Input ---------- */
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                float x = sample_buffer[i] / 12.0f;

                int32_t q =
                    (int32_t)(x / input->params.scale +
                              input->params.zero_point);

                if (q > 127)
                    q = 127;

                if (q < -128)
                    q = -128;

                input->data.int8[i] = (int8_t)q;
            }

            /* ---------- 9. Run Inference ---------- */
            TfLiteStatus invoke_status = interpreter.Invoke();

            if (invoke_status == kTfLiteOk)
            {
                /* ---------- 10. Dequantize Output ---------- */
                int predicted = 0;
                float best_score = -9999.0f;

                for (int i = 0; i < 3; i++)
                {
                    float score =
                        (output->data.int8[i] -
                         output->params.zero_point) *
                        output->params.scale;

                    if (score > best_score)
                    {
                        best_score = score;
                        predicted = i;
                    }
                }

                /* ---------- 11. Store Result ---------- */
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5)) == pdTRUE)
                {
                    health_state = predicted;
                    ml_confidence = best_score;
                    xSemaphoreGive(xMutex);
                }

                /* ---------- 12. Debug Print ---------- */
                if (predicted == 0)
                    Serial.println("HEALTHY");
                else if (predicted == 1)
                    Serial.println("DEGRADING");
                else
                    Serial.println("FAULTY");
            }
            else
            {
                Serial.println("Invoke failed");
            }

            /* Reset buffer */
            buf_index = 0;
        }

        /* Run slower than PI loop */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}