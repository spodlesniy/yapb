//
// AiPB - ONNX Runtime model runner unit tests.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <ai/ai_onnx_model_runner.h>

#include <cstring>

using ai::test::expect;

AI_TEST (testOnnxModelRunnerUnavailableWithoutRuntime) {
   ai::OnnxModelRunner runner {};

   expect (!runner.isReady (),
      "ONNX runner is unavailable until a model is loaded");
   expect (runner.getLastError () != nullptr,
      "ONNX runner exposes an error string");
}

AI_TEST (testOnnxModelRunnerRejectsMissingModel) {
   ai::OnnxModelRunner runner {};

   expect (!runner.load ("__aipb_missing_model__.onnx"),
      "missing ONNX model is rejected");
   expect (!runner.isReady (),
      "failed ONNX model load leaves runner inactive");
   expect (runner.getLastError () != nullptr && std::strlen (runner.getLastError ()) > 0,
      "failed model load records an error");
}

AI_TEST (testOnnxModelRunnerCanBeUnloaded) {
   ai::OnnxModelRunner runner {};
   runner.unload ();

   expect (!runner.isReady (),
      "unloaded ONNX runner is inactive");
}
