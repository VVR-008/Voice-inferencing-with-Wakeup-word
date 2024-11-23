# include <WakeupWord_inferencing.h>

4/* Edge Impulse Arduino examples */

#include <I2S.h>
#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16

/** Audio buffers, pointers and selectors */
typedef struct {
  int16_t *buffer;
  uint8_t buf_ready;
  uint32_t buf_count;
  uint32_t n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool debug_nn = false;  // Set this to true to debug features generated from the raw signal
static bool record_status = true;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ; // Wait for Serial connection
  Serial.println("Edge Impulse Inferencing Demo");

  // Configure the LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Turn off

  // Initialize I2S
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1)
      ;
  }

  // Print model settings
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: ");
  ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf(" ms.\n");
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  ei_printf("\nStarting continuous inference in 2 seconds...\n");
  ei_sleep(2000);

  if (!microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT)) {
    ei_printf("ERR: Could not allocate audio buffer (size %d).\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    return;
  }

  ei_printf("Recording...\n");
}
bool run_wake_word_inference() {
    if (!microphone_inference_record()) {
        ei_printf("ERR: Failed to record audio for wake word...\n");
        return false;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run wake word classifier (%d)\n", r);
        return false;
    }

    int wake_word_index = -1; // Replace with the correct index for the wake word
    float wake_word_confidence = 0.0;

    // Iterate through the classifications
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: ", result.classification[ix].label);
        ei_printf_float(result.classification[ix].value);
        ei_printf("\n");

        // Check if this is the wake word
        if (strcmp(result.classification[ix].label, "Hello Esp") == 0) { // Replace with the actual wake word label
            wake_word_index = ix;
            wake_word_confidence = result.classification[ix].value;
        }
    }

    // Determine if the wake word is detected
    if (wake_word_index >= 0 && wake_word_confidence > 0.80) {
        ei_printf("Wake word detected with confidence: ");
        ei_printf_float(wake_word_confidence);
        ei_printf("\n");
        return true;
    } else {
        ei_printf("No wake word detected.\n");
        return false;
    }
}

enum State {
    WAKE_WORD,
    VOICE_COMMAND
};

State current_state = WAKE_WORD;
void handle_command_inference_result(ei_impulse_result_t result);

void loop() {
    switch (current_state) {
        case WAKE_WORD: {
    Serial.println("Listening for wake word...");
    if (!microphone_inference_record()) {
        Serial.println("Error capturing audio for wake word inference!");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        Serial.print("Error running wake word classifier: ");
        Serial.println(r);
        return;
    }

    // Check if wake word is detected
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: ", result.classification[ix].label);
        ei_printf_float(result.classification[ix].value);
        ei_printf("\n");

        if (strcmp(result.classification[ix].label, "Hello Esp") == 0 &&
            result.classification[ix].value > 0.80) { // Adjust confidence threshold if needed
            ei_printf("Wake word detected with confidence: ");
            ei_printf_float(result.classification[ix].value);
            ei_printf("\n");
            Serial.println("Wake word detected! Transitioning to voice command detection...");

            delay(5000); // Add a 5-second delay before transitioning

            current_state = VOICE_COMMAND;
            break;
        }
    }
    break;
}

        
        case VOICE_COMMAND: {
    Serial.println("Listening for voice command...");
    if (!microphone_inference_record()) {
        Serial.println("Error capturing audio for command inference!");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        Serial.print("Error running command classifier: ");
        Serial.println(r);
        return;
    }

    // Check if a valid command is detected
    bool valid_command_detected = false;

    // Iterate through the results to find the highest score
    int best_index = -1;
    float best_score = 0.0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: ", result.classification[ix].label);
        ei_printf_float(result.classification[ix].value);
        ei_printf("\n");

        if (result.classification[ix].value > best_score) {
            best_score = result.classification[ix].value;
            best_index = ix;
        }
    }

    if (best_index >= 0 && best_score > 0.80) {
        ei_printf("Command detected: %s with confidence: ", result.classification[best_index].label);
        ei_printf_float(best_score);
        ei_printf("\n");

        // Example actions based on label
        if (strcmp(result.classification[best_index].label, "Light on") == 0) {
            digitalWrite(LED_BUILTIN, LOW); // Turn LED on
            Serial.println("Action: Turned on the device.");
            valid_command_detected = true;
        } else if (strcmp(result.classification[best_index].label, "Off light") == 0) {
            digitalWrite(LED_BUILTIN, HIGH); // Turn LED off
            Serial.println("Action: Turned off the device.");
            valid_command_detected = true;
        }
    } else {
        ei_printf("No valid command detected.\n");
    }

    // Only transition back to wake word detection if a valid command was detected
    if (valid_command_detected) {
        current_state = WAKE_WORD;
    } else {
        Serial.println("No valid command detected. Continuing to listen for voice commands...");
    }

    break;
}

    }
}

void handle_command_inference_result(ei_impulse_result_t result) {
    int best_index = -1;
    float best_score = 0.0;

    // Iterate through the results to find the highest score
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("    %s: ", result.classification[ix].label);
        ei_printf_float(result.classification[ix].value);
        ei_printf("\n");

        if (result.classification[ix].value > best_score) {
            best_score = result.classification[ix].value;
            best_index = ix;
        }
    }

    if (best_index >= 0 && best_score > 0.80) {
        ei_printf("Command detected: %s with confidence: ", result.classification[best_index].label);
        ei_printf_float(best_score);
        ei_printf("\n");

        // Example actions based on label
        if (strcmp(result.classification[best_index].label, "Light on") == 0) {
            digitalWrite(LED_BUILTIN, LOW); // Turn LED on
            Serial.println("Action: Turned on the device.");
        } else if (strcmp(result.classification[best_index].label, "Off light") == 0) {
            digitalWrite(LED_BUILTIN, HIGH); // Turn LED off
            Serial.println("Action: Turned off the device.");
        }
        // Add more actions for other commands
    } else {
        ei_printf("No valid command detected.\n");
    }
}

// Audio inference helpers (unchanged)
static void audio_inference_callback(uint32_t n_bytes) {
  for (int i = 0; i < n_bytes >> 1; i++) {
    inference.buffer[inference.buf_count++] = sampleBuffer[i];

    if (inference.buf_count >= inference.n_samples) {
      inference.buf_count = 0;
      inference.buf_ready = 1;
    }
  }
}

static void capture_samples(void *arg) {
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read;

  while (record_status) {
    esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, (void *)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

    if (bytes_read > 0) {
      for (int x = 0; x < bytes_read / 2; x++) {
        sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
      }

      if (record_status) {
        audio_inference_callback(i2s_bytes_to_read);
      }
    }
  }
  vTaskDelete(NULL);
}

static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));
  if (!inference.buffer) {
    return false;
  }

  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  ei_sleep(100);
  record_status = true;

  xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void *)sample_buffer_size, 10, NULL);
  return true;
}

static bool microphone_inference_record(void) {
  while (inference.buf_ready == 0) {
    delay(10);
  }
  inference.buf_ready = 0;
  return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
  return 0;
}

static void microphone_inference_end(void) {
  free(sampleBuffer);
  ei_free(inference.buffer);
}
