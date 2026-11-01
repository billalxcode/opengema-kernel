#pragma once

// Fixed GSL paths exposed to applications. The underlying constants come
// from the shared layout header and are part of the system ABI.

#include "layout.h"

namespace gemapi::paths {

inline constexpr const char* kRoot = gsl::layout::kRoot;
inline constexpr const char* kSystem = gsl::layout::kSystem;
inline constexpr const char* kBoot = gsl::layout::kBoot;
inline constexpr const char* kInit = gsl::layout::kInit;
inline constexpr const char* kLdGem = gsl::layout::kLdGem;
inline constexpr const char* kLibraries = gsl::layout::kLibraries;
inline constexpr const char* kApplications = gsl::layout::kApplications;
inline constexpr const char* kGemabox = gsl::layout::kGemabox;
inline constexpr const char* kCommands = gsl::layout::kCommands;
inline constexpr const char* kEtc = gsl::layout::kEtc;
inline constexpr const char* kUsers = gsl::layout::kUsers;
inline constexpr const char* kTmp = gsl::layout::kTmp;
inline constexpr const char* kVar = gsl::layout::kVar;
inline constexpr const char* kDev = gsl::layout::kDev;
inline constexpr const char* kProc = gsl::layout::kProc;
inline constexpr const char* kVolumes = gsl::layout::kVolumes;
inline constexpr const char* kBootVolume = gsl::layout::kBootVolume;

}  // namespace gemapi::paths
