/* Copyright 2019 The MLPerf Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef MLPERF_MLPERF_DRIVER_H_
#define MLPERF_MLPERF_DRIVER_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cpp/dataset.h"
#include "query_sample_library.h"
#include "system_under_test.h"
#include "tensorflow/lite/tools/evaluation/proto/evaluation_stages.pb.h"
#include "tensorflow/lite/tools/evaluation/stages/tflite_inference_stage.h"

namespace tflite {
namespace mlperf {

// TfliteMlperfDriver implements the SystemUnderTest and QuerySampleLibrary
// interfaces of mlperf. These are required to run mlperf inference.
class TfliteMlperfDriver : public ::mlperf::SystemUnderTest,
                           public ::mlperf::QuerySampleLibrary {
 public:
  // TfliteMlperfDriver will take control and has the responsibility to
  // delete Dataset*.
  TfliteMlperfDriver(std::string model_file_path, int num_threads,
                     std::string delegates, int expected_input_size,
                     int expected_output_size,
                     std::unique_ptr<Dataset> dataset);

  // A human-readable string for loggin purposes.
  const std::string& Name() const override { return name_; }

  // Run N samples generated by loadgen. This function blocks until completion.
  void IssueQuery(const std::vector<::mlperf::QuerySample>& samples) override;

  // Flush the staged queries immediately.
  void FlushQueries() override {}

  // Called by loadgen to show us the recorded latencies.
  void ReportLatencyResults(
      const std::vector<::mlperf::QuerySampleLatency>& latencies_ns) override {
    latencies_ns_.insert(latencies_ns_.end(), latencies_ns.begin(),
                         latencies_ns.end());
  }

  // Total number of samples in library.
  size_t TotalSampleCount() override { return dataset_->size(); }

  // The number of samples that are guaranteed to fit in RAM.
  size_t PerformanceSampleCount() override {
    return std::min<int64_t>(dataset_->size(), 1024);
  }

  // Loads the requested query samples into memory.
  void LoadSamplesToRam(
      const std::vector<::mlperf::QuerySampleIndex>& samples) override {
    dataset_->LoadSamplesToRam(samples,
                               inference_stage_->GetModelInfo()->inputs);
  };

  // Unloads the requested query samples from memory.
  void UnloadSamplesFromRam(
      const std::vector<::mlperf::QuerySampleIndex>& samples) override {
    dataset_->UnloadSamplesFromRam(samples,
                                   inference_stage_->GetModelInfo()->inputs);
  };

  // Run MLPerf tests.
  void StartMLPerfTest(std::string mode, int min_query_count, int min_duration,
                       std::string output_dir);

  // Asks the dataset to calculate the accuracy.
  std::string ComputeAccuracyString(std::string groundtruth_file) {
    return dataset_->ComputeAccuracyString(groundtruth_file);
  }

  // Forms a string to report 90 percentile latencies in ms.
  std::string ComputeLatencyString() {
    if (latencies_ns_.empty()) {
      return std::string("No data received");
    }
    std::sort(latencies_ns_.begin(), latencies_ns_.end());
    float latency =
        static_cast<float>(latencies_ns_[latencies_ns_.size() * 0.9]) / 1e6;
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << latency << " ms";
    return stream.str();
  }

 private:
  const std::string name_ = "TFLite";
  std::unique_ptr<Dataset> dataset_;
  std::vector<int64_t> latencies_ns_;
  std::unique_ptr<evaluation::TfliteInferenceStage> inference_stage_;
};

}  // namespace mlperf
}  // namespace tflite
#endif  // MLPERF_MLPERF_DRIVER_H_