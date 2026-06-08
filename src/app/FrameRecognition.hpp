#pragma once

#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceServices.hpp"
#include "face/Recognizer.hpp"

#include <vector>

namespace ccamera {

std::vector<RecognitionResult> recognize_frame_faces(
    const Frame& frame,
    const std::vector<FaceDetection>& faces,
    const FaceAligner& aligner,
    FaceEmbeddingService& extractor,
    const FaceDatabaseSnapshot& database,
    const Recognizer& recognizer);

}  // namespace ccamera
