//
// AiPB - ONNX Runtime model runner implementation.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_onnx_model_runner.h>
#include <ai/ai_inference_model_contract.h>

#include <cstdio>
#include <cstring>
#include <vector>

#if defined(AIPB_WITH_ONNXRUNTIME)
#  include <onnxruntime_c_api.h>
#endif

namespace ai {

struct OnnxModelRunner::Impl {
#if defined(AIPB_WITH_ONNXRUNTIME)
   const OrtApi *api {};
   OrtEnv *env {};
   OrtSession *session {};
   OrtSessionOptions *sessionOptions {};
   OrtMemoryInfo *memoryInfo {};
   char inputName[128] {};
   char outputName[128] {};
#endif
   bool ready {};
   char lastError[256] { "ONNX Runtime backend is not enabled." };

   void setError (const char *message) {
      std::snprintf (lastError, sizeof (lastError), "%s", message != nullptr ? message : "Unknown ONNX Runtime error.");
   }

#if defined(AIPB_WITH_ONNXRUNTIME)
   bool check (OrtStatus *status) {
      if (status == nullptr) {
         return true;
      }

      setError (api->GetErrorMessage (status));
      api->ReleaseStatus (status);
      return false;
   }

   bool validateTensorContract (OrtTypeInfo *typeInfo, size_t expectedWidth) {
      const OrtTensorTypeAndShapeInfo *tensorInfo {};

      if (!check (api->CastTypeInfoToTensorInfo (typeInfo, &tensorInfo))) {
         setError ("ONNX model input/output is not a tensor.");
         return false;
      }

      ONNXTensorElementDataType elementType {};
      if (!check (api->GetTensorElementType (tensorInfo, &elementType))) {
         return false;
      }

      size_t dimensionCount {};
      if (!check (api->GetDimensionsCount (tensorInfo, &dimensionCount))) {
         return false;
      }

      std::vector<int64_t> dimensions (dimensionCount);
      if (!dimensions.empty () && !check (api->GetDimensions (tensorInfo, dimensions.data (), dimensionCount))) {
         return false;
      }

      InferenceTensorMetadata metadata {
         true,
         elementType == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
         dimensionCount,
         dimensionCount > 0 ? dimensions[0] : 0,
         dimensionCount > 1 ? dimensions[1] : 0
      };

      const auto error = validateInferenceTensor (metadata, expectedWidth);
      if (error != InferenceTensorValidationError::None) {
         switch (error) {
         case InferenceTensorValidationError::NotTensor:
            setError ("ONNX model input/output is not a tensor.");
            break;
         case InferenceTensorValidationError::WrongElementType:
            setError ("ONNX model input/output must use float32.");
            break;
         case InferenceTensorValidationError::WrongRank:
            setError ("ONNX model input/output must have rank 2.");
            break;
         case InferenceTensorValidationError::WrongBatch:
            setError ("ONNX model input/output batch dimension must be 1.");
            break;
         case InferenceTensorValidationError::WrongWidth:
            setError ("ONNX model input/output width does not match AiPB contract.");
            break;
         default:
            setError ("ONNX model tensor contract is invalid.");
            break;
         }
         return false;
      }

      return true;
   }

   void reset () {
      ready = false;

      if (api != nullptr) {
         if (memoryInfo != nullptr) {
            api->ReleaseMemoryInfo (memoryInfo);
            memoryInfo = nullptr;
         }
         if (session != nullptr) {
            api->ReleaseSession (session);
            session = nullptr;
         }
         if (sessionOptions != nullptr) {
            api->ReleaseSessionOptions (sessionOptions);
            sessionOptions = nullptr;
         }
         if (env != nullptr) {
            api->ReleaseEnv (env);
            env = nullptr;
         }
      }
   }
#else
   void reset () {
      ready = false;
   }
#endif
};

OnnxModelRunner::OnnxModelRunner ()
   : m_impl (new Impl {}) {
#if defined(AIPB_WITH_ONNXRUNTIME)
   m_impl->api = OrtGetApiBase ()->GetApi (ORT_API_VERSION);

   if (m_impl->api == nullptr) {
      m_impl->setError ("Could not acquire ONNX Runtime API.");
   }
   else {
      m_impl->setError ("ONNX model is not loaded.");
   }
#endif
}

OnnxModelRunner::~OnnxModelRunner () {
   unload ();
   delete m_impl;
}

bool OnnxModelRunner::load (const char *modelPath, const char *inputName, const char *outputName) {
   unload ();

   if (modelPath == nullptr || *modelPath == '\0') {
      m_impl->setError ("ONNX model path is empty.");
      return false;
   }

#if !defined(AIPB_WITH_ONNXRUNTIME)
   (void) inputName;
   (void) outputName;
   m_impl->setError ("ONNX Runtime backend is not enabled in this build.");
   return false;
#else
   if (m_impl->api == nullptr) {
      m_impl->setError ("ONNX Runtime API is unavailable.");
      return false;
   }

   if (inputName == nullptr || outputName == nullptr || *inputName == '\0' || *outputName == '\0') {
      m_impl->setError ("ONNX model input/output name is empty.");
      return false;
   }

   if (std::strlen (inputName) >= sizeof (m_impl->inputName)
      || std::strlen (outputName) >= sizeof (m_impl->outputName)) {
      m_impl->setError ("ONNX model input/output name is too long.");
      return false;
   }

   if (!m_impl->check (m_impl->api->CreateEnv (
      ORT_LOGGING_LEVEL_WARNING, "AiPB", &m_impl->env))) {
      return false;
   }

   if (!m_impl->check (m_impl->api->CreateSessionOptions (&m_impl->sessionOptions))) {
      unload ();
      return false;
   }

   if (!m_impl->check (m_impl->api->SetIntraOpNumThreads (m_impl->sessionOptions, 1))) {
      unload ();
      return false;
   }

   std::FILE *modelFile = std::fopen (modelPath, "rb");
   if (modelFile == nullptr) {
      m_impl->setError ("Could not open ONNX model file.");
      unload ();
      return false;
   }

   std::fseek (modelFile, 0, SEEK_END);
   const long modelSize = std::ftell (modelFile);
   std::fseek (modelFile, 0, SEEK_SET);

   if (modelSize <= 0) {
      std::fclose (modelFile);
      m_impl->setError ("ONNX model file is empty.");
      unload ();
      return false;
   }

   std::vector<uint8_t> modelData (static_cast<size_t> (modelSize));
   const size_t bytesRead = std::fread (modelData.data (), 1, modelData.size (), modelFile);
   std::fclose (modelFile);

   if (bytesRead != modelData.size ()) {
      m_impl->setError ("Could not read complete ONNX model file.");
      unload ();
      return false;
   }

   if (!m_impl->check (m_impl->api->CreateSessionFromArray (
      m_impl->env, modelData.data (), modelData.size (), m_impl->sessionOptions, &m_impl->session))) {
      unload ();
      return false;
   }

   size_t inputCount {};
   size_t outputCount {};

   if (!m_impl->check (m_impl->api->SessionGetInputCount (m_impl->session, &inputCount))
      || !m_impl->check (m_impl->api->SessionGetOutputCount (m_impl->session, &outputCount))) {
      unload ();
      return false;
   }

   if (inputCount != 1 || outputCount != 1) {
      m_impl->setError ("ONNX model must expose exactly one input and one output.");
      unload ();
      return false;
   }

   OrtTypeInfo *inputTypeInfo {};
   OrtTypeInfo *outputTypeInfo {};

   if (!m_impl->check (m_impl->api->SessionGetInputTypeInfo (m_impl->session, 0, &inputTypeInfo))) {
      unload ();
      return false;
   }

   const bool validInput = m_impl->validateTensorContract (inputTypeInfo, kInferenceFeatureCount);
   m_impl->api->ReleaseTypeInfo (inputTypeInfo);

   if (!validInput) {
      unload ();
      return false;
   }

   if (!m_impl->check (m_impl->api->SessionGetOutputTypeInfo (m_impl->session, 0, &outputTypeInfo))) {
      unload ();
      return false;
   }

   const bool validOutput = m_impl->validateTensorContract (outputTypeInfo, kInferenceActionTensorSize);
   m_impl->api->ReleaseTypeInfo (outputTypeInfo);

   if (!validOutput) {
      unload ();
      return false;
   }

   if (!m_impl->check (m_impl->api->CreateCpuMemoryInfo (
      "Cpu", OrtArenaAllocator, &m_impl->memoryInfo))) {
      unload ();
      return false;
   }

   std::snprintf (m_impl->inputName, sizeof (m_impl->inputName), "%s", inputName);
   std::snprintf (m_impl->outputName, sizeof (m_impl->outputName), "%s", outputName);

   m_impl->ready = true;
   m_impl->setError ("");
   return true;
#endif
}

void OnnxModelRunner::unload () {
   if (m_impl == nullptr) {
      return;
   }

   m_impl->reset ();

#if defined(AIPB_WITH_ONNXRUNTIME)
   m_impl->setError ("ONNX model is not loaded.");
#else
   m_impl->setError ("ONNX Runtime backend is not enabled in this build.");
#endif
}

bool OnnxModelRunner::isReady () const {
   return m_impl->ready;
}

const char *OnnxModelRunner::getLastError () const {
   return m_impl->lastError;
}

InferenceResult OnnxModelRunner::run (const InferenceFeatures &features) const {
   InferenceResult result {};

#if !defined(AIPB_WITH_ONNXRUNTIME)
   (void) features;
   result.status = InferenceStatus::Error;
   return result;
#else
   if (!m_impl->ready || m_impl->api == nullptr || m_impl->session == nullptr) {
      result.status = InferenceStatus::Error;
      return result;
   }

   constexpr int64_t inputShape[] = { 1, static_cast<int64_t> (kInferenceFeatureCount) };

   OrtValue *input = nullptr;
   OrtValue *output = nullptr;

   if (!m_impl->check (m_impl->api->CreateTensorWithDataAsOrtValue (
      m_impl->memoryInfo,
      const_cast<float *> (features.values.data ()),
      features.values.size () * sizeof (float),
      inputShape,
      2,
      ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
      &input))) {
      result.status = InferenceStatus::Error;
      return result;
   }

   const char *inputNames[] = { m_impl->inputName };
   const char *outputNames[] = { m_impl->outputName };

   OrtStatus *runStatus = m_impl->api->Run (
      m_impl->session,
      nullptr,
      inputNames,
      &input,
      1,
      outputNames,
      1,
      &output);

   m_impl->api->ReleaseValue (input);

   if (runStatus != nullptr) {
      m_impl->setError (m_impl->api->GetErrorMessage (runStatus));
      m_impl->api->ReleaseStatus (runStatus);
      result.status = InferenceStatus::Error;
      return result;
   }

   float *outputData = nullptr;
   if (!m_impl->check (m_impl->api->GetTensorMutableData (output, &outputData))) {
      m_impl->api->ReleaseValue (output);
      result.status = InferenceStatus::Error;
      return result;
   }

   result.status = InferenceStatus::Success;
   result.output.actionId = static_cast<uint8_t> (outputData[static_cast<size_t> (InferenceActionTensorIndex::ActionId)]);
   result.output.targetNode = static_cast<int32_t> (outputData[static_cast<size_t> (InferenceActionTensorIndex::TargetNode)]);
   result.output.targetPlayer = static_cast<int32_t> (outputData[static_cast<size_t> (InferenceActionTensorIndex::TargetPlayer)]);
   result.output.targetPosition = Vec3 {
      outputData[static_cast<size_t> (InferenceActionTensorIndex::TargetPositionX)],
      outputData[static_cast<size_t> (InferenceActionTensorIndex::TargetPositionY)],
      outputData[static_cast<size_t> (InferenceActionTensorIndex::TargetPositionZ)]
   };
   result.output.weaponType = static_cast<uint8_t> (outputData[static_cast<size_t> (InferenceActionTensorIndex::WeaponType)]);
   result.output.grenadeType = static_cast<uint8_t> (outputData[static_cast<size_t> (InferenceActionTensorIndex::GrenadeType)]);
   result.output.duration = outputData[static_cast<size_t> (InferenceActionTensorIndex::Duration)];
   result.output.confidence = outputData[static_cast<size_t> (InferenceActionTensorIndex::Confidence)];

   m_impl->api->ReleaseValue (output);
   return result;
#endif
}

} // namespace ai
