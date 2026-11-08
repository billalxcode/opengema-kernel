#pragma once

// Fixed GSL filesystem layout.
//
// This is part of the system ABI. Users, applications, and services cannot
// add, remove, or move these paths. Only /volumes/<name> may receive an
// additional volume, and even that mount point is fixed by the system.
//
// The kernel and user space share this header so the layout has one source
// of truth.

namespace gsl::layout {

constexpr char kRoot[] = "/";

constexpr char kSystem[] = "/system";
constexpr char kBoot[] = "/system/boot";
constexpr char kInit[] = "/system/boot/init";
constexpr char kLdGem[] = "/system/boot/ld-gem";
constexpr char kLibraries[] = "/system/libraries";
constexpr char kLibGemapi[] = "/system/libraries/libgemapi.so.1";
constexpr char kLibKgema[] = "/system/libraries/libkgema.so.1";
constexpr char kServices[] = "/system/services";
constexpr char kConfig[] = "/system/config";
constexpr char kConfigVersion[] = "/system/config/version";
constexpr char kConfigGsl[] = "/system/config/gsl.conf";
constexpr char kSystemCommands[] = "/system/commands";
constexpr char kPackages[] = "/system/packages";
constexpr char kPackageRegistry[] = "/system/packages/registry";

constexpr char kBin[] = "/bin";
constexpr char kApplications[] = "/applications";
constexpr char kGemabox[] = "/bin/gemabox";
constexpr char kCommands[] = "/commands";

constexpr char kEtc[] = "/etc";
constexpr char kUsers[] = "/users";
constexpr char kTmp[] = "/tmp";
constexpr char kVar[] = "/var";
constexpr char kVarLog[] = "/var/log";
constexpr char kVarRun[] = "/var/run";
constexpr char kVarCache[] = "/var/cache";
constexpr char kVarSpool[] = "/var/spool";

constexpr char kDev[] = "/dev";
constexpr char kProc[] = "/proc";
constexpr char kVolumes[] = "/volumes";
constexpr char kBootVolume[] = "/volumes/boot";

}  // namespace gsl::layout
